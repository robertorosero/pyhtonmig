
/* Type object implementation */

#include "Python.h"
#include "structmember.h"

staticforward int add_members(PyTypeObject *, struct memberlist *);

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
	int size;
	void *mem;
	PyObject *obj, *res;

	if (type->tp_construct == NULL) {
		PyErr_Format(PyExc_TypeError,
			     "cannot construct '%.100s' instances",
			     type->tp_name);
		return NULL;
	}

	/* Inline PyObject_New() so we can zero the memory */
	size = _PyObject_SIZE(type);
	mem = PyObject_MALLOC(size);
	if (mem == NULL)
		return PyErr_NoMemory();
	memset(mem, '\0', size);
	if (PyType_IS_GC(type))
		obj = PyObject_FROM_GC(mem);
	else
		obj = (PyObject *)mem;
	if (type->tp_flags & Py_TPFLAGS_HEAPTYPE)
		Py_INCREF(type);
	PyObject_INIT(obj, type);

	res = (type->tp_construct)(obj, args, kwds);
	if (res == NULL) {
		Py_DECREF(obj);
		return NULL;
	}
	if (PyType_IS_GC(type))
		PyObject_GC_Init(res);
	return res;
}

/* Helper for subtyping */

static void
subtype_dealloc(PyObject *self)
{
	int dictoffset = self->ob_type->tp_dictoffset;
	PyTypeObject *base;
	destructor f;

	/* This exists so we can DECREF self->ob_type */

	base = self->ob_type->tp_base;
	while ((f = base->tp_dealloc) == subtype_dealloc)
		base = base->tp_base;
	if (dictoffset && !base->tp_dictoffset) {
		PyObject **dictptr = (PyObject **) ((char *)self + dictoffset);
		PyObject *dict = *dictptr;
		if (dict != NULL) {
			Py_DECREF(dict);
			*dictptr = NULL;
		}
	}
	f(self);
	if (self->ob_type->tp_flags & Py_TPFLAGS_HEAPTYPE) {
		Py_DECREF(self->ob_type);
	}
}

staticforward void override_slots(PyTypeObject *type, PyObject *dict);

typedef struct {
	PyTypeObject type;
	PyNumberMethods as_number;
	PySequenceMethods as_sequence;
	PyMappingMethods as_mapping;
	PyBufferProcs as_buffer;
	PyObject *name, *slots;
	struct memberlist members[1];
} etype;

