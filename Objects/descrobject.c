/* Descriptors -- a new, flexible way to describe attributes */

#include "Python.h"
#include "structmember.h" /* Why is this not included in Python.h? */

struct wrapperdescr {
	struct wrapperbase *base;
	void *wrapped; /* This can be any function pointer */
};

/* Descriptor object */
struct PyDescrObject {
	PyObject_HEAD
	int d_flavor;
	PyTypeObject *d_type;
	union {
		PyMethodDef *d_method;
		struct memberlist *d_member;
		struct getsetlist *d_getset;
		struct wrapperdescr d_wrapper;
	} d_union;
};

/* Descriptor flavors */
#define DF_NULL		0
#define DF_METHOD	1
#define DF_MEMBER	2
#define DF_GETSET	3
#define DF_WRAPPER	4

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
	case DF_WRAPPER:
		return descr->d_union.d_wrapper.base->name;
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
	case DF_WRAPPER:
		sprintf(buffer,
			"<wrapper '%.300s' of '%.100s' objects>",
			descr->d_union.d_wrapper.base->name,
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
descr_get(PyDescrObject *descr, PyObject *obj)
{
	if (obj == NULL) {
		Py_INCREF(descr);
		return (PyObject *)descr;
	}
 
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

	case DF_WRAPPER:
		return PyWrapper_New(descr, obj);

	}

	PyErr_Format(PyExc_NotImplementedError,
		     "PyDescr_Get() not implemented for descriptor type %d "
		     "of '%.50s' object",
		     descr->d_flavor, obj->ob_type->tp_name);
	return NULL;
}

int
descr_set(PyDescrObject *descr, PyObject *obj, PyObject *value)
{
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
	case DF_WRAPPER:
		PyErr_Format(PyExc_TypeError,
			     "can't %s method attribute '%.400s' "
			     "of '%.50s' object",
			     value==NULL ? "delete" : "assign to",
			     descr_name(descr),
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
				"descriptor call needs a self argument");
		return NULL;
	}
	self = PyTuple_GET_ITEM(args, 0);
	if (!PyObject_IsInstance(self, (PyObject *)(descr->d_type))) {
		char *name = descr_name(descr);
		if (name == NULL)
			name = "?";
		PyErr_Format(PyExc_TypeError,
			     "descriptor '%.100s' "
			     "requires a '%.100s', "
			     "received a '%.100s'",
			     name,
			     descr->d_type->tp_name,
			     self->ob_type->tp_name);
		return NULL;
	}

	if (descr->d_flavor == DF_METHOD || descr->d_flavor == DF_WRAPPER) {
		PyObject *func, *result;
		if (descr->d_flavor == DF_METHOD)
			func = PyCFunction_New(descr->d_union.d_method, self);
		else
			func = PyWrapper_New(descr, self);
		if (func == NULL)
			return NULL;
		args = PyTuple_GetSlice(args, 1, argc);
		if (args == NULL) {
			Py_DECREF(func);
			return NULL;
		}
		result = PyEval_CallObjectWithKeywords(func, args, kwds);
		Py_DECREF(args);
		Py_DECREF(func);
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
		return descr_get(descr, self);
	if (argc == 2) {
		PyObject *value = PyTuple_GET_ITEM(args, 1);
		if (descr_set(descr, self, value) < 0)
			return NULL;
		Py_INCREF(Py_None);
		return Py_None;
	}
	PyErr_SetString(PyExc_TypeError,
			"too many arguments to descriptor call");
	return NULL;
}

static PyObject *
descr_get_api(PyDescrObject *descr, PyObject *args)
{
	PyObject *obj;

	if (!PyArg_ParseTuple(args, "O:get", &obj))
		return NULL;
	return descr_get(descr, obj);
}

