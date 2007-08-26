#include "Python.h"
#include "structmember.h"

PyDoc_STRVAR(pickle_module_documentation,
"Optimized C implementation for the Python pickle module.");

#define WRITE_BUF_SIZE 256

/* Bump this when new opcodes are added to the pickle protocol. */
#define HIGHEST_PROTOCOL 2

/*
 * Pickle opcodes.  These must be kept in synch with pickle.py.  Extensive
 * docs are in pickletools.py.
 */
enum opcodes {
    MARK            = '(',
    STOP            = '.',
    POP             = '0',
    POP_MARK        = '1',
    DUP             = '2',
    FLOAT           = 'F',
    BINFLOAT        = 'G',
    INT             = 'I',
    BININT          = 'J',
    BININT1         = 'K',
    LONG            = 'L',
    BININT2         = 'M',
    NONE            = 'N',
    PERSID          = 'P',
    BINPERSID       = 'Q',
    REDUCE          = 'R',
    STRING          = 'S',
    BINSTRING       = 'T',
    SHORT_BINSTRING = 'U',
    UNICODE         = 'V',
    BINUNICODE      = 'X',
    APPEND          = 'a',
    BUILD           = 'b',
    GLOBAL          = 'c',
    DICT            = 'd',
    EMPTY_DICT      = '}',
    APPENDS         = 'e',
    GET             = 'g',
    BINGET          = 'h',
    INST            = 'i',
    LONG_BINGET     = 'j',
    LIST            = 'l',
    EMPTY_LIST      = ']',
    OBJ             = 'o',
    PUT             = 'p',
    BINPUT          = 'q',
    LONG_BINPUT     = 'r',
    SETITEM         = 's',
    TUPLE           = 't',
    EMPTY_TUPLE     = ')',
    SETITEMS        = 'u'
};

/* Protocol 2. */
enum {
    PROTO = '\x80', /* identify pickle protocol */
    NEWOBJ,         /* build object by applying cls.__new__ to argtuple */
    EXT1,           /* push object from extension registry; 1-byte index */
    EXT2,           /* ditto but 2-byte index */
    EXT4,           /* ditto but 4-byte index */
    TUPLE1,         /* build 1-tuple from stack top */
    TUPLE2,         /* build 2-tuple from two topmost stack items */
    TUPLE3,         /* build 3-tuple from three topmost stack items */
    NEWTRUE,        /* push True */
    NEWFALSE,       /* push False */
    LONG1,          /* push long from < 256 bytes */
    LONG4,          /* push really big long */
};

/* These aren't opcodes -- they're ways to pickle bools before protocol 2
 * so that unpicklers written before bools were introduced unpickle them
 * as ints, but unpicklers after can recognize that bools were intended.
 * Note that protocol 2 added direct ways to pickle bools.
 */
#undef TRUE
#define TRUE  "I01\n"
#undef FALSE
#define FALSE "I00\n"

static const char MARKv = MARK;

/* Keep in synch with pickle.Pickler._BATCHSIZE.  This is how many elements
 * batch_list/dict() pumps out before doing APPENDS/SETITEMS.  Nothing will
 * break if this gets out of synch with pickle.py, but it's unclear that
 * would help anything either.
 */
#define BATCHSIZE 1000

static PyObject *PickleError;
static PyObject *PicklingError;
static PyObject *UnpicklingError;

/* As the name says, an empty tuple. */
static PyObject *empty_tuple;

/* copy_reg.dispatch_table, {type_object: pickling_function} */
static PyObject *dispatch_table;

/* For EXT[124] opcodes. */
/* copy_reg._extension_registry, {(module_name, function_name): code} */
static PyObject *extension_registry;
/* copy_reg._inverted_registry, {code: (module_name, function_name)} */
static PyObject *inverted_registry;
/* copy_reg._extension_cache, {code: object} */
static PyObject *extension_cache;

/* For looking up name pairs in copy_reg._extension_registry. */
static PyObject *two_tuple;

#define INIT_STR(S)                                 \
    if (!(S = PyString_InternFromString(#S))) \
        return -1;

/* Static reference to commonly used self's methods. */
static PyObject *__class__, *__dict__, *__setstate__, *__name__,    \
    *__reduce__, *__reduce_ex__, *__main__;

/*************************************************************************
 Internal Data type for pickle data.                                     */

typedef struct {
    PyObject_HEAD
    int length;   /* number of initial slots in data currently used */
    int size;     /* number of slots in data allocated */
    PyObject **data;
} Pdata;

static void
Pdata_dealloc(Pdata *self)
{
    int i;
    PyObject **p;

    for (i = self->length, p = self->data; --i >= 0; p++) {
        Py_DECREF(*p);
    }
    if (self->data)
        free(self->data);
    PyObject_Del(self);
}

static PyTypeObject Pdata_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_pickle.Pdata",              /*tp_name*/
    sizeof(Pdata),                /*tp_basicsize*/
    0,                            /*tp_itemsize*/
    (destructor)Pdata_dealloc,    /*tp_dealloc*/
};

static PyObject *
Pdata_New(void)
{
    Pdata *self;

    if (!(self = PyObject_New(Pdata, &Pdata_Type)))
        return NULL;
    self->size = 8;
    self->length = 0;
    self->data = malloc(self->size * sizeof(PyObject *));
    if (self->data)
        return (PyObject *)self;
    Py_DECREF(self);
    return PyErr_NoMemory();
}

static int
stackUnderflow(void)
{
    PyErr_SetString(UnpicklingError, "unpickling stack underflow");
    return -1;
}

/* Retain only the initial clearto items.  If clearto >= the current
 * number of items, this is a (non-erroneous) NOP.
 */
static int
Pdata_clear(Pdata *self, int clearto)
{
    int i;
    PyObject **p;

    if (clearto < 0)
        return stackUnderflow();
    if (clearto >= self->length)
        return 0;

    for (i = self->length, p = self->data + clearto; --i >= clearto; p++) {
        Py_CLEAR(*p);
    }
    self->length = clearto;

    return 0;
}

static int
Pdata_grow(Pdata *self)
{
    int bigger;
    size_t nbytes;
    PyObject **tmp;

    bigger = self->size << 1;
    if (bigger <= 0)            /* was 0, or new value overflows */
        goto nomemory;
    if ((int)(size_t)bigger != bigger)
        goto nomemory;
    nbytes = (size_t)bigger * sizeof(PyObject *);
    if (nbytes / sizeof(PyObject *) != (size_t)bigger)
        goto nomemory;
    tmp = realloc(self->data, nbytes);
    if (tmp == NULL)
        goto nomemory;
    self->data = tmp;
    self->size = bigger;
    return 0;

  nomemory:
    PyErr_NoMemory();
    return -1;
}

/* D is a Pdata*.  Pop the topmost element and store it into V, which
 * must be an lvalue holding PyObject*.  On stack underflow, UnpicklingError
 * is raised and V is set to NULL.  D and V may be evaluated several times.
 */
#define PDATA_POP(D, V) {                                           \
        if ((D)->length)                                            \
            (V) = (D)->data[--((D)->length)];                       \
        else {                                                      \
            PyErr_SetString(UnpicklingError, "bad pickle data");    \
            (V) = NULL;                                             \
        }                                                           \
    }

/* PDATA_PUSH and PDATA_APPEND both push rvalue PyObject* O on to Pdata*
 * D.  If the Pdata stack can't be grown to hold the new value, both
 * raise MemoryError and execute "return ER".  The difference is in ownership
 * of O after:  _PUSH transfers ownership of O from the caller to the stack
 * (no incref of O is done, and in case of error O is decrefed), while
 * _APPEND pushes a new reference.
 */

/* Push O on stack D, giving ownership of O to the stack. */
#define PDATA_PUSH(D, O, ER) {                                  \
        if (((Pdata*)(D))->length == ((Pdata*)(D))->size &&     \
            Pdata_grow((Pdata*)(D)) < 0) {                      \
            Py_DECREF(O);                                       \
            return ER;                                          \
        }                                                       \
        ((Pdata*)(D))->data[((Pdata*)(D))->length++] = (O);     \
    }

/* Push O on stack D, pushing a new reference. */
#define PDATA_APPEND(D, O, ER) {                                \
        if (((Pdata*)(D))->length == ((Pdata*)(D))->size &&     \
            Pdata_grow((Pdata*)(D)) < 0)                        \
            return ER;                                          \
        Py_INCREF(O);                                           \
        ((Pdata*)(D))->data[((Pdata*)(D))->length++] = (O);     \
}

static PyObject *
Pdata_popTuple(Pdata *self, int start)
{
    PyObject *r;
    int i, j, l;

    l = self->length - start;
    r = PyTuple_New(l);
    if (r == NULL)
        return NULL;
    for (i = start, j = 0; j < l; i++, j++)
        PyTuple_SET_ITEM(r, j, self->data[i]);

    self->length = start;
    return r;
}

static PyObject *
Pdata_popList(Pdata *self, int start)
{
    PyObject *r;
    int i, j, l;

    l = self->length - start;
    if (!(r = PyList_New(l)))
        return NULL;
    for (i = start, j = 0; j < l; i++, j++)
        PyList_SET_ITEM(r, j, self->data[i]);

    self->length = start;
    return r;
}

/*************************************************************************/

#define ARG_TUP(self, o) {                              \
        if (self->arg || (self->arg=PyTuple_New(1))) {  \
            Py_XDECREF(PyTuple_GET_ITEM(self->arg,0));  \
            PyTuple_SET_ITEM(self->arg,0,o);            \
        }                                               \
        else {                                          \
            Py_DECREF(o);                               \
        }                                               \
    }

#define FREE_ARG_TUP(self) {                        \
        if (self->arg->ob_refcnt > 1) {             \
            Py_DECREF(self->arg);                   \
            self->arg=NULL;                         \
        }                                           \
    }

typedef struct PicklerObject {
    PyObject_HEAD
    FILE *fp;
    PyObject *write;
    PyObject *file;
    PyObject *memo;
    PyObject *arg;
    PyObject *pers_func;

    /* pickle protocol number, >= 0 */
    int proto;

    /* bool, true if proto > 0 */
    int bin;

    int fast;   /* Fast mode doesn't save in memo, don't use if circ ref */
    int nesting;
    int (*write_func) (struct PicklerObject *, const char *, Py_ssize_t);
    char *write_buf;
    int buf_size;
    PyObject *dispatch_table;
    int fast_container;         /* count nested container dumps */
    PyObject *fast_memo;
} PicklerObject;

#ifndef PY_CPICKLE_FAST_LIMIT
#define PY_CPICKLE_FAST_LIMIT 50
#endif

static PyTypeObject Pickler_Type;

typedef struct UnpicklerObject {
    PyObject_HEAD
    FILE *fp;
    PyObject *file;
    PyObject *readline;
    PyObject *read;
    PyObject *memo;
    PyObject *arg;
    Pdata *stack;
    PyObject *mark;
    PyObject *pers_func;
    PyObject *last_string;
    int *marks;
    int num_marks;
    int marks_size;
    Py_ssize_t(*read_func) (struct UnpicklerObject *, char **, Py_ssize_t);
    Py_ssize_t(*readline_func) (struct UnpicklerObject *, char **);
    int buf_size;
    char *buf;
    PyObject *find_class;
} UnpicklerObject;

static PyTypeObject Unpickler_Type;

/* Forward decls that need the above structs */
static int save(PicklerObject *, PyObject *, int);
static int put2(PicklerObject *, PyObject *);

static PyObject *
pickle_ErrFormat(PyObject *ErrType, char *stringformat, char *format, ...)
{
    va_list va;
    PyObject *args = 0, *retval = 0;
    va_start(va, format);

    if (format)
        args = Py_VaBuildValue(format, va);
    va_end(va);
    if (format && !args)
        return NULL;
    if (stringformat && !(retval = PyString_FromString(stringformat)))
        return NULL;

    if (retval) {
        if (args) {
            PyObject *v;
            v = PyString_Format(retval, args);
            Py_DECREF(retval);
            Py_DECREF(args);
            if (!v)
                return NULL;
            retval = v;
        }
    }
    else if (args)
        retval = args;
    else {
        PyErr_SetObject(ErrType, Py_None);
        return NULL;
    }
    PyErr_SetObject(ErrType, retval);
    Py_DECREF(retval);
    return NULL;
}

static int
write_other(PicklerObject *self, const char *s, Py_ssize_t _n)
{
    PyObject *py_str, *result = NULL;
    int n;

    if (_n > INT_MAX)
        return -1;
    n = (int) _n;
    if (s == NULL) {
        if (!(self->buf_size))
            return 0;
        py_str = PyString_FromStringAndSize(self->write_buf, self->buf_size);
        if (!py_str)
            return -1;
    }
    else {
        if (self->buf_size && (n + self->buf_size) > WRITE_BUF_SIZE) {
            if (write_other(self, NULL, 0) < 0)
                return -1;
        }

        if (n > WRITE_BUF_SIZE) {
            if (!(py_str = PyString_FromStringAndSize(s, n)))
                return -1;
        }
        else {
            memcpy(self->write_buf + self->buf_size, s, n);
            self->buf_size += n;
            return n;
        }
    }

    /* object with write method */
    ARG_TUP(self, py_str);
    if (self->arg) {
        result = PyObject_Call(self->write, self->arg, NULL);
        FREE_ARG_TUP(self);
    }
    if (result == NULL)
        return -1;

    Py_DECREF(result);
    self->buf_size = 0;
    return n;
}

static Py_ssize_t
read_other(UnpicklerObject *self, char **s, Py_ssize_t n)
{
    PyObject *bytes, *str = 0;

    if (!(bytes = PyInt_FromSsize_t(n)))
        return -1;

    ARG_TUP(self, bytes);
    if (self->arg) {
        str = PyObject_Call(self->read, self->arg, NULL);
        FREE_ARG_TUP(self);
    }
    if (!str)
        return -1;

    Py_XDECREF(self->last_string);
    self->last_string = str;

    if (!(*s = PyString_AsString(str)))
        return -1;
    return n;
}

static Py_ssize_t
readline_other(UnpicklerObject *self, char **s)
{
    PyObject *str;
    Py_ssize_t str_size;

    if (!(str = PyObject_CallObject(self->readline, empty_tuple))) {
        return -1;
    }

    if ((str_size = PyString_Size(str)) < 0)
        return -1;

    Py_XDECREF(self->last_string);
    self->last_string = str;

    if (!(*s = PyString_AsString(str)))
        return -1;

    return str_size;
}

/* Copy the first n bytes from s into newly malloc'ed memory, plus a
 * trailing 0 byte.  Return a pointer to that, or NULL if out of memory.
 * The caller is responsible for free()'ing the return value.
 */
static char *
pystrndup(const char *s, int n)
{
    char *r = (char *) malloc(n + 1);
    if (r == NULL)
        return (char *) PyErr_NoMemory();
    memcpy(r, s, n);
    r[n] = 0;
    return r;
}

