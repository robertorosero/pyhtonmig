/* Descriptors -- a new, flexible way to describe attributes */

#include "Python.h"
#include "structmember.h" /* Why is this not included in Python.h? */

/* Descriptor object */
struct PyDescrObject {
	PyObject_HEAD
	int d_flavor;
	PyTypeObject *d_type;
	union {
		PyMethodDef *d_method;
		struct memberlist *d_member;
		struct getsetlist *d_getset;
	} d_union;
};

/* Descriptor flavors */
#define DF_NULL		0
#define DF_METHOD	1
#define DF_MEMBER	2
#define DF_GETSET	3

static void
descr_dealloc(PyDescrObject *descr)
{
	PyObject_DEL(descr);
}

static char *
descr_name(PyDescrObject *descr)
{
	switch (descr->d_flavor) {
	case DF_METHOD:
		return descr->d_union.d_method->ml_name;
	case DF_MEMBER:
		return descr->d_union.d_member->name;
	case DF_GETSET:
		return descr->d_union.d_getset->name;
	default:
		return NULL;
	}
}

static PyObject *
descr_repr(PyDescrObject *descr)
{
	char buffer[500];

	switch (descr->d_flavor) {
	case DF_NULL:
		sprintf(buffer,
			"<null descriptor of '%.100s' objects>",
			descr->d_type->tp_name);
		break;
	case DF_METHOD:
		sprintf(buffer,
			"<method '%.300s' of '%.100s' objects>",
			descr->d_union.d_method->ml_name,
			descr->d_type->tp_name);
		break;
	case DF_MEMBER:
		sprintf(buffer,
			"<member '%.300s' of '%.100s' objects>",
			descr->d_union.d_member->name,
			descr->d_type->tp_name);
		break;
	case DF_GETSET:
		sprintf(buffer,
			"<attribute '%.300s' of '%.100s' objects>",
			descr->d_union.d_getset->name,
			descr->d_type->tp_name);
		break;
	default:
		sprintf(buffer, "<flavor %d attribute of '%.100s' objects>",
			descr->d_flavor,
			descr->d_type->tp_name);
		break;
	}

	return PyString_FromString(buffer);
}