/* TypeType's constructor is called when a type is subclassed */
static PyObject *
type_construct(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *name, *bases, *dict, *x, *slots;
	PyTypeObject *base;
	char *dummy = NULL;
	etype *et;
	struct memberlist *mp;
	int i, nslots, slotoffset, allocsize;

	/* Check arguments */
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "SOO", &dummy,
					 &name, &bases, &dict))
		return NULL;
	if (!PyTuple_Check(bases) || !PyDict_Check(dict)) {
		PyErr_SetString(PyExc_TypeError,
				"usage: TypeType(name, bases, dict) ");
		return NULL;
	}
	if (PyTuple_GET_SIZE(bases) > 1) {
		PyErr_SetString(PyExc_TypeError,
				"can't multiple-inherit from types");
		return NULL;
	}
	if (PyTuple_GET_SIZE(bases) < 1) {
		PyErr_SetString(PyExc_TypeError,
				"can't create a new type without a base type");
		return NULL;
	}
	base = (PyTypeObject *)PyTuple_GET_ITEM(bases, 0);
	if (!PyType_Check((PyObject *)base)) {
		PyErr_SetString(PyExc_TypeError,
				"base type must be a type");
		return NULL;
	}
	if (base->tp_construct == NULL) {
		PyErr_SetString(PyExc_TypeError,
				"base type must have a constructor slot");
		return NULL;
	}

	/* Check for a __slots__ sequence variable in dict, and count it */
	slots = PyDict_GetItemString(dict, "__slots__");
	nslots = 0;
	if (slots != NULL) {
		/* Make it into a tuple */
		if (PyString_Check(slots))
			slots = Py_BuildValue("(O)", slots);
		else
			slots = PySequence_Tuple(slots);
		if (slots == NULL)
			return NULL;
		nslots = PyTuple_GET_SIZE(slots);
		for (i = 0; i < nslots; i++) {
			if (!PyString_Check(PyTuple_GET_ITEM(slots, i))) {
				PyErr_SetString(PyExc_TypeError,
				"__slots__ must be a sequence of strings");
				Py_DECREF(slots);
				return NULL;
			}
		}
	}
	if (slots == NULL && base->tp_dictoffset == 0 &&
	    (base->tp_setattro == PyGeneric_SetAttr ||
	     base->tp_setattro == NULL))
		nslots = 1;

	/* Allocate memory and construct a type object in it */
	allocsize = sizeof(etype) + nslots*sizeof(struct memberlist);
	if (type == NULL) {
		et = PyObject_MALLOC(allocsize);
		if (et == NULL)
			return NULL;
		memset(et, '\0', allocsize);
		type = &et->type;
		PyObject_INIT(type, &PyType_Type);
	}
	else {
		if (type->ob_type->tp_basicsize < allocsize) {
			PyErr_Format(
				PyExc_SystemError,
				"insufficient allocated memory for subtype: "
				"allocated %d, needed %d",
				type->ob_type->tp_basicsize,
				allocsize);
			return NULL;
		}
		et = (etype *)type;
	}
	Py_INCREF(name);
	et->name = name;
	et->slots = slots;
	type->tp_as_number = &et->as_number;
	type->tp_as_sequence = &et->as_sequence;
	type->tp_as_mapping = &et->as_mapping;
	type->tp_as_buffer = &et->as_buffer;
	type->tp_name = PyString_AS_STRING(name);
	type->tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE;

	/* Copy slots and dict from the base type */
	Py_INCREF(base);
	type->tp_base = base;
	if (PyType_InitDict(type) < 0) {
		Py_DECREF(type);
		return NULL;
	}

	/* Override some slots with specific requirements */
	if (type->tp_dealloc)
		type->tp_dealloc = subtype_dealloc;
	if (type->tp_getattro == NULL) {
		type->tp_getattro = PyGeneric_GetAttr;
		type->tp_getattr = NULL;
	}
	if (type->tp_setattro == NULL) {
		type->tp_setattro = PyGeneric_SetAttr;
		type->tp_setattr = NULL;
	}

	/* Add custom slots */
	mp = et->members;
	slotoffset = type->tp_basicsize;
	if (type->tp_flags & Py_TPFLAGS_GC)
		slotoffset -= PyGC_HEAD_SIZE;
	if (slots != NULL) {
		for (i = 0; i < nslots; i++, mp++) {
			mp->name = PyString_AS_STRING(
				PyTuple_GET_ITEM(slots, i));
			mp->type = T_OBJECT;
			mp->offset = slotoffset + i*sizeof(PyObject *);
		}
		type->tp_basicsize += nslots*sizeof(PyObject *);
	}
	else if (nslots) {
		type->tp_dictoffset = slotoffset;
		type->tp_basicsize += sizeof(PyObject *);
		mp->name = "__dict__";
		mp->type = T_OBJECT;
		mp->offset = slotoffset;
		mp->readonly = 1;
	}
	add_members(type, et->members);

	x = PyObject_CallMethod(type->tp_dict, "update", "O", dict);
	if (x == NULL) {
		Py_DECREF(type);
		return NULL;
	}
	Py_DECREF(x); /* throw away None */
	override_slots(type, dict);
	return (PyObject *)type;
}

static void
type_dealloc(PyTypeObject *type)
{
	etype *et;

	/* Assert this is a heap-allocated type object, or uninitialized */
	if (type->tp_flags != 0)
		assert(type->tp_flags & Py_TPFLAGS_HEAPTYPE);
	et = (etype *)type;
	Py_XDECREF(type->tp_base);
	Py_XDECREF(type->tp_dict);
	Py_XDECREF(et->name);
	Py_XDECREF(et->slots);
	PyObject_DEL(type);
}