static int
get(PicklerObject *self, PyObject * id)
{
    PyObject *value, *mv;
    long c_value;
    char s[30];
    size_t len;

    if (!(mv = PyDict_GetItem(self->memo, id))) {
        PyErr_SetObject(PyExc_KeyError, id);
        return -1;
    }

    if (!(value = PyTuple_GetItem(mv, 0)))
        return -1;

    if (!(PyInt_Check(value))) {
        PyErr_SetString(PicklingError, "no int where int expected in memo");
        return -1;
    }
    c_value = PyInt_AsLong(value);
    if (c_value == -1 && PyErr_Occurred())
        return -1;

    if (!self->bin) {
        s[0] = GET;
        PyOS_snprintf(s + 1, sizeof(s) - 1, "%ld\n", c_value);
        len = strlen(s);
    }
    else {
        if (c_value < 256) {
            s[0] = BINGET;
            s[1] = (int)(c_value & 0xff);
            len = 2;
        }
        else {
            s[0] = LONG_BINGET;
            s[1] = (int)(c_value & 0xff);
            s[2] = (int)((c_value >> 8) & 0xff);
            s[3] = (int)((c_value >> 16) & 0xff);
            s[4] = (int)((c_value >> 24) & 0xff);
            len = 5;
        }
    }

    if (self->write_func(self, s, len) < 0)
        return -1;

    return 0;
}

static int
put(PicklerObject *self, PyObject *ob)
{
    if (self->fast)
        return 0;

    return put2(self, ob);
}

static int
put2(PicklerObject *self, PyObject *ob)
{
    char c_str[30];
    int p;
    size_t len;
    int res = -1;
    PyObject *py_ob_id = 0, *memo_len = 0, *t = 0;

    if (self->fast)
        return 0;

    if ((p = PyDict_Size(self->memo)) < 0)
        goto finally;

    if (!(py_ob_id = PyLong_FromVoidPtr(ob)))
        goto finally;

    if (!(memo_len = PyInt_FromLong(p)))
        goto finally;

    if (!(t = PyTuple_New(2)))
        goto finally;

    PyTuple_SET_ITEM(t, 0, memo_len);
    Py_INCREF(memo_len);
    PyTuple_SET_ITEM(t, 1, ob);
    Py_INCREF(ob);

    if (PyDict_SetItem(self->memo, py_ob_id, t) < 0)
        goto finally;

    if (!self->bin) {
        c_str[0] = PUT;
        PyOS_snprintf(c_str + 1, sizeof(c_str) - 1, "%d\n", p);
        len = strlen(c_str);
    }
    else {
        if (p >= 256) {
            c_str[0] = LONG_BINPUT;
            c_str[1] = (int)(p & 0xff);
            c_str[2] = (int)((p >> 8) & 0xff);
            c_str[3] = (int)((p >> 16) & 0xff);
            c_str[4] = (int)((p >> 24) & 0xff);
            len = 5;
        }
        else {
            c_str[0] = BINPUT;
            c_str[1] = p;
            len = 2;
        }
    }

    if (self->write_func(self, c_str, len) < 0)
        goto finally;

    res = 0;

  finally:
    Py_XDECREF(py_ob_id);
    Py_XDECREF(memo_len);
    Py_XDECREF(t);

    return res;
}

static PyObject *
whichmodule(PyObject *global, PyObject *global_name)
{
    Py_ssize_t i, j;
    PyObject *module = 0, *modules_dict = 0, *global_name_attr = 0, *name = 0;

    module = PyObject_GetAttrString(global, "__module__");
    if (module)
        return module;
    if (PyErr_ExceptionMatches(PyExc_AttributeError))
        PyErr_Clear();
    else
        return NULL;

    if (!(modules_dict = PySys_GetObject("modules")))
        return NULL;

    i = 0;
    while ((j = PyDict_Next(modules_dict, &i, &name, &module))) {

        if (PyObject_Compare(name, __main__) == 0)
            continue;

        global_name_attr = PyObject_GetAttr(module, global_name);
        if (!global_name_attr) {
            if (PyErr_ExceptionMatches(PyExc_AttributeError))
                PyErr_Clear();
            else
                return NULL;
            continue;
        }

        if (global_name_attr != global) {
            Py_DECREF(global_name_attr);
            continue;
        }

        Py_DECREF(global_name_attr);

        break;
    }

    /* The following implements the rule in pickle.py added in 1.5
     * that used __main__ if no module is found.  I don't actually
     * like this rule. jlf
     */
    if (!j) {
        j = 1;
        name = __main__;
    }

    Py_INCREF(name);
    return name;
}

static int
fast_save_enter(PicklerObject *self, PyObject *obj)
{
    /* if fast_container < 0, we're doing an error exit. */
    if (++self->fast_container >= PY_CPICKLE_FAST_LIMIT) {
        PyObject *key = NULL;
        if (self->fast_memo == NULL) {
            self->fast_memo = PyDict_New();
            if (self->fast_memo == NULL) {
                self->fast_container = -1;
                return 0;
            }
        }
        key = PyLong_FromVoidPtr(obj);
        if (key == NULL)
            return 0;
        if (PyDict_GetItem(self->fast_memo, key)) {
            Py_DECREF(key);
            PyErr_Format(PyExc_ValueError,
                         "fast mode: can't pickle cyclic objects "
                         "including object type %s at %p",
                         obj->ob_type->tp_name, obj);
            self->fast_container = -1;
            return 0;
        }
        if (PyDict_SetItem(self->fast_memo, key, Py_None) < 0) {
            Py_DECREF(key);
            self->fast_container = -1;
            return 0;
        }
        Py_DECREF(key);
    }
    return 1;
}

static int
fast_save_leave(PicklerObject *self, PyObject *obj)
{
    if (self->fast_container-- >= PY_CPICKLE_FAST_LIMIT) {
        PyObject *key = PyLong_FromVoidPtr(obj);
        if (key == NULL)
            return 0;
        if (PyDict_DelItem(self->fast_memo, key) < 0) {
            Py_DECREF(key);
            return 0;
        }
        Py_DECREF(key);
    }
    return 1;
}

static int
save_none(PicklerObject *self, PyObject *args)
{
    static char none = NONE;
    if (self->write_func(self, &none, 1) < 0)
        return -1;

    return 0;
}

static int
save_bool(PicklerObject *self, PyObject *args)
{
    static const char *buf[2] = { FALSE, TRUE };
    static char len[2] = { sizeof(FALSE) - 1, sizeof(TRUE) - 1 };
    long l = args == Py_True;

    if (self->proto >= 2) {
        char opcode = l ? NEWTRUE : NEWFALSE;
        if (self->write_func(self, &opcode, 1) < 0)
            return -1;
    }
    else if (self->write_func(self, buf[l], len[l]) < 0)
        return -1;
    return 0;
}

static int
save_int(PicklerObject *self, long l)
{
    char c_str[32];
    int len = 0;

    if (!self->bin
#if SIZEOF_LONG > 4
        || l > 0x7fffffffL || l < -0x80000000L
#endif
        ) {
        /* Text-mode pickle, or long too big to fit in the 4-byte
         * signed BININT format:  store as a string.
         */
        c_str[0] = INT;
        PyOS_snprintf(c_str + 1, sizeof(c_str) - 1, "%ld\n", l);
        if (self->write_func(self, c_str, strlen(c_str)) < 0)
            return -1;
    }
    else {
        /* Binary pickle and l fits in a signed 4-byte int. */
        c_str[1] = (int)(l & 0xff);
        c_str[2] = (int)((l >> 8) & 0xff);
        c_str[3] = (int)((l >> 16) & 0xff);
        c_str[4] = (int)((l >> 24) & 0xff);

        if ((c_str[4] == 0) && (c_str[3] == 0)) {
            if (c_str[2] == 0) {
                c_str[0] = BININT1;
                len = 2;
            }
            else {
                c_str[0] = BININT2;
                len = 3;
            }
        }
        else {
            c_str[0] = BININT;
            len = 5;
        }

        if (self->write_func(self, c_str, len) < 0)
            return -1;
    }

    return 0;
}

static int
save_long(PicklerObject *self, PyObject *args)
{
    Py_ssize_t size;
    int res = -1;
    PyObject *repr = NULL;
    long val = PyInt_AsLong(args);
    static char l = LONG;

    if (val == -1 && PyErr_Occurred()) {
        /* out of range for int pickling */
        PyErr_Clear();
    }
    else
        return save_int(self, val);

    if (self->proto >= 2) {
        /* Linear-time pickling. */
        size_t nbits;
        size_t nbytes;
        unsigned char *pdata;
        char c_str[5];
        int i;
        int sign = _PyLong_Sign(args);

        if (sign == 0) {
            /* It's 0 -- an empty bytestring. */
            c_str[0] = LONG1;
            c_str[1] = 0;
            i = self->write_func(self, c_str, 2);
            if (i < 0)
                goto finally;
            res = 0;
            goto finally;
        }
        nbits = _PyLong_NumBits(args);
        if (nbits == (size_t) - 1 && PyErr_Occurred())
            goto finally;
        /* How many bytes do we need?  There are nbits >> 3 full
         * bytes of data, and nbits & 7 leftover bits.  If there
         * are any leftover bits, then we clearly need another
         * byte.  Wnat's not so obvious is that we *probably*
         * need another byte even if there aren't any leftovers:
         * the most-significant bit of the most-significant byte
         * acts like a sign bit, and it's usually got a sense
         * opposite of the one we need.  The exception is longs
         * of the form -(2**(8*j-1)) for j > 0.  Such a long is
         * its own 256's-complement, so has the right sign bit
         * even without the extra byte.  That's a pain to check
         * for in advance, though, so we always grab an extra
         * byte at the start, and cut it back later if possible.
         */
        nbytes = (nbits >> 3) + 1;
        if (nbytes > INT_MAX) {
            PyErr_SetString(PyExc_OverflowError, "long too large "
                            "to pickle");
            goto finally;
        }
        repr = PyString_FromStringAndSize(NULL, (int) nbytes);
        if (repr == NULL)
            goto finally;
        pdata = (unsigned char *) PyString_AS_STRING(repr);
        i = _PyLong_AsByteArray((PyLongObject *) args,
                                pdata, nbytes,
                                1 /* little endian */ , 1 /* signed */ );
        if (i < 0)
            goto finally;
        /* If the long is negative, this may be a byte more than
         * needed.  This is so iff the MSB is all redundant sign
         * bits.
         */
        if (sign < 0 && nbytes > 1 && pdata[nbytes - 1] == 0xff &&
            (pdata[nbytes - 2] & 0x80) != 0)
            --nbytes;

        if (nbytes < 256) {
            c_str[0] = LONG1;
            c_str[1] = (char)nbytes;
            size = 2;
        }
        else {
            c_str[0] = LONG4;
            size = (int) nbytes;
            for (i = 1; i < 5; i++) {
                c_str[i] = (char)(size & 0xff);
                size >>= 8;
            }
            size = 5;
        }
        i = self->write_func(self, c_str, size);
        if (i < 0)
            goto finally;
        i = self->write_func(self, (char *)pdata, (int)nbytes);
        if (i < 0)
            goto finally;
        res = 0;
        goto finally;
    }

    /* proto < 2:  write the repr and newline.  This is quadratic-time
     * (in the number of digits), in both directions.
     */
    if (!(repr = PyObject_Repr(args)))
        goto finally;

    if ((size = PyString_Size(repr)) < 0)
        goto finally;

    if (self->write_func(self, &l, 1) < 0)
        goto finally;

    if (self->write_func(self,
                         PyString_AS_STRING((PyStringObject *)repr),
                         size) < 0)
        goto finally;

    if (self->write_func(self, "\n", 1) < 0)
        goto finally;

    res = 0;

  finally:
    Py_XDECREF(repr);
    return res;
}

static int
save_float(PicklerObject *self, PyObject *args)
{
    double x = PyFloat_AS_DOUBLE((PyFloatObject *)args);

    if (self->bin) {
        char str[9];
        str[0] = BINFLOAT;
        if (_PyFloat_Pack8(x, (unsigned char *)&str[1], 0) < 0)
            return -1;
        if (self->write_func(self, str, 9) < 0)
            return -1;
    }
    else {
        char c_str[250];
        c_str[0] = FLOAT;
        PyOS_ascii_formatd(c_str + 1, sizeof(c_str) - 2, "%.17g", x);
        /* Extend the formatted string with a newline character */
        strcat(c_str, "\n");

        if (self->write_func(self, c_str, strlen(c_str)) < 0)
            return -1;
    }

    return 0;
}

static int
save_string(PicklerObject *self, PyObject *args, int doput)
{
    int size, len;
    PyObject *repr = 0;

    if ((size = PyString_Size(args)) < 0)
        return -1;

    if (!self->bin) {
        char *repr_str;

        static char string = STRING;

        if (!(repr = PyObject_Repr(args)))
            return -1;

        if ((len = PyString_Size(repr)) < 0)
            goto error;
        repr_str = PyString_AS_STRING((PyStringObject *)repr);

        if (self->write_func(self, &string, 1) < 0)
            goto error;

        if (self->write_func(self, repr_str, len) < 0)
            goto error;

        if (self->write_func(self, "\n", 1) < 0)
            goto error;

        Py_XDECREF(repr);
    }
    else {
        int i;
        char c_str[5];

        if ((size = PyString_Size(args)) < 0)
            return -1;

        if (size < 256) {
            c_str[0] = SHORT_BINSTRING;
            c_str[1] = size;
            len = 2;
        }
        else if (size <= INT_MAX) {
            c_str[0] = BINSTRING;
            for (i = 1; i < 5; i++)
                c_str[i] = (int) (size >> ((i - 1) * 8));
            len = 5;
        }
        else
            return -1;          /* string too large */

        if (self->write_func(self, c_str, len) < 0)
            return -1;

        if (self->write_func(self,
                             PyString_AS_STRING((PyStringObject *)args),
                             size) < 0)
            return -1;
    }

    if (doput)
        if (put(self, args) < 0)
            return -1;

    return 0;

  error:
    Py_XDECREF(repr);
    return -1;
}

#ifdef Py_USING_UNICODE
/* A copy of PyUnicode_EncodeRawUnicodeEscape() that also translates
   backslash and newline characters to \uXXXX escapes. */
static PyObject *
modified_EncodeRawUnicodeEscape(const Py_UNICODE *s, int size)
{
    PyObject *repr;
    char *p;
    char *q;

    static const char *hexdigit = "0123456789ABCDEF";

    repr = PyString_FromStringAndSize(NULL, 6 * size);
    if (repr == NULL)
        return NULL;
    if (size == 0)
        return repr;

    p = q = PyString_AS_STRING(repr);
    while (size-- > 0) {
        Py_UNICODE ch = *s++;
        /* Map 16-bit characters to '\uxxxx' */
        if (ch >= 256 || ch == '\\' || ch == '\n') {
            *p++ = '\\';
            *p++ = 'u';
            *p++ = hexdigit[(ch >> 12) & 0xf];
            *p++ = hexdigit[(ch >> 8) & 0xf];
            *p++ = hexdigit[(ch >> 4) & 0xf];
            *p++ = hexdigit[ch & 15];
        }
        /* Copy everything else as-is */
        else
            *p++ = (char)ch;
    }
    *p = '\0';
    _PyString_Resize(&repr, p - q);
    return repr;
}

