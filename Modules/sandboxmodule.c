#include "Python.h"
#include "structmember.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
   Destroy the sandboxed interpreter and dealloc memory.
*/
static void
sandbox_dealloc(PyObject *self)
{
    PyThreadState *sand_tstate = NULL;
    PyThreadState *cur_tstate = NULL;

    /* To destory an interpreter using Py_EndInterpreter() it must be the
       currently running interpreter.  This means you must temporariy make the
       sanboxed interpreter the running interpreter again, destroy it, and then
       swap back to the interpreter that created the interpreter in the first
       place. */
    sand_tstate = ((PySandboxObject *)self)->tstate;
    cur_tstate = PyThreadState_Swap(sand_tstate);

    Py_EndInterpreter(sand_tstate);
    PyEval_RestoreThread(cur_tstate);

    /* XXX need to do any special memory dealloc for sandboxed interpreter? */
    self->ob_type->tp_free(self);
}

/*
   Create new sandboxed interpreter.
   XXX Might be better to wait until actual execution occurs to create the interpreter.  Would make these objects one-offs, but could then change this to a function instead where you pass in the needed arguments to handle everything (which is fine since object-capabilities puts the security into __builtins__ and thus only need that)).
   XXX Could also create thread/interpreter from scratch and avoid all of this swapping of thread states (would need to do initialization that Py_NewInterpreter() did, though). */
static PyObject *
sandbox_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PySandboxObject *self;
    PyThreadState *cur_tstate;

    self = (PySandboxObject *)type->tp_alloc(type, 0);
    if (self == NULL)
	return NULL;

    /* Creating a new interpreter swaps out the current one. */
    cur_tstate = PyThreadState_GET();

    /* XXX chang eto call PySandbox_NewInterpreter() */
    if (Py_NewInterpreter() == NULL) {
	Py_DECREF(self);
	/* XXX SandboxError best exception to use here? */
	PyErr_SetString(PyExc_SandboxError, "sub-interpreter creation failed");
	return NULL;
    }

    self->tstate = PyThreadState_Swap(cur_tstate);
    if (self->tstate == NULL) {
	Py_DECREF(self);
	PyErr_SetString(PyExc_SandboxError, "sub-interpreter swap failed");
	return NULL;
    }

    return (PyObject *)self;
}


PyDoc_STRVAR(sandbox_type_doc,
"XXX\n\
\n\
XXX");

PyTypeObject PySandbox_Type = {
	PyObject_HEAD_INIT(NULL)
	0,					/* ob_size */
	"sandbox.Sandbox",			/* tp_name */
	sizeof(PySandboxObject),		/* tp_basicsize */
	0,               			/* tp_itemsize */
	sandbox_dealloc,                      	/* tp_dealloc */
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
	sandbox_type_doc,    			/* tp_doc */
	0,                      		/* tp_traverse */
	0,              			/* tp_clear */
	0,					/* tp_richcompare */
	0,                              	/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	0,      				/* tp_methods */
	0,	         			/* tp_members */
	0,		          		/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,                              	/* tp_dictoffset */
	0,					/* tp_init */
	0,					/* tp_alloc */
	sandbox_new,     			/* tp_new */
	0,                      		/* tp_free */
	0,              			/* tp_is_gc */
};


PyMODINIT_FUNC
initsandbox(void)
{
    PyObject *module;

    module = Py_InitModule3("sandbox", NULL,
	    "Provide a sandbox to safely execute Python code in.");
    if (module == NULL)
	return;

    Py_INCREF(&PySandbox_Type);
    if (PyType_Ready(&PySandbox_Type) < 0)
	return;

    if (PyModule_AddObject(module, "Sandbox", (PyObject *)&PySandbox_Type) < 0)
	return;
}

#ifdef __cplusplus
}
#endif
