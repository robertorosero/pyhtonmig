/* Bytebuf object interface */

#include "Python.h"

/* Note: the object's structure is private */

#ifndef Py_BYTEBUFOBJECT_H
#define Py_BYTEBUFOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif


PyAPI_DATA(PyTypeObject) PyBytebuf_Type;

#define PyBytebuf_Check(op) ((op)->ob_type == &PyBytebuf_Type)

#define Py_END_OF_BYTEBUF       (-1)

PyAPI_FUNC(PyObject *) PyBytebuf_New(Py_ssize_t size);

#ifdef __cplusplus
}
#endif
#endif /* !Py_BYTEBUFOBJECT_H */


/* Byte Buffer object implementation */

/*
 * bytebuf object structure declaration.
 */
typedef struct {
    PyObject_HEAD

    /* Base pointer location */
    void*      b_ptr;

    /* Total size in bytes of the area that we can access.  The allocated
       memory must be at least as large as this size. */
    Py_ssize_t b_size;

} PyBytebufObject;


/*
 * Given a bytebuf object, return the buffer memory (in 'ptr' and 'size') and
 * true if there was no error.
 */
static int
get_buf(PyBytebufObject *self, void **ptr, Py_ssize_t *size)
{
    assert(ptr != NULL);
    *ptr = self->b_ptr;
    *size = self->b_size;
    return 1;
}

/*
 * Create a new bytebuf where we allocate the memory ourselves.
 */
PyObject *
PyBytebuf_New(Py_ssize_t size)
{
    PyObject *o;
    PyBytebufObject * b;

    if (size < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "size must be zero or positive");
        return NULL;
    }

    /* FIXME: check for overflow in multiply */
    o = (PyObject *)PyObject_MALLOC(sizeof(*b) + size);
    if ( o == NULL )
        return PyErr_NoMemory();
    b = (PyBytebufObject *) PyObject_INIT(o, &PyBytebuf_Type);

    /* We setup the memory buffer to be right after the object itself. */
    b->b_ptr = (void *)(b + 1);
    b->b_size = size;

    return o;
}

/* Methods */

/*
 * Constructor.
 */
static PyObject *
bytebuf_new(PyTypeObject *type, PyObject *args, PyObject *kw)
{
    Py_ssize_t size = -1;

    if (!_PyArg_NoKeywords("bytebuf()", kw))
        return NULL;
 
    if (!PyArg_ParseTuple(args, "n:bytebuf", &size))
        return NULL;

    if ( size <= 0 ) {
        PyErr_SetString(PyExc_TypeError,
                        "size must be greater than zero");
        return NULL;
    }

    return PyBytebuf_New(size);
}

/*
 * Destructor.
 */
static void
bytebuf_dealloc(PyBytebufObject *self)
{
    /* Note: by virtue of the memory buffer being allocated with the PyObject
       itself, this frees the buffer as well. */
    PyObject_DEL(self);
}

/* 
 * Comparison.
 */
static int
bytebuf_compare(PyBytebufObject *self, PyBytebufObject *other)
{
    void *p1, *p2;
    Py_ssize_t len_self, len_other, min_len;
    int cmp;

    if (!get_buf(self, &p1, &len_self))
        return -1;
    if (!get_buf(other, &p2, &len_other))
        return -1;
    min_len = (len_self < len_other) ? len_self : len_other;
    if (min_len > 0) {
        cmp = memcmp(p1, p2, min_len);
        if (cmp != 0)
            return cmp;
    }
    return (len_self < len_other) ? -1 : (len_self > len_other) ? 1 : 0;
}


/* 
 * Conversion to 'repr' string.
 */
static PyObject *
bytebuf_repr(PyBytebufObject *self)
{
    return PyString_FromFormat("<bytebuf ptr %p, size %zd at %p>",
                               self->b_ptr,
                               self->b_size,
                               self);
}

/* 
 * Conversion to string.
 */
static PyObject *
bytebuf_str(PyBytebufObject *self)
{
    void *ptr;
    Py_ssize_t size;
    if (!get_buf(self, &ptr, &size))
        return NULL;
    return PyString_FromStringAndSize((const char *)ptr, size);
}


/* Bytebuf methods */

/*
 * Returns the buffer for reading or writing.
 */
