
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


/* Initialize the __dict__ in a type object */

static struct PyMethodDef intrinsic_methods[] = {
	{0}
};

static struct memberlist intrinsic_members[] = {
	{"__class__", T_OBJECT, offsetof(PyObject, ob_type), READONLY},
	{0}
};

static struct getsetlist intrinsic_getsets[] = {
	{0}
};

static int
add_methods(PyTypeObject *type, PyMethodDef *meth)
{
	PyObject *dict = type->tp_dict;

	for (; meth->ml_name != NULL; meth++) {
		PyObject *descr = PyDescr_NewMethod(type, meth);
		if (descr == NULL)
			return -1;
		if (PyDict_SetItemString(dict,meth->ml_name,descr) < 0)
			return -1;
		Py_DECREF(descr);
	}
	return 0;
}

static int
add_members(PyTypeObject *type, struct memberlist *memb)
{
	PyObject *dict = type->tp_dict;

	for (; memb->name != NULL; memb++) {
		PyObject *descr = PyDescr_NewMember(type, memb);
		if (descr == NULL)
			return -1;
		if (PyDict_SetItemString(dict, memb->name, descr) < 0)
			return -1;
		Py_DECREF(descr);
	}
	return 0;
}

static int
add_getset(PyTypeObject *type, struct getsetlist *gsp)
{
	PyObject *dict = type->tp_dict;

	for (; gsp->name != NULL; gsp++) {
		PyObject *descr = PyDescr_NewGetSet(type, gsp);

		if (descr == NULL)
			return -1;
		if (PyDict_SetItemString(dict, gsp->name, descr) < 0)
			return -1;
		Py_DECREF(descr);
	}
	return 0;
}

staticforward int add_operators(PyTypeObject *);

int
PyType_InitDict(PyTypeObject *type)
{
	PyObject *dict;

	if (type->tp_dict != NULL)
		return 0;
	dict = PyDict_New();
	if (dict == NULL)
		return -1;
	type->tp_dict = dict;

	/* Add intrinsics */
	if (add_methods(type, intrinsic_methods) < 0)
		return -1;
	if (add_members(type, intrinsic_members) < 0)
		return -1;
	if (add_getset(type, intrinsic_getsets) < 0)
		return -1;
	if (add_operators(type) < 0)
		return -1;

	/* Add type-specific descriptors */
	if (type->tp_methods != NULL) {
		if (add_methods(type, type->tp_methods) < 0)
			return -1;
	}
	if (type->tp_members != NULL) {
		if (add_members(type, type->tp_members) < 0)
			return -1;
	}
	if (type->tp_getset != NULL) {
		if (add_getset(type, type->tp_getset) < 0)
			return -1;
	}
	return 0;
}


/* Generic wrappers for overloadable 'operators' such as __getitem__ */

static PyObject *
wrap_len(PyObject *self, PyObject *args)
{
	long res;

	if (!PyArg_ParseTuple(args, ":__len__"))
		return NULL;
	res = PyObject_Size(self);
	if (res < 0 && PyErr_Occurred())
		return NULL;
	return PyInt_FromLong(res);
}

static PyMethodDef tab_len[] = {
	{"__len__", wrap_len, METH_VARARGS, "XXX"},
	{0}
};

static PyObject *
wrap_add(PyObject *self, PyObject *args)
{
	PyObject *other;

	if (!PyArg_ParseTuple(args, "O:__add__", &other))
		return NULL;
	return PyNumber_Add(self, other);
}

static PyObject *
wrap_radd(PyObject *self, PyObject *args)
{
	PyObject *other;

	if (!PyArg_ParseTuple(args, "O:__radd__", &other))
		return NULL;
	return PyNumber_Add(other, self);
}

static PyMethodDef tab_sq_concat[] = {
	{"__add__", wrap_add, METH_VARARGS, "XXX"},
	{"__radd__", wrap_radd, METH_VARARGS, "XXX"},
	{0}
};

static PyObject *
wrap_mul(PyObject *self, PyObject *args)
{
	PyObject *other;

	if (!PyArg_ParseTuple(args, "O:__mul__", &other))
		return NULL;
	return PyNumber_Multiply(self, other);
}

static PyObject *
wrap_rmul(PyObject *self, PyObject *args)
{
	PyObject *other;

	if (!PyArg_ParseTuple(args, "O:__rmul__", &other))
		return NULL;
	return PyNumber_Multiply(other, self);
}