static int
save_unicode(PicklerObject *self, PyObject *args, int doput)
{
    Py_ssize_t size, len;
    PyObject *repr = 0;

    if (!PyUnicode_Check(args))
        return -1;

    if (!self->bin) {
        char *repr_str;
        static char string = UNICODE;

        repr =
            modified_EncodeRawUnicodeEscape(PyUnicode_AS_UNICODE(args),
                                            PyUnicode_GET_SIZE(args));
        if (!repr)
            return -1;

        if ((len = PyString_Size(repr)) < 0)
            goto error;
        repr_str = PyString_AS_STRING((PyStringObject *)repr);

        if (self->write_func(self, &string, 1) < 0)
            goto error;

        if (self->write_func(self, repr_str, len) < 0)
            goto error;

        if (self->write_func(self, "\n", 1) < 0)
            goto error;

        Py_XDECREF(repr);
    }
    else {
        int i;
        char c_str[5];

        if (!(repr = PyUnicode_AsUTF8String(args)))
            return -1;

        if ((size = PyString_Size(repr)) < 0)
            goto error;
        if (size > INT_MAX)
            return -1;          /* string too large */

        c_str[0] = BINUNICODE;
        for (i = 1; i < 5; i++)
            c_str[i] = (int) (size >> ((i - 1) * 8));
        len = 5;

        if (self->write_func(self, c_str, len) < 0)
            goto error;

        if (self->write_func(self, PyString_AS_STRING(repr), size) < 0)
            goto error;

        Py_DECREF(repr);
    }

    if (doput)
        if (put(self, args) < 0)
            return -1;

    return 0;

  error:
    Py_XDECREF(repr);
    return -1;
}
#endif

/* A helper for save_tuple.  Push the len elements in tuple t on the stack. */
static int
store_tuple_elements(PicklerObject *self, PyObject *t, int len)
{
    int i;
    int res = -1;               /* guilty until proved innocent */

    assert(PyTuple_Size(t) == len);

    for (i = 0; i < len; i++) {
        PyObject *element = PyTuple_GET_ITEM(t, i);

        if (element == NULL)
            goto finally;
        if (save(self, element, 0) < 0)
            goto finally;
    }
    res = 0;

  finally:
    return res;
}

/* Tuples are ubiquitous in the pickle protocols, so many techniques are
 * used across protocols to minimize the space needed to pickle them.
 * Tuples are also the only builtin immutable type that can be recursive
 * (a tuple can be reached from itself), and that requires some subtle
 * magic so that it works in all cases.  IOW, this is a long routine.
 */
static int
save_tuple(PicklerObject *self, PyObject *args)
{
    PyObject *py_tuple_id = NULL;
    int len, i;
    int res = -1;

    static char tuple = TUPLE;
    static char pop = POP;
    static char pop_mark = POP_MARK;
    static char len2opcode[] = { EMPTY_TUPLE, TUPLE1, TUPLE2, TUPLE3 };

    if ((len = PyTuple_Size(args)) < 0)
        goto finally;

    if (len == 0) {
        char c_str[2];

        if (self->proto) {
            c_str[0] = EMPTY_TUPLE;
            len = 1;
        }
        else {
            c_str[0] = MARK;
            c_str[1] = TUPLE;
            len = 2;
        }
        if (self->write_func(self, c_str, len) >= 0)
            res = 0;
        /* Don't memoize an empty tuple. */
        goto finally;
    }

    /* A non-empty tuple. */

    /* id(tuple) isn't in the memo now.  If it shows up there after
     * saving the tuple elements, the tuple must be recursive, in
     * which case we'll pop everything we put on the stack, and fetch
     * its value from the memo.
     */
    py_tuple_id = PyLong_FromVoidPtr(args);
    if (py_tuple_id == NULL)
        goto finally;

    if (len <= 3 && self->proto >= 2) {
        /* Use TUPLE{1,2,3} opcodes. */
        if (store_tuple_elements(self, args, len) < 0)
            goto finally;
        if (PyDict_GetItem(self->memo, py_tuple_id)) {
            /* pop the len elements */
            for (i = 0; i < len; ++i)
                if (self->write_func(self, &pop, 1) < 0)
                    goto finally;
            /* fetch from memo */
            if (get(self, py_tuple_id) < 0)
                goto finally;
            res = 0;
            goto finally;
        }
        /* Not recursive. */
        if (self->write_func(self, len2opcode + len, 1) < 0)
            goto finally;
        goto memoize;
    }

    /* proto < 2 and len > 0, or proto >= 2 and len > 3.
     * Generate MARK elt1 elt2 ... TUPLE
     */
    if (self->write_func(self, &MARKv, 1) < 0)
        goto finally;

    if (store_tuple_elements(self, args, len) < 0)
        goto finally;

    if (PyDict_GetItem(self->memo, py_tuple_id)) {
        /* pop the stack stuff we pushed */
        if (self->bin) {
            if (self->write_func(self, &pop_mark, 1) < 0)
                goto finally;
        }
        else {
            /* Note that we pop one more than len, to remove
             * the MARK too.
             */
            for (i = 0; i <= len; i++)
                if (self->write_func(self, &pop, 1) < 0)
                    goto finally;
        }
        /* fetch from memo */
        if (get(self, py_tuple_id) >= 0)
            res = 0;
        goto finally;
    }

    /* Not recursive. */
    if (self->write_func(self, &tuple, 1) < 0)
        goto finally;

  memoize:
    if (put(self, args) >= 0)
        res = 0;

  finally:
    Py_XDECREF(py_tuple_id);
    return res;
}

/* iter is an iterator giving items, and we batch up chunks of
 *     MARK item item ... item APPENDS
 * opcode sequences.  Calling code should have arranged to first create an
 * empty list, or list-like object, for the APPENDS to operate on.
 * Returns 0 on success, <0 on error.
 */
static int
batch_list(PicklerObject *self, PyObject *iter)
{
    PyObject *obj;
    PyObject *slice[BATCHSIZE];
    int i, n;

    static char append = APPEND;
    static char appends = APPENDS;

    assert(iter != NULL);

    if (self->proto == 0) {
        /* APPENDS isn't available; do one at a time. */
        for (;;) {
            obj = PyIter_Next(iter);
            if (obj == NULL) {
                if (PyErr_Occurred())
                    return -1;
                break;
            }
            i = save(self, obj, 0);
            Py_DECREF(obj);
            if (i < 0)
                return -1;
            if (self->write_func(self, &append, 1) < 0)
                return -1;
        }
        return 0;
    }

    /* proto > 0:  write in batches of BATCHSIZE. */
    do {
        /* Get next group of (no more than) BATCHSIZE elements. */
        for (n = 0; n < BATCHSIZE; ++n) {
            obj = PyIter_Next(iter);
            if (obj == NULL) {
                if (PyErr_Occurred())
                    goto BatchFailed;
                break;
            }
            slice[n] = obj;
        }

        if (n > 1) {
            /* Pump out MARK, slice[0:n], APPENDS. */
            if (self->write_func(self, &MARKv, 1) < 0)
                goto BatchFailed;
            for (i = 0; i < n; ++i) {
                if (save(self, slice[i], 0) < 0)
                    goto BatchFailed;
            }
            if (self->write_func(self, &appends, 1) < 0)
                goto BatchFailed;
        }
        else if (n == 1) {
            if (save(self, slice[0], 0) < 0)
                goto BatchFailed;
            if (self->write_func(self, &append, 1) < 0)
                goto BatchFailed;
        }

        for (i = 0; i < n; ++i) {
            Py_DECREF(slice[i]);
        }
    } while (n == BATCHSIZE);
    return 0;

  BatchFailed:
    while (--n >= 0) {
        Py_DECREF(slice[n]);
    }
    return -1;
}

static int
save_list(PicklerObject *self, PyObject *args)
{
    int res = -1;
    char s[3];
    int len;
    PyObject *iter;

    if (self->fast && !fast_save_enter(self, args))
        goto finally;

    /* Create an empty list. */
    if (self->bin) {
        s[0] = EMPTY_LIST;
        len = 1;
    }
    else {
        s[0] = MARK;
        s[1] = LIST;
        len = 2;
    }

    if (self->write_func(self, s, len) < 0)
        goto finally;

    /* Get list length, and bow out early if empty. */
    if ((len = PyList_Size(args)) < 0)
        goto finally;

    /* Memoize. */
    if (len == 0) {
        if (put(self, args) >= 0)
            res = 0;
        goto finally;
    }
    if (put2(self, args) < 0)
        goto finally;

    /* Materialize the list elements. */
    iter = PyObject_GetIter(args);
    if (iter == NULL)
        goto finally;
    res = batch_list(self, iter);
    Py_DECREF(iter);

  finally:
    if (self->fast && !fast_save_leave(self, args))
        res = -1;

    return res;
}

/* iter is an iterator giving (key, value) pairs, and we batch up chunks of
 *     MARK key value ... key value SETITEMS
 * opcode sequences.  Calling code should have arranged to first create an
 * empty dict, or dict-like object, for the SETITEMS to operate on.
 * Returns 0 on success, <0 on error.
 *
 * This is very much like batch_list().  The difference between saving
 * elements directly, and picking apart two-tuples, is so long-winded at
 * the C level, though, that attempts to combine these routines were too
 * ugly to bear.
 */
static int
batch_dict(PicklerObject *self, PyObject *iter)
{
    PyObject *p;
    PyObject *slice[BATCHSIZE];
    int i, n;

    static char setitem = SETITEM;
    static char setitems = SETITEMS;

    assert(iter != NULL);

    if (self->proto == 0) {
        /* SETITEMS isn't available; do one at a time. */
        for (;;) {
            p = PyIter_Next(iter);
            if (p == NULL) {
                if (PyErr_Occurred())
                    return -1;
                break;
            }
            if (!PyTuple_Check(p) || PyTuple_Size(p) != 2) {
                PyErr_SetString(PyExc_TypeError, "dict items "
                                "iterator must return 2-tuples");
                return -1;
            }
            i = save(self, PyTuple_GET_ITEM(p, 0), 0);
            if (i >= 0)
                i = save(self, PyTuple_GET_ITEM(p, 1), 0);
            Py_DECREF(p);
            if (i < 0)
                return -1;
            if (self->write_func(self, &setitem, 1) < 0)
                return -1;
        }
        return 0;
    }

    /* proto > 0:  write in batches of BATCHSIZE. */
    do {
        /* Get next group of (no more than) BATCHSIZE elements. */
        for (n = 0; n < BATCHSIZE; ++n) {
            p = PyIter_Next(iter);
            if (p == NULL) {
                if (PyErr_Occurred())
                    goto BatchFailed;
                break;
            }
            if (!PyTuple_Check(p) || PyTuple_Size(p) != 2) {
                PyErr_SetString(PyExc_TypeError, "dict items "
                                "iterator must return 2-tuples");
                goto BatchFailed;
            }
            slice[n] = p;
        }

        if (n > 1) {
            /* Pump out MARK, slice[0:n], SETITEMS. */
            if (self->write_func(self, &MARKv, 1) < 0)
                goto BatchFailed;
            for (i = 0; i < n; ++i) {
                p = slice[i];
                if (save(self, PyTuple_GET_ITEM(p, 0), 0) < 0)
                    goto BatchFailed;
                if (save(self, PyTuple_GET_ITEM(p, 1), 0) < 0)
                    goto BatchFailed;
            }
            if (self->write_func(self, &setitems, 1) < 0)
                goto BatchFailed;
        }
        else if (n == 1) {
            p = slice[0];
            if (save(self, PyTuple_GET_ITEM(p, 0), 0) < 0)
                goto BatchFailed;
            if (save(self, PyTuple_GET_ITEM(p, 1), 0) < 0)
                goto BatchFailed;
            if (self->write_func(self, &setitem, 1) < 0)
                goto BatchFailed;
        }

        for (i = 0; i < n; ++i) {
            Py_DECREF(slice[i]);
        }
    } while (n == BATCHSIZE);
    return 0;

  BatchFailed:
    while (--n >= 0) {
        Py_DECREF(slice[n]);
    }
    return -1;
}

static int
save_dict(PicklerObject *self, PyObject *args)
{
    int res = -1;
    char s[3];
    int len;
    PyObject *items, *iter;

    if (self->fast && !fast_save_enter(self, args))
        goto finally;

    /* Create an empty dict. */
    if (self->bin) {
        s[0] = EMPTY_DICT;
        len = 1;
    }
    else {
        s[0] = MARK;
        s[1] = DICT;
        len = 2;
    }

    if (self->write_func(self, s, len) < 0)
        goto finally;

    /* Get dict size, and bow out early if empty. */
    if ((len = PyDict_Size(args)) < 0)
        goto finally;

    if (len == 0) {
        if (put(self, args) >= 0)
            res = 0;
        goto finally;
    }
    if (put2(self, args) < 0)
        goto finally;

    /* Materialize the dict items. */
    items = PyObject_CallMethod(args, "items", "()");
    if (items == NULL)
        goto finally;
    iter = PyObject_GetIter(items);
    Py_DECREF(items);
    if (iter == NULL)
        goto finally;
    res = batch_dict(self, iter);
    Py_DECREF(iter);

  finally:
    if (self->fast && !fast_save_leave(self, args))
        res = -1;

    return res;
}