PyTypeObject PyType_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,			/* Number of items for varobject */
	"type",			/* Name of this type */
	sizeof(etype) + sizeof(struct memberlist), /* Basic object size */
	0,			/* Item size for varobject */
	(destructor)type_dealloc,		/* tp_dealloc */
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
	PyGeneric_GetAttr,			/* tp_getattro */
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
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	(ternaryfunc)type_construct,		/* tp_construct */
	offsetof(PyTypeObject, tp_dict),	/* tp_dictoffset */
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
	int oldsize, newsize;

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

	/* Copying basicsize is connected to the GC flags */
	oldsize = base->tp_basicsize;
	if (base->tp_flags & Py_TPFLAGS_GC)
		oldsize -= PyGC_HEAD_SIZE;
	newsize = type->tp_basicsize;
	if (newsize && (type->tp_flags & Py_TPFLAGS_GC))
		newsize -= PyGC_HEAD_SIZE;
	if (!newsize)
		newsize = oldsize;
	if (!(type->tp_flags & Py_TPFLAGS_GC) &&
	    (base->tp_flags & Py_TPFLAGS_GC) &&
	    (type->tp_flags & Py_TPFLAGS_HAVE_RICHCOMPARE/*GC slots exist*/) &&
	    (!type->tp_traverse && !type->tp_clear)) {
		type->tp_flags |= Py_TPFLAGS_GC;
		COPYSLOT(tp_traverse);
		COPYSLOT(tp_clear);
	}
	if (type->tp_flags & Py_TPFLAGS_GC)
		newsize += PyGC_HEAD_SIZE;
	type->tp_basicsize = newsize;

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
		COPYSLOT(tp_dictoffset);
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

static struct wrapperbase tab_getattr[] = {
	{"__getattr__", (wrapperfunc)wrap_binaryfunc,
	 "x.__getattr__('name') <==> x.name"},
	{0}
};

