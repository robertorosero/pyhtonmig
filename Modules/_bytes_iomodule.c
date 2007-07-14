#include "Python.h"

PyDoc_STRVAR(module_doc,
"A fast implementation of BytesIO.");

typedef struct {
    PyObject_HEAD
    char *buf;
    Py_ssize_t pos;
    Py_ssize_t string_size;
    Py_ssize_t buf_size;
} BytesIOObject;


static PyObject *
err_closed(void)
{
    PyErr_SetString(PyExc_ValueError, "I/O operation on closed file");
    return NULL;
}

/* Internal routine to get a line from the buffer of a BytesIO
   object. Returns the number of bytes read. */
static Py_ssize_t
get_line(BytesIOObject *self, char **output)
{
    char *n;
    const char *str_end;
    Py_ssize_t len;

    /* XXX: Should we ckeck here if the object is closed,
       for thread-safety? */
    assert(self->buf != NULL);
    str_end = self->buf + self->string_size;

    /* Move to the end of the line, up to the end of the string, s. */
    for (n = self->buf + self->pos;
         n < str_end && *n != '\n';
         n++);

    /* Skip the newline character */
    if (n < str_end)
        n++;

    /* Get the length from the current position to the end of the line. */
    len = n - (self->buf + self->pos);
    *output = self->buf + self->pos;

    assert(self->pos + len < PY_SSIZE_T_MAX);
    assert(len >= 0);
    self->pos += len;

    return len;
}

/* Internal routine for changing the size of the buffer of BytesIO
   objects. Returns the new buffer size, or -1 on error. */
static Py_ssize_t
resize_buffer(BytesIOObject *self, Py_ssize_t new_size)
{
    /* Here we doing some direct memory manipulation for speed and to keep the
       implementation of this module relatively simple. */

    if (new_size >= self->buf_size) {
        /* Allocate memory to the nearest 16K chunk. You shouldn't see any
           significant performance gain (or lost) by changing this value. */
        self->buf_size = (new_size + 16383) & ~16383;

        PyMem_Resize(self->buf, char, self->buf_size);
        if (self->buf == NULL) {
            PyErr_SetString(PyExc_MemoryError, "Out of memory");
            PyMem_Del(self->buf);
            self->buf_size = self->pos = 0;
            return -1;
        }
    }
    return self->buf_size;
}

/* Internal routine for writing a string of bytes to the buffer of a BytesIO
   object. Returns the number of bytes wrote, or -1 on error. */
static Py_ssize_t
write_bytes(BytesIOObject *self, const char *bytes, Py_ssize_t len)
{
    assert(self->buf != NULL);

    if (resize_buffer(self, self->pos + len) < 0)
        return -1;  /* out of memory */

    /* Copy the data to the internal buffer, overwriting some of the existing
       data if self->pos < self->string_size. */
    memcpy(self->buf + self->pos, bytes, len);

    assert(self->pos + len < PY_SSIZE_T_MAX);
    self->pos += len;

    /* Unless we *only* overwritten some data, set the new length of the
       internal string. */
    if (self->string_size < self->pos) {
        self->string_size = self->pos;
    }

    return len;
}


static PyObject *
bytes_io_get_closed(BytesIOObject *self)
{
    PyObject *result = Py_False;

    if (self->buf == NULL)
        result = Py_True;

    Py_INCREF(result);
    return result;
}

/* Generic getter for the writable, readable and seekable properties */
static PyObject *
generic_true(BytesIOObject *self)
{
    Py_RETURN_TRUE;
}

static PyObject *
bytes_io_flush(BytesIOObject *self)
{
    if (self->buf == NULL)
        return err_closed();

    Py_RETURN_NONE;
}

static PyObject *
bytes_io_getvalue(BytesIOObject *self)
{
    if (self->buf == NULL)
        return err_closed();

    return PyString_FromStringAndSize(self->buf, self->string_size);
}

