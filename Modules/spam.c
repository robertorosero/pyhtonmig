#include "Python.h"

/* spamlist -- a list subtype */

typedef struct {
	PyListObject list;
	int state;
} spamlistobject;

static PyObject *
spamlist_getstate(spamlistobject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":getstate"))
		return NULL;
	return PyInt_FromLong(self->state);
}

static PyObject *
spamlist_setstate(spamlistobject *self, PyObject *args)
{
	int state;

	if (!PyArg_ParseTuple(args, "i:setstate", &state))
		return NULL;
	self->state = state;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef spamlist_methods[] = {
	{"getstate", (PyCFunction)spamlist_getstate, METH_VARARGS,
	 "getstate() -> state"},
	{"setstate", (PyCFunction)spamlist_setstate, METH_VARARGS,
	 "setstate(state)"},
	{0},
};

staticforward PyTypeObject spamlist_type;

static int
spamlist_init(spamlistobject *self, PyObject *args, PyObject *kwds)
{
	if (PyList_Type.tp_init((PyObject *)self, args, kwds) < 0)
		return -1;
	self->state = 0;
	return 0;
}

static PyTypeObject spamlist_type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"spamlist",
	sizeof(spamlistobject),
	0,
	0,					/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	0,					/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	PyGeneric_GetAttr,			/* tp_getattro */
	0,					/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,			/* tp_flags */
	0,					/* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	spamlist_methods,			/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	&PyList_Type,				/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	(initproc)spamlist_init,		/* tp_init */
	PyType_GenericAlloc,			/* tp_alloc */
	PyType_GenericNew,			/* tp_new */
};

static PyObject *
spamlist_new(void)
{
	return PyObject_CallObject((PyObject *) &spamlist_type, NULL);
}

/* spamdict -- a dictf subtype */

typedef struct {
	PyDictObject dict;
	int state;
} spamdictobject;

static PyObject *
spamdict_getstate(spamdictobject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":getstate"))
		return NULL;
	return PyInt_FromLong(self->state);
}

static PyObject *
spamdict_setstate(spamdictobject *self, PyObject *args)
{
	int state;

	if (!PyArg_ParseTuple(args, "i:setstate", &state))
		return NULL;
	self->state = state;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef spamdict_methods[] = {
	{"getstate", (PyCFunction)spamdict_getstate, METH_VARARGS,
	 "getstate() -> state"},
	{"setstate", (PyCFunction)spamdict_setstate, METH_VARARGS,
	 "setstate(state)"},
	{0},
};

staticforward PyTypeObject spamdict_type;

static int
spamdict_init(spamdictobject *self, PyObject *args, PyObject *kwds)
{
	if (PyDict_Type.tp_init((PyObject *)self, args, kwds) < 0)
		return -1;
	self->state = 0;
	return 0;
}

static PyTypeObject spamdict_type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"spamdict",
	sizeof(spamdictobject),
	0,
	0,					/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	0,					/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	PyGeneric_GetAttr,			/* tp_getattro */
	0,					/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,			/* tp_flags */
	0,					/* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	spamdict_methods,			/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	&PyDict_Type,				/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	(initproc)spamdict_init,		/* tp_init */
	PyType_GenericAlloc,			/* tp_alloc */
	PyType_GenericNew,			/* tp_new */
};

static PyObject *
spamdict_new(void)
{
	return PyObject_CallObject((PyObject *) &spamdict_type, NULL);
}

static PyObject *
spam_dict(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":dict"))
		return NULL;
	return spamdict_new();
}

/* Module spam */

static PyObject *
spam_list(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":list"))
		return NULL;
	return spamlist_new();
}

static PyMethodDef spam_functions[] = {
	{"list", spam_list, METH_VARARGS, "create a new spamlist object"},
	{"dict", spam_dict, METH_VARARGS, "create a new spamdict object"},
	{0}
};

DL_EXPORT(void)
initspam(void)
{
	PyObject *m, *d;

	m = Py_InitModule("spam", spam_functions);
	if (m == NULL)
		return;
	if (PyType_InitDict(&spamlist_type) < 0)
		return;
	if (PyType_InitDict(&spamdict_type) < 0)
		return;
	d = PyModule_GetDict(m);
	if (d == NULL)
		return;
	Py_INCREF(&spamlist_type);
	if (PyDict_SetItemString(d, "SpamListType",
				 (PyObject *) &spamlist_type) < 0)
		return;
	Py_INCREF(&spamdict_type);
	if (PyDict_SetItemString(d, "SpamDictType",
				 (PyObject *) &spamdict_type) < 0)
		return;
}