static int
save_global(PicklerObject *self, PyObject *args, PyObject *name)
{
    PyObject *global_name = 0, *module = 0, *mod = 0, *klass = 0;
    char *name_str, *module_str;
    int module_size, name_size, res = -1;

    static char global = GLOBAL;

    if (name) {
        global_name = name;
        Py_INCREF(global_name);
    }
    else {
        if (!(global_name = PyObject_GetAttr(args, __name__)))
            goto finally;
    }

    if (!(module = whichmodule(args, global_name)))
        goto finally;

    if ((module_size = PyString_Size(module)) < 0 ||
        (name_size = PyString_Size(global_name)) < 0)
        goto finally;

    module_str = PyString_AS_STRING((PyStringObject *) module);
    name_str = PyString_AS_STRING((PyStringObject *) global_name);

    /* XXX This can be doing a relative import.  Clearly it shouldn't,
     * but I don't know how to stop it. :-( */
    mod = PyImport_ImportModule(module_str);
    if (mod == NULL) {
        pickle_ErrFormat(PicklingError,
                          "Can't pickle %s: import of module %s "
                          "failed", "OS", args, module);
        goto finally;
    }
    klass = PyObject_GetAttrString(mod, name_str);
    if (klass == NULL) {
        pickle_ErrFormat(PicklingError,
                          "Can't pickle %s: attribute lookup %s.%s "
                          "failed", "OSS", args, module, global_name);
        goto finally;
    }
    if (klass != args) {
        Py_DECREF(klass);
        pickle_ErrFormat(PicklingError,
                          "Can't pickle %s: it's not the same object "
                          "as %s.%s", "OSS", args, module, global_name);
        goto finally;
    }
    Py_DECREF(klass);

    if (self->proto >= 2) {
        /* See whether this is in the extension registry, and if
         * so generate an EXT opcode.
         */
        PyObject *py_code;      /* extension code as Python object */
        long code;              /* extension code as C value */
        char c_str[5];
        int n;

        PyTuple_SET_ITEM(two_tuple, 0, module);
        PyTuple_SET_ITEM(two_tuple, 1, global_name);
        py_code = PyDict_GetItem(extension_registry, two_tuple);
        if (py_code == NULL)
            goto gen_global;    /* not registered */

        /* Verify py_code has the right type and value. */
        if (!PyInt_Check(py_code)) {
            pickle_ErrFormat(PicklingError, "Can't pickle %s: "
                              "extension code %s isn't an integer",
                              "OO", args, py_code);
            goto finally;
        }
        code = PyInt_AS_LONG(py_code);
        if (code <= 0 || code > 0x7fffffffL) {
            pickle_ErrFormat(PicklingError, "Can't pickle %s: "
                              "extension code %ld is out of range",
                              "Ol", args, code);
            goto finally;
        }

        /* Generate an EXT opcode. */
        if (code <= 0xff) {
            c_str[0] = EXT1;
            c_str[1] = (char)code;
            n = 2;
        }
        else if (code <= 0xffff) {
            c_str[0] = EXT2;
            c_str[1] = (char)(code & 0xff);
            c_str[2] = (char)((code >> 8) & 0xff);
            n = 3;
        }
        else {
            c_str[0] = EXT4;
            c_str[1] = (char)(code & 0xff);
            c_str[2] = (char)((code >> 8) & 0xff);
            c_str[3] = (char)((code >> 16) & 0xff);
            c_str[4] = (char)((code >> 24) & 0xff);
            n = 5;
        }

        if (self->write_func(self, c_str, n) >= 0)
            res = 0;
        goto finally;           /* and don't memoize */
    }

  gen_global:
    if (self->write_func(self, &global, 1) < 0)
         goto finally;

    if (self->write_func(self, module_str, module_size) < 0)
        goto finally;

    if (self->write_func(self, "\n", 1) < 0)
        goto finally;

    if (self->write_func(self, name_str, name_size) < 0)
        goto finally;

    if (self->write_func(self, "\n", 1) < 0)
        goto finally;

    if (put(self, args) < 0)
        goto finally;

    res = 0;

  finally:
    Py_XDECREF(module);
    Py_XDECREF(global_name);
    Py_XDECREF(mod);

    return res;
}

static int
save_pers(PicklerObject *self, PyObject *args, PyObject *f)
{
    PyObject *pid = 0;
    int size, res = -1;

    static char persid = PERSID, binpersid = BINPERSID;

    Py_INCREF(args);
    ARG_TUP(self, args);
    if (self->arg) {
        pid = PyObject_Call(f, self->arg, NULL);
        FREE_ARG_TUP(self);
    }
    if (!pid)
        return -1;

    if (pid != Py_None) {
        if (!self->bin) {
            if (!PyString_Check(pid)) {
                PyErr_SetString(PicklingError, "persistent id must be string");
                goto finally;
            }

            if (self->write_func(self, &persid, 1) < 0)
                goto finally;

            if ((size = PyString_Size(pid)) < 0)
                goto finally;

            if (self->write_func(self,
                                 PyString_AS_STRING((PyStringObject *) pid),
                                 size) < 0)
                goto finally;

            if (self->write_func(self, "\n", 1) < 0)
                goto finally;

            res = 1;
            goto finally;
        }
        else if (save(self, pid, 1) >= 0) {
            if (self->write_func(self, &binpersid, 1) < 0)
                res = -1;
            else
                res = 1;
        }

        goto finally;
    }

    res = 0;

  finally:
    Py_XDECREF(pid);

    return res;
}

/* We're saving ob, and args is the 2-thru-5 tuple returned by the
 * appropriate __reduce__ method for ob.
 */
static int
save_reduce(PicklerObject *self, PyObject *args, PyObject *ob)
{
    PyObject *callable;
    PyObject *argtup;
    PyObject *state = NULL;
    PyObject *listitems = NULL;
    PyObject *dictitems = NULL;

    int use_newobj = self->proto >= 2;

    static char reduce = REDUCE;
    static char build = BUILD;
    static char newobj = NEWOBJ;

    if (!PyArg_UnpackTuple(args, "save_reduce", 2, 5,
                           &callable, &argtup, &state, &listitems, &dictitems))
        return -1;

    if (!PyTuple_Check(argtup)) {
        PyErr_SetString(PicklingError, "args from reduce() should be a tuple");
        return -1;
    }

    if (state == Py_None)
        state = NULL;
    if (listitems == Py_None)
        listitems = NULL;
    if (dictitems == Py_None)
        dictitems = NULL;

    /* Protocol 2 special case: if callable's name is __newobj__, use
     * NEWOBJ.  This consumes a lot of code.
     */
    if (use_newobj) {
        PyObject *temp = PyObject_GetAttr(callable, __name__);

        if (temp == NULL) {
            if (PyErr_ExceptionMatches(PyExc_AttributeError))
                PyErr_Clear();
            else
                return -1;
            use_newobj = 0;
        }
        else {
            use_newobj = PyString_Check(temp) &&
                strcmp(PyString_AS_STRING(temp), "__newobj__") == 0;
            Py_DECREF(temp);
        }
    }
    if (use_newobj) {
        PyObject *cls;
        PyObject *newargtup;
        int n, i;

        /* Sanity checks. */
        n = PyTuple_Size(argtup);
        if (n < 1) {
            PyErr_SetString(PicklingError, "__newobj__ arglist " "is empty");
            return -1;
        }

        cls = PyTuple_GET_ITEM(argtup, 0);
        if (!PyObject_HasAttrString(cls, "__new__")) {
            PyErr_SetString(PicklingError, "args[0] from "
                            "__newobj__ args has no __new__");
            return -1;
        }

        /* XXX How could ob be NULL? */
        if (ob != NULL) {
            PyObject *ob_dot_class;

            ob_dot_class = PyObject_GetAttr(ob, __class__);
            if (ob_dot_class == NULL) {
                if (PyErr_ExceptionMatches(PyExc_AttributeError))
                    PyErr_Clear();
                else
                    return -1;
            }
            i = ob_dot_class != cls;    /* true iff a problem */
            Py_XDECREF(ob_dot_class);
            if (i) {
                PyErr_SetString(PicklingError, "args[0] from "
                                "__newobj__ args has the wrong class");
                return -1;
            }
        }

        /* Save the class and its __new__ arguments. */
        if (save(self, cls, 0) < 0)
            return -1;

        newargtup = PyTuple_New(n - 1); /* argtup[1:] */
        if (newargtup == NULL)
            return -1;
        for (i = 1; i < n; ++i) {
            PyObject *temp = PyTuple_GET_ITEM(argtup, i);
            Py_INCREF(temp);
            PyTuple_SET_ITEM(newargtup, i - 1, temp);
        }
        i = save(self, newargtup, 0) < 0;
        Py_DECREF(newargtup);
        if (i < 0)
            return -1;

        /* Add NEWOBJ opcode. */
        if (self->write_func(self, &newobj, 1) < 0)
            return -1;
    }
    else {
        /* Not using NEWOBJ. */
        if (save(self, callable, 0) < 0 ||
            save(self, argtup, 0) < 0 ||
            self->write_func(self, &reduce, 1) < 0)
            return -1;
    }

    /* Memoize. */
    /* XXX How can ob be NULL? */
    if (ob != NULL) {
        if (state && !PyDict_Check(state)) {
            if (put2(self, ob) < 0)
                return -1;
        }
        else if (put(self, ob) < 0)
            return -1;
    }


    if (listitems && batch_list(self, listitems) < 0)
        return -1;

    if (dictitems && batch_dict(self, dictitems) < 0)
        return -1;

    if (state) {
        if (save(self, state, 0) < 0 || self->write_func(self, &build, 1) < 0)
            return -1;
    }

    return 0;
}

static int
save(PicklerObject *self, PyObject *args, int pers_save)
{
    PyTypeObject *type;
    PyObject *py_ob_id = 0, *__reduce__ = 0, *t = 0;
    PyObject *arg_tup;
    PyObject *pers_func = NULL;
    int res = -1;
    int tmp, size;

    if (self->nesting++ > Py_GetRecursionLimit()) {
        PyErr_SetString(PyExc_RuntimeError,
                        "maximum recursion depth exceeded");
        goto finally;
    }

    pers_func = PyObject_GetAttrString((PyObject *)self, "persistent_id");
    if (pers_func == NULL)
        PyErr_Clear();

    if (!pers_save && pers_func) {
        if ((tmp = save_pers(self, args, pers_func)) != 0) {
            res = tmp;
            goto finally;
        }
    }

    if (args == Py_None) {
        res = save_none(self, args);
        goto finally;
    }

    type = args->ob_type;

    switch (type->tp_name[0]) {
    case 'b':
        if (args == Py_False || args == Py_True) {
            res = save_bool(self, args);
            goto finally;
        }
        break;
    case 'i':
        if (type == &PyLong_Type) {
            res = save_long(self, args);
            goto finally;
        }
        break;

    case 'f':
        if (type == &PyFloat_Type) {
            res = save_float(self, args);
            goto finally;
        }
        break;

    case 't':
        if (type == &PyTuple_Type && PyTuple_Size(args) == 0) {
            res = save_tuple(self, args);
            goto finally;
        }
        break;

    case 's':
        if ((type == &PyString_Type) && (PyString_GET_SIZE(args) < 2)) {
            res = save_string(self, args, 0);
            goto finally;
        }

#ifdef Py_USING_UNICODE
    case 'u':
        if ((type == &PyUnicode_Type) && (PyString_GET_SIZE(args) < 2)) {
            res = save_unicode(self, args, 0);
            goto finally;
        }
#endif
    }

    if (args->ob_refcnt > 1) {
        if (!(py_ob_id = PyLong_FromVoidPtr(args)))
            goto finally;

        if (PyDict_GetItem(self->memo, py_ob_id)) {
            if (get(self, py_ob_id) < 0)
                goto finally;

            res = 0;
            goto finally;
        }
    }

    switch (type->tp_name[0]) {
    case 's':
        if (type == &PyString_Type) {
            res = save_string(self, args, 1);
            goto finally;
        }
        break;

#ifdef Py_USING_UNICODE
    case 'u':
        if (type == &PyUnicode_Type) {
            res = save_unicode(self, args, 1);
            goto finally;
        }
        break;
#endif

    case 't':
        if (type == &PyTuple_Type) {
            res = save_tuple(self, args);
            goto finally;
        }
        if (type == &PyType_Type) {
            res = save_global(self, args, NULL);
            goto finally;
        }
        break;

    case 'l':
        if (type == &PyList_Type) {
            res = save_list(self, args);
            goto finally;
        }
        break;

    case 'd':
        if (type == &PyDict_Type) {
            res = save_dict(self, args);
            goto finally;
        }
        break;

    case 'i':
        break;

    case 'c':
        break;

    case 'f':
        if (type == &PyFunction_Type) {
            res = save_global(self, args, NULL);
            if (res && PyErr_ExceptionMatches(PickleError)) {
                /* fall back to reduce */
                PyErr_Clear();
                break;
            }
            goto finally;
        }
        break;

    case 'b':
        if (type == &PyCFunction_Type) {
            res = save_global(self, args, NULL);
            goto finally;
        }
    }

    if (PyType_IsSubtype(type, &PyType_Type)) {
        res = save_global(self, args, NULL);
        goto finally;
    }

    /* Get a reduction callable, and call it.  This may come from
     * copy_reg.dispatch_table, the object's __reduce_ex__ method,
     * or the object's __reduce__ method.
     */
    __reduce__ = PyDict_GetItem(dispatch_table, (PyObject *) type);
    if (__reduce__ != NULL) {
        Py_INCREF(__reduce__);
        Py_INCREF(args);
        ARG_TUP(self, args);
        if (self->arg) {
            t = PyObject_Call(__reduce__, self->arg, NULL);
            FREE_ARG_TUP(self);
        }
    }
    else {
        /* Check for a __reduce_ex__ method. */
        __reduce__ = PyObject_GetAttr(args, __reduce_ex__);
        if (__reduce__ != NULL) {
            t = PyInt_FromLong(self->proto);
            if (t != NULL) {
                ARG_TUP(self, t);
                t = NULL;
                if (self->arg) {
                    t = PyObject_Call(__reduce__, self->arg, NULL);
                    FREE_ARG_TUP(self);
                }
            }
        }
        else {
            if (PyErr_ExceptionMatches(PyExc_AttributeError))
                PyErr_Clear();
            else
                goto finally;
            /* Check for a __reduce__ method. */
            __reduce__ = PyObject_GetAttr(args, __reduce__);
            if (__reduce__ != NULL) {
                t = PyObject_Call(__reduce__, empty_tuple, NULL);
            }
            else {
                pickle_ErrFormat(PicklingError, "Can't pickle '%s' object: %r",
                                 "sO", type->tp_name, args);
                goto finally;
            }
        }
    }

    if (t == NULL)
        goto finally;

    if (PyString_Check(t)) {
        res = save_global(self, args, t);
        goto finally;
    }

    if (!PyTuple_Check(t)) {
        pickle_ErrFormat(PicklingError, "Value returned by "
                          "%s must be string or tuple", "O", __reduce__);
        goto finally;
    }

    size = PyTuple_Size(t);
    if (size < 2 || size > 5) {
        pickle_ErrFormat(PicklingError, "tuple returned by "
                          "%s must contain 2 through 5 elements",
                          "O", __reduce__);
        goto finally;
    }

    arg_tup = PyTuple_GET_ITEM(t, 1);
    if (!(PyTuple_Check(arg_tup) || arg_tup == Py_None)) {
        pickle_ErrFormat(PicklingError, "Second element of "
                          "tuple returned by %s must be a tuple",
                          "O", __reduce__);
        goto finally;
    }

    res = save_reduce(self, t, args);

  finally:
    self->nesting--;
    Py_XDECREF(pers_func);
    Py_XDECREF(py_ob_id);
    Py_XDECREF(__reduce__);
    Py_XDECREF(t);

    return res;
}

static int
dump(PicklerObject *self, PyObject *args)
{
    static char stop = STOP;

    if (self->proto >= 2) {
        char bytes[2];

        bytes[0] = PROTO;
        assert(self->proto >= 0 && self->proto < 256);
        bytes[1] = (char) self->proto;
        if (self->write_func(self, bytes, 2) < 0)
            return -1;
    }

    if (save(self, args, 0) < 0)
        return -1;

    if (self->write_func(self, &stop, 1) < 0)
        return -1;

    if (self->write_func(self, NULL, 0) < 0)
        return -1;

    return 0;
}