static PyObject *
descr_set_api(PyDescrObject *descr, PyObject *args)
{
	PyObject *obj, *val;

	if (!PyArg_ParseTuple(args, "OO:set", &obj, &val))
		return NULL;
	if (descr_set(descr, obj, val) < 0)
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef descr_methods[] = {
	{"get",		(PyCFunction)descr_get_api,	METH_VARARGS},
	{"set",		(PyCFunction)descr_set_api,	METH_VARARGS},
	{"call",	(PyCFunction)descr_call,
	                                  METH_KEYWORDS|METH_VARARGS},
	{"bind",	(PyCFunction)descr_get_api,	METH_VARARGS},
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
	{"name",	(getter)descr_get_name},
	{"__name__",	(getter)descr_get_name},
	{"doc",		(getter)descr_get_doc},
	{"__doc__",	(getter)descr_get_doc},
	{"kind",	(getter)descr_get_kind},
	{"readonly",	(getter)descr_get_readonly},
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
	PyObject_GenericGetAttr,		/* tp_getattro */
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
	(descrgetfunc)descr_get,		/* tp_descr_get */
	(descrsetfunc)descr_set,		/* tp_descr_set */
};

int
PyDescr_IsMethod(PyObject *d)
{
	if (PyDescr_Check(d)) {
		switch (((PyDescrObject *)d)->d_flavor) {
		case DF_METHOD:
		case DF_WRAPPER:
			return 1;
		}
	}
	return 0;
}

int
PyDescr_IsData(PyObject *d)
{
	if (PyDescr_Check(d)) {
		switch (((PyDescrObject *)d)->d_flavor) {
		case DF_MEMBER:
		case DF_GETSET:
			return 1;
		}
	}
	return 0;
}

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

	if (descr != NULL) {
		descr->d_union.d_method = method;
		descr->d_flavor = DF_METHOD;
	}
	return (PyObject *)descr;
}

PyObject *
PyDescr_NewMember(PyTypeObject *type, struct memberlist *member)
{
	PyDescrObject *descr = PyDescr_New(type);

	if (descr != NULL) {
		descr->d_union.d_member = member;
		descr->d_flavor = DF_MEMBER;
	}
	return (PyObject *)descr;
}

PyObject *
PyDescr_NewGetSet(PyTypeObject *type, struct getsetlist *getset)
{
	PyDescrObject *descr = PyDescr_New(type);

	if (descr != NULL) {
		descr->d_union.d_getset = getset;
		descr->d_flavor = DF_GETSET;
	}
	return (PyObject *)descr;
}

PyObject *
PyDescr_NewWrapper(PyTypeObject *type,
		   struct wrapperbase *base, void *wrapped)
{
	PyDescrObject *descr = PyDescr_New(type);

	if (descr != NULL) {
		descr->d_union.d_wrapper.base = base;
		descr->d_union.d_wrapper.wrapped = wrapped;
		descr->d_flavor = DF_WRAPPER;
	}
	return (PyObject *)descr;
}


/* --- Readonly proxy for dictionaries (actually any mapping) --- */

/* This has no reason to be in this file except that adding new files is a
   bit of a pain */

typedef struct {
	PyObject_HEAD
	PyObject *dict;
} proxyobject;

static int
proxy_len(proxyobject *pp)
{
	return PyObject_Size(pp->dict);
}

static PyObject *
proxy_getitem(proxyobject *pp, PyObject *key)
{
	return PyObject_GetItem(pp->dict, key);
}

static PyMappingMethods proxy_as_mapping = {
	(inquiry)proxy_len,			/* mp_length */
	(binaryfunc)proxy_getitem,		/* mp_subscript */
	0,					/* mp_ass_subscript */
};

static int
proxy_contains(proxyobject *pp, PyObject *key)
{
	return PySequence_Contains(pp->dict, key);
}

static PySequenceMethods proxy_as_sequence = {
	0,					/* sq_length */
	0,					/* sq_concat */
	0,					/* sq_repeat */
	0,					/* sq_item */
	0,					/* sq_slice */
	0,					/* sq_ass_item */
	0,					/* sq_ass_slice */
	(objobjproc)proxy_contains,		/* sq_contains */
	0,					/* sq_inplace_concat */
	0,					/* sq_inplace_repeat */
};

static PyObject *
proxy_has_key(proxyobject *pp, PyObject *args)
{
	PyObject *key;

	if (!PyArg_ParseTuple(args, "O:has_key", &key))
		return NULL;
	return PyInt_FromLong(PySequence_Contains(pp->dict, key));
}

static PyObject *
proxy_get(proxyobject *pp, PyObject *args)
{
	PyObject *key, *def = Py_None;

	if (!PyArg_ParseTuple(args, "O|O:get", &key, &def))
		return NULL;
	return PyObject_CallMethod(pp->dict, "get", "(OO)", key, def);
}

static PyObject *
proxy_keys(proxyobject *pp, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":keys"))
		return NULL;
	return PyMapping_Keys(pp->dict);
}

static PyObject *
proxy_values(proxyobject *pp, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":values"))
		return NULL;
	return PyMapping_Values(pp->dict);
}

static PyObject *
proxy_items(proxyobject *pp, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":items"))
		return NULL;
	return PyMapping_Items(pp->dict);
}

static PyObject *
proxy_copy(proxyobject *pp, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":copy"))
		return NULL;
	return PyObject_CallMethod(pp->dict, "copy", NULL);
}