static PyMethodDef tab_sq_repeat[] = {
	{"__mul__", wrap_mul, METH_VARARGS, "XXX"},
	{"__rmul__", wrap_rmul, METH_VARARGS, "XXX"},
	{0}
};

static PyObject *
wrap_getitem(PyObject *self, PyObject *args)
{
	PyObject *key;

	if (!PyArg_ParseTuple(args, "O:__getitem__", &key))
		return NULL;
	return PyObject_GetItem(self, key);
}

static PyMethodDef tab_getitem[] = {
	{"__getitem__", wrap_getitem, METH_VARARGS, "XXX"},
	{0}
};

static PyObject *
wrap_getslice(PyObject *self, PyObject *args)
{
	int i, j;

	if (!PyArg_ParseTuple(args, "ii:__getslice__", &i, &j))
		return NULL;
	return PySequence_GetSlice(self, i, j);
}

static PyMethodDef tab_sq_slice[] = {
	{"__getslice__", wrap_getslice, METH_VARARGS, "XXX"},
	{0}
};

static PyObject *
wrap_setitem(PyObject *self, PyObject *args)
{
	PyObject *key, *value;

	if (!PyArg_ParseTuple(args, "OO:__setitem__", &key, &value))
		return NULL;
	if (PyObject_SetItem(self, key, value) < 0)
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef tab_setitem[] = {
	{"__setitem__", wrap_setitem, METH_VARARGS, "XXX"},
	{0}
};

static PyObject *
wrap_setslice(PyObject *self, PyObject *args)
{
	int i, j;
	PyObject *value;

	if (!PyArg_ParseTuple(args, "iiO:__setslice__", &i, &j, &value))
		return NULL;
	if (PySequence_SetSlice(self, i, j, value) < 0)
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef tab_setslice[] = {
	{"__setslice__", wrap_setslice, METH_VARARGS, "XXX"},
	{0}
};

static PyObject *
wrap_contains(PyObject *self, PyObject *args)
{
	PyObject *value;
	long res;

	if (!PyArg_ParseTuple(args, "O:__contains__", &value))
		return NULL;
	res = PySequence_Contains(self, value);
	if (res < 0 && PyErr_Occurred())
		return NULL;
	return PyInt_FromLong(res);
}

static PyMethodDef tab_contains[] = {
	{"__contains__", wrap_contains, METH_VARARGS, "XXX"},
	{0}
};

static PyObject *
wrap_iadd(PyObject *self, PyObject *args)
{
	PyObject *other;

	if (!PyArg_ParseTuple(args, "O:__iadd__", &other))
		return NULL;
	return PyNumber_InPlaceAdd(self, other);
}

static PyMethodDef tab_iadd[] = {
	{"__iadd__", wrap_iadd, METH_VARARGS, "XXX"},
	{0}
};

static PyObject *
wrap_imul(PyObject *self, PyObject *args)
{
	PyObject *other;

	if (!PyArg_ParseTuple(args, "O:__imul__", &other))
		return NULL;
	return PyNumber_InPlaceMultiply(self, other);
}

static PyMethodDef tab_imul[] = {
	{"__imul__", wrap_imul, METH_VARARGS, "XXX"},
	{0}
};

static int
add_operators(PyTypeObject *type)
{
	PySequenceMethods *sq;
	PyMappingMethods *mp;

#undef ADD
#define ADD(SLOT, TABLE) \
		if (SLOT) { \
			if (add_methods(type, TABLE) < 0) \
				return -1; \
		}

	if ((sq = type->tp_as_sequence) != NULL) {
		ADD(sq->sq_length, tab_len);
		ADD(sq->sq_concat, tab_sq_concat);
		ADD(sq->sq_repeat, tab_sq_repeat);
		ADD(sq->sq_item, tab_getitem);
		ADD(sq->sq_slice, tab_sq_slice);
		ADD(sq->sq_ass_item, tab_setitem);
		ADD(sq->sq_ass_slice, tab_setslice);
		ADD(sq->sq_contains, tab_contains);
		ADD(sq->sq_inplace_concat, tab_iadd);
		ADD(sq->sq_inplace_repeat, tab_imul);
	}
	if ((mp = type->tp_as_mapping) != NULL) {
		ADD(mp->mp_length, tab_len);
		ADD(mp->mp_subscript, tab_getitem);
		ADD(mp->mp_ass_subscript, tab_setitem);
	}
	return 0;
}