static PyObject *
Pickle_clear_memo(PicklerObject *self, PyObject *args)
{
    if (self->memo)
        PyDict_Clear(self->memo);

    Py_RETURN_NONE;
}

static PyObject *
Pickler_dump(PicklerObject *self, PyObject *args)
{
    PyObject *ob;

    if (!(PyArg_ParseTuple(args, "O:dump", &ob)))
        return NULL;

    if (dump(self, ob) < 0)
        return NULL;

    Py_RETURN_NONE;
}

PyDoc_STRVAR(Pickler_dump_doc,
"dump(obj) -> None. Write a pickled representation of obj to the open file.");

PyDoc_STRVAR(Pickler_clear_memo_doc,
"clear_memo() -> None. Clears the pickler's \"memo\"."
"\n"
"The memo is the data structure that remembers which objects the\n"
"pickler has already seen, so that shared or recursive objects are\n"
"pickled by reference and not by value.  This method is useful when\n"
"re-using picklers.");

static struct PyMethodDef Pickler_methods[] = {
    {"dump", (PyCFunction) Pickler_dump, METH_VARARGS,
     Pickler_dump_doc},
    {"clear_memo", (PyCFunction) Pickle_clear_memo, METH_NOARGS,
     Pickler_clear_memo_doc},
    {NULL, NULL}                /* sentinel */
};

static PyObject *
Pickler_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PicklerObject *self;

    self = (PicklerObject *)type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;

    self->proto = 0;
    self->bin = 0;
	self->fp = NULL;
	self->write = NULL;
	self->memo = NULL;
	self->arg = NULL;
	self->write_buf = NULL;
	self->fast = 0;
    self->nesting = 0;
	self->fast_container = 0;
	self->fast_memo = NULL;
	self->buf_size = 0;
	self->dispatch_table = NULL;
    self->pers_func = NULL;

    self->memo = PyDict_New();
    if (self->memo == NULL) {
        Py_DECREF(self);
        return NULL;
    }

    return (PyObject *)self;
}

static int
Pickler_init(PicklerObject *self, PyObject *args, PyObject *kwds)
{ 
    static char *kwlist[] = {"file", "protocol", NULL, 0};
    PyObject *file;
    int proto = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|i:Pickler",
                                     kwlist, &file, &proto))
        return -1;

    if (proto < 0)
        proto = HIGHEST_PROTOCOL;
    if (proto > HIGHEST_PROTOCOL) {
        PyErr_Format(PyExc_ValueError, "pickle protocol must be <= %d",
                     HIGHEST_PROTOCOL);
        return -1;
    }
	self->proto = proto;
	self->bin = proto > 0;

    Py_INCREF(dispatch_table);
    self->dispatch_table = dispatch_table;

    Py_INCREF(file);
    self->file = file;
    self->write = PyObject_GetAttrString(file, "write");
    if (self->write == NULL) {
        PyErr_Clear();
        PyErr_SetString(PyExc_TypeError,
                        "argument must have 'write' attribute");
        return -1;
    }

    self->write_buf = (char *)PyMem_Malloc(WRITE_BUF_SIZE);
    if (self->write_buf == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    self->write_func = write_other;

    self->pers_func = NULL;
    if (PyObject_HasAttrString((PyObject *)self, "persistent_id")) {
        self->pers_func = PyObject_GetAttrString((PyObject *)self,
                                                 "persistent_id");
        if (self->pers_func == NULL)
            return -1;
    }

    return 0;
}

static void
Pickler_dealloc(PicklerObject *self)
{
    PyObject_GC_UnTrack(self);
    Py_XDECREF(self->file);
    Py_XDECREF(self->write);
    Py_XDECREF(self->memo);
    Py_XDECREF(self->fast_memo);
    Py_XDECREF(self->arg);
    Py_XDECREF(self->pers_func);
    Py_XDECREF(self->dispatch_table);
    PyMem_Free(self->write_buf);
    Py_Type(self)->tp_free((PyObject *)self);
}

static int
Pickler_traverse(PicklerObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->write);
    Py_VISIT(self->memo);
    Py_VISIT(self->fast_memo);
    Py_VISIT(self->arg);
    Py_VISIT(self->pers_func);
    Py_VISIT(self->dispatch_table);
    return 0;
}

static int
Pickler_clear(PicklerObject *self)
{
    Py_CLEAR(self->write);
    Py_CLEAR(self->memo);
    Py_CLEAR(self->fast_memo);
    Py_CLEAR(self->arg);
    Py_CLEAR(self->pers_func);
    Py_CLEAR(self->dispatch_table);
    return 0;
}

static PyObject *
Pickler_get_memo(PicklerObject *p)
{
    if (p->memo == NULL)
        PyErr_SetString(PyExc_AttributeError, "memo");
    else
        Py_INCREF(p->memo);
    return p->memo;
}

static int
Pickler_set_memo(PicklerObject *p, PyObject *v)
{
    if (v == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "attribute deletion is not supported");
        return -1;
    }
    if (!PyDict_Check(v)) {
        PyErr_SetString(PyExc_TypeError, "memo must be a dictionary");
        return -1;
    }
    Py_XDECREF(p->memo);
    Py_INCREF(v);
    p->memo = v;
    return 0;
}

static PyMemberDef Pickler_members[] = {
    {"bin", T_INT, offsetof(PicklerObject, bin)},
    {"fast", T_INT, offsetof(PicklerObject, fast)},
    {NULL}
};

static PyGetSetDef Pickler_getsets[] = {
    {"memo", (getter)Pickler_get_memo, (setter)Pickler_set_memo},
    {NULL}
};

PyDoc_STRVAR(Pickler_doc,
"Pickler(file, protocol=0) -> new pickler object"
"\n"
"This takes a file-like object for writing a pickle data stream.\n"
"\n"
"The optional protocol argument tells the pickler to use the\n"
"given protocol; supported protocols are 0, 1, 2.  The default\n"
"protocol is 0, to be backwards compatible.  (Protocol 0 is the\n"
"only protocol that can be written to a file opened in text\n"
"mode and read back successfully.  When using a protocol higher\n"
"than 0, make sure the file is opened in binary mode, both when\n"
"pickling and unpickling.)\n"
"\n"
"Protocol 1 is more efficient than protocol 0; protocol 2 is\n"
"more efficient than protocol 1.\n"
"\n"
"Specifying a negative protocol version selects the highest\n"
"protocol version supported.  The higher the protocol used, the\n"
"more recent the version of Python needed to read the pickle\n"
"produced.\n"
"\n"
"The file parameter must have a write() method that accepts a single\n"
"string argument.  It can thus be an open file object, a StringIO\n"
"object, or any other custom object that meets this interface.\n");


static PyTypeObject Pickler_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_pickle.Pickler"  ,                /*tp_name*/
    sizeof(PicklerObject),              /*tp_basicsize*/
    0,                                  /*tp_itemsize*/
    (destructor)Pickler_dealloc,        /*tp_dealloc*/
    0,                                  /*tp_print*/
    0,                                  /*tp_getattr*/
    0,                                  /*tp_setattr*/
    0,                                  /*tp_compare*/
    0,                                  /*tp_repr*/
    0,                                  /*tp_as_number*/
    0,                                  /*tp_as_sequence*/
    0,                                  /*tp_as_mapping*/
    0,                                  /*tp_hash*/
    0,                                  /*tp_call*/
    0,                                  /*tp_str*/
    PyObject_GenericGetAttr,            /*tp_getattro*/
    PyObject_GenericSetAttr,            /*tp_setattro*/
    0,                                  /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    Pickler_doc,                        /*tp_doc*/
    (traverseproc)Pickler_traverse,     /*tp_traverse*/
    (inquiry)Pickler_clear,             /*tp_clear*/
    0,                                  /*tp_richcompare*/
    0,                                  /*tp_weaklistoffset*/
    0,                                  /*tp_iter*/
    0,                                  /*tp_iternext*/
    Pickler_methods,                    /*tp_methods*/
    Pickler_members,                    /*tp_members*/
    Pickler_getsets,                    /*tp_getset*/
    0,                                  /*tp_base*/
    0,                                  /*tp_dict*/
    0,                                  /*tp_descr_get*/
    0,                                  /*tp_descr_set*/
    0,                                  /*tp_dictoffset*/
    (initproc)Pickler_init,             /*tp_init*/
    0,                                  /*tp_alloc*/
    Pickler_new,                        /*tp_new*/
    0,                                  /*tp_free*/
    0,                                  /*tp_is_gc*/
};

static PyObject *
find_class(PyObject *py_module_name, PyObject *py_global_name, PyObject *fc)
{
    PyObject *global = 0, *module;

    if (fc) {
        if (fc == Py_None) {
            PyErr_SetString(UnpicklingError, "Global and instance "
                            "pickles are not supported.");
            return NULL;
        }
        return PyObject_CallFunctionObjArgs(fc, py_module_name,
                                            py_global_name, NULL);
    }

    module = PySys_GetObject("modules");
    if (module == NULL)
        return NULL;

    module = PyDict_GetItem(module, py_module_name);
    if (module == NULL) {
        module = PyImport_Import(py_module_name);
        if (!module)
            return NULL;
        global = PyObject_GetAttr(module, py_global_name);
        Py_DECREF(module);
    }
    else
        global = PyObject_GetAttr(module, py_global_name);
    return global;
}

static int
marker(UnpicklerObject *self)
{
    if (self->num_marks < 1) {
        PyErr_SetString(UnpicklingError, "could not find MARK");
        return -1;
    }

    return self->marks[--self->num_marks];
}

static int
load_none(UnpicklerObject *self)
{
    PDATA_APPEND(self->stack, Py_None, -1);
    return 0;
}

static int
bad_readline(void)
{
    PyErr_SetString(UnpicklingError, "pickle data was truncated");
    return -1;
}

static int
load_int(UnpicklerObject *self)
{
    PyObject *py_int = 0;
    char *endptr, *s;
    int len, res = -1;
    long l;

    if ((len = self->readline_func(self, &s)) < 0)
        return -1;
    if (len < 2)
        return bad_readline();
    if (!(s = pystrndup(s, len)))
        return -1;

    errno = 0;
    l = strtol(s, &endptr, 0);

    if (errno || (*endptr != '\n') || (endptr[1] != '\0')) {
        /* Hm, maybe we've got something long.  Let's try reading
         * it as a Python long object. */
        errno = 0;
        py_int = PyLong_FromString(s, NULL, 0);
        if (py_int == NULL) {
            PyErr_SetString(PyExc_ValueError,
                            "could not convert string to int");
            goto finally;
        }
    }
    else {
        if (len == 3 && (l == 0 || l == 1)) {
            if (!(py_int = PyBool_FromLong(l)))
                goto finally;
        }
        else {
            if (!(py_int = PyInt_FromLong(l)))
                goto finally;
        }
    }

    free(s);
    PDATA_PUSH(self->stack, py_int, -1);
    return 0;

  finally:
    free(s);

    return res;
}

static int
load_bool(UnpicklerObject *self, PyObject *boolean)
{
    assert(boolean == Py_True || boolean == Py_False);
    PDATA_APPEND(self->stack, boolean, -1);
    return 0;
}

/* s contains x bytes of a little-endian integer.  Return its value as a
 * C int.  Obscure:  when x is 1 or 2, this is an unsigned little-endian
 * int, but when x is 4 it's a signed one.  This is an historical source
 * of x-platform bugs.
 */
static long
calc_binint(char *s, int x)
{
    unsigned char c;
    int i;
    long l;

    for (i = 0, l = 0L; i < x; i++) {
        c = (unsigned char) s[i];
        l |= (long) c << (i * 8);
    }
#if SIZEOF_LONG > 4
    /* Unlike BININT1 and BININT2, BININT (more accurately BININT4)
     * is signed, so on a box with longs bigger than 4 bytes we need
     * to extend a BININT's sign bit to the full width.
     */
    if (x == 4 && l & (1L << 31))
        l |= (~0L) << 32;
#endif
    return l;
}

static int
load_binintx(UnpicklerObject *self, char *s, int x)
{
    PyObject *py_int = 0;
    long l;

    l = calc_binint(s, x);

    if (!(py_int = PyInt_FromLong(l)))
        return -1;

    PDATA_PUSH(self->stack, py_int, -1);
    return 0;
}

static int
load_binint(UnpicklerObject *self)
{
    char *s;

    if (self->read_func(self, &s, 4) < 0)
        return -1;

    return load_binintx(self, s, 4);
}

static int
load_binint1(UnpicklerObject *self)
{
    char *s;

    if (self->read_func(self, &s, 1) < 0)
        return -1;

    return load_binintx(self, s, 1);
}

static int
load_binint2(UnpicklerObject *self)
{
    char *s;

    if (self->read_func(self, &s, 2) < 0)
        return -1;

    return load_binintx(self, s, 2);
}

static int
load_long(UnpicklerObject *self)
{
    PyObject *l = 0;
    char *end, *s;
    int len, res = -1;

    if ((len = self->readline_func(self, &s)) < 0)
        return -1;
    if (len < 2)
        return bad_readline();
    if (!(s = pystrndup(s, len)))
        return -1;

    if (!(l = PyLong_FromString(s, &end, 0)))
        goto finally;

    free(s);
    PDATA_PUSH(self->stack, l, -1);
    return 0;

  finally:
    free(s);

    return res;
}

/* 'size' bytes contain the # of bytes of little-endian 256's-complement
 * data following.
 */
static int
load_counted_long(UnpicklerObject *self, int size)
{
    Py_ssize_t i;
    char *nbytes;
    unsigned char *pdata;
    PyObject *along;

    assert(size == 1 || size == 4);
    i = self->read_func(self, &nbytes, size);
    if (i < 0)
        return -1;

    size = calc_binint(nbytes, size);
    if (size < 0) {
        /* Corrupt or hostile pickle -- we never write one like
         * this.
         */
        PyErr_SetString(UnpicklingError, "LONG pickle has negative "
                        "byte count");
        return -1;
    }

    if (size == 0)
        along = PyLong_FromLong(0L);
    else {
        /* Read the raw little-endian bytes & convert. */
        i = self->read_func(self, (char **) &pdata, size);
        if (i < 0)
            return -1;
        along = _PyLong_FromByteArray(pdata, (size_t) size,
                                      1 /* little endian */ , 1 /* signed */ );
    }
    if (along == NULL)
        return -1;
    PDATA_PUSH(self->stack, along, -1);
    return 0;
}

