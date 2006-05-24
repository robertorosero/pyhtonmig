/* ===========================================================================
 * Hotbuf object
 */

#include "Python.h"

/* Note: the object's structure is private */

#ifndef Py_HOTBUFOBJECT_H
#define Py_HOTBUFOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif


PyAPI_DATA(PyTypeObject) PyHotbuf_Type;

#define PyHotbuf_Check(op) ((op)->ob_type == &PyHotbuf_Type)

#define Py_END_OF_HOTBUF       (-1)

PyAPI_FUNC(PyObject *) PyHotbuf_New(Py_ssize_t size);

#ifdef __cplusplus
}
#endif
#endif /* !Py_HOTBUFOBJECT_H */


/* ===========================================================================
 * Byte Buffer object implementation 
 */


/*
 * hotbuf object structure declaration.
 */
typedef struct {
    PyObject_HEAD

    /* Base pointer location */
    void*      b_ptr;

    /* Total size in bytes of the area that we can access.  The allocated
       memory must be at least as large as this size. */
    Py_ssize_t b_size;

} PyHotbufObject;


/*
 * Given a hotbuf object, return the buffer memory (in 'ptr' and 'size') and
 * true if there was no error.
 */


/* FIXME remove get_buf() everywhere, at least the checks for the return
   value. */

static int
get_buf(PyHotbufObject *self, void **ptr, Py_ssize_t *size)
{
    assert(ptr != NULL);
    *ptr = self->b_ptr;
    *size = self->b_size;
    return 1;
}

/*
 * Create a new hotbuf where we allocate the memory ourselves.
 */
PyObject *
PyHotbuf_New(Py_ssize_t size)
{
    PyObject *o;
    PyHotbufObject * b;

    if (size < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "size must be zero or positive");
        return NULL;
    }

    /* FIXME: check for overflow in multiply */
    o = (PyObject *)PyObject_MALLOC(sizeof(*b) + size);
    if ( o == NULL )
        return PyErr_NoMemory();
    b = (PyHotbufObject *) PyObject_INIT(o, &PyHotbuf_Type);

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
hotbuf_new(PyTypeObject *type, PyObject *args, PyObject *kw)
{
    Py_ssize_t size = -1;

    if (!_PyArg_NoKeywords("hotbuf()", kw))
        return NULL;
 
    if (!PyArg_ParseTuple(args, "n:hotbuf", &size))
        return NULL;

    if ( size <= 0 ) {
        PyErr_SetString(PyExc_TypeError,
                        "size must be greater than zero");
        return NULL;
    }

    return PyHotbuf_New(size);
}

/*
 * Destructor.
 */
static void
hotbuf_dealloc(PyHotbufObject *self)
{
    /* Note: by virtue of the memory buffer being allocated with the PyObject
       itself, this frees the buffer as well. */
    PyObject_DEL(self);
}

/* 
 * Comparison.
 */
static int
hotbuf_compare(PyHotbufObject *self, PyHotbufObject *other)
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
hotbuf_repr(PyHotbufObject *self)
{
    return PyString_FromFormat("<hotbuf ptr %p, size %zd at %p>",
                               self->b_ptr,
                               self->b_size,
                               self);
}

/* 
 * Conversion to string.
 */
static PyObject *
hotbuf_str(PyHotbufObject *self)
{
    void *ptr;
    Py_ssize_t size;
    if (!get_buf(self, &ptr, &size))
        return NULL;
    return PyString_FromStringAndSize((const char *)ptr, size);
}


/* Hotbuf methods */

/*
 * Returns the buffer for reading or writing.
 */
static Py_ssize_t
hotbuf_getwritebuf(PyHotbufObject *self, Py_ssize_t idx, void **pp)
{
    Py_ssize_t size;
    if ( idx != 0 ) {
        PyErr_SetString(PyExc_SystemError,
                        "accessing non-existent hotbuf segment");
        return -1;
    }
    if (!get_buf(self, pp, &size))
        return -1;
    return size;
}

