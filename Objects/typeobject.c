
/* Type object implementation */

#include "Python.h"
#include "structmember.h"

struct memberlist type_members[] = {
	{"__name__", T_STRING, offsetof(PyTypeObject, tp_name), READONLY},
	{"__doc__", T_STRING, offsetof(PyTypeObject, tp_doc), READONLY},
	{0}
};

static PyObject *
type_bases(PyTypeObject *type, void *context)
{
	return PyTuple_New(0);
}

static PyObject *
type_dict(PyTypeObject *type, void *context)
{
	if (type->tp_dict == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return PyDictProxy_New(type->tp_dict);
}

struct getsetlist type_getsets[] = {
	{"__bases__", (getter)type_bases, NULL, NULL},
	{"__dict__",  (getter)type_dict,  NULL, NULL},
	{0}
};

static PyObject *
type_repr(PyTypeObject *type)
{
	char buf[100];
	sprintf(buf, "<type '%.80s'>", type->tp_name);
	return PyString_FromString(buf);
}

static PyObject *
type_getattro(PyTypeObject *type, PyObject *name)
{
	PyTypeObject *tp = type->ob_type; /* Usually == &PyType_Type below */
	PyObject *descr;
	descrgetfunc f;

	assert(PyString_Check(name));

	if (type->tp_flags & Py_TPFLAGS_HAVE_CLASS) {
		if (type->tp_dict == NULL) {
			if (PyType_InitDict(type) < 0)
				return NULL;
		}
	}

	if (tp->tp_flags & Py_TPFLAGS_HAVE_CLASS) {
		if (tp->tp_dict == NULL) {
			if (PyType_InitDict(tp) < 0)
				return NULL;
		}
		descr = PyDict_GetItem(tp->tp_dict, name);
		if (descr != NULL &&
		    (f = descr->ob_type->tp_descr_get) != NULL)
			return (*f)(descr, (PyObject *)type);
	}

	if (type->tp_flags & Py_TPFLAGS_HAVE_CLASS) {
		descr = PyDict_GetItem(type->tp_dict, name);
		if (descr != NULL &&
		    (f = descr->ob_type->tp_descr_get) != NULL)
			return (*f)(descr, NULL);
	}

	PyErr_Format(PyExc_AttributeError,
		     "type '%.50s' has no attribute '%.400s'",
		     type->tp_name, PyString_AS_STRING(name));
	return NULL;
}

PyTypeObject PyType_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,			/* Number of items for varobject */
	"type",			/* Name of this type */
	sizeof(PyTypeObject),	/* Basic object size */
	0,			/* Item size for varobject */
	0,					/* tp_dealloc */
	0,					/* tp_print */
	0,			 		/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	(reprfunc)type_repr,			/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	(getattrofunc)type_getattro,		/* tp_getattro */
	0,					/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,			/* tp_flags */
	"Define the behavior of a particular type of object.", /* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	0,					/* tp_methods */
	type_members,				/* tp_members */
	type_getsets,				/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
};