static int
load_float(UnpicklerObject *self)
{
    PyObject *py_float = 0;
    char *endptr, *s;
    int len, res = -1;
    double d;

    if ((len = self->readline_func(self, &s)) < 0)
        return -1;
    if (len < 2)
        return bad_readline();
    if (!(s = pystrndup(s, len)))
        return -1;

    errno = 0;
    d = PyOS_ascii_strtod(s, &endptr);

    if (errno || (endptr[0] != '\n') || (endptr[1] != '\0')) {
        PyErr_SetString(PyExc_ValueError, "could not convert string to float");
        goto finally;
    }

    if (!(py_float = PyFloat_FromDouble(d)))
        goto finally;

    free(s);
    PDATA_PUSH(self->stack, py_float, -1);
    return 0;

  finally:
    free(s);

    return res;
}

static int
load_binfloat(UnpicklerObject *self)
{
    PyObject *py_float;
    double x;
    char *p;

    if (self->read_func(self, &p, 8) < 0)
        return -1;

    x = _PyFloat_Unpack8((unsigned char *) p, 0);
    if (x == -1.0 && PyErr_Occurred())
        return -1;

    py_float = PyFloat_FromDouble(x);
    if (py_float == NULL)
        return -1;

    PDATA_PUSH(self->stack, py_float, -1);
    return 0;
}

static int
load_string(UnpicklerObject *self)
{
    PyObject *str = 0;
    int len, res = -1;
    char *s, *p;

    if ((len = self->readline_func(self, &s)) < 0)
        return -1;
    if (len < 2)
        return bad_readline();
    if (!(s = pystrndup(s, len)))
        return -1;


    /* Strip outermost quotes */
    while (s[len - 1] <= ' ')
        len--;
    if (s[0] == '"' && s[len - 1] == '"') {
        s[len - 1] = '\0';
        p = s + 1;
        len -= 2;
    }
    else if (s[0] == '\'' && s[len - 1] == '\'') {
        s[len - 1] = '\0';
        p = s + 1;
        len -= 2;
    }
    else
        goto insecure;

    str = PyString_DecodeEscape(p, len, NULL, 0, NULL);
    free(s);
    if (str) {
        PDATA_PUSH(self->stack, str, -1);
        res = 0;
    }
    return res;

  insecure:
    free(s);
    PyErr_SetString(PyExc_ValueError, "insecure string pickle");
    return -1;
}

static int
load_binstring(UnpicklerObject *self)
{
    PyObject *py_string = 0;
    long l;
    char *s;

    if (self->read_func(self, &s, 4) < 0)
        return -1;

    l = calc_binint(s, 4);

    if (self->read_func(self, &s, l) < 0)
        return -1;

    if (!(py_string = PyString_FromStringAndSize(s, l)))
        return -1;

    PDATA_PUSH(self->stack, py_string, -1);
    return 0;
}

static int
load_short_binstring(UnpicklerObject *self)
{
    PyObject *py_string = 0;
    unsigned char l;
    char *s;

    if (self->read_func(self, &s, 1) < 0)
        return -1;

    l = (unsigned char) s[0];

    if (self->read_func(self, &s, l) < 0)
        return -1;

    if (!(py_string = PyString_FromStringAndSize(s, l)))
        return -1;

    PDATA_PUSH(self->stack, py_string, -1);
    return 0;
}

#ifdef Py_USING_UNICODE
static int
load_unicode(UnpicklerObject *self)
{
    PyObject *str = 0;
    int len, res = -1;
    char *s;

    if ((len = self->readline_func(self, &s)) < 0)
        return -1;
    if (len < 1)
        return bad_readline();

    if (!(str = PyUnicode_DecodeRawUnicodeEscape(s, len - 1, NULL)))
        goto finally;

    PDATA_PUSH(self->stack, str, -1);
    return 0;

  finally:
    return res;
}
#endif


#ifdef Py_USING_UNICODE
static int
load_binunicode(UnpicklerObject *self)
{
    PyObject *unicode;
    long l;
    char *s;

    if (self->read_func(self, &s, 4) < 0)
        return -1;

    l = calc_binint(s, 4);

    if (self->read_func(self, &s, l) < 0)
        return -1;

    if (!(unicode = PyUnicode_DecodeUTF8(s, l, NULL)))
        return -1;

    PDATA_PUSH(self->stack, unicode, -1);
    return 0;
}
#endif


static int
load_tuple(UnpicklerObject *self)
{
    PyObject *tup;
    int i;

    if ((i = marker(self)) < 0)
        return -1;
    if (!(tup = Pdata_popTuple(self->stack, i)))
        return -1;
    PDATA_PUSH(self->stack, tup, -1);
    return 0;
}

static int
load_counted_tuple(UnpicklerObject *self, int len)
{
    PyObject *tup = PyTuple_New(len);

    if (tup == NULL)
        return -1;

    while (--len >= 0) {
        PyObject *element;

        PDATA_POP(self->stack, element);
        if (element == NULL)
            return -1;
        PyTuple_SET_ITEM(tup, len, element);
    }
    PDATA_PUSH(self->stack, tup, -1);
    return 0;
}

static int
load_empty_list(UnpicklerObject *self)
{
    PyObject *list;

    if (!(list = PyList_New(0)))
        return -1;
    PDATA_PUSH(self->stack, list, -1);
    return 0;
}

static int
load_empty_dict(UnpicklerObject *self)
{
    PyObject *dict;

    if (!(dict = PyDict_New()))
        return -1;
    PDATA_PUSH(self->stack, dict, -1);
    return 0;
}

static int
load_list(UnpicklerObject *self)
{
    PyObject *list = 0;
    int i;

    if ((i = marker(self)) < 0)
        return -1;
    if (!(list = Pdata_popList(self->stack, i)))
        return -1;
    PDATA_PUSH(self->stack, list, -1);
    return 0;
}

static int
load_dict(UnpicklerObject *self)
{
    PyObject *dict, *key, *value;
    int i, j, k;

    if ((i = marker(self)) < 0)
        return -1;
    j = self->stack->length;

    if (!(dict = PyDict_New()))
        return -1;

    for (k = i + 1; k < j; k += 2) {
        key = self->stack->data[k - 1];
        value = self->stack->data[k];
        if (PyDict_SetItem(dict, key, value) < 0) {
            Py_DECREF(dict);
            return -1;
        }
    }
    Pdata_clear(self->stack, i);
    PDATA_PUSH(self->stack, dict, -1);
    return 0;
}

static PyObject *
Instance_New(PyObject *cls, PyObject *args)
{
    PyObject *r = 0;

    if ((r = PyObject_CallObject(cls, args)))
        return r;

    {
        PyObject *tp, *v, *tb, *tmp_value;

        PyErr_Fetch(&tp, &v, &tb);
        tmp_value = v;
        /* NULL occurs when there was a KeyboardInterrupt */
        if (tmp_value == NULL)
            tmp_value = Py_None;
        if ((r = PyTuple_Pack(3, tmp_value, cls, args))) {
            Py_XDECREF(v);
            v = r;
        }
        PyErr_Restore(tp, v, tb);
    }
    return NULL;
}

static int
load_obj(UnpicklerObject *self)
{
    PyObject *class, *tup, *obj = 0;
    int i;

    if ((i = marker(self)) < 0)
        return -1;
    if (!(tup = Pdata_popTuple(self->stack, i + 1)))
        return -1;
    PDATA_POP(self->stack, class);
    if (class) {
        obj = Instance_New(class, tup);
        Py_DECREF(class);
    }
    Py_DECREF(tup);

    if (!obj)
        return -1;
    PDATA_PUSH(self->stack, obj, -1);
    return 0;
}

static int
load_inst(UnpicklerObject *self)
{
    PyObject *tup, *class = 0, *obj = 0, *module_name, *class_name;
    int i, len;
    char *s;

    if ((i = marker(self)) < 0)
        return -1;

    if ((len = self->readline_func(self, &s)) < 0)
        return -1;
    if (len < 2)
        return bad_readline();
    module_name = PyString_FromStringAndSize(s, len - 1);
    if (!module_name)
        return -1;

    if ((len = self->readline_func(self, &s)) >= 0) {
        if (len < 2)
            return bad_readline();
        if ((class_name = PyString_FromStringAndSize(s, len - 1))) {
            class = find_class(module_name, class_name, self->find_class);
            Py_DECREF(class_name);
        }
    }
    Py_DECREF(module_name);

    if (!class)
        return -1;

    if ((tup = Pdata_popTuple(self->stack, i))) {
        obj = Instance_New(class, tup);
        Py_DECREF(tup);
    }
    Py_DECREF(class);

    if (!obj)
        return -1;

    PDATA_PUSH(self->stack, obj, -1);
    return 0;
}

static int
load_newobj(UnpicklerObject *self)
{
    PyObject *args = NULL;
    PyObject *clsraw = NULL;
    PyTypeObject *cls;          /* clsraw cast to its true type */
    PyObject *obj;

    /* Stack is ... cls argtuple, and we want to call
     * cls.__new__(cls, *argtuple).
     */
    PDATA_POP(self->stack, args);
    if (args == NULL)
        goto Fail;
    if (!PyTuple_Check(args)) {
        PyErr_SetString(UnpicklingError, "NEWOBJ expected an arg " "tuple.");
        goto Fail;
    }

    PDATA_POP(self->stack, clsraw);
    cls = (PyTypeObject *) clsraw;
    if (cls == NULL)
        goto Fail;
    if (!PyType_Check(cls)) {
        PyErr_SetString(UnpicklingError, "NEWOBJ class argument "
                        "isn't a type object");
        goto Fail;
    }
    if (cls->tp_new == NULL) {
        PyErr_SetString(UnpicklingError, "NEWOBJ class argument "
                        "has NULL tp_new");
        goto Fail;
    }

    /* Call __new__. */
    obj = cls->tp_new(cls, args, NULL);
    if (obj == NULL)
        goto Fail;

    Py_DECREF(args);
    Py_DECREF(clsraw);
    PDATA_PUSH(self->stack, obj, -1);
    return 0;

  Fail:
    Py_XDECREF(args);
    Py_XDECREF(clsraw);
    return -1;
}

static int
load_global(UnpicklerObject *self)
{
    PyObject *class = 0, *module_name = 0, *class_name = 0;
    int len;
    char *s;

    if ((len = self->readline_func(self, &s)) < 0)
        return -1;
    if (len < 2)
        return bad_readline();
    module_name = PyString_FromStringAndSize(s, len - 1);
    if (!module_name)
        return -1;

    if ((len = self->readline_func(self, &s)) >= 0) {
        if (len < 2) {
            Py_DECREF(module_name);
            return bad_readline();
        }
        if ((class_name = PyString_FromStringAndSize(s, len - 1))) {
            class = find_class(module_name, class_name, self->find_class);
            Py_DECREF(class_name);
        }
    }
    Py_DECREF(module_name);

    if (!class)
        return -1;
    PDATA_PUSH(self->stack, class, -1);
    return 0;
}

static int
load_persid(UnpicklerObject *self)
{
    PyObject *pid = 0;
    int len;
    char *s;

    if (self->pers_func) {
        if ((len = self->readline_func(self, &s)) < 0)
            return -1;
        if (len < 2)
            return bad_readline();

        pid = PyString_FromStringAndSize(s, len - 1);
        if (!pid)
            return -1;

        if (PyList_Check(self->pers_func)) {
            if (PyList_Append(self->pers_func, pid) < 0) {
                Py_DECREF(pid);
                return -1;
            }
        }
        else {
            ARG_TUP(self, pid);
            if (self->arg) {
                pid = PyObject_Call(self->pers_func, self->arg, NULL);
                FREE_ARG_TUP(self);
            }
        }

        if (!pid)
            return -1;

        PDATA_PUSH(self->stack, pid, -1);
        return 0;
    }
    else {
        PyErr_SetString(UnpicklingError,
                        "A load persistent id instruction was encountered,\n"
                        "but no persistent_load function was specified.");
        return -1;
    }
}

static int
load_binpersid(UnpicklerObject *self)
{
    PyObject *pid = 0;

    if (self->pers_func) {
        PDATA_POP(self->stack, pid);
        if (!pid)
            return -1;

        if (PyList_Check(self->pers_func)) {
            if (PyList_Append(self->pers_func, pid) < 0) {
                Py_DECREF(pid);
                return -1;
            }
        }
        else {
            ARG_TUP(self, pid);
            if (self->arg) {
                pid = PyObject_Call(self->pers_func, self->arg, NULL);
                FREE_ARG_TUP(self);
            }
            if (!pid)
                return -1;
        }

        PDATA_PUSH(self->stack, pid, -1);
        return 0;
    }
    else {
        PyErr_SetString(UnpicklingError,
                        "A load persistent id instruction was encountered,\n"
                        "but no persistent_load function was specified.");
        return -1;
    }
}

static int
load_pop(UnpicklerObject *self)
{
    int len;

    if (!((len = self->stack->length) > 0))
        return stackUnderflow();

    /* Note that we split the (pickle.py) stack into two stacks,
     * an object stack and a mark stack. We have to be clever and
     * pop the right one. We do this by looking at the top of the
     * mark stack.
     */

    if ((self->num_marks > 0) && (self->marks[self->num_marks - 1] == len))
        self->num_marks--;
    else {
        len--;
        Py_DECREF(self->stack->data[len]);
        self->stack->length = len;
    }

    return 0;
}

static int
load_pop_mark(UnpicklerObject *self)
{
    int i;

    if ((i = marker(self)) < 0)
        return -1;

    Pdata_clear(self->stack, i);

    return 0;
}

static int
load_dup(UnpicklerObject *self)
{
    PyObject *last;
    int len;

    if ((len = self->stack->length) <= 0)
        return stackUnderflow();
    last = self->stack->data[len - 1];
    Py_INCREF(last);
    PDATA_PUSH(self->stack, last, -1);
    return 0;
}

static int
load_get(UnpicklerObject *self)
{
    PyObject *py_key, *value;
    int len;
    char *s;

    if ((len = self->readline_func(self, &s)) < 0)
        return -1;
    if (len < 2)
        return bad_readline();

    py_key = PyString_FromStringAndSize(s, len - 1);
    if (!py_key)
        return -1;

    value = PyDict_GetItem(self->memo, py_key);
    if (!value) {
        PyErr_SetObject(PyExc_KeyError, py_key);
        Py_DECREF(py_key);
        return -1;
    }

        PDATA_APPEND(self->stack, value, -1);
    Py_DECREF(py_key);
    return 0;
    }

static int
load_binget(UnpicklerObject *self)
{
    PyObject *py_key, *value;
    unsigned char key;
    char *s;

    if (self->read_func(self, &s, 1) < 0)
        return -1;

    key = (unsigned char) s[0];
    py_key = PyInt_FromLong((long)key);
    if (!py_key)
        return -1;

    value = PyDict_GetItem(self->memo, py_key);
    if (!value) {
        PyErr_SetObject(PyExc_KeyError, py_key);
        Py_DECREF(py_key);
        return -1;
    }

    PDATA_APPEND(self->stack, value, -1);
    Py_DECREF(py_key);
    return 0;
}