static PyObject *
descr_call(PyDescrObject *descr, PyObject *args, PyObject *kwds)
{
	int argc;
	PyObject *self;

	/* Make sure that the first argument is acceptable as 'self' */
	assert(PyTuple_Check(args));
	argc = PyTuple_GET_SIZE(args);
	if (argc < 1) {
		PyErr_SetString(PyExc_TypeError,
				"descriptor call needs 'self' argument");
		return NULL;
	}
	self = PyTuple_GET_ITEM(args, 0);
	if (!PyObject_IsInstance(self, (PyObject *)(descr->d_type))) {
		PyErr_Format(PyExc_TypeError,
			     "descriptor call 'self' is type '%.100s', "
			     "expected type '%.100s'",
			     self->ob_type->tp_name,
			     descr->d_type->tp_name);
		return NULL;
	}

	if (descr->d_flavor == DF_METHOD) {
		PyObject *func, *result;
		func = PyCFunction_New(descr->d_union.d_method, self);
		if (func == NULL)
			return NULL;
		args = PyTuple_GetSlice(args, 1, argc);
		if (args == NULL)
			return NULL;
		result = PyEval_CallObjectWithKeywords(func, args, kwds);
		Py_DECREF(args);
		return result;
	}

	/* Make sure there are no keyword arguments */
	if (kwds != NULL) {
		assert(PyDict_Check(kwds));
		if (PyDict_Size(kwds) > 0) {
			PyErr_SetString(PyExc_TypeError,
					"this descriptor object can't called "
					"called with keyword arguments");
			return NULL;
		}
	}

	if (argc == 1)
		return PyDescr_Get((PyObject *)descr, self);
	if (argc == 2) {
		PyObject *value = PyTuple_GET_ITEM(args, 1);
		if (PyDescr_Set((PyObject *)descr, self, value) < 0)
			return NULL;
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyErr_SetString(PyExc_TypeError,
			"too many arguments to descriptor call");
	return NULL;
}

static PyObject *
descr_get(PyObject *descr, PyObject *args)
{
	PyObject *obj;

	if (!PyArg_ParseTuple(args, "O:get", &obj))
		return NULL;
	return PyDescr_Get(descr, obj);
}

static PyObject *
descr_set(PyObject *descr, PyObject *args)
{
	PyObject *obj, *val;

	if (!PyArg_ParseTuple(args, "OO:set", &obj, &val))
		return NULL;
	if (PyDescr_Set(descr, obj, val) < 0)
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef descr_methods[] = {
	{"get",		descr_get,	METH_VARARGS},
	{"set",		descr_set,	METH_VARARGS},
	{"call",	descr_call,	METH_VARARGS|METH_KEYWORDS},
	{"bind",	descr_get,	METH_VARARGS},
	{0}
};

static PyObject *
descr_get_name(PyDescrObject *descr)
{
	char *s = descr_name(descr);

	if (s != NULL)
		return PyString_FromString(s);
	PyErr_SetString(PyExc_AttributeError, "unnamed descriptor");
	return NULL;
}

static PyObject *
descr_get_doc(PyDescrObject *descr) {
	char *s = NULL;

	if (descr->d_flavor == DF_METHOD)
		s = descr->d_union.d_method->ml_doc;

	if (s != NULL)
		return PyString_FromString(s);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
descr_get_kind(PyDescrObject *descr) {
	char *s = "data";

	if (descr->d_flavor == DF_METHOD)
		s = "method";
	return PyString_FromString(s);
}

static PyObject *
descr_get_readonly(PyDescrObject *descr) {
	int readonly = 1;

	switch (descr->d_flavor) {
	case DF_MEMBER:
		readonly = descr->d_union.d_member->readonly;
		break;
	case DF_GETSET:
		readonly = descr->d_union.d_getset->set == NULL;
		break;
	}
	return PyInt_FromLong(readonly);
}

static struct getsetlist descr_getsets[] = {
	{"name",	descr_get_name},
	{"__name__",	descr_get_name},
	{"doc",		descr_get_doc},
	{"__doc__",	descr_get_doc},
	{"kind",	descr_get_kind},
	{"readonly",	descr_get_readonly},
	{0}
};

static struct memberlist descr_members[] = {
	{"objclass",	T_OBJECT, offsetof(struct PyDescrObject, d_type),
			READONLY},
	{"_flavor",	T_INT, offsetof(struct PyDescrObject, d_flavor),
			READONLY},
	{0}
};

PyTypeObject PyDescr_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"descriptor",
	sizeof(PyDescrObject),
	0,
	(destructor)descr_dealloc,		/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	(reprfunc)descr_repr,			/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	(ternaryfunc)descr_call,		/* tp_call */
	0,					/* tp_str */
	0,					/* tp_getattro */
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
	descr_methods,				/* tp_methods */
	descr_members,				/* tp_members */
	descr_getsets,				/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
};

static PyDescrObject *
PyDescr_New(PyTypeObject *type)
{
	PyDescrObject *descr =  PyObject_NEW(PyDescrObject, &PyDescr_Type);

	if (descr == NULL)
		return NULL;
	descr->d_type = type;
	descr->d_flavor = DF_NULL;
	return descr;
}

PyObject *
PyDescr_NewMethod(PyTypeObject *type, PyMethodDef *method)
{
	PyDescrObject *descr = PyDescr_New(type);

	if (descr == NULL)
		return NULL;
	descr->d_union.d_method = method;
	descr->d_flavor = DF_METHOD;
	return (PyObject *)descr;
}

PyObject *
PyDescr_NewMember(PyTypeObject *type, struct memberlist *member)
{
	PyDescrObject *descr = PyDescr_New(type);

	if (descr == NULL)
		return NULL;
	descr->d_union.d_member = member;
	descr->d_flavor = DF_MEMBER;
	return (PyObject *)descr;
}

PyObject *
PyDescr_NewGetSet(PyTypeObject *type, struct getsetlist *getset)
{
	PyDescrObject *descr = PyDescr_New(type);

	if (descr == NULL)
		return NULL;
	descr->d_union.d_getset = getset;
	descr->d_flavor = DF_GETSET;
	return (PyObject *)descr;
}

PyObject *
PyDescr_Get(PyObject *d, PyObject *obj)
{
	PyDescrObject *descr;

	if (obj == NULL || !PyDescr_Check(d)) {
		Py_INCREF(d);
		return d;
	}

	descr = (PyDescrObject *)d;
 
	if (!PyObject_IsInstance(obj, (PyObject *)(descr->d_type))) {
		PyErr_Format(PyExc_TypeError,
			     "descriptor for '%.100s' objects "
			     "doesn't apply to '%.100s' object",
			     descr->d_type->tp_name,
			     obj->ob_type->tp_name);
		return NULL;
	}

	switch (descr->d_flavor) {

	case DF_METHOD:
		return PyCFunction_New(descr->d_union.d_method, obj);

	case DF_MEMBER:
		return PyMember_Get((char *)obj,
				    descr->d_union.d_member,
				    descr->d_union.d_member->name);

	case DF_GETSET:
		if (descr->d_union.d_getset->get != NULL)
			return descr->d_union.d_getset->get(
				obj, descr->d_union.d_getset->closure);

	}

	PyErr_Format(PyExc_NotImplementedError,
		     "PyDescr_Get() not implemented for descriptor type %d "
		     "of '%.50s' object",
		     descr->d_flavor, obj->ob_type->tp_name);
	return NULL;
}

int
PyDescr_Set(PyObject *d, PyObject *obj, PyObject *value)
{
	PyDescrObject *descr = (PyDescrObject *)d;

	assert(PyDescr_Check(d));

	if (!PyObject_IsInstance(obj, (PyObject *)(descr->d_type))) {
		PyErr_Format(PyExc_TypeError,
			     "descriptor for '%.100s' objects "
			     "doesn't apply to '%.100s' object",
			     descr->d_type->tp_name,
			     obj->ob_type->tp_name);
		return -1;
	}

	switch (descr->d_flavor) {

	case DF_METHOD:
		PyErr_Format(PyExc_TypeError,
			     "can't %s method attribute '%.400s' "
			     "of '%.50s' object",
			     value==NULL ? "delete" : "assign to",
			     descr->d_union.d_method->ml_name,
			     obj->ob_type->tp_name);
		return -1;

	case DF_MEMBER:
		return PyMember_Set((char *)obj,
				    descr->d_union.d_member,
				    descr->d_union.d_member->name,
				    value);

	case DF_GETSET:
		if (descr->d_union.d_getset->set == NULL) {
			PyErr_Format(PyExc_TypeError,
				     "can't %s read-only attribute "
				     "'%.400s' of '%.50s' object",
				     value==NULL ? "delete" : "assign to",
				     descr->d_union.d_getset->name,
				     obj->ob_type->tp_name);
			return -1;
		}
		return descr->d_union.d_getset->set(
			obj, value, descr->d_union.d_getset->closure);

	}

	PyErr_Format(PyExc_NotImplementedError,
		     "PyDescr_Set() not implemented for descriptor type %d "
		     "of '%.50s' object",
		     descr->d_flavor, obj->ob_type->tp_name);
	return -1;
}


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
	/* Add intrinsics */
	if (add_methods(type, intrinsic_methods) < 0)
		return -1;
	if (add_members(type, intrinsic_members) < 0)
		return -1;
	if (add_getset(type, intrinsic_getsets) < 0)
		return -1;
	return 0;
}