/* Not exposed as a method of BytesIO. */
static int
bytes_io_setvalue(BytesIOObject *self, PyObject *value)
{
    const char *bytes;
    Py_ssize_t len;

    if (self->buf == NULL) {
        err_closed();
        return -1;
    }

    self->pos = 0;
    self->string_size = 0;

    if (value == NULL)
        return 0;
    
    if (PyObject_AsCharBuffer(value, &bytes, &len) == -1)
        return -1;

    if (write_bytes(self, bytes, len) < 0) {
        return -1;  /* out of memory */
    }
    /* Reset the position back to beginning-of-file, since 
       write_bytes changed it. */
    self->pos = 0;

    return 0;
}

static PyObject *
bytes_io_isatty(BytesIOObject *self)
{
    if (self->buf == NULL)
        return err_closed();

    Py_RETURN_FALSE;
}

static PyObject *
bytes_io_tell(BytesIOObject *self)
{
    if (self->buf == NULL)
        return err_closed();

    return PyInt_FromSsize_t(self->pos);
}

static PyObject *
bytes_io_read(BytesIOObject *self, PyObject *args)
{
    Py_ssize_t len, n = -1;
    char *output;

    if (self->buf == NULL)
        return err_closed();

    if (!PyArg_ParseTuple(args, "|n:read", &n))
        return NULL;

    /* adjust invalid sizes */
    len = self->string_size - self->pos;
    if (n < 0 || n > len) {
        n = len;
        if (n < 0)
            n = 0;
    }

    output = self->buf + self->pos;
    self->pos += n;

    return PyString_FromStringAndSize(output, n);
}

static PyObject *
bytes_io_readline(BytesIOObject *self, PyObject *args)
{
    Py_ssize_t n, size = -1;
    char *output;

    if (self->buf == NULL)
        return err_closed();

    if (!PyArg_ParseTuple(args, "|i:readline", &size))
        return NULL;

    n = get_line(self, &output);

    if (size >= 0 && size < n) {
        size = n - size;
        n -= size;
        self->pos -= size;
    }

    return PyString_FromStringAndSize(output, n);
}

static PyObject *
bytes_io_readlines(BytesIOObject *self, PyObject *args)
{
    Py_ssize_t n, size = 0, len = 0;
    PyObject *result, *line;
    char *output;

    if (self->buf == NULL)
        return err_closed();

    if (!PyArg_ParseTuple(args, "|i:readlines", &size))
        return NULL;

    result = PyList_New(0);
    if (!result)
        return NULL;

    while ((n = get_line(self, &output)) != 0) {
        line = PyString_FromStringAndSize(output, n);
        if (!line)
            goto on_error;
        if (PyList_Append(result, line) == -1) {
            Py_DECREF(line);
            goto on_error;
        }
        Py_DECREF(line);
        len += n;
        if (size > 0 && len >= size)
            break;
    }
    return result;

  on_error:
    Py_DECREF(result);
    return NULL;
}

static PyObject *
bytes_io_readinto(BytesIOObject *self, PyObject *buffer)
{
    void *raw_buffer;
    Py_ssize_t len;

    if (self->buf == NULL)
        return err_closed();

    if (PyObject_AsWriteBuffer(buffer, &raw_buffer, &len) == -1)
        return NULL;

    if (self->pos + len > self->string_size)
        len = self->string_size - self->pos;

    memcpy(raw_buffer, self->buf + self->pos, len);
    assert(self->pos + len < PY_SSIZE_T_MAX);
    assert(len >= 0);
    self->pos += len;

    return PyInt_FromSsize_t(len);
}

static PyObject *
bytes_io_truncate(BytesIOObject *self, PyObject *args)
{
    Py_ssize_t size;

    /* Truncate to current position if no argument is passed. */
    size = self->pos;

    if (self->buf == NULL)
        return err_closed();

    if (!PyArg_ParseTuple(args, "|n:truncate", &size))
        return NULL;

    if (size < 0) {
        errno = EINVAL;
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }

    if (self->string_size > size)
        self->string_size = size;
    self->pos = self->string_size;

    return PyInt_FromSsize_t(self->string_size);
}