static int
load_long_binget(UnpicklerObject *self)
{
    PyObject *py_key = 0, *value = 0;
    unsigned char c;
    char *s;
    long key;

    if (self->read_func(self, &s, 4) < 0)
        return -1;

    c = (unsigned char)s[0];
    key = (long)c;
    c = (unsigned char)s[1];
    key |= (long)c << 8;
    c = (unsigned char)s[2];
    key |= (long)c << 16;
    c = (unsigned char)s[3];
    key |= (long)c << 24;

    if (!(py_key = PyInt_FromLong((long)key)))
        return -1;

    value = PyDict_GetItem(self->memo, py_key);
    if (!value) {
        Py_DECREF(py_key);
        PyErr_SetObject(PyExc_KeyError, py_key);
    }

    PDATA_APPEND(self->stack, value, -1);
    Py_DECREF(py_key);
    return 0;
}

/* Push an object from the extension registry (EXT[124]).  nbytes is
 * the number of bytes following the opcode, holding the index (code) value.
 */
static int
load_extension(UnpicklerObject *self, int nbytes)
{
    char *codebytes;            /* the nbytes bytes after the opcode */
    long code;                  /* calc_binint returns long */
    PyObject *py_code;          /* code as a Python int */
    PyObject *obj;              /* the object to push */
    PyObject *pair;             /* (module_name, class_name) */
    PyObject *module_name, *class_name;

    assert(nbytes == 1 || nbytes == 2 || nbytes == 4);
    if (self->read_func(self, &codebytes, nbytes) < 0)
        return -1;
    code = calc_binint(codebytes, nbytes);
    if (code <= 0) {            /* note that 0 is forbidden */
        /* Corrupt or hostile pickle. */
        PyErr_SetString(UnpicklingError, "EXT specifies code <= 0");
        return -1;
    }

    /* Look for the code in the cache. */
    py_code = PyInt_FromLong(code);
    if (py_code == NULL)
        return -1;
    obj = PyDict_GetItem(extension_cache, py_code);
    if (obj != NULL) {
        /* Bingo. */
        Py_DECREF(py_code);
        PDATA_APPEND(self->stack, obj, -1);
        return 0;
    }

    /* Look up the (module_name, class_name) pair. */
    pair = PyDict_GetItem(inverted_registry, py_code);
    if (pair == NULL) {
        Py_DECREF(py_code);
        PyErr_Format(PyExc_ValueError, "unregistered extension "
                     "code %ld", code);
        return -1;
    }
    /* Since the extension registry is manipulable via Python code,
     * confirm that pair is really a 2-tuple of strings.
     */
    if (!PyTuple_Check(pair) || PyTuple_Size(pair) != 2 ||
        !PyString_Check(module_name = PyTuple_GET_ITEM(pair, 0)) ||
        !PyString_Check(class_name = PyTuple_GET_ITEM(pair, 1))) {
        Py_DECREF(py_code);
        PyErr_Format(PyExc_ValueError, "_inverted_registry[%ld] "
                     "isn't a 2-tuple of strings", code);
        return -1;
    }
    /* Load the object. */
    obj = find_class(module_name, class_name, self->find_class);
    if (obj == NULL) {
        Py_DECREF(py_code);
        return -1;
    }
    /* Cache code -> obj. */
    code = PyDict_SetItem(extension_cache, py_code, obj);
    Py_DECREF(py_code);
    if (code < 0) {
        Py_DECREF(obj);
        return -1;
    }
    PDATA_PUSH(self->stack, obj, -1);
    return 0;
}

static int
load_put(UnpicklerObject *self)
{
    PyObject *py_str = 0, *value = 0;
    int len, l;
    char *s;

    if ((l = self->readline_func(self, &s)) < 0)
        return -1;
    if (l < 2)
        return bad_readline();
    if (!(len = self->stack->length))
        return stackUnderflow();
    if (!(py_str = PyString_FromStringAndSize(s, l - 1)))
        return -1;
    value = self->stack->data[len - 1];
    l = PyDict_SetItem(self->memo, py_str, value);
    Py_DECREF(py_str);
    return l;
}

static int
load_binput(UnpicklerObject *self)
{
    PyObject *py_key = 0, *value = 0;
    unsigned char key;
    char *s;
    int len;

    if (self->read_func(self, &s, 1) < 0)
        return -1;
    if (!((len = self->stack->length) > 0))
        return stackUnderflow();

    key = (unsigned char)s[0];

    if (!(py_key = PyInt_FromLong((long)key)))
        return -1;
    value = self->stack->data[len - 1];
    len = PyDict_SetItem(self->memo, py_key, value);
    Py_DECREF(py_key);
    return len;
}

static int
load_long_binput(UnpicklerObject *self)
{
    PyObject *py_key = 0, *value = 0;
    long key;
    unsigned char c;
    char *s;
    int len;

    if (self->read_func(self, &s, 4) < 0)
        return -1;
    if (!(len = self->stack->length))
        return stackUnderflow();

    c = (unsigned char)s[0];
    key = (long)c;
    c = (unsigned char)s[1];
    key |= (long)c << 8;
    c = (unsigned char)s[2];
    key |= (long)c << 16;
    c = (unsigned char)s[3];
    key |= (long)c << 24;

    if (!(py_key = PyInt_FromLong(key)))
        return -1;
    value = self->stack->data[len - 1];
    len = PyDict_SetItem(self->memo, py_key, value);
    Py_DECREF(py_key);
    return len;
}

static int
do_append(UnpicklerObject *self, int x)
{
    PyObject *value = 0, *list = 0, *append_method = 0;
    static PyObject *append = NULL;
    int len, i;

    if (append == NULL)
        INIT_STR(append);

    len = self->stack->length;
    if (!(len >= x && x > 0))
        return stackUnderflow();
    /* nothing to do */
    if (len == x)
        return 0;

    list = self->stack->data[x - 1];

    if (PyList_Check(list)) {
        PyObject *slice;
        int list_len;

        slice = Pdata_popList(self->stack, x);
        if (!slice)
            return -1;
        list_len = PyList_GET_SIZE(list);
        i = PyList_SetSlice(list, list_len, list_len, slice);
        Py_DECREF(slice);
        return i;
    }
    else {

        if (!(append_method = PyObject_GetAttr(list, append)))
            return -1;

        for (i = x; i < len; i++) {
            PyObject *junk;

            value = self->stack->data[i];
            junk = 0;
            ARG_TUP(self, value);
            if (self->arg) {
                junk = PyObject_Call(append_method, self->arg, NULL);
                FREE_ARG_TUP(self);
            }
            if (!junk) {
                Pdata_clear(self->stack, i + 1);
                self->stack->length = x;
                Py_DECREF(append_method);
                return -1;
            }
            Py_DECREF(junk);
        }
        self->stack->length = x;
        Py_DECREF(append_method);
    }

    return 0;
}

static int
load_append(UnpicklerObject *self)
{
    return do_append(self, self->stack->length - 1);
}

static int
load_appends(UnpicklerObject *self)
{
    return do_append(self, marker(self));
}

static int
do_setitems(UnpicklerObject *self, int x)
{
    PyObject *value = 0, *key = 0, *dict = 0;
    int len, i, r = 0;

    if (!((len = self->stack->length) >= x && x > 0))
        return stackUnderflow();

    dict = self->stack->data[x - 1];

    for (i = x + 1; i < len; i += 2) {
        key = self->stack->data[i - 1];
        value = self->stack->data[i];
        if (PyObject_SetItem(dict, key, value) < 0) {
            r = -1;
            break;
        }
    }

    Pdata_clear(self->stack, x);

    return r;
}

static int
load_setitem(UnpicklerObject *self)
{
    return do_setitems(self, self->stack->length - 2);
}

static int
load_setitems(UnpicklerObject *self)
{
    return do_setitems(self, marker(self));
}

static int
load_build(UnpicklerObject *self)
{
    PyObject *state, *inst, *slotstate;
    PyObject *setstate;
    PyObject *d_key, *d_value;
    Py_ssize_t i;
    int res = -1;

    /* Stack is ... instance, state.  We want to leave instance at
     * the stack top, possibly mutated via instance.__setstate__(state).
     */
    if (self->stack->length < 2)
        return stackUnderflow();
    PDATA_POP(self->stack, state);
    if (state == NULL)
        return -1;
    inst = self->stack->data[self->stack->length - 1];

    setstate = PyObject_GetAttr(inst, __setstate__);
    if (setstate != NULL) {
        PyObject *junk = NULL;

        /* The explicit __setstate__ is responsible for everything. */
        ARG_TUP(self, state);
        if (self->arg) {
            junk = PyObject_Call(setstate, self->arg, NULL);
            FREE_ARG_TUP(self);
        }
        Py_DECREF(setstate);
        if (junk == NULL)
            return -1;
        Py_DECREF(junk);
        return 0;
    }
    if (!PyErr_ExceptionMatches(PyExc_AttributeError))
        return -1;
    PyErr_Clear();

    /* A default __setstate__.  First see whether state embeds a
     * slot state dict too (a proto 2 addition).
     */
    if (PyTuple_Check(state) && PyTuple_Size(state) == 2) {
        PyObject *temp = state;
        state = PyTuple_GET_ITEM(temp, 0);
        slotstate = PyTuple_GET_ITEM(temp, 1);
        Py_INCREF(state);
        Py_INCREF(slotstate);
        Py_DECREF(temp);
    }
    else
        slotstate = NULL;

    /* Set inst.__dict__ from the state dict (if any). */
    if (state != Py_None) {
        PyObject *dict;
        if (!PyDict_Check(state)) {
            PyErr_SetString(UnpicklingError, "state is not a " "dictionary");
            goto finally;
        }
        dict = PyObject_GetAttr(inst, __dict__);
        if (dict == NULL)
            goto finally;

        i = 0;
        while (PyDict_Next(state, &i, &d_key, &d_value)) {
            if (PyObject_SetItem(dict, d_key, d_value) < 0)
                goto finally;
        }
        Py_DECREF(dict);
    }

    /* Also set instance attributes from the slotstate dict (if any). */
    if (slotstate != NULL) {
        if (!PyDict_Check(slotstate)) {
            PyErr_SetString(UnpicklingError, "slot state is not "
                            "a dictionary");
            goto finally;
        }
        i = 0;
        while (PyDict_Next(slotstate, &i, &d_key, &d_value)) {
            if (PyObject_SetAttr(inst, d_key, d_value) < 0)
                goto finally;
        }
    }
    res = 0;

  finally:
    Py_DECREF(state);
    Py_XDECREF(slotstate);
    return res;
}

static int
load_mark(UnpicklerObject *self)
{
    int s;

    /* Note that we split the (pickle.py) stack into two stacks, an
     * object stack and a mark stack. Here we push a mark onto the
     * mark stack.
     */

    if ((self->num_marks + 1) >= self->marks_size) {
        int *marks;
        s = self->marks_size + 20;
        if (s <= self->num_marks)
            s = self->num_marks + 1;
        if (self->marks == NULL)
            marks = (int *) malloc(s * sizeof(int));
        else
            marks = (int *) realloc(self->marks, s * sizeof(int));
        if (!marks) {
            PyErr_NoMemory();
            return -1;
        }
        self->marks = marks;
        self->marks_size = s;
    }

    self->marks[self->num_marks++] = self->stack->length;

    return 0;
}

static int
load_reduce(UnpicklerObject *self)
{
    PyObject *callable = 0, *arg_tup = 0, *ob = 0;

    PDATA_POP(self->stack, arg_tup);
    if (!arg_tup)
        return -1;
    PDATA_POP(self->stack, callable);
    if (callable) {
        ob = Instance_New(callable, arg_tup);
        Py_DECREF(callable);
    }
    Py_DECREF(arg_tup);

    if (!ob)
        return -1;

    PDATA_PUSH(self->stack, ob, -1);
    return 0;
}

/* Just raises an error if we don't know the protocol specified.  PROTO
 * is the first opcode for protocols >= 2.
 */
static int
load_proto(UnpicklerObject *self)
{
    int i;
    char *protobyte;

    i = self->read_func(self, &protobyte, 1);
    if (i < 0)
        return -1;

    i = calc_binint(protobyte, 1);
    /* No point checking for < 0, since calc_binint returns an unsigned
     * int when chewing on 1 byte.
     */
    assert(i >= 0);
    if (i <= HIGHEST_PROTOCOL)
        return 0;

    PyErr_Format(PyExc_ValueError, "unsupported pickle protocol: %d", i);
    return -1;
}

static PyObject *
load(UnpicklerObject *self)
{
    PyObject *err = 0, *val = 0;
    char *s;

    self->num_marks = 0;
    if (self->stack->length)
        Pdata_clear(self->stack, 0);

    while (1) {
        if (self->read_func(self, &s, 1) < 0)
            break;

        switch (s[0]) {
        case NONE:
            if (load_none(self) < 0)
                break;
            continue;

        case BININT:
            if (load_binint(self) < 0)
                break;
            continue;

        case BININT1:
            if (load_binint1(self) < 0)
                break;
            continue;

        case BININT2:
            if (load_binint2(self) < 0)
                break;
            continue;

        case INT:
            if (load_int(self) < 0)
                break;
            continue;

        case LONG:
            if (load_long(self) < 0)
                break;
            continue;

        case LONG1:
            if (load_counted_long(self, 1) < 0)
                break;
            continue;

        case LONG4:
            if (load_counted_long(self, 4) < 0)
                break;
            continue;

        case FLOAT:
            if (load_float(self) < 0)
                break;
            continue;

        case BINFLOAT:
            if (load_binfloat(self) < 0)
                break;
            continue;

        case BINSTRING:
            if (load_binstring(self) < 0)
                break;
            continue;

        case SHORT_BINSTRING:
            if (load_short_binstring(self) < 0)
                break;
            continue;

        case STRING:
            if (load_string(self) < 0)
                break;
            continue;

#ifdef Py_USING_UNICODE
        case UNICODE:
            if (load_unicode(self) < 0)
                break;
            continue;

        case BINUNICODE:
            if (load_binunicode(self) < 0)
                break;
            continue;
#endif

        case EMPTY_TUPLE:
            if (load_counted_tuple(self, 0) < 0)
                break;
            continue;

        case TUPLE1:
            if (load_counted_tuple(self, 1) < 0)
                break;
            continue;

        case TUPLE2:
            if (load_counted_tuple(self, 2) < 0)
                break;
            continue;

        case TUPLE3:
            if (load_counted_tuple(self, 3) < 0)
                break;
            continue;

        case TUPLE:
            if (load_tuple(self) < 0)
                break;
            continue;

        case EMPTY_LIST:
            if (load_empty_list(self) < 0)
                break;
            continue;

        case LIST:
            if (load_list(self) < 0)
                break;
            continue;

        case EMPTY_DICT:
            if (load_empty_dict(self) < 0)
                break;
            continue;

        case DICT:
            if (load_dict(self) < 0)
                break;
            continue;

        case OBJ:
            if (load_obj(self) < 0)
                break;
            continue;

        case INST:
            if (load_inst(self) < 0)
                break;
            continue;

        case NEWOBJ:
            if (load_newobj(self) < 0)
                break;
            continue;

        case GLOBAL:
            if (load_global(self) < 0)
                break;
            continue;

        case APPEND:
            if (load_append(self) < 0)
                break;
            continue;

        case APPENDS:
            if (load_appends(self) < 0)
                break;
            continue;

        case BUILD:
            if (load_build(self) < 0)
                break;
            continue;

        case DUP:
            if (load_dup(self) < 0)
                break;
            continue;

        case BINGET:
            if (load_binget(self) < 0)
                break;
            continue;

        case LONG_BINGET:
            if (load_long_binget(self) < 0)
                break;
            continue;

        case GET:
            if (load_get(self) < 0)
                break;
            continue;

        case EXT1:
            if (load_extension(self, 1) < 0)
                break;
            continue;

        case EXT2:
            if (load_extension(self, 2) < 0)
                break;
            continue;

        case EXT4:
            if (load_extension(self, 4) < 0)
                break;
            continue;

        case MARK:
            if (load_mark(self) < 0)
                break;
            continue;

        case BINPUT:
            if (load_binput(self) < 0)
                break;
            continue;

        case LONG_BINPUT:
            if (load_long_binput(self) < 0)
                break;
            continue;

        case PUT:
            if (load_put(self) < 0)
                break;
            continue;

        case POP:
            if (load_pop(self) < 0)
                break;
            continue;

        case POP_MARK:
            if (load_pop_mark(self) < 0)
                break;
            continue;

        case SETITEM:
            if (load_setitem(self) < 0)
                break;
            continue;

        case SETITEMS:
            if (load_setitems(self) < 0)
                break;
            continue;

        case STOP:
            break;

        case PERSID:
            if (load_persid(self) < 0)
                break;
            continue;

        case BINPERSID:
            if (load_binpersid(self) < 0)
                break;
            continue;

        case REDUCE:
            if (load_reduce(self) < 0)
                break;
            continue;

        case PROTO:
            if (load_proto(self) < 0)
                break;
            continue;

        case NEWTRUE:
            if (load_bool(self, Py_True) < 0)
                break;
            continue;

        case NEWFALSE:
            if (load_bool(self, Py_False) < 0)
                break;
            continue;

        case '\0':
            /* end of file */
            PyErr_SetNone(PyExc_EOFError);
            break;

        default:
            pickle_ErrFormat(UnpicklingError,
                              "invalid load key, '%s'.", "c", s[0]);
            return NULL;
        }

        break;
    }

    if ((err = PyErr_Occurred())) {
        if (err == PyExc_EOFError) {
            PyErr_SetNone(PyExc_EOFError);
        }
        return NULL;
    }

    PDATA_POP(self->stack, val);
    return val;
}

