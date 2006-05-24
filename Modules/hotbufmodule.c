/* ===========================================================================
 * Hotbuf object: an equivalent to Java's NIO ByteBuffer class for fast
 * network I/O.
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

   From the Java Buffer docs:

     A buffer is a linear, finite sequence of elements of a specific
     primitive type. Aside from its content, the essential properties of a
     buffer are its capacity, limit, and position:

       A buffer's capacity is the number of elements it contains. The
       capacity of a buffer is never negative and never changes.

       A buffer's limit is the index of the first element that should not
       be read or written. A buffer's limit is never negative and is never
       greater than its capacity.

       A buffer's position is the index of the next element to be read or
       written. A buffer's position is never negative and is never greater
       than its limit.

   The following invariant holds for the mark, position, limit, and
   capacity values:

      0 <= mark <= position <= limit <= capacity (length)

 */
typedef struct {
    PyObject_HEAD

    /* Base pointer location */
    void*      b_ptr;

    /* Total size in bytes of the area that we can access.  The allocated
       memory must be at least as large as this size. */
    Py_ssize_t b_capacity;

    /*
     * The "active window" is defined by the interval [position, limit[.
     */
      
    /* The current position in the buffer. */
    int b_position;

    /* The limit position in the buffer. */
    int b_limit;

    /* The mark. From the Java Buffer docs:

         A buffer's mark is the index to which its position will be reset when
         the reset method is invoked. The mark is not always defined, but when
         it is defined it is never negative and is never greater than the
         position. If the mark is defined then it is discarded when the
         position or the limit is adjusted to a value smaller than the mark. If
         the mark is not defined then invoking the reset method causes an
         InvalidMarkException to be thrown.

       The mark is set to -1 to indicate that the mark is unset.
    */
    int b_mark;

} PyHotbufObject;


/*
 * Given a hotbuf object, return the buffer memory (in 'ptr' and 'size') and
 * true if there was no error.
 */

/*
 * Create a new hotbuf where we allocate the memory ourselves.
 */