static Py_ssize_t
bytebuf_getwritebuf(PyBytebufObject *self, Py_ssize_t idx, void **pp)
{
    Py_ssize_t size;
    if ( idx != 0 ) {
        PyErr_SetString(PyExc_SystemError,
                        "accessing non-existent bytebuf segment");
        return -1;
    }
    if (!get_buf(self, pp, &size))
        return -1;
    return size;
}

static Py_ssize_t
bytebuf_getsegcount(PyBytebufObject *self, Py_ssize_t *lenp)
{
    void *ptr;
    Py_ssize_t size;
    if (!get_buf(self, &ptr, &size))
        return -1;
    if (lenp)
        *lenp = size;
    return 1;
}

static Py_ssize_t
bytebuf_getcharbuf(PyBytebufObject *self, Py_ssize_t idx, const char **pp)
{
    void *ptr;
    Py_ssize_t size;
    if ( idx != 0 ) {
        PyErr_SetString(PyExc_SystemError,
                        "accessing non-existent bytebuf segment");
        return -1;
    }
    if (!get_buf(self, &ptr, &size))
        return -1;
    *pp = (const char *)ptr;
    return size;
}


PyDoc_STRVAR(module_doc,
             "This module defines an object type which can represent a fixed size\n\
buffer of bytes in momery, from which you can directly read and into\n\
which you can directly write objects in various other types.  This is\n\
used to avoid buffer copies in network I/O as much as possible.  For\n\
example, socket recv() can directly fill a byte buffer's memory and\n\
send() can read the data to be sent from one as well.\n\
\n\
In addition, a byte buffer has two pointers within it, that delimit\n\
an active slice, the current \"position\" and the \"limit\".  The\n\
active region of a byte buffer is located within these boundaries.\n\
\n\
This class is heaviliy inspired from Java's NIO ByteBuffer class.\n\
\n\
The constructor is:\n\
\n\
bytebuf(nbytes) -- create a new bytebuf\n\
");


/* FIXME: needs an update */

/* PyDoc_STRVAR(bytebuf_doc, */
/*              "bytebuf(object [, offset[, size]])\n\ */
/* \n\ */
/* Create a new bytebuf object which references the given object.\n\ */
/* The bytebuf will reference a slice of the target object from the\n\ */
/* start of the object (or at the specified offset). The slice will\n\ */
/* extend to the end of the target object (or with the specified size)."); */

PyDoc_STRVAR(bytebuftype_doc,
             "bytebuf(size) -> bytebuf\n\
\n\
Return a new bytebuf with a new buffer of fixed size 'size'.\n\
\n\
Methods:\n\
\n\
Attributes:\n\
\n\
");


static PyBufferProcs bytebuf_as_buffer = {
    (readbufferproc)bytebuf_getwritebuf,
    (writebufferproc)bytebuf_getwritebuf,
    (segcountproc)bytebuf_getsegcount,
    (charbufferproc)bytebuf_getcharbuf,
};

PyTypeObject PyBytebuf_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "bytebuf",
    sizeof(PyBytebufObject),
    0,
    (destructor)bytebuf_dealloc,                /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    (cmpfunc)bytebuf_compare,           /* tp_compare */
    (reprfunc)bytebuf_repr,                     /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    (reprfunc)bytebuf_str,                      /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    &bytebuf_as_buffer,                 /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    bytebuftype_doc,                    /* tp_doc */
    0,                                  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    0,                                  /* tp_iter */
    0,                                  /* tp_iternext */
    0,                                  /* tp_methods */        
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    bytebuf_new,                                /* tp_new */
};


/*********************** Install Module **************************/

/* No functions in array module. */
static PyMethodDef a_methods[] = {
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


PyMODINIT_FUNC
initbytebuf(void)
{
    PyObject *m;

    PyBytebuf_Type.ob_type = &PyType_Type;
    m = Py_InitModule3("bytebuf", a_methods, module_doc);
    if (m == NULL)
        return;

    Py_INCREF((PyObject *)&PyBytebuf_Type);
    PyModule_AddObject(m, "BytebufType", (PyObject *)&PyBytebuf_Type);
    Py_INCREF((PyObject *)&PyBytebuf_Type);
    PyModule_AddObject(m, "bytebuf", (PyObject *)&PyBytebuf_Type);
    /* No need to check the error here, the caller will do that */
}



/* 
   TODO
   ----
   - Update doc.
   - Add hash function
   - Add support for sequence methods.


   Pending Issues
   --------------
   - Should we support weakrefs?

*/