static PyObject *
bytes_io_iternext(BytesIOObject *self)
{
    char *next;
    Py_ssize_t n;

    if (self->buf == NULL)
        return err_closed();

    n = get_line(self, &next);

    if (!next || n == 0)
        return NULL;

    return PyString_FromStringAndSize(next, n);
}

static PyObject *
bytes_io_seek(BytesIOObject *self, PyObject *args)
{
    Py_ssize_t newpos, prevpos;
    int mode = 0;

    if (self->buf == NULL)
        return err_closed();

    if (!PyArg_ParseTuple(args, "n|i:seek", &newpos, &mode))
        return NULL;

    /* mode 0: offset relative to beginning of the string.
       mode 1: offset relative to current position.
       mode 2: offset relative the end of the string. */
    if (mode == 1) {
        newpos += self->pos;
    }
    else if (mode == 2) {
        newpos += self->string_size;
    }
    else if (mode != 0) {
        PyErr_SetString(PyExc_IOError, "invalid whence value");
        return NULL;
    }

    if (newpos < 0)
        newpos = 0;

    if (resize_buffer(self, newpos) < 0)
        return NULL;  /* out of memory */

    prevpos = self->pos;
    self->pos = newpos;

    /* Pad with zeros the buffer region larger than the string size and
       not previously padded with zeros. */
    while (newpos >= self->string_size && newpos >= prevpos) {
        self->buf[newpos] = 0;
        newpos--;
    }

    return PyInt_FromSsize_t(self->pos);
}

static PyObject *
bytes_io_write(BytesIOObject *self, PyObject *args)
{
    const char *bytes;
    Py_ssize_t len, n;

    if (self->buf == NULL)
        return err_closed();

    if (!PyArg_ParseTuple(args, "t#:write", &bytes, &n))
        return NULL;

    len = write_bytes(self, bytes, n);
    if (len == -1)
        return NULL;

    return PyInt_FromSsize_t(len);
}

static PyObject *
bytes_io_writelines(BytesIOObject *self, PyObject *v)
{
    PyObject *it, *item;
    const char *bytes;
    Py_ssize_t len;

    if (self->buf == NULL)
        return err_closed();

    it = PyObject_GetIter(v);
    if (it == NULL)
        return NULL;

    while ((item = PyIter_Next(it)) != NULL) {
        if (PyObject_AsCharBuffer(item, &bytes, &len) == -1) {
            Py_DECREF(it);
            Py_DECREF(item);
            return NULL;
        }
        Py_DECREF(item);

        if (write_bytes(self, bytes, len) == -1) {
            Py_DECREF(it);
            return NULL;
        }
    }
    Py_DECREF(it);

    /* See if PyIter_Next failed */
    if (PyErr_Occurred())
        return NULL;

    Py_RETURN_NONE;
}

static PyObject *
bytes_io_close(BytesIOObject *self)
{
    if (self->buf != NULL) {
        PyMem_Del(self->buf);
        self->buf = NULL;
    }

    self->pos = self->string_size = self->buf_size = 0;

    Py_RETURN_NONE;
}

static void
BytesIO_dealloc(BytesIOObject *op)
{
    if (op->buf != NULL)
        PyMem_Del(op->buf);

    op->ob_type->tp_free((PyObject *)op);
}

static PyObject *
BytesIO_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    BytesIOObject *self;
    const char *buf;
    Py_ssize_t size = 0;

    assert(type != NULL && type->tp_alloc != NULL);

    if (!PyArg_ParseTuple(args, "|t#:BytesIO", &buf, &size))
        return NULL;

    self = (BytesIOObject *)type->tp_alloc(type, 0);

    if (self == NULL)
        return NULL;

    self->buf = PyMem_New(char, size + 1);

    /* These variables need to be initialized before attempting to write
       anything to the object. */
    self->pos = 0;
    self->string_size = 0;
    self->buf_size = size + 1;

    if (size > 0) {
        if (write_bytes(self, buf, size) == -1) {
            Py_DECREF(self);            
            return NULL;
        }
        self->pos = 0;
    }
    if (self->buf == NULL) {
        Py_DECREF(self);
        return PyErr_NoMemory();
    }

    return (PyObject *)self;
}