static PyObject *
Unpickler_load(UnpicklerObject *self)
{
    return load(self);
}

static struct PyMethodDef Unpickler_methods[] = {
    {"load", (PyCFunction)Unpickler_load, METH_NOARGS,
     PyDoc_STR("load() -> None. Load a pickle")},
    {NULL, NULL}                /* sentinel */
};

static PyObject *
Unpickler_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    UnpicklerObject *self;

    self = (UnpicklerObject *)type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;

    self->fp = NULL;
    self->arg = NULL;
    self->pers_func = NULL;
    self->last_string = NULL;
    self->marks = NULL;
    self->num_marks = 0;
    self->marks_size = 0;
    self->buf_size = 0;
    self->read = NULL;
    self->readline = NULL;
    self->find_class = NULL;

    self->memo = PyDict_New();
    if (self->memo == NULL) {
        Py_DECREF(self);
        return NULL;
    }

    self->stack = (Pdata *)Pdata_New();
    if (self->stack == NULL) {
        Py_DECREF(self);
        return NULL;
    }

    return (PyObject *)self;
}

static int
Unpickler_init(UnpicklerObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"file", 0};
    PyObject *file;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O:Unpickler",
                                     kwlist, &file))
        return -1;

    Py_INCREF(file);
    self->file = file;

    self->readline = PyObject_GetAttrString(file, "readline");
    self->read = PyObject_GetAttrString(file, "read");
    if (self->readline == NULL || self->read == NULL) {
        PyErr_Clear();
        PyErr_SetString(PyExc_TypeError,
                        "argument must have 'read' and 'readline' attributes");
        return -1;
    }
    self->read_func = read_other;
    self->readline_func = readline_other;

    self->pers_func = NULL;
    if (PyObject_HasAttrString((PyObject *)self, "persistent_load")) {
        self->pers_func = PyObject_GetAttrString((PyObject *)self,
                                                 "persistent_load");
        if (self->pers_func == NULL)
            return -1;
    }

    return 0;
}

static void
Unpickler_dealloc(UnpicklerObject *self)
{
    PyObject_GC_UnTrack((PyObject *) self);
    Py_XDECREF(self->readline);
    Py_XDECREF(self->read);
    Py_XDECREF(self->file);
    Py_XDECREF(self->memo);
    Py_XDECREF(self->stack);
    Py_XDECREF(self->pers_func);
    Py_XDECREF(self->arg);
    Py_XDECREF(self->last_string);
    Py_XDECREF(self->find_class);

    if (self->marks) {
        free(self->marks);
    }

    if (self->buf_size) {
        free(self->buf);
    }

    Py_Type(self)->tp_free((PyObject *)self);
}

static int
Unpickler_traverse(UnpicklerObject *self, visitproc visit, void *arg)
{
    Py_VISIT(self->readline);
    Py_VISIT(self->read);
    Py_VISIT(self->file);
    Py_VISIT(self->memo);
    Py_VISIT(self->stack);
    Py_VISIT(self->pers_func);
    Py_VISIT(self->arg);
    Py_VISIT(self->last_string);
    Py_VISIT(self->find_class);
    return 0;
}

static int
Unpickler_clear(UnpicklerObject *self)
{
    Py_CLEAR(self->readline);
    Py_CLEAR(self->read);
    Py_CLEAR(self->file);
    Py_CLEAR(self->memo);
    Py_CLEAR(self->stack);
    Py_CLEAR(self->pers_func);
    Py_CLEAR(self->arg);
    Py_CLEAR(self->last_string);
    Py_CLEAR(self->find_class);
    return 0;
}

static PyObject *
Unpickler_getattr(UnpicklerObject *self, char *name)
{
    if (strcmp(name, "persistent_load") == 0) {
        if (!self->pers_func) {
            PyErr_SetString(PyExc_AttributeError, name);
            return NULL;
        }

        Py_INCREF(self->pers_func);
        return self->pers_func;
    }

    if (strcmp(name, "find_global") == 0) {
        if (!self->find_class) {
            PyErr_SetString(PyExc_AttributeError, name);
            return NULL;
        }

        Py_INCREF(self->find_class);
        return self->find_class;
    }

    if (strcmp(name, "memo") == 0) {
        if (!self->memo) {
            PyErr_SetString(PyExc_AttributeError, name);
            return NULL;
        }

        Py_INCREF(self->memo);
        return self->memo;
    }
    
    /* XXX Why this is there? */
    if (strcmp(name, "UnpicklingError") == 0) {
        Py_INCREF(UnpicklingError);
        return UnpicklingError;
    }

    /* XXX To remove. Using tp_methods instead. */
    return Py_FindMethod(Unpickler_methods, (PyObject *)self, name);
}

static int
Unpickler_setattr(UnpicklerObject *self, char *name, PyObject *value)
{

    if (strcmp(name, "persistent_load") == 0) {
        Py_XDECREF(self->pers_func);
        self->pers_func = value;
        Py_XINCREF(value);
        return 0;
    }

    if (strcmp(name, "find_global") == 0) {
        Py_XDECREF(self->find_class);
        self->find_class = value;
        Py_XINCREF(value);
        return 0;
    }

    if (!value) {
        PyErr_SetString(PyExc_TypeError,
                        "attribute deletion is not supported");
        return -1;
    }

    if (strcmp(name, "memo") == 0) {
        if (!PyDict_Check(value)) {
            PyErr_SetString(PyExc_TypeError, "memo must be a dictionary");
            return -1;
        }
        Py_XDECREF(self->memo);
        self->memo = value;
        Py_INCREF(value);
        return 0;
    }

    PyErr_SetString(PyExc_AttributeError, name);
    return -1;
}

PyDoc_STRVAR(Unpickler_doc,
"Unpickler(file) -> new unpickler object"
"\n"
"This takes a file-like object for reading a pickle data stream.\n"
"\n"
"The protocol version of the pickle is detected automatically, so no\n"
"proto argument is needed.\n"
"\n"
"The file-like object must have two methods, a read() method that\n"
"takes an integer argument, and a readline() method that requires no\n"
"arguments.  Both methods should return a string.  Thus file-like\n"
"object can be a file object opened for reading, a StringIO object,\n"
"or any other custom object that meets this interface.\n");

static PyTypeObject Unpickler_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_pickle.Unpickler",                /*tp_name*/
    sizeof(UnpicklerObject),            /*tp_basicsize*/
    0,                                  /*tp_itemsize*/
    (destructor)Unpickler_dealloc,      /*tp_dealloc*/
    0,                                  /*tp_print*/
    (getattrfunc)Unpickler_getattr,     /*tp_getattr*/
    (setattrfunc)Unpickler_setattr,	    /*tp_setattr*/
    0,                                  /*tp_compare*/
    0,                                  /*tp_repr*/
    0,                                  /*tp_as_number*/
    0,                                  /*tp_as_sequence*/
    0,                                  /*tp_as_mapping*/
    0,                                  /*tp_hash*/
    0,                                  /*tp_call*/
    0,                                  /*tp_str*/
    0,                                  /*tp_getattro*/
    0,                                  /*tp_setattro*/
    0,                                  /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    Unpickler_doc,                      /*tp_doc*/
    (traverseproc)Unpickler_traverse,   /*tp_traverse*/
    (inquiry)Unpickler_clear,           /*tp_clear*/
    0,                                  /*tp_richcompare*/
    0,                                  /*tp_weaklistoffset*/
    0,                                  /*tp_iter*/
    0,                                  /*tp_iternext*/
    Unpickler_methods,                  /*tp_methods*/
    0,                                  /*tp_members*/
    0,                                  /*tp_getset*/
    0,                                  /*tp_base*/
    0,                                  /*tp_dict*/
    0,                                  /*tp_descr_get*/
    0,                                  /*tp_descr_set*/
    0,                                  /*tp_dictoffset*/
    (initproc)Unpickler_init,           /*tp_init*/
    0,                                  /*tp_alloc*/
    Unpickler_new,                      /*tp_new*/
    0,                                  /*tp_free*/
    0,                                  /*tp_is_gc*/
};

static int
init_stuff(PyObject *module_dict)
{
    PyObject *copy_reg, *t, *r;

    INIT_STR(__class__);
    INIT_STR(__dict__);
    INIT_STR(__setstate__);
    INIT_STR(__name__);
    INIT_STR(__main__);
    INIT_STR(__reduce__);
    INIT_STR(__reduce_ex__);

    copy_reg = PyImport_ImportModule("copy_reg");
    if (!copy_reg)
        return -1;

    /* This is special because we want to use a different
     * one in restricted mode. */
    dispatch_table = PyObject_GetAttrString(copy_reg, "dispatch_table");
    if (!dispatch_table)
        return -1;

    extension_registry = PyObject_GetAttrString(copy_reg,
                                                "_extension_registry");
    if (!extension_registry)
        return -1;

    inverted_registry = PyObject_GetAttrString(copy_reg, "_inverted_registry");
    if (!inverted_registry)
        return -1;

    extension_cache = PyObject_GetAttrString(copy_reg, "_extension_cache");
    if (!extension_cache)
        return -1;

    Py_DECREF(copy_reg);

    if (!(empty_tuple = PyTuple_New(0)))
        return -1;

    two_tuple = PyTuple_New(2);
    if (two_tuple == NULL)
        return -1;
    /* We use this temp container with no regard to refcounts, or to
     * keeping containees alive.  Exempt from GC, because we don't
     * want anything looking at two_tuple() by magic.
     */
    PyObject_GC_UnTrack(two_tuple);

    if (!(t = PyDict_New()))
        return -1;
    if (!(r = PyRun_String(
              "def __str__(self):\n"
              "   return self.args and ('%s' % self.args[0]) or ''\n",
              Py_file_input, module_dict, t)))
        return -1;
    Py_DECREF(r);

    PickleError = PyErr_NewException("pickle.PickleError", NULL, t);
    if (!PickleError)
        return -1;

    Py_DECREF(t);

    PicklingError = PyErr_NewException("pickle.PicklingError",
                                       PickleError, NULL);
    if (!PicklingError)
        return -1;

    UnpicklingError = PyErr_NewException("pickle.UnpicklingError",
                                         PickleError, NULL);
    if (!UnpicklingError)
        return -1;

    if (PyDict_SetItemString(module_dict, "PickleError", PickleError) < 0)
        return -1;

    if (PyDict_SetItemString(module_dict, "PicklingError", PicklingError) < 0)
        return -1;

    if (PyDict_SetItemString(module_dict, "UnpicklingError",
                             UnpicklingError) < 0)
        return -1;

    return 0;
}

PyMODINIT_FUNC
init_pickle(void)
{
    PyObject *m, *d, *v;
    Py_ssize_t i;
    char *rev = "$Revision: 57481 $";
    PyObject *format_version;
    PyObject *compatible_formats;

    if (PyType_Ready(&Unpickler_Type) < 0)
        return;
    if (PyType_Ready(&Pickler_Type) < 0)
        return;
    if (PyType_Ready(&Pdata_Type) < 0)
        return;

    /* Create the module and add the functions */
    m = Py_InitModule3("_pickle", NULL, pickle_module_documentation);
    if (m == NULL)
        return;

    PyModule_AddObject(m, "Pickler", (PyObject *)&Pickler_Type);
    PyModule_AddObject(m, "Unpickler", (PyObject *)&Unpickler_Type);

    /* Add some symbolic constants to the module */
    d = PyModule_GetDict(m);
    v = PyString_FromString(rev);
    PyDict_SetItemString(d, "__version__", v);
    Py_XDECREF(v);

    if (init_stuff(d) < 0)
        return;

    i = PyModule_AddIntConstant(m, "HIGHEST_PROTOCOL", HIGHEST_PROTOCOL);
    if (i < 0)
        return;

    /* These are purely informational; no code uses them. */
    /* File format version we write. */
    format_version = PyString_FromString("2.0");
    /* Format versions we can read. */
    compatible_formats = Py_BuildValue("[sssss]",
                                       "1.0",   /* Original protocol 0 */
                                       "1.1",   /* Protocol 0 + INST */
                                       "1.2",   /* Original protocol 1 */
                                       "1.3",   /* Protocol 1 + BINFLOAT */
                                       "2.0");  /* Original protocol 2 */
    PyDict_SetItemString(d, "format_version", format_version);
    PyDict_SetItemString(d, "compatible_formats", compatible_formats);
    Py_XDECREF(format_version);
    Py_XDECREF(compatible_formats);
}
