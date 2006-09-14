#include "Python.h"
#include "interpreter.h"

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
   Execute Python source code in the interpreter.
*/
static PyObject *
interpreter_exec(PyObject *self, PyObject *arg)
{
    PyInterpreterObject *interp_self = (PyInterpreterObject *)self;
    const char *str_arg = NULL;
    PyThreadState* cur_tstate = NULL;
    int result = 0;

    if (!PyString_Check(arg)) {
	PyErr_SetString(PyExc_TypeError, "argument must be a string");
	return NULL;
    }

    str_arg = PyString_AsString(arg);
    if (!str_arg)
	return NULL;

    cur_tstate = PyThreadState_Swap(interp_self->tstate);

    result = PyRun_SimpleString(str_arg);
    if (result < 0) {
	PyErr_Clear();
    }

    PyThreadState_Swap(cur_tstate);

    if (result < 0) {
	PyErr_SetString(PyExc_Exception,
			"exception during execution in interpreter.");
	return NULL;
    }

    Py_RETURN_NONE;
}

static PyMethodDef interpreter_methods[] = {
    {"execute", interpreter_exec, METH_O,
	"Execute the passed-in string in the interpreter"},
    {NULL}
};


/*
   Getter for 'builtins'.
*/
static PyObject *
interpreter_get_builtins(PyObject *self, void *optional)
{
	PyObject *builtins = PyInterpreter_GET_INTERP(self)->builtins;

	Py_INCREF(builtins);
	return builtins;
}

/*
   Setter for 'builtins'.
*/
static int
interpreter_set_builtins(PyObject *self, PyObject *arg, void *optional)
{
	PyObject *old_builtins = PyInterpreter_GET_INTERP(self)->builtins;

	if (!PyDict_CheckExact(arg)) {
		PyErr_SetString(PyExc_TypeError,
				"'builtins' must be set to a dict");
		return -1;
	}

	Py_INCREF(arg);
	Py_XDECREF(old_builtins);
	PyInterpreter_GET_INTERP(self)->builtins = arg;

	return 0;
}

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
	Py_XDECREF(old_modules);
	PyInterpreter_GET_INTERP(self)->modules = arg;

	return 0;
}


static PyGetSetDef interpreter_getset[] = {
	{"builtins", interpreter_get_builtins, interpreter_set_builtins,
		"The built-ins dict for the interpreter.", NULL},
	/*{"sys_dict", XXX, XXX, "The modules dict for 'sys'.", NULL},*/
	{"modules", interpreter_get_modules, interpreter_set_modules,
		"The dict used for sys.modules.", NULL},
	{NULL}
};


PyDoc_STRVAR(interpreter_type_doc,
"XXX\n\
\n\
XXX");

PyTypeObject PyInterpreter_Type = {
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
}