PyDoc_STRVAR(BytesIO_doc,
"BytesIO([buffer]) -> Return a BytesIO stream for reading and writing.");

PyDoc_STRVAR(BytesIO_flush_doc,
"flush() -> None.  Does nothing.");

PyDoc_STRVAR(BytesIO_getval_doc,
"getvalue() -> string.\n"
"\n"
"Retrieve the entire contents of the BytesIO object. Raise an\n"
"exception if the object is closed.");

PyDoc_STRVAR(BytesIO_isatty_doc,
"isatty() -> False.\n"
"\n"
"Always returns False since BytesIO objects are not connected\n"
"to a tty-like device.");

PyDoc_STRVAR(BytesIO_read_doc,
"read([size]) -> read at most size bytes, returned as a string.\n"
"\n"
"If the size argument is negative or omitted, read until EOF is reached.\n"
"Return an empty string at EOF.");

PyDoc_STRVAR(BytesIO_readline_doc,
"readline([size]) -> next line from the file, as a string.\n"
"\n"
"Retain newline.  A non-negative size argument limits the maximum\n"
"number of bytes to return (an incomplete line may be returned then).\n"
"Return an empty string at EOF.\n");

PyDoc_STRVAR(BytesIO_readlines_doc,
"readlines([size]) -> list of strings, each a line from the file.\n"
"\n"
"Call readline() repeatedly and return a list of the lines so read.\n"
"The optional size argument, if given, is an approximate bound on the\n"
"total number of bytes in the lines returned.\n");

PyDoc_STRVAR(BytesIO_readinto_doc,
"readinto(bytes) -> int.  Read up to len(b) bytes into b.\n"
"\n"
"Returns number of bytes read (0 for EOF), or None if the object\n"
"is set not to block as has no data to read.");

PyDoc_STRVAR(BytesIO_tell_doc,
"tell() -> current file position, an integer\n");

PyDoc_STRVAR(BytesIO_truncate_doc,
"truncate([size]) -> int.  Truncate the file to at most size bytes.\n"
"\n"
"Size defaults to the current file position, as returned by tell().\n"
"Returns the new size.");

PyDoc_STRVAR(BytesIO_close_doc,
"close() -> None.  Close the file and release the resources held.");

PyDoc_STRVAR(BytesIO_seek_doc,
"seek(pos, whence=0) -> int.  Change stream position.\n"
"\n"
"Seek to byte offset pos relative to position indicated by whence:\n"
"     0  Start of stream (the default).  pos should be >= 0;\n"
"     1  Current position - whence may be negative;\n"
"     2  End of stream - whence usually negative.\n"
"Returns the new absolute position.");

PyDoc_STRVAR(BytesIO_write_doc,
"write(str) -> int.  Write string str to file.\n"
"\n"
"Return the number of bytes written.");

PyDoc_STRVAR(BytesIO_writelines_doc,
"writelines(sequence_of_strings) -> None.  Write the strings to the file.\n"
"\n"
"Note that newlines are not added.  The sequence can be any iterable object\n"
"producing strings. This is equivalent to calling write() for each string.");

PyDoc_STRVAR(generic_true_doc, "Always True.");


static PyGetSetDef BytesIO_getsetlist[] = {
    {"closed",    (getter) bytes_io_get_closed, NULL,
     "True if the file is closed."},
    {"_buffer", (getter) bytes_io_getvalue, (setter) bytes_io_setvalue,
     NULL},
    {0},            /* sentinel */
};