static PyObject *
wrap_setattr(PyObject *self, PyObject *args, void *wrapped)
{
	setattrofunc func = (setattrofunc)wrapped;
	int res;
	PyObject *name, *value;

	if (!PyArg_ParseTuple(args, "OO", &name, &value))
		return NULL;
	res = (*func)(self, name, value);
	if (res < 0)
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
wrap_delattr(PyObject *self, PyObject *args, void *wrapped)
{
	setattrofunc func = (setattrofunc)wrapped;
	int res;
	PyObject *name;

	if (!PyArg_ParseTuple(args, "O", &name))
		return NULL;
	res = (*func)(self, name, NULL);
	if (res < 0)
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static struct wrapperbase tab_setattr[] = {
	{"__setattr__", (wrapperfunc)wrap_setattr,
	 "x.__setattr__('name', value) <==> x.name = value"},
	{"__delattr__", (wrapperfunc)wrap_delattr,
	 "x.__delattr__('name') <==> del x.name"},
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

static struct wrapperbase tab_descr_get[] = {
	{"__get__", (wrapperfunc)wrap_binaryfunc,
	 "descr.__get__(obj) -> value"},
	{0}
};

static PyObject *
wrap_descrsetfunc(PyObject *self, PyObject *args, void *wrapped)
{
	descrsetfunc func = (descrsetfunc)wrapped;
	PyObject *obj, *value;
	int ret;

	if (!PyArg_ParseTuple(args, "OO", &obj, &value))
		return NULL;
	ret = (*func)(self, obj, value);
	if (ret < 0)
		return NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

static struct wrapperbase tab_descr_set[] = {
	{"__set__", (wrapperfunc)wrap_descrsetfunc,
	 "descr.__set__(obj, value)"},
	{0}
};

static PyObject *
wrap_init(PyObject *self, PyObject *args, void *wrapped)
{
	ternaryfunc func = (ternaryfunc)wrapped;
	PyObject *res;

	/* XXX What about keyword arguments? */
	res = (*func)(self, args, NULL);
	if (res == NULL)
		return NULL;
	/* tp_construct doesn't return a new object; it just returns self,
	   un-INCREF-ed */
	Py_INCREF(Py_None);
	return Py_None;
}

static struct wrapperbase tab_init[] = {
	{"__init__", (wrapperfunc)wrap_init,
	 "x.__init__() -> initialize object"},
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

	ADD(type->tp_getattro, tab_getattr);
	ADD(type->tp_setattro, tab_setattr);
	ADD(type->tp_compare, tab_cmp);
	ADD(type->tp_repr, tab_repr);
	ADD(type->tp_hash, tab_hash);
	ADD(type->tp_call, tab_call);
	ADD(type->tp_str, tab_str);
	ADD(type->tp_richcompare, tab_richcmp);
	ADD(type->tp_iter, tab_iter);
	ADD(type->tp_iternext, tab_next);
	ADD(type->tp_descr_get, tab_descr_get);
	ADD(type->tp_descr_set, tab_descr_set);
	ADD(type->tp_construct, tab_init);

	return 0;
}

/* Slot wrappers that call the corresponding __foo__ slot */

#define SLOT0(SLOTNAME, OPNAME) \
static PyObject * \
slot_##SLOTNAME(PyObject *self) \
{ \
	return PyObject_CallMethod(self, "__" #OPNAME "__", ""); \
}

#define SLOT1(SLOTNAME, OPNAME, ARG1TYPE, ARGCODES) \
static PyObject * \
slot_##SLOTNAME(PyObject *self, ARG1TYPE arg1) \
{ \
	return PyObject_CallMethod(self, "__" #OPNAME "__", #ARGCODES, arg1); \
}

#define SLOT2(SLOTNAME, OPNAME, ARG1TYPE, ARG2TYPE, ARGCODES) \
static PyObject * \
slot_##SLOTNAME(PyObject *self, ARG1TYPE arg1, ARG2TYPE arg2) \
{ \
	return PyObject_CallMethod(self, "__" #OPNAME "__", \
               #ARGCODES, arg1, arg2); \
}

static int
slot_sq_length(PyObject *self)
{
	PyObject *res = PyObject_CallMethod(self, "__len__", "");

	if (res == NULL)
		return -1;
	return (int)PyInt_AsLong(res);
}

SLOT1(sq_concat, add, PyObject *, O);
SLOT1(sq_repeat, mul, int, i);
SLOT1(sq_item, getitem, int, i);
SLOT2(sq_slice, getslice, int, int, ii);

static int
slot_sq_ass_item(PyObject *self, int index, PyObject *value)
{
	PyObject *res = PyObject_CallMethod(self, "__setitem__",
					    "iO", index, value);
	if (res == NULL)
		return -1;
	Py_DECREF(res);
	return 0;
}

static int
slot_sq_ass_slice(PyObject *self, int i, int j, PyObject *value)
{
	PyObject *res = PyObject_CallMethod(self, "__setitem__",
					    "iiO", i, j, value);
	if (res == NULL)
		return -1;
	Py_DECREF(res);
	return 0;
}

static int
slot_sq_contains(PyObject *self, PyObject *value)
{
	PyObject *res = PyObject_CallMethod(self, "__contains__", "O", value);
	int r;

	if (res == NULL)
		return -1;
	r = PyInt_AsLong(res);
	Py_DECREF(res);
	return r;
}

SLOT1(sq_inplace_concat, iadd, PyObject *, O);
SLOT1(sq_inplace_repeat, imul, int, i);

#define slot_mp_length slot_sq_length

SLOT1(mp_subscript, getitem, PyObject *, O);

static int
slot_mp_ass_subscript(PyObject *self, PyObject *key, PyObject *value)
{
	PyObject *res = PyObject_CallMethod(self, "__setitem__",
					    "OO", key, value);
	if (res == NULL)
		return -1;
	Py_DECREF(res);
	return 0;
}

/* XXX the numerical slots should call the reverse operators too;
   but how do they know their type? */
SLOT1(nb_add, add, PyObject *, O);
SLOT1(nb_subtract, sub, PyObject *, O);
SLOT1(nb_multiply, mul, PyObject *, O);
SLOT1(nb_divide, div, PyObject *, O);
SLOT1(nb_remainder, mod, PyObject *, O);
SLOT1(nb_divmod, divmod, PyObject *, O);
SLOT2(nb_power, pow, PyObject *, PyObject *, OO);
SLOT0(nb_negative, neg);
SLOT0(nb_positive, pos);
SLOT0(nb_absolute, abs);

static int
slot_nb_nonzero(PyObject *self)
{
	PyObject *res = PyObject_CallMethod(self, "__nonzero__", "");

	if (res == NULL)
		return -1;
	return (int)PyInt_AsLong(res);
}

SLOT0(nb_invert, invert);
SLOT1(nb_lshift, lshift, PyObject *, O);
SLOT1(nb_rshift, rshift, PyObject *, O);
SLOT1(nb_and, and, PyObject *, O);
SLOT1(nb_xor, xor, PyObject *, O);
SLOT1(nb_or, or, PyObject *, O);
/* Not coerce() */
SLOT0(nb_int, int);
SLOT0(nb_long, long);
SLOT0(nb_float, float);
SLOT0(nb_oct, oct);
SLOT0(nb_hex, hex);
SLOT1(nb_inplace_add, iadd, PyObject *, O);
SLOT1(nb_inplace_subtract, isub, PyObject *, O);
SLOT1(nb_inplace_multiply, imul, PyObject *, O);
SLOT1(nb_inplace_divide, idiv, PyObject *, O);
SLOT1(nb_inplace_remainder, imod, PyObject *, O);
SLOT2(nb_inplace_power, ipow, PyObject *, PyObject *, OO);
SLOT1(nb_inplace_lshift, ilshift, PyObject *, O);
SLOT1(nb_inplace_rshift, irshift, PyObject *, O);
SLOT1(nb_inplace_and, iand, PyObject *, O);
SLOT1(nb_inplace_xor, ixor, PyObject *, O);
SLOT1(nb_inplace_or, ior, PyObject *, O);

static int
slot_tp_compare(PyObject *self, PyObject *other)
{
	PyObject *res = PyObject_CallMethod(self, "__cmp__", "O", other);
	long r;

	if (res == NULL)
		return -1;
	r = PyInt_AsLong(res);
	Py_DECREF(res);
	return (int)r;
}

SLOT0(tp_repr, repr);

static long
slot_tp_hash(PyObject *self)
{
	PyObject *res = PyObject_CallMethod(self, "__hash__", "");
	long h;

	if (res == NULL)
		return -1;
	h = PyInt_AsLong(res);
	if (h == -1 && !PyErr_Occurred())
		h = -2;
	return h;
}

static PyObject *
slot_tp_call(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *meth = PyObject_GetAttrString(self, "__call__");
	PyObject *res;

	if (meth == NULL)
		return NULL;
	res = PyObject_Call(meth, args, kwds);
	Py_DECREF(meth);
	return res;
}

SLOT0(tp_str, str);

static PyObject *
slot_tp_getattro(PyObject *self, PyObject *name)
{
	PyTypeObject *tp = self->ob_type;
	PyObject *dict = NULL;
	PyObject *getattr;

	if (tp->tp_flags & Py_TPFLAGS_HEAPTYPE)
		dict = tp->tp_dict;
	if (dict == NULL) {
		PyErr_Format(PyExc_SystemError,
			     "'%.100s' type object has no __dict__???",
			     tp->tp_name);
		return NULL;
	}
	getattr = PyDict_GetItemString(dict, "__getattr__");
	if (getattr == NULL) {
		PyErr_SetString(PyExc_AttributeError, "__getattr__");
		return NULL;
	}
	return PyObject_CallFunction(getattr, "OO", self, name);
}

static int
slot_tp_setattro(PyObject *self, PyObject *name, PyObject *value)
{
	PyObject *res;

	if (value == NULL)
		res = PyObject_CallMethod(self, "__delattr__", "O", name);
	else
		res = PyObject_CallMethod(self, "__setattr__",
					  "OO", name, value);
	if (res == NULL)
		return -1;
	Py_DECREF(res);
	return 0;
}

/* Map rich comparison operators to their __xx__ namesakes */
static char *name_op[] = {
	"__lt__",
	"__le__",
	"__eq__",
	"__ne__",
	"__gt__",
	"__ge__",
};

static PyObject *
slot_tp_richcompare(PyObject *self, PyObject *other, int op)
{
	PyObject *meth = PyObject_GetAttrString(self, name_op[op]);
	PyObject *res;

	if (meth == NULL)
		return NULL;
	res = PyObject_CallFunction(meth, "O", other);
	Py_DECREF(meth);
	return res;
}

SLOT0(tp_iter, iter);

static PyObject *
slot_tp_iternext(PyObject *self)
{
	return PyObject_CallMethod(self, "next", "");
}

SLOT1(tp_descr_get, get, PyObject *, O);

static int
slot_tp_descr_set(PyObject *self, PyObject *target, PyObject *value)
{
	PyObject *res = PyObject_CallMethod(self, "__set__",
					    "OO", target, value);
	if (res == NULL)
		return -1;
	Py_DECREF(res);
	return 0;
}

static PyObject *
slot_tp_construct(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *meth = PyObject_GetAttrString(self, "__init__");
	PyObject *res;

	if (meth == NULL)
		return NULL;
	res = PyObject_Call(meth, args, kwds);
	Py_DECREF(meth);
	if (res == NULL)
		return NULL;
	Py_DECREF(res);
	return self;
}

static void
override_slots(PyTypeObject *type, PyObject *dict)
{
	PySequenceMethods *sq = type->tp_as_sequence;
	PyMappingMethods *mp = type->tp_as_mapping;
	PyNumberMethods *nb = type->tp_as_number;

#define SQSLOT(OPNAME, SLOTNAME) \
	if (PyDict_GetItemString(dict, "__" #OPNAME "__")) { \
		sq->SLOTNAME = slot_##SLOTNAME; \
	}

#define MPSLOT(OPNAME, SLOTNAME) \
	if (PyDict_GetItemString(dict, "__" #OPNAME "__")) { \
		mp->SLOTNAME = slot_##SLOTNAME; \
	}

#define NBSLOT(OPNAME, SLOTNAME) \
	if (PyDict_GetItemString(dict, "__" #OPNAME "__")) { \
		nb->SLOTNAME = slot_##SLOTNAME; \
	}

#define TPSLOT(OPNAME, SLOTNAME) \
	if (PyDict_GetItemString(dict, "__" #OPNAME "__")) { \
		type->SLOTNAME = slot_##SLOTNAME; \
	}

	SQSLOT(len, sq_length);
	SQSLOT(add, sq_concat);
	SQSLOT(mul, sq_repeat);
	SQSLOT(getitem, sq_item);
	SQSLOT(getslice, sq_slice);
	SQSLOT(setitem, sq_ass_item);
	SQSLOT(setslice, sq_ass_slice);
	SQSLOT(contains, sq_contains);
	SQSLOT(iadd, sq_inplace_concat);
	SQSLOT(imul, sq_inplace_repeat);

	MPSLOT(len, mp_length);
	MPSLOT(getitem, mp_subscript);
	MPSLOT(setitem, mp_ass_subscript);

	NBSLOT(add, nb_add);
	NBSLOT(sub, nb_subtract);
	NBSLOT(mul, nb_multiply);
	NBSLOT(div, nb_divide);
	NBSLOT(mod, nb_remainder);
	NBSLOT(divmod, nb_divmod);
	NBSLOT(pow, nb_power);
	NBSLOT(neg, nb_negative);
	NBSLOT(pos, nb_positive);
	NBSLOT(abs, nb_absolute);
	NBSLOT(nonzero, nb_nonzero);
	NBSLOT(invert, nb_invert);
	NBSLOT(lshift, nb_lshift);
	NBSLOT(rshift, nb_rshift);
	NBSLOT(and, nb_and);
	NBSLOT(xor, nb_xor);
	NBSLOT(or, nb_or);
	/* Not coerce() */
	NBSLOT(int, nb_int);
	NBSLOT(long, nb_long);
	NBSLOT(float, nb_float);
	NBSLOT(oct, nb_oct);
	NBSLOT(hex, nb_hex);
	NBSLOT(iadd, nb_inplace_add);
	NBSLOT(isub, nb_inplace_subtract);
	NBSLOT(imul, nb_inplace_multiply);
	NBSLOT(idiv, nb_inplace_divide);
	NBSLOT(imod, nb_inplace_remainder);
	NBSLOT(ipow, nb_inplace_power);
	NBSLOT(ilshift, nb_inplace_lshift);
	NBSLOT(irshift, nb_inplace_rshift);
	NBSLOT(iand, nb_inplace_and);
	NBSLOT(ixor, nb_inplace_xor);
	NBSLOT(ior, nb_inplace_or);

	if (PyDict_GetItemString(dict, "__str__") ||
	    PyDict_GetItemString(dict, "__repr__"))
		type->tp_print = NULL;
	
	TPSLOT(cmp, tp_compare);
	TPSLOT(repr, tp_repr);
	TPSLOT(hash, tp_hash);
	TPSLOT(call, tp_call);
	TPSLOT(str, tp_str);
	TPSLOT(getattr, tp_getattro);
	TPSLOT(setattr, tp_setattro);
	TPSLOT(lt, tp_richcompare);
	TPSLOT(le, tp_richcompare);
	TPSLOT(eq, tp_richcompare);
	TPSLOT(ne, tp_richcompare);
	TPSLOT(gt, tp_richcompare);
	TPSLOT(ge, tp_richcompare);
	TPSLOT(iter, tp_iter);
	if (PyDict_GetItemString(dict, "next"))
		type->tp_iternext = slot_tp_iternext;
	TPSLOT(get, tp_descr_get);
	TPSLOT(set, tp_descr_set);
	TPSLOT(init, tp_construct);
}