PyObject *
PyHotbuf_New(Py_ssize_t capacity)
{
    PyObject *o;
    PyHotbufObject * b;

    if (capacity < 0) {
        PyErr_SetString(PyExc_ValueError,
                        "capacity must be zero or positive");
        return NULL;
    }

    /* FIXME: check for overflow in multiply */
    o = (PyObject *)PyObject_MALLOC(sizeof(*b) + capacity);
    if ( o == NULL )
        return PyErr_NoMemory();
    b = (PyHotbufObject *) PyObject_INIT(o, &PyHotbuf_Type);

    /* We setup the memory buffer to be right after the object itself. */
    b->b_ptr = (void *)(b + 1);
    b->b_position = 0;
    b->b_mark = -1;
    b->b_limit = capacity;
    b->b_capacity = capacity;

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
        PyErr_SetString(PyExc_ValueError,
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
 * Comparison.  We compare the active windows, not the entire allocated buffer
 * memory.
 */
static int
hotbuf_compare(PyHotbufObject *self, PyHotbufObject *other)
{
    Py_ssize_t min_len;
    int cmp;
    
    min_len = ((self->b_capacity < other->b_capacity) ?
               self->b_capacity : other->b_capacity);
    if (min_len > 0) {
        cmp = memcmp(self->b_ptr, other->b_ptr, min_len);
        if (cmp != 0)
            return cmp;
    }
    return ((self->b_capacity < other->b_capacity) ?
            -1 : (self->b_capacity > other->b_capacity) ? 1 : 0);
}


/*
 * Conversion to 'repr' string.
 */
static PyObject *
hotbuf_repr(PyHotbufObject *self)
{
    return PyString_FromFormat("<hotbuf ptr %p, size %zd at %p>",
                               self->b_ptr,
                               self->b_capacity,
                               self);
}

/*
 * Conversion to string.  We convert only the active window.
 */
static PyObject *
hotbuf_str(PyHotbufObject *self)
{
    return PyString_FromStringAndSize((const char *)self->b_ptr, self->b_capacity);
}



/* ===========================================================================
 * Object Methods
 */

PyDoc_STRVAR(capacity__doc__,
"B.capacity() -> int\n\
\n\
Returns this buffer's capacity. \n\
(the entire size of the allocated buffer.)");

static PyObject*
hotbuf_capacity(PyHotbufObject *self)
{
    return PyInt_FromLong(self->b_capacity);
}


PyDoc_STRVAR(position__doc__,
"B.position() -> int\n\
\n\
Returns this buffer's position.");

static PyObject*
hotbuf_position(PyHotbufObject *self)
{
    return PyInt_FromLong(self->b_position);
}


PyDoc_STRVAR(setposition__doc__,
"B.setposition(int)\n\
\n\
Sets this buffer's position. If the mark is defined and larger than\n\
the new position then it is discarded.  If the given position is\n\
larger than the limit an exception is raised.");

static PyObject*
hotbuf_setposition(PyHotbufObject *self, PyObject* arg)
{
    int newposition;

    newposition = PyInt_AsLong(arg);
    if (newposition == -1 && PyErr_Occurred())
        return NULL;

    if ( newposition > self->b_capacity ) {
        PyErr_SetString(PyExc_IndexError,
                        "position must be smaller than capacity");
        return NULL;
    }

    /* Set the new position */
    self->b_position = newposition;

    /* Discard the mark if it is beyond the new position */
    if ( self->b_mark > self->b_position ) 
        self->b_mark = -1;

    return Py_None;
}


PyDoc_STRVAR(limit__doc__,
"B.limit() -> int\n\
\n\
Returns this buffer's limit.");

static PyObject*
hotbuf_limit(PyHotbufObject *self)
{
    return PyInt_FromLong(self->b_limit);
}


PyDoc_STRVAR(setlimit__doc__,
"B.setlimit(int)\n\
\n\
Sets this buffer's limit. If the position is larger than the new limit\n\
then it is set to the new limit. If the mark is defined and larger\n\
than the new limit then it is discarded.");

static PyObject*
hotbuf_setlimit(PyHotbufObject *self, PyObject* arg)
{
    int newlimit;

    newlimit = PyInt_AsLong(arg);
    if (newlimit == -1 && PyErr_Occurred())
        return NULL;

    if ( newlimit > self->b_capacity ) {
        PyErr_SetString(PyExc_IndexError,
                        "limit must be smaller than capacity");
        return NULL;
    }

    /* Set the new limit. */
    self->b_limit = newlimit;

    /* If the position is larger than the new limit, set it to the new
       limit. */
    if ( self->b_position > self->b_limit )
        self->b_position = newlimit;

    /* Discard the mark if it is beyond the new limit */
    if ( self->b_mark > self->b_position ) 
        self->b_mark = -1;

    return Py_None;
}


PyDoc_STRVAR(mark__doc__,
"B.mark() -> int\n\
\n\
Returns this buffer's mark. \n\
Return -1 if the mark is not set.");

static PyObject*
hotbuf_mark(PyHotbufObject *self)
{
    return PyInt_FromLong(self->b_mark);
}


PyDoc_STRVAR(setmark__doc__,
"B.setmark()\n\
\n\
Sets this buffer's mark at its position.");

static PyObject*
hotbuf_setmark(PyHotbufObject *self)
{
    self->b_mark = self->b_position;
    return Py_None;
}


PyDoc_STRVAR(reset__doc__,
"B.reset() -> int\n\
\n\
Resets this buffer's position to the previously-marked position.\n\
Invoking this method neither changes nor discards the mark's value.\n\
An IndexError is raised if the mark has not been set.\n\
This method returns the new position's value.");

static PyObject*
hotbuf_reset(PyHotbufObject *self)
{
    if ( self->b_mark == -1 ) {
        PyErr_SetString(PyExc_IndexError,
                        "mark has not been yet set");
        return NULL;
    }

    self->b_position = self->b_mark;
    return PyInt_FromLong(self->b_position);
}


PyDoc_STRVAR(clear__doc__,
"B.clear()\n\
\n\
Clears this buffer. The position is set to zero, the limit is set to\n\
the capacity, and the mark is discarded.\n\
\n\
Invoke this method before using a sequence of channel-read or put\n\
operations to fill this buffer. For example:\n\
\n\
     buf.clear()     # Prepare buffer for reading\n\
     in.read(buf)    # Read data\n\
\n\
(This method does not actually erase the data in the buffer, but it is\n\
named as if it did because it will most often be used in situations in\n\
which that might as well be the case.)");

static PyObject*
hotbuf_clear(PyHotbufObject *self)
{
    self->b_position = 0;
    self->b_limit = self->b_capacity;
    self->b_mark = -1;
    return Py_None;
}


PyDoc_STRVAR(flip__doc__,
"B.flip()\n\
\n\
Flips this buffer. The limit is set to the current position and then\n\
the position is set to zero. If the mark is defined then it is\n\
discarded.\n\
\n\
After a sequence of channel-read or put operations, invoke this method\n\
to prepare for a sequence of channel-write or relative get\n\
operations. For example:\n\
\n\
     buf.put(magic)    # Prepend header\n\
     in.read(buf)      # Read data into rest of buffer\n\
     buf.flip()        # Flip buffer\n\
     out.write(buf)    # Write header + data to channel\n\
\n\
This method is often used in conjunction with the compact method when\n\
transferring data from one place to another.");

static PyObject*
hotbuf_flip(PyHotbufObject *self)
{
    self->b_limit = self->b_position;
    self->b_position = 0;
    self->b_mark = -1;
    return Py_None;
}


PyDoc_STRVAR(rewind__doc__,
"B.rewind()\n\
\n\
Rewinds this buffer. The position is set to zero and the mark is\n\
discarded.\n\
\n\
Invoke this method before a sequence of channel-write or get\n\
operations, assuming that the limit has already been set\n\
appropriately. For example:\n\
\n\
     out.write(buf)    # Write remaining data\n\
     buf.rewind()      # Rewind buffer\n\
     buf.get(array)    # Copy data into array\n\
");

static PyObject*
hotbuf_rewind(PyHotbufObject *self)
{
    self->b_position = 0;
    self->b_mark = -1;
    return Py_None;
}


PyDoc_STRVAR(remaining__doc__,
"B.remaining() -> int\n\
\n\
Returns the number of bytes between the current position and the limit.");

static PyObject*
hotbuf_remaining(PyHotbufObject *self)
{
    return PyInt_FromLong(self->b_limit - self->b_position);
}



/* ===========================================================================
 * Buffer protocol methods 
 */

/*
 * Returns the buffer for reading or writing.
 */
static Py_ssize_t
hotbuf_getwritebuf(PyHotbufObject *self, Py_ssize_t idx, void **pp)
{
    if ( idx != 0 ) {
        PyErr_SetString(PyExc_SystemError,
                        "accessing non-existent hotbuf segment");
        return -1;
    }

    *pp = self->b_ptr;
    return self->b_capacity;
}

static Py_ssize_t
hotbuf_getsegcount(PyHotbufObject *self, Py_ssize_t *lenp)
{
    if (lenp)
        *lenp = self->b_capacity;
    return 1;
}

static Py_ssize_t
hotbuf_getcharbuf(PyHotbufObject *self, Py_ssize_t idx, const char **pp)
{
    if ( idx != 0 ) {
        PyErr_SetString(PyExc_SystemError,
                        "accessing non-existent hotbuf segment");
        return -1;
    }

    *pp = (const char *)self->b_ptr;
    return self->b_capacity;
}



/* ===========================================================================
 * Sequence methods
 */

static Py_ssize_t
hotbuf_length(PyHotbufObject *self)
{
    assert(self->b_position <= self->b_limit);
    return self->b_limit - self->b_position;
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


static PyMethodDef
hotbuf_methods[] = {
	{"clear", (PyCFunction)hotbuf_clear, METH_NOARGS, clear__doc__},
	{"capacity", (PyCFunction)hotbuf_capacity, METH_NOARGS, capacity__doc__},
	{"position", (PyCFunction)hotbuf_position, METH_NOARGS, position__doc__},
	{"setposition", (PyCFunction)hotbuf_setposition, METH_O, setposition__doc__},
	{"limit", (PyCFunction)hotbuf_limit, METH_NOARGS, limit__doc__},
	{"setlimit", (PyCFunction)hotbuf_setlimit, METH_O, setlimit__doc__},
	{"mark", (PyCFunction)hotbuf_mark, METH_NOARGS, mark__doc__},
	{"setmark", (PyCFunction)hotbuf_setmark, METH_NOARGS, setmark__doc__},
	{"reset", (PyCFunction)hotbuf_reset, METH_NOARGS, reset__doc__},
	{"flip", (PyCFunction)hotbuf_flip, METH_NOARGS, flip__doc__},
	{"rewind", (PyCFunction)hotbuf_rewind, METH_NOARGS, rewind__doc__},
	{"remaining", (PyCFunction)hotbuf_remaining, METH_NOARGS, remaining__doc__},
	{NULL, NULL} /* sentinel */
};

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
    (destructor)hotbuf_dealloc,         /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    (cmpfunc)hotbuf_compare,            /* tp_compare */
    (reprfunc)hotbuf_repr,              /* tp_repr */
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
    hotbuf_methods,                     /* tp_methods */
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
   - Perhaps implement returning the buffer object itself from some of
     the methods in order to allow chaining of operations on a single line.

   Pending Issues
   --------------
   - Should we support weakrefs?

*/

