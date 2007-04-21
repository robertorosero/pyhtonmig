#include "Python.h"
#include "interpreter.h"
#include "cStringIO.h"

static struct PycStringIO_CAPI* PycStringIO = NULL;

#define PyInterpreter_GET_INTERP(interp) \
	(((PyInterpreterObject *)interp)->istate)

/*
   Destroy the interpreter and dealloc memory.
*/
static void
interpreter_dealloc(PyObject *self)
{
    PyThreadState *new_tstate = NULL;
    PyThreadState *cur_tstate = NULL;

    /* To destory an interpreter using Py_EndInterpreter() it must be the
       currently running interpreter.  This means you must temporariy make the
       created interpreter the running interpreter again, destroy it, and then
       swap back to the interpreter that created the interpreter in the first
       place. */
    new_tstate = ((PyInterpreterObject *)self)->tstate;
    cur_tstate = PyThreadState_Swap(new_tstate);

    Py_EndInterpreter(new_tstate);
    PyEval_RestoreThread(cur_tstate);

    self->ob_type->tp_free(self);
}

/*
   Create a new interpreter.
*/
static PyObject *
interpreter_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyInterpreterObject *self;
    PyThreadState *cur_tstate;

    self = (PyInterpreterObject *)type->tp_alloc(type, 0);
    if (self == NULL)
	return NULL;

    /* Creating a new interpreter swaps out the current one. */
    cur_tstate = PyThreadState_GET();

    if (Py_NewInterpreter() == NULL) {
	Py_DECREF(self);
	/* XXX Exception best exception to use here? */
	PyErr_SetString(PyExc_Exception, "sub-interpreter creation failed");
	return NULL;
    }

    self->tstate = PyThreadState_Swap(cur_tstate);
    if (self->tstate == NULL) {
	Py_DECREF(self);
	PyErr_SetString(PyExc_Exception, "sub-interpreter swap failed");
	return NULL;
    }
    self->istate = (self->tstate)->interp;

    return (PyObject *)self;
}

/*
 Getter for 'builtins'.
 
 There is not setter because the creation of a new interpreter automatically
 creates the initial execution frame which caches the built-in namespace.
 */
static PyObject *
interpreter_builtins(PyObject *self)
{
    PyObject *builtins = PyInterpreter_GET_INTERP(self)->builtins;
    
    Py_INCREF(builtins);
    return builtins;
}

/*
 Getter for 'sys_dict'.
 
 There is no setter because the dict gets cached somewhere.
 */
static PyObject *
interpreter_sys_dict(PyObject *self)
{
    PyObject *sys_dict = PyInterpreter_GET_INTERP(self)->sysdict;
    
    Py_INCREF(sys_dict);
    return sys_dict;
}

/*
   Execute Python source code in the interpreter.
*/
static PyObject *
interpreter_exec(PyInterpreterObject *self, PyObject *arg)
{
    const char *str_arg = NULL;
    PyThreadState* cur_tstate = NULL;
    PyObject *main_module = NULL;
    PyObject *main_dict = NULL;
    PyObject *result = NULL;
    const char *exc_name = NULL;

    if (!PyString_Check(arg)) {
	PyErr_SetString(PyExc_TypeError, "argument must be a string");
	return NULL;
    }

    str_arg = PyString_AsString(arg);
    if (!str_arg)
	return NULL;

    /* Execute in 'self'. */
    cur_tstate = PyThreadState_Swap(self->tstate);
    
    /* If a previous exception was present, clear it out. */
    if (PyErr_Occurred())
        PyErr_Clear();
    
    /* Code borrowed from PyRun_SimpleStringFlags(). */
    main_module = PyImport_AddModule("__main__");
    if (!main_module) {
        goto back_to_caller;
    }
    
    main_dict = PyModule_GetDict(main_module);

    result = PyRun_String(str_arg, Py_file_input, main_dict, main_dict);
    
    if (result) {
        Py_DECREF(result);
    }
    else {
        exc_name = ((PyTypeObject *)PyErr_Occurred())->tp_name;
    }

 back_to_caller:
    /* Execute in calling interpreter. */
    PyThreadState_Swap(cur_tstate);

    if (!result) {
	PyErr_Format(PyExc_RuntimeError,
			"execution raised during execution (%s)", exc_name);
	return NULL;
    }
    
    Py_RETURN_NONE;
}

static PyObject *
redirect_output(PyObject *self, PyObject *args)
{
    PyObject *py_stdout = NULL;
    PyObject *py_stderr = NULL;
    PyObject *used_stdout_stderr = NULL;
    
    if (!PyArg_ParseTuple(args, "|OO", &py_stdout, &py_stderr))
        return NULL;
    
    if (!py_stdout) {
        /* Argument for NewOutput() copied from PycStringIO->NewInput(). */
        py_stdout = (PycStringIO->NewOutput)(512);
        if (!py_stdout)
            return NULL;
    }
    
    if (!py_stderr) {
        /* Argument for NewOutput() copied from PycStringIO->NewInput(). */
        py_stderr = (PycStringIO->NewOutput)(512);
        if (!py_stderr)
            return NULL;
    }
    
    used_stdout_stderr = PyTuple_New(2);
    if (!used_stdout_stderr)
        return NULL;
    
    if (PyDict_SetItemString(PyInterpreter_GET_INTERP(self)->sysdict, "stdout",
                         py_stdout) < 0)
        return NULL;
    if (PyDict_SetItemString(PyInterpreter_GET_INTERP(self)->sysdict, "stderr",
                         py_stderr) < 0)
        return NULL;
    
    Py_INCREF(py_stdout);
    Py_INCREF(py_stderr);
    PyTuple_SET_ITEM(used_stdout_stderr, 0, py_stdout);
    PyTuple_SET_ITEM(used_stdout_stderr, 1, py_stderr);
    
    return used_stdout_stderr;
}