static struct PyMethodDef BytesIO_methods[] = {
    {"readable",   (PyCFunction) generic_true, METH_NOARGS,
     generic_true_doc},
    {"seekable",   (PyCFunction) generic_true, METH_NOARGS,
     generic_true_doc},
    {"writable",   (PyCFunction) generic_true, METH_NOARGS, 
     generic_true_doc},
    {"flush",      (PyCFunction) bytes_io_flush, METH_NOARGS,
     BytesIO_flush_doc},
    {"getvalue",   (PyCFunction) bytes_io_getvalue, METH_VARARGS,
     BytesIO_getval_doc},
    {"isatty",     (PyCFunction) bytes_io_isatty, METH_NOARGS,
     BytesIO_isatty_doc},
    {"read",       (PyCFunction) bytes_io_read, METH_VARARGS,
     BytesIO_read_doc},
    {"readline",   (PyCFunction) bytes_io_readline, METH_VARARGS,
     BytesIO_readline_doc},
    {"readlines",  (PyCFunction) bytes_io_readlines, METH_VARARGS,
     BytesIO_readlines_doc},
    {"readinto",   (PyCFunction) bytes_io_readinto, METH_O,
     BytesIO_readinto_doc},
    {"tell",       (PyCFunction) bytes_io_tell, METH_NOARGS,
     BytesIO_tell_doc},
    {"truncate",   (PyCFunction) bytes_io_truncate, METH_VARARGS,
     BytesIO_truncate_doc},
    {"close",      (PyCFunction) bytes_io_close, METH_NOARGS,
     BytesIO_close_doc},
    {"seek",       (PyCFunction) bytes_io_seek, METH_VARARGS,
     BytesIO_seek_doc},
    {"write",      (PyCFunction) bytes_io_write, METH_VARARGS,
     BytesIO_write_doc},
    {"writelines", (PyCFunction) bytes_io_writelines, METH_O,
     BytesIO_writelines_doc},
    {NULL, NULL}        /* sentinel */
};


static PyTypeObject BytesIO_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                                         /*ob_size*/
    "_bytes_io.BytesIO",                       /*tp_name*/
    sizeof(BytesIOObject),                     /*tp_basicsize*/
    0,                                         /*tp_itemsize*/
    (destructor)BytesIO_dealloc,               /*tp_dealloc*/
    0,                                         /*tp_print*/
    0,                                         /*tp_getattr*/
    0,                                         /*tp_setattr*/
    0,                                         /*tp_compare*/
    0,                                         /*tp_repr*/
    0,                                         /*tp_as_number*/
    0,                                         /*tp_as_sequence*/
    0,                                         /*tp_as_mapping*/
    0,                                         /*tp_hash*/
    0,                                         /*tp_call*/
    0,                                         /*tp_str*/
    0,                                         /*tp_getattro*/
    0,                                         /*tp_setattro*/
    0,                                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /*tp_flags*/
    BytesIO_doc,                               /*tp_doc*/
    0,                                         /*tp_traverse*/
    0,                                         /*tp_clear*/
    0,                                         /*tp_richcompare*/
    0,                                         /*tp_weaklistoffset*/
    PyObject_SelfIter,                         /*tp_iter*/
    (iternextfunc)bytes_io_iternext,           /*tp_iternext*/
    BytesIO_methods,                           /*tp_methods*/
    0,                                         /*tp_members*/
    BytesIO_getsetlist,                        /*tp_getset*/
    0,                                         /*tp_base*/
    0,                                         /*tp_dict*/
    0,                                         /*tp_descr_get*/
    0,                                         /*tp_descr_set*/
    0,                                         /*tp_dictoffset*/
    0,                                         /*tp_init*/
    0,                                         /*tp_alloc*/
    BytesIO_new,                               /*tp_new*/
    0,                                         /*tp_free*/
    0,                                         /*tp_is_gc*/
};

PyMODINIT_FUNC
init_bytes_io(void)
{
    PyObject *m;

    if (PyType_Ready(&BytesIO_Type) < 0)
        return;
    m = Py_InitModule3("_bytes_io", NULL, module_doc);
    if (m == NULL)
        return;
    Py_INCREF(&BytesIO_Type);
    PyModule_AddObject(m, "BytesIO", (PyObject *)&BytesIO_Type);
}