static PyMethodDef proxy_methods[] = {
	{"has_key", (PyCFunction)proxy_has_key, METH_VARARGS, "XXX"},
	{"get",	    (PyCFunction)proxy_get,     METH_VARARGS, "XXX"},
	{"keys",    (PyCFunction)proxy_keys,    METH_VARARGS, "XXX"},
	{"values",  (PyCFunction)proxy_values,  METH_VARARGS, "XXX"},
	{"items",   (PyCFunction)proxy_items,   METH_VARARGS, "XXX"},
	{"copy",    (PyCFunction)proxy_copy,    METH_VARARGS, "XXX"},
	{0}
};

static void
proxy_dealloc(proxyobject *pp)
{
	Py_DECREF(pp->dict);
	PyObject_DEL(pp);
}

static PyObject *
proxy_getiter(proxyobject *pp)
{
	return PyObject_GetIter(pp->dict);
}

PyObject *
proxy_str(proxyobject *pp)
{
	return PyObject_Str(pp->dict);
}

PyTypeObject proxytype = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,					/* ob_size */
	"dict-proxy",				/* tp_name */
	sizeof(proxyobject),			/* tp_basicsize */
	0,					/* tp_itemsize */
	/* methods */
	(destructor)proxy_dealloc, 		/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	0,					/* tp_repr */
	0,					/* tp_as_number */
	&proxy_as_sequence,			/* tp_as_sequence */
	&proxy_as_mapping,			/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	(reprfunc)proxy_str,			/* tp_str */
	PyObject_GenericGetAttr,		/* tp_getattro */
	0,					/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,			/* tp_flags */
 	0,					/* tp_doc */
 	0,					/* tp_traverse */
 	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	(getiterfunc)proxy_getiter,		/* tp_iter */
	0,					/* tp_iternext */
	proxy_methods,				/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
};

PyObject *
PyDictProxy_New(PyObject *dict)
{
	proxyobject *pp;

	pp = PyObject_NEW(proxyobject, &proxytype);
	if (pp != NULL) {
		Py_INCREF(dict);
		pp->dict = dict;
	}
	return (PyObject *)pp;
}


/* --- Wrapper object for "slot" methods --- */

/* This has no reason to be in this file except that adding new files is a
   bit of a pain */

typedef struct {
	PyObject_HEAD
	PyDescrObject *descr;
	PyObject *self;
} wrapperobject;

static void
wrapper_dealloc(wrapperobject *wp)
{
	Py_DECREF(wp->descr);
	Py_DECREF(wp->self);
	PyObject_DEL(wp);
}

static PyMethodDef wrapper_methods[] = {
	{0}
};

static PyObject *
wrapper_name(wrapperobject *wp)
{
	char *s = wp->descr->d_union.d_wrapper.base->name;

	return PyString_FromString(s);
}

static PyObject *
wrapper_doc(wrapperobject *wp)
{
	char *s = wp->descr->d_union.d_wrapper.base->doc;

	if (s == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	else {
		return PyString_FromString(s);
	}
}

static struct getsetlist wrapper_getsets[] = {
	{"__name__", (getter)wrapper_name},
	{"__doc__", (getter)wrapper_doc},
	{0}
};

static PyObject *
wrapper_call(wrapperobject *wp, PyObject *args, PyObject *kwds)
{
	wrapperfunc wrapper = wp->descr->d_union.d_wrapper.base->wrapper;
	PyObject *self = wp->self;

	return (*wrapper)(self, args, wp->descr->d_union.d_wrapper.wrapped);
}

PyTypeObject wrappertype = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,					/* ob_size */
	"method-wrapper",			/* tp_name */
	sizeof(wrapperobject),			/* tp_basicsize */
	0,					/* tp_itemsize */
	/* methods */
	(destructor)wrapper_dealloc, 		/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	0,					/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,		       			/* tp_as_mapping */
	0,					/* tp_hash */
	(ternaryfunc)wrapper_call,		/* tp_call */
	0,					/* tp_str */
	PyObject_GenericGetAttr,		/* tp_getattro */
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
	wrapper_methods,			/* tp_methods */
	0,					/* tp_members */
	wrapper_getsets,			/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
};

PyObject *
PyWrapper_New(PyDescrObject *descr, PyObject *self)
{
	wrapperobject *wp;

	assert(descr->d_flavor == DF_WRAPPER);
	assert(PyObject_IsInstance(self, (PyObject *)(descr->d_type)));

	wp = PyObject_NEW(wrapperobject, &wrappertype);
	if (wp != NULL) {
		Py_INCREF(descr);
		wp->descr = descr;
		Py_INCREF(self);
		wp->self = self;
	}
	return (PyObject *)wp;
}