static PyObject *
exc_matches(PyInterpreterObject *self, PyObject *arg)
{
    PyThreadState *starting_tstate = NULL;
    PyObject *raised_exc = NULL;
    int result = 0;
    
    /* Can only compare against exception classes or instances. */
    if (!(PyExceptionClass_Check(arg) || PyExceptionInstance_Check(arg))) {
        PyErr_SetString(PyExc_TypeError,
                        "argument must be an exception class or instance");
        return NULL;
    }
    
    /* Now executing under 'self'. */
    starting_tstate = PyThreadState_Swap(self->tstate);
    
    raised_exc = PyErr_Occurred();
    
    if (!raised_exc) {
        /* Executing under calling interpreter. */
        PyThreadState_Swap(starting_tstate);
        PyErr_SetString(PyExc_LookupError, "no exception set");
        return NULL;
    }
    
    if (PyErr_GivenExceptionMatches(raised_exc, arg))
        result = 1;
    
    /* Execute under calling interpreter. */
    PyThreadState_Swap(starting_tstate);
    
    if (result)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyMethodDef interpreter_methods[] = {
    {"builtins", (PyCFunction)interpreter_builtins, METH_NOARGS,
        "Return the built-in namespace dict."},
    {"sys_dict", (PyCFunction)interpreter_sys_dict, METH_NOARGS,
        "Return the 'sys' module's data dictionary."},
    {"execute", (PyCFunction)interpreter_exec, METH_O,
	"Execute the passed-in string in the interpreter."},
    {"redirect_output", (PyCFunction)redirect_output, METH_VARARGS,
        "Redirect stdout to stderr.  Returns tuple of objects used."},
    {"exc_matches", (PyCFunction)exc_matches, METH_O,
    "Check if the raised exception in the interpreter matches the argument"},
    {NULL}
};


/*
   Getter for 'modules'.
*/
static PyObject *
interpreter_get_modules(PyObject *self, void *optional)
{
	PyObject *modules = PyInterpreter_GET_INTERP(self)->modules;

	Py_INCREF(modules);
	return modules;
}

/*
   Setter for 'modules'.
 
 Assumes no one has stored away a reference to sys.modules .  Since sys.modules
 is set during interpreter creation, its reference is not updated unless done so
 explicitly.
 
 One option would have been to take the approach of builtins(), which is to have
 a function that returns the initial dict and thus prevent complete dict
 replacement.  But since assigning to sys.modules directly has not effect,
 assuming people would treat sys.modules as something to not store away seemed
 reasonable.
*/
static int
interpreter_set_modules(PyObject *self, PyObject *arg, void *optional)
{
	PyObject *old_modules = PyInterpreter_GET_INTERP(self)->modules;

	if (!PyDict_CheckExact(arg)) {
		PyErr_SetString(PyExc_TypeError,
				"'modules' must be set to a dict");
		return -1;
	}

	Py_INCREF(arg);
	Py_DECREF(old_modules);
	PyInterpreter_GET_INTERP(self)->modules = arg;
        PyDict_SetItemString(PyInterpreter_GET_INTERP(self)->sysdict,
                             "modules", arg);

	return 0;
}


static PyGetSetDef interpreter_getset[] = {
	{"modules", interpreter_get_modules, interpreter_set_modules,
		"The dict used for sys.modules.", NULL},
	{NULL}
};


PyDoc_STRVAR(interpreter_type_doc,
"XXX\n\
\n\
XXX");

static PyTypeObject PyInterpreter_Type = {
	PyObject_HEAD_INIT(NULL)
	0,					/* ob_size */
	"interpreterInterpreter",		/* tp_name */
	sizeof(PyInterpreterObject),		/* tp_basicsize */
	0,               			/* tp_itemsize */
	interpreter_dealloc,                    /* tp_dealloc */
	0,					/* tp_print */
	0,			 		/* tp_getattr */
	0,					/* tp_setattr */
	0,       				/* tp_compare */
	0,               			/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,                      		/* tp_hash */
	0,              			/* tp_call */
	0,					/* tp_str */
	0,                      		/* tp_getattro */
	0,	                        	/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT |
	    Py_TPFLAGS_BASETYPE,     		/* tp_flags */
	interpreter_type_doc,    		/* tp_doc */
	0,                      		/* tp_traverse */
	0,              			/* tp_clear */
	0,					/* tp_richcompare */
	0,                              	/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	interpreter_methods,      		/* tp_methods */
	0,	         			/* tp_members */
	interpreter_getset,          		/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,                              	/* tp_dictoffset */
	0,					/* tp_init */
	0,					/* tp_alloc */
	interpreter_new,     			/* tp_new */
	0,                      		/* tp_free */
	0,              			/* tp_is_gc */
};


PyMODINIT_FUNC
initinterpreter(void)
{
    PyObject *module;

    module = Py_InitModule3("interpreter", NULL,
	    "Create other Python interpreters to execute code within.");
    if (module == NULL)
	return;

    Py_INCREF(&PyInterpreter_Type);
    if (PyType_Ready(&PyInterpreter_Type) < 0)
	return;

    if (PyModule_AddObject(module, "Interpreter",
			    (PyObject *)&PyInterpreter_Type) < 0)
	return;
    
    PycString_IMPORT;
    if (!PycStringIO)
        return;
}