static Py_ssize_t
hotbuf_getsegcount(PyHotbufObject *self, Py_ssize_t *lenp)
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
hotbuf_getcharbuf(PyHotbufObject *self, Py_ssize_t idx, const char **pp)
{
    void *ptr;
    Py_ssize_t size;
    if ( idx != 0 ) {
        PyErr_SetString(PyExc_SystemError,
                        "accessing non-existent hotbuf segment");
        return -1;
    }
    if (!get_buf(self, &ptr, &size))
        return -1;
    *pp = (const char *)ptr;
    return size;
}

/* ===========================================================================
 * Sequence methods 
 */

static Py_ssize_t
hotbuf_length(PyHotbufObject *self)
{
    void *ptr;
    Py_ssize_t size;
    if (!get_buf(self, &ptr, &size))
        return -1;
    return size;
}


/* ===========================================================================
 * Object interfaces declaration 
 */

/* FIXME: needs an update */

/* PyDoc_STRVAR(hotbuf_doc, */
/*              "hotbuf(object [, offset[, size]])\n\ */
/* \n\ */
/* Create a new hotbuf object which references the given object.\n\ */
/* The hotbuf will reference a slice of the target object from the\n\ */
/* start of the object (or at the specified offset). The slice will\n\ */
/* extend to the end of the target object (or with the specified size)."); */

PyDoc_STRVAR(hotbuftype_doc,
             "hotbuf(size) -> hotbuf\n\
\n\
Return a new hotbuf with a new buffer of fixed size 'size'.\n\
\n\
Methods:\n\
\n\
Attributes:\n\
\n\
");


static PySequenceMethods hotbuf_as_sequence = {
        (lenfunc)hotbuf_length,                 /*sq_length*/
        0 /* (binaryfunc)hotbuf_concat */,              /*sq_concat*/
        0 /* (ssizeargfunc)hotbuf_repeat */,            /*sq_repeat*/
        0 /* (ssizeargfunc)hotbuf_item */,              /*sq_item*/
        0 /*(ssizessizeargfunc)hotbuf_slice*/,        /*sq_slice*/
        0 /*(ssizeobjargproc)hotbuf_ass_item*/,       /*sq_ass_item*/
        0 /*(ssizessizeobjargproc)hotbuf_ass_slice*/, /*sq_ass_slice*/
};

static PyBufferProcs hotbuf_as_buffer = {
    (readbufferproc)hotbuf_getwritebuf,
    (writebufferproc)hotbuf_getwritebuf,
    (segcountproc)hotbuf_getsegcount,
    (charbufferproc)hotbuf_getcharbuf,
};

PyTypeObject PyHotbuf_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "hotbuf",
    sizeof(PyHotbufObject),
    0,
    (destructor)hotbuf_dealloc,        /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    (cmpfunc)hotbuf_compare,           /* tp_compare */
    (reprfunc)hotbuf_repr,             /* tp_repr */
    0,                                  /* tp_as_number */
    &hotbuf_as_sequence,               /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    (reprfunc)hotbuf_str,              /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    &hotbuf_as_buffer,                 /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    hotbuftype_doc,                    /* tp_doc */
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
    hotbuf_new,                        /* tp_new */
};


/* ===========================================================================
 * Install Module
 */

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
This class is heaviliy inspired from Java's NIO Hotbuffer class.\n\
\n\
The constructor is:\n\
\n\
hotbuf(nbytes) -- create a new hotbuf\n\
");


/* No functions in array module. */
static PyMethodDef a_methods[] = {
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


PyMODINIT_FUNC
inithotbuf(void)
{
    PyObject *m;

    PyHotbuf_Type.ob_type = &PyType_Type;
    m = Py_InitModule3("hotbuf", a_methods, module_doc);
    if (m == NULL)
        return;

    Py_INCREF((PyObject *)&PyHotbuf_Type);
    PyModule_AddObject(m, "HotbufType", (PyObject *)&PyHotbuf_Type);
    Py_INCREF((PyObject *)&PyHotbuf_Type);
    PyModule_AddObject(m, "hotbuf", (PyObject *)&PyHotbuf_Type);
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

