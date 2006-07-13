#include "Python.h"

#ifdef __cplusplus
extern "C" {
#endif

PyDoc_STRVAR(sandbox_type_doc,
"XXX\n\
\n\
XXX");

PyTypeObject PySandbox_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,					/* ob_size */
	"sandbox.Sandbox",			/* tp_name */
	sizeof(PySandboxObject),		/* tp_basicsize */
	0,               			/* tp_itemsize */
	0,                      		/* tp_dealloc */
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
	Py_TPFLAGS_DEFAULT,     		/* tp_flags */
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
	0,      				/* tp_new */
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

    PySandbox_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PySandbox_Type) < 0)
	return;

    if (PyModule_AddObject(module, "Sandbox", (PyObject *)&PySandbox_Type) < 0)
	return;
}

#ifdef __cplusplus
}
#endif
