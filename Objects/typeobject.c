
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
	if (type->tp_base == NULL)
		return PyTuple_New(0);
	else
		return Py_BuildValue("(O)", type->tp_base);
}

static PyObject *
type_module(PyTypeObject *type, void *context)
{
	return PyString_FromString("__builtin__");
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
	{"__module__", (getter)type_module, NULL, NULL},
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
type_call(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	char *dummy = NULL;
	PyObject *obj, *res;
	char buffer[100];

	sprintf(buffer, ":<type '%.80s'>", type->tp_name);
	if (!PyArg_ParseTupleAndKeywords(args, kwds, buffer, &dummy))
		return NULL;

	if (type->tp_construct == NULL) {
		PyErr_Format(PyExc_TypeError,
			     "cannot construct '%.100s' instances",
			     type->tp_name);
		return NULL;
	}
	obj = PyObject_New(PyObject, type);
	if (obj == NULL)
		return NULL;
	res = (type->tp_construct)(obj);
	if (res != obj) {
		Py_DECREF(obj);
		if (res == NULL)
			return NULL;
	}
	if (PyType_IS_GC(type))
		PyObject_GC_Init(res);
	return res;
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
		descr = PyDict_GetItem(type->tp_dict, name);
		if (descr != NULL && PyDescr_IsMethod(descr) &&
		    (f = descr->ob_type->tp_descr_get) != NULL)
			return (*f)(descr, NULL);
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
	(ternaryfunc)type_call,			/* tp_call */
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
add_wrappers(PyTypeObject *type, struct wrapperbase *base, void *wrapped)
{
	PyObject *dict = type->tp_dict;

	for (; base->name != NULL; base++) {
		PyObject *descr = PyDescr_NewWrapper(type, base, wrapped);
		if (descr == NULL)
			return -1;
		if (PyDict_SetItemString(dict, base->name, descr) < 0)
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

static int
inherit_slots(PyTypeObject *type, PyTypeObject *base)
{
#undef COPYSLOT
#undef COPYNUM
#undef COPYSEQ
#undef COPYMAP
#define COPYSLOT(SLOT) \
	if (!type->SLOT) type->SLOT = base->SLOT

#define COPYNUM(SLOT) COPYSLOT(tp_as_number->SLOT)
#define COPYSEQ(SLOT) COPYSLOT(tp_as_sequence->SLOT)
#define COPYMAP(SLOT) COPYSLOT(tp_as_mapping->SLOT)

	if (type->tp_as_number == NULL)
		type->tp_as_number = base->tp_as_number;
	else if (base->tp_as_number) {
		COPYNUM(nb_add);
		COPYNUM(nb_subtract);
		COPYNUM(nb_multiply);
		COPYNUM(nb_divide);
		COPYNUM(nb_remainder);
		COPYNUM(nb_divmod);
		COPYNUM(nb_power);
		COPYNUM(nb_negative);
		COPYNUM(nb_positive);
		COPYNUM(nb_absolute);
		COPYNUM(nb_nonzero);
		COPYNUM(nb_invert);
		COPYNUM(nb_lshift);
		COPYNUM(nb_rshift);
		COPYNUM(nb_and);
		COPYNUM(nb_xor);
		COPYNUM(nb_or);
		COPYNUM(nb_coerce);
		COPYNUM(nb_int);
		COPYNUM(nb_long);
		COPYNUM(nb_float);
		COPYNUM(nb_oct);
		COPYNUM(nb_hex);
		COPYNUM(nb_inplace_add);
		COPYNUM(nb_inplace_subtract);
		COPYNUM(nb_inplace_multiply);
		COPYNUM(nb_inplace_divide);
		COPYNUM(nb_inplace_remainder);
		COPYNUM(nb_inplace_power);
		COPYNUM(nb_inplace_lshift);
		COPYNUM(nb_inplace_rshift);
		COPYNUM(nb_inplace_and);
		COPYNUM(nb_inplace_xor);
		COPYNUM(nb_inplace_or);
	}

	if (type->tp_as_sequence == NULL)
		type->tp_as_sequence = base->tp_as_sequence;
	else if (base->tp_as_sequence) {
		COPYSEQ(sq_length);
		COPYSEQ(sq_concat);
		COPYSEQ(sq_repeat);
		COPYSEQ(sq_item);
		COPYSEQ(sq_slice);
		COPYSEQ(sq_ass_item);
		COPYSEQ(sq_ass_slice);
		COPYSEQ(sq_contains);
		COPYSEQ(sq_inplace_concat);
		COPYSEQ(sq_inplace_repeat);
	}

	if (type->tp_as_mapping == NULL)
		type->tp_as_mapping = base->tp_as_mapping;
	else if (base->tp_as_mapping) {
		COPYMAP(mp_length);
		COPYMAP(mp_subscript);
		COPYMAP(mp_ass_subscript);
	}

	/* Special flag magic */
	if (!type->tp_as_buffer && base->tp_as_buffer) {
		type->tp_flags &= ~Py_TPFLAGS_HAVE_GETCHARBUFFER;
		type->tp_flags |=
			base->tp_flags & Py_TPFLAGS_HAVE_GETCHARBUFFER;
	}
	if (!type->tp_as_sequence && base->tp_as_sequence) {
		type->tp_flags &= ~Py_TPFLAGS_HAVE_SEQUENCE_IN;
		type->tp_flags |= base->tp_flags & Py_TPFLAGS_HAVE_SEQUENCE_IN;
	}
	if (!(type->tp_flags & Py_TPFLAGS_GC) &&
	    (base->tp_flags & Py_TPFLAGS_GC) &&
	    (type->tp_flags & Py_TPFLAGS_HAVE_RICHCOMPARE/*GC slots exist*/) &&
	    (!type->tp_traverse && !type->tp_clear)) {
		type->tp_flags |= Py_TPFLAGS_GC;
		type->tp_basicsize += PyGC_HEAD_SIZE;
		COPYSLOT(tp_traverse);
		COPYSLOT(tp_clear);
	}
	if ((type->tp_flags & Py_TPFLAGS_HAVE_INPLACEOPS) !=
	    (base->tp_flags & Py_TPFLAGS_HAVE_INPLACEOPS)) {
		if ((!type->tp_as_number && base->tp_as_number) ||
		    (!type->tp_as_sequence && base->tp_as_sequence)) {
			type->tp_flags &= ~Py_TPFLAGS_HAVE_INPLACEOPS;
			if (!type->tp_as_number && !type->tp_as_sequence) {
				type->tp_flags |= base->tp_flags &
					Py_TPFLAGS_HAVE_INPLACEOPS;
			}
		}
		/* Wow */
	}
	if (!type->tp_as_number && base->tp_as_number) {
		type->tp_flags &= ~Py_TPFLAGS_CHECKTYPES;
		type->tp_flags |= base->tp_flags & Py_TPFLAGS_CHECKTYPES;
	}

	COPYSLOT(tp_name);
	COPYSLOT(tp_basicsize);
	COPYSLOT(tp_itemsize);
	COPYSLOT(tp_dealloc);
	COPYSLOT(tp_print);
	COPYSLOT(tp_getattr);
	COPYSLOT(tp_setattr);
	COPYSLOT(tp_compare);
	COPYSLOT(tp_repr);
	COPYSLOT(tp_hash);
	COPYSLOT(tp_call);
	COPYSLOT(tp_str);
	COPYSLOT(tp_getattro);
	COPYSLOT(tp_setattro);
	COPYSLOT(tp_as_buffer);
	COPYSLOT(tp_flags);
	COPYSLOT(tp_doc);
	if (type->tp_flags & base->tp_flags & Py_TPFLAGS_HAVE_RICHCOMPARE) {
		COPYSLOT(tp_richcompare);
	}
	if (type->tp_flags & base->tp_flags & Py_TPFLAGS_HAVE_WEAKREFS) {
		COPYSLOT(tp_weaklistoffset);
	}
	if (type->tp_flags & base->tp_flags & Py_TPFLAGS_HAVE_ITER) {
		COPYSLOT(tp_iter);
		COPYSLOT(tp_iternext);
	}
	if (type->tp_flags & base->tp_flags & Py_TPFLAGS_HAVE_CLASS) {
		COPYSLOT(tp_descr_get);
		COPYSLOT(tp_descr_set);
		COPYSLOT(tp_construct);
	}

	return 0;
}

int
PyType_InitDict(PyTypeObject *type)
{
	PyObject *dict;
	PyTypeObject *base = type->tp_base;

	if (type->tp_dict != NULL)
		return 0;
	if (base) {
		if (PyType_InitDict(base) < 0)
			return -1;
		dict = PyDict_Copy(base->tp_dict);
	}
	else
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

	/* Inherit base class slots and methods */
	if (base) {
		if (inherit_slots(type, base) < 0)
			return -1;
	}

	return 0;
}


/* Generic wrappers for overloadable 'operators' such as __getitem__ */

/* There's a wrapper *function* for each distinct function typedef used
   for type object slots (e.g. binaryfunc, ternaryfunc, etc.).  There's a
   wrapper *table* for each distinct operation (e.g. __len__, __add__).
   Most tables have only one entry; the tables for binary operators have two
   entries, one regular and one with reversed arguments. */

static PyObject *
wrap_inquiry(PyObject *self, PyObject *args, void *wrapped)
{
	inquiry func = (inquiry)wrapped;
	int res;

	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	res = (*func)(self);
	if (res == -1 && PyErr_Occurred())
		return NULL;
	return PyInt_FromLong((long)res);
}

static struct wrapperbase tab_len[] = {
	{"__len__", (wrapperfunc)wrap_inquiry, "x.__len__() <==> len(x)"},
	{0}
};

static PyObject *
wrap_binaryfunc(PyObject *self, PyObject *args, void *wrapped)
{
	binaryfunc func = (binaryfunc)wrapped;
	PyObject *other;

	if (!PyArg_ParseTuple(args, "O", &other))
		return NULL;
	return (*func)(self, other);
}

static PyObject *
wrap_binaryfunc_r(PyObject *self, PyObject *args, void *wrapped)
{
	binaryfunc func = (binaryfunc)wrapped;
	PyObject *other;

	if (!PyArg_ParseTuple(args, "O", &other))
		return NULL;
	return (*func)(other, self);
}

#undef BINARY
#define BINARY(NAME, OP) \
static struct wrapperbase tab_##NAME[] = { \
	{"__" #NAME "__", \
	 (wrapperfunc)wrap_binaryfunc, \
	 "x.__" #NAME "__(y) <==> " #OP}, \
	{"__r" #NAME "__", \
	 (wrapperfunc)wrap_binaryfunc_r, \
	 "y.__r" #NAME "__(x) <==> " #OP}, \
	{0} \
}

BINARY(add, "x+y");
BINARY(sub, "x-y");
BINARY(mul, "x*y");
BINARY(div, "x/y");
BINARY(mod, "x%y");
BINARY(divmod, "divmod(x,y)");
BINARY(lshift, "x<<y");
BINARY(rshift, "x>>y");
BINARY(and, "x&y");
BINARY(xor, "x^y");
BINARY(or, "x|y");

static PyObject *
wrap_ternaryfunc(PyObject *self, PyObject *args, void *wrapped)
{
	ternaryfunc func = (ternaryfunc)wrapped;
	PyObject *other;
	PyObject *third = Py_None;

	/* Note: This wrapper only works for __pow__() */

	if (!PyArg_ParseTuple(args, "O|O", &other, &third))
		return NULL;
	return (*func)(self, other, third);
}

#undef TERNARY
#define TERNARY(NAME, OP) \
static struct wrapperbase tab_##NAME[] = { \
	{"__" #NAME "__", \
	 (wrapperfunc)wrap_ternaryfunc, \
	 "x.__" #NAME "__(y, z) <==> " #OP}, \
	{"__r" #NAME "__", \
	 (wrapperfunc)wrap_ternaryfunc, \
	 "y.__r" #NAME "__(x, z) <==> " #OP}, \
	{0} \
}

TERNARY(pow, "(x**y) % z");

#undef UNARY
#define UNARY(NAME, OP) \
static struct wrapperbase tab_##NAME[] = { \
	{"__" #NAME "__", \
	 (wrapperfunc)wrap_unaryfunc, \
	 "x.__" #NAME "__() <==> " #OP}, \
	{0} \
}

static PyObject *
wrap_unaryfunc(PyObject *self, PyObject *args, void *wrapped)
{
	unaryfunc func = (unaryfunc)wrapped;

	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	return (*func)(self);
}

UNARY(neg, "-x");
UNARY(pos, "+x");
UNARY(abs, "abs(x)");
UNARY(nonzero, "x != 0");
UNARY(invert, "~x");
UNARY(int, "int(x)");
UNARY(long, "long(x)");
UNARY(float, "float(x)");
UNARY(oct, "oct(x)");
UNARY(hex, "hex(x)");

#undef IBINARY
#define IBINARY(NAME, OP) \
static struct wrapperbase tab_##NAME[] = { \
	{"__" #NAME "__", \
	 (wrapperfunc)wrap_binaryfunc, \
	 "x.__" #NAME "__(y) <==> " #OP}, \
	{0} \
}

IBINARY(iadd, "x+=y");
IBINARY(isub, "x-=y");
IBINARY(imul, "x*=y");
IBINARY(idiv, "x/=y");
IBINARY(imod, "x%=y");
IBINARY(ilshift, "x<<=y");
IBINARY(irshift, "x>>=y");
IBINARY(iand, "x&=y");
IBINARY(ixor, "x^=y");
IBINARY(ior, "x|=y");

#undef ITERNARY
#define ITERNARY(NAME, OP) \
static struct wrapperbase tab_##NAME[] = { \
	{"__" #NAME "__", \
	 (wrapperfunc)wrap_ternaryfunc, \
	 "x.__" #NAME "__(y) <==> " #OP}, \
	{0} \
}

ITERNARY(ipow, "x = (x**y) % z");

static struct wrapperbase tab_getitem[] = {
	{"__getitem__", (wrapperfunc)wrap_binaryfunc,
	 "x.__getitem__(y) <==> x[y]"},
	{0}
};

static PyObject *
wrap_intargfunc(PyObject *self, PyObject *args, void *wrapped)
{
	intargfunc func = (intargfunc)wrapped;
	int i;

	if (!PyArg_ParseTuple(args, "i", &i))
		return NULL;
	return (*func)(self, i);
}

static struct wrapperbase tab_mul_int[] = {
	{"__mul__", (wrapperfunc)wrap_intargfunc, "x.__mul__(n) <==> x*n"},
	{"__rmul__", (wrapperfunc)wrap_intargfunc, "x.__rmul__(n) <==> n*x"},
	{0}
};

static struct wrapperbase tab_imul_int[] = {
	{"__imul__", (wrapperfunc)wrap_intargfunc, "x.__imul__(n) <==> x*=n"},
	{0}
};

static struct wrapperbase tab_getitem_int[] = {
	{"__getitem__", (wrapperfunc)wrap_intargfunc,
	 "x.__getitem__(i) <==> x[i]"},
	{0}
};

static PyObject *
wrap_intintargfunc(PyObject *self, PyObject *args, void *wrapped)
{
	intintargfunc func = (intintargfunc)wrapped;
	int i, j;

	if (!PyArg_ParseTuple(args, "ii", &i, &j))
		return NULL;
	return (*func)(self, i, j);
}

static struct wrapperbase tab_getslice[] = {
	{"__getslice__", (wrapperfunc)wrap_intintargfunc,
	 "x.__getslice__(i, j) <==> x[i:j]"},
	{0}
};

static PyObject *
wrap_intobjargproc(PyObject *self, PyObject *args, void *wrapped)
{
	intobjargproc func = (intobjargproc)wrapped;
	int i, res;
	PyObject *value;

	if (!PyArg_ParseTuple(args, "iO", &i, &value))
		return NULL;
	res = (*func)(self, i, value);
	if (res == -1 && PyErr_Occurred())
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static struct wrapperbase tab_setitem_int[] = {
	{"__setitem__", (wrapperfunc)wrap_intobjargproc,
	 "x.__setitem__(i, y) <==> x[i]=y"},
	{0}
};

static PyObject *
wrap_intintobjargproc(PyObject *self, PyObject *args, void *wrapped)
{
	intintobjargproc func = (intintobjargproc)wrapped;
	int i, j, res;
	PyObject *value;

	if (!PyArg_ParseTuple(args, "iiO", &i, &j, &value))
		return NULL;
	res = (*func)(self, i, j, value);
	if (res == -1 && PyErr_Occurred())
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static struct wrapperbase tab_setslice[] = {
	{"__setslice__", (wrapperfunc)wrap_intintobjargproc,
	 "x.__setslice__(i, j, y) <==> x[i:j]=y"},
	{0}
};

/* XXX objobjproc is a misnomer; should be objargpred */
static PyObject *
wrap_objobjproc(PyObject *self, PyObject *args, void *wrapped)
{
	objobjproc func = (objobjproc)wrapped;
	int res;
	PyObject *value;

	if (!PyArg_ParseTuple(args, "O", &value))
		return NULL;
	res = (*func)(self, value);
	if (res == -1 && PyErr_Occurred())
		return NULL;
	return PyInt_FromLong((long)res);
}

static struct wrapperbase tab_contains[] = {
	{"__contains__", (wrapperfunc)wrap_objobjproc,
	 "x.__contains__(y) <==> y in x"},
	{0}
};

static PyObject *
wrap_objobjargproc(PyObject *self, PyObject *args, void *wrapped)
{
	objobjargproc func = (objobjargproc)wrapped;
	int res;
	PyObject *key, *value;

	if (!PyArg_ParseTuple(args, "OO", &key, &value))
		return NULL;
	res = (*func)(self, key, value);
	if (res == -1 && PyErr_Occurred())
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static struct wrapperbase tab_setitem[] = {
	{"__setitem__", (wrapperfunc)wrap_objobjargproc,
	 "x.__setitem__(y, z) <==> x[y]=z"},
	{0}
};

static PyObject *
wrap_cmpfunc(PyObject *self, PyObject *args, void *wrapped)
{
	cmpfunc func = (cmpfunc)wrapped;
	int res;
	PyObject *other;

	if (!PyArg_ParseTuple(args, "O", &other))
		return NULL;
	res = (*func)(self, other);
	if (PyErr_Occurred())
		return NULL;
	return PyInt_FromLong((long)res);
}

static struct wrapperbase tab_cmp[] = {
	{"__cmp__", (wrapperfunc)wrap_cmpfunc,
	 "x.__cmp__(y) <==> cmp(x,y)"},
	{0}
};

static struct wrapperbase tab_repr[] = {
	{"__repr__", (wrapperfunc)wrap_unaryfunc,
	 "x.__repr__() <==> repr(x)"},
	{0}
};

static PyObject *
wrap_hashfunc(PyObject *self, PyObject *args, void *wrapped)
{
	hashfunc func = (hashfunc)wrapped;
	long res;

	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	res = (*func)(self);
	if (res == -1 && PyErr_Occurred())
		return NULL;
	return PyInt_FromLong(res);
}

static struct wrapperbase tab_hash[] = {
	{"__hash__", (wrapperfunc)wrap_hashfunc,
	 "x.__hash__() <==> hash(x)"},
	{0}
};

static PyObject *
wrap_call(PyObject *self, PyObject *args, void *wrapped)
{
	ternaryfunc func = (ternaryfunc)wrapped;

	/* XXX What about keyword arguments? */
	return (*func)(self, args, NULL);
}

static struct wrapperbase tab_call[] = {
	{"__call__", (wrapperfunc)wrap_call,
	 "x.__call__(...) <==> x(...)"},
	{0}
};

static struct wrapperbase tab_str[] = {
	{"__str__", (wrapperfunc)wrap_unaryfunc,
	 "x.__str__() <==> str(x)"},
	{0}
};

static PyObject *
wrap_richcmpfunc(PyObject *self, PyObject *args, void *wrapped, int op)
{
	richcmpfunc func = (richcmpfunc)wrapped;
	PyObject *other;

	if (!PyArg_ParseTuple(args, "O", &other))
		return NULL;
	return (*func)(self, other, op);
}

#undef RICHCMP_WRAPPER
#define RICHCMP_WRAPPER(NAME, OP) \
static PyObject * \
richcmp_##NAME(PyObject *self, PyObject *args, void *wrapped) \
{ \
	return wrap_richcmpfunc(self, args, wrapped, OP); \
}

RICHCMP_WRAPPER(lt, Py_LT);
RICHCMP_WRAPPER(le, Py_LE);
RICHCMP_WRAPPER(eq, Py_EQ);
RICHCMP_WRAPPER(ne, Py_NE);
RICHCMP_WRAPPER(gt, Py_GT);
RICHCMP_WRAPPER(ge, Py_GE);

#undef RICHCMP_ENTRY
#define RICHCMP_ENTRY(NAME, EXPR) \
	{"__" #NAME "__", (wrapperfunc)richcmp_##NAME, \
	 "x.__" #NAME "__(y) <==> " EXPR}

static struct wrapperbase tab_richcmp[] = {
	RICHCMP_ENTRY(lt, "x<y"),
	RICHCMP_ENTRY(le, "x<=y"),
	RICHCMP_ENTRY(eq, "x==y"),
	RICHCMP_ENTRY(ne, "x!=y"),
	RICHCMP_ENTRY(gt, "x>y"),
	RICHCMP_ENTRY(ge, "x>=y"),
	{0}
};

static struct wrapperbase tab_iter[] = {
	{"__iter__", (wrapperfunc)wrap_unaryfunc, "x.__iter__() <==> iter(x)"},
	{0}
};

static struct wrapperbase tab_next[] = {
	{"next", (wrapperfunc)wrap_unaryfunc, "x.next() -> next value"},
	{0}
};

static int
add_operators(PyTypeObject *type)
{
	PySequenceMethods *sq;
	PyMappingMethods *mp;
	PyNumberMethods *nb;

#undef ADD
#define ADD(SLOT, TABLE) \
		if (SLOT) { \
			if (add_wrappers(type, TABLE, SLOT) < 0) \
				return -1; \
		}

	if ((sq = type->tp_as_sequence) != NULL) {
		ADD(sq->sq_length, tab_len);
		ADD(sq->sq_concat, tab_add);
		ADD(sq->sq_repeat, tab_mul_int);
		ADD(sq->sq_item, tab_getitem_int);
		ADD(sq->sq_slice, tab_getslice);
		ADD(sq->sq_ass_item, tab_setitem_int);
		ADD(sq->sq_ass_slice, tab_setslice);
		ADD(sq->sq_contains, tab_contains);
		ADD(sq->sq_inplace_concat, tab_iadd);
		ADD(sq->sq_inplace_repeat, tab_imul_int);
	}

	if ((mp = type->tp_as_mapping) != NULL) {
		if (sq->sq_length == NULL)
			ADD(mp->mp_length, tab_len);
		ADD(mp->mp_subscript, tab_getitem);
		ADD(mp->mp_ass_subscript, tab_setitem);
	}

	/* We don't support "old-style numbers" because their binary
	   operators require that both arguments have the same type;
	   the wrappers here only work for new-style numbers. */
	if ((type->tp_flags & Py_TPFLAGS_CHECKTYPES) &&
	    (nb = type->tp_as_number) != NULL) {
		ADD(nb->nb_add, tab_add);
		ADD(nb->nb_subtract, tab_sub);
		ADD(nb->nb_multiply, tab_mul);
		ADD(nb->nb_divide, tab_div);
		ADD(nb->nb_remainder, tab_mod);
		ADD(nb->nb_divmod, tab_divmod);
		ADD(nb->nb_power, tab_pow);
		ADD(nb->nb_negative, tab_neg);
		ADD(nb->nb_positive, tab_pos);
		ADD(nb->nb_absolute, tab_abs);
		ADD(nb->nb_nonzero, tab_nonzero);
		ADD(nb->nb_invert, tab_invert);
		ADD(nb->nb_lshift, tab_lshift);
		ADD(nb->nb_rshift, tab_rshift);
		ADD(nb->nb_and, tab_and);
		ADD(nb->nb_xor, tab_xor);
		ADD(nb->nb_or, tab_or);
		/* We don't support coerce() -- see above comment */
		ADD(nb->nb_int, tab_int);
		ADD(nb->nb_long, tab_long);
		ADD(nb->nb_float, tab_float);
		ADD(nb->nb_oct, tab_oct);
		ADD(nb->nb_hex, tab_hex);
		ADD(nb->nb_inplace_add, tab_iadd);
		ADD(nb->nb_inplace_subtract, tab_isub);
		ADD(nb->nb_inplace_multiply, tab_imul);
		ADD(nb->nb_inplace_divide, tab_idiv);
		ADD(nb->nb_inplace_remainder, tab_imod);
		ADD(nb->nb_inplace_power, tab_ipow);
		ADD(nb->nb_inplace_lshift, tab_ilshift);
		ADD(nb->nb_inplace_rshift, tab_irshift);
		ADD(nb->nb_inplace_and, tab_iand);
		ADD(nb->nb_inplace_xor, tab_ixor);
		ADD(nb->nb_inplace_or, tab_ior);
	}

	/* Not yet supported: __getattr__, __setattr__ */
	ADD(type->tp_compare, tab_cmp);
	ADD(type->tp_repr, tab_repr);
	ADD(type->tp_hash, tab_hash);
	ADD(type->tp_call, tab_call);
	ADD(type->tp_str, tab_str);
	ADD(type->tp_richcompare, tab_richcmp);
	ADD(type->tp_iter, tab_iter);
	ADD(type->tp_iternext, tab_next);

	return 0;
}
