#include "Python.h"

PyDoc_STRVAR(module_doc,
"A fast implementation of StringIO.");

typedef struct {
    PyObject_HEAD
    Py_UNICODE *buf;
    Py_ssize_t pos;
    Py_ssize_t string_size;
    Py_ssize_t buf_size;
} StringIOObject;


/* Internal routine to get a line from the buffer of a StringIO
   object. Returns the length between the current position to the
   next newline character. */
static Py_ssize_t
get_line(StringIOObject *self, Py_UNICODE **output)
{
    Py_UNICODE *n;
    const Py_UNICODE *str_end;
    Py_ssize_t len;

    assert(self->buf != NULL);
    str_end = self->buf + self->string_size;

    /* Move to the end of the line, up to the end of the string, s. */
    for (n = self->buf + self->pos;
         n < str_end && *n != '\n';
         n++);

    /* Skip the newline character */
    if (n < str_end)
        n++;

    len = n - (self->buf + self->pos);
    *output = self->buf + self->pos;

    assert(self->pos + len < PY_SSIZE_T_MAX);
    assert(len >= 0);
    self->pos += len;

    return len;
}

/* Internal routine for changing the size of the buffer of StringIO
   objects. Returns 0 on success, -1 otherwise. */
static int
resize_buffer(StringIOObject *self, Py_ssize_t size)
{
    Py_ssize_t alloc = self->buf_size;

    if (size < alloc / 2) {
        /* Major downsize; resize down to exact size */
        alloc = size + 1;
    }
    else if (size < alloc) {
        /* Within allocated size; quick exit */
        return 0;
    }
    else if (size <= alloc * 1.125) {
        /* Moderate upsize; overallocate similar to list_resize() */
        alloc = size + (size >> 3) + (size < 9 ? 3 : 6);
    }
    else {
        /* Major upsize; resize up to exact size */
        alloc = size + 1;
    }

    PyMem_Resize(self->buf, Py_UNICODE, alloc);
    if (self->buf == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    self->buf_size = alloc;
    return 0;
}

/* Internal routine for writing a string of bytes to the buffer of a StringIO
   object. Returns the number of bytes wrote, or -1 on error. */
static Py_ssize_t
write_str(StringIOObject *self, const Py_UNICODE *ustr, Py_ssize_t len)
{
    assert(self->buf != NULL);

    if (resize_buffer(self, self->pos + len) < 0)
        return -1;  /* out of memory */

    /* Copy the data to the internal buffer, overwriting some of the
       existing data if self->pos < self->string_size. */
    memcpy(self->buf + self->pos, ustr, len * sizeof(Py_UNICODE));

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
stringio_get_closed(StringIOObject *self)
{
    /* close() does nothing, so the object can't be closed */
    Py_RETURN_FALSE;
}

/* Generic getter for the writable, readable and seekable properties */
static PyObject *
generic_true(StringIOObject *self)
{
    Py_RETURN_TRUE;
}

static PyObject *
stringio_flush(StringIOObject *self)
{
    Py_RETURN_NONE;
}

static PyObject *
stringio_getvalue(StringIOObject *self)
{
    return PyUnicode_FromUnicode(self->buf, self->string_size);
}

static PyObject *
stringio_isatty(StringIOObject *self)
{
    Py_RETURN_FALSE;
}

static PyObject *
stringio_tell(StringIOObject *self)
{
    return PyInt_FromSsize_t(self->pos);
}

static PyObject *
stringio_read(StringIOObject *self, PyObject *args)
{
    Py_ssize_t size, n;
    Py_UNICODE *output;
    PyObject *arg = Py_None;

    if (!PyArg_UnpackTuple(args, "read", 0, 1, &arg))
        return NULL;

    if (PyInt_Check(arg)) {
        size = PyInt_AsSsize_t(arg);
    }
    else if (arg == Py_None) {
        /* Read until EOF is reached, by default. */
        size = -1;
    }
    else {
        PyErr_Format(PyExc_TypeError, "integer argument expected, got %s",
                     Py_Type(arg)->tp_name);
        return NULL;
    }

    /* adjust invalid sizes */
    n = self->string_size - self->pos;
    if (size < 0 || size > n) {
        size = n;
        if (size < 0)
            size = 0;
    }

    assert(self->buf != NULL);
    output = self->buf + self->pos;
    self->pos += size;

    return PyUnicode_FromUnicode(output, size);
}

static PyObject *
stringio_readline(StringIOObject *self, PyObject *args)
{
    Py_ssize_t size, n;
    Py_UNICODE *output;
    PyObject *arg = Py_None;

    if (!PyArg_UnpackTuple(args, "readline", 0, 1, &arg))
        return NULL;

    if (PyInt_Check(arg)) {
        size = PyInt_AsSsize_t(arg);
    }
    else if (arg == Py_None) {
        /* No size limit, by default. */
        size = -1;
    }
    else {
        PyErr_Format(PyExc_TypeError, "integer argument expected, got %s",
                     Py_Type(arg)->tp_name);
        return NULL;
    }

    n = get_line(self, &output);

    if (size >= 0 && size < n) {
        size = n - size;
        n -= size;
        self->pos -= size;
    }

    return PyUnicode_FromUnicode(output, n);
}

static PyObject *
stringio_readlines(StringIOObject *self, PyObject *args)
{
    Py_ssize_t maxsize, size, n;
    PyObject *result, *line;
    Py_UNICODE *output;
    PyObject *arg = Py_None;

    if (!PyArg_UnpackTuple(args, "readlines", 0, 1, &arg))
        return NULL;

    if (PyInt_Check(arg)) {
        maxsize = PyInt_AsSsize_t(arg);
    }
    else if (arg == Py_None) {
        /* No size limit, by default. */
        maxsize = -1;
    }
    else {
        PyErr_Format(PyExc_TypeError, "integer argument expected, got %s",
                     Py_Type(arg)->tp_name);
        return NULL;
    }

    size = 0;
    result = PyList_New(0);
    if (!result)
        return NULL;

    while ((n = get_line(self, &output)) != 0) {
        line = PyUnicode_FromUnicode(output, n);
        if (!line)
            goto on_error;
        if (PyList_Append(result, line) == -1) {
            Py_DECREF(line);
            goto on_error;
        }
        Py_DECREF(line);
        size += n;
        if (maxsize > 0 && size >= maxsize)
            break;
    }
    return result;

  on_error:
    Py_DECREF(result);
    return NULL;
}

static PyObject *
stringio_truncate(StringIOObject *self, PyObject *args)
{
    Py_ssize_t size;
    PyObject *arg = Py_None;

    if (!PyArg_UnpackTuple(args, "truncate", 0, 1, &arg))
        return NULL;

    if (PyInt_Check(arg)) {
        size = PyInt_AsSsize_t(arg);
    }
    else if (arg == Py_None) {
        /* Truncate to current position if no argument is passed. */
        size = self->pos;
    }
    else {
        PyErr_Format(PyExc_TypeError, "integer argument expected, got %s",
                     Py_Type(arg)->tp_name);
        return NULL;
    }

    if (size < 0) {
        PyErr_Format(PyExc_ValueError,
                     "Negative size value %zd", size);
        return NULL;
    }

    if (size < self->string_size) {
        self->string_size = size;
        if (resize_buffer(self, size) < 0)
            return NULL;
    }
    self->pos = self->string_size;

    return PyInt_FromSsize_t(self->string_size);
}

static PyObject *
stringio_iternext(StringIOObject *self)
{
    Py_UNICODE *next;
    Py_ssize_t n;

    n = get_line(self, &next);

    if (!next || n == 0)
        return NULL;

    return PyUnicode_FromUnicode(next, n);
}

static PyObject *
stringio_seek(StringIOObject *self, PyObject *args)
{
    Py_ssize_t newpos, prevpos;
    int mode = 0;

    if (!PyArg_ParseTuple(args, "n|i:seek", &newpos, &mode))
        return NULL;

    if (newpos < 0 && mode == 0) {
        PyErr_Format(PyExc_ValueError,
                     "Negative seek position %zd", newpos);
        return NULL;
    }
    if (newpos != 0 && mode != 0) {
        PyErr_SetString(PyExc_IOError, 
                        "Can't do nonzero cur-relative seeks");
    }

    /* mode 0: offset relative to beginning of the string.
       mode 1: no change to current position.
       mode 2: change position to end of file. */
    if (mode == 1) {
        newpos = self->pos;
    }
    else if (mode == 2) {
        newpos = self->string_size;
    }
    else if (mode != 0) {
        PyErr_Format(PyExc_ValueError,
                     "Invalid whence (%i, should be 0, 1 or 2)", mode);
        return NULL;
    }

    if (newpos > self->string_size) {
        if (resize_buffer(self, newpos + 1) < 0)
            return NULL;  /* out of memory */
    }

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
stringio_write(StringIOObject *self, PyObject *obj)
{
    const Py_UNICODE *str;
    Py_ssize_t size, n = 0;
    PyObject *ustr = NULL;

    if (PyUnicode_Check(obj)) {
        str = PyUnicode_AsUnicode(obj);
        size = PyUnicode_GetSize(obj);
    }
    /* Temporary condition for str8 objects. */
    else if (PyString_Check(obj)) {
        ustr = PyObject_Unicode(obj);
        if (ustr == NULL)
            return NULL;
        str = PyUnicode_AsUnicode(ustr);
        size = PyUnicode_GetSize(ustr);
    }
    else {
        PyErr_Format(PyExc_TypeError, "string argument expected, got %s",
                     Py_Type(obj)->tp_name);
        return NULL;
    }

    if (size != 0)
        n = write_str(self, str, size);

    Py_XDECREF(ustr);
    if (n == -1)
        return NULL;

    return PyInt_FromSsize_t(n);
}

static PyObject *
stringio_writelines(StringIOObject *self, PyObject *v)
{
    PyObject *it, *item;
    PyObject *ret;

    it = PyObject_GetIter(v);
    if (it == NULL)
        return NULL;

    while ((item = PyIter_Next(it)) != NULL) {
        ret = stringio_write(self, item);
        if (ret == NULL)
            return NULL;
        Py_DECREF(ret);
        Py_DECREF(item);
    }
    Py_DECREF(it);

    /* See if PyIter_Next failed */
    if (PyErr_Occurred())
        return NULL;

    Py_RETURN_NONE;
}

static PyObject *
stringio_close(StringIOObject *self)
{
    Py_RETURN_NONE;
}

static void
StringIO_dealloc(StringIOObject *self)
{
    if (self->buf != NULL)
        PyMem_Del(self->buf);

    Py_Type(self)->tp_free(self);
}

static PyObject *
StringIO_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    StringIOObject *self;
    PyObject *initvalue = NULL, *ret;
    enum { INIT_BUFSIZE = 1 };

    assert(type != NULL && type->tp_alloc != NULL);

    if (!PyArg_ParseTuple(args, "|O:StringIO", &initvalue))
        return NULL;

    self = (StringIOObject *)type->tp_alloc(type, 0);

    if (self == NULL)
        return NULL;

    self->buf = PyMem_New(Py_UNICODE, INIT_BUFSIZE);
    if (self->buf == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    /* These variables need to be initialized before attempting to write
       anything to the object. */
    self->pos = 0;
    self->string_size = 0;
    self->buf_size = INIT_BUFSIZE;

    if (initvalue && initvalue != Py_None) {
        ret = stringio_write(self, initvalue);
        if (ret == NULL)
            return NULL;
        Py_DECREF(ret);
        self->pos = 0;
    }

    return (PyObject *)self;
}


PyDoc_STRVAR(StringIO_doc,
"StringIO([buffer]) -> Return a StringIO stream for reading and writing.");

PyDoc_STRVAR(StringIO_flush_doc,
"flush() -> None.  Does nothing.");

PyDoc_STRVAR(StringIO_getval_doc,
"getvalue() -> string.\n"
"\n"
"Retrieve the entire contents of the StringIO object. Raise an\n"
"exception if the object is closed.");

PyDoc_STRVAR(StringIO_isatty_doc,
"isatty() -> False.\n"
"\n"
"Always returns False since StringIO objects are not connected\n"
"to a tty-like device.");

PyDoc_STRVAR(StringIO_read_doc,
"read([size]) -> read at most size bytes, returned as a string.\n"
"\n"
"If the size argument is negative or omitted, read until EOF is reached.\n"
"Return an empty string at EOF.");

PyDoc_STRVAR(StringIO_readline_doc,
"readline([size]) -> next line from the file, as a string.\n"
"\n"
"Retain newline.  A non-negative size argument limits the maximum\n"
"number of characters to return (an incomplete line may be returned then).\n"
"Return an empty string at EOF.\n");

PyDoc_STRVAR(StringIO_readlines_doc,
"readlines([size]) -> list of strings, each a line from the file.\n"
"\n"
"Call readline() repeatedly and return a list of the lines so read.\n"
"The optional size argument, if given, is an approximate bound on the\n"
"total number of bytes in the lines returned.\n");

PyDoc_STRVAR(StringIO_tell_doc,
"tell() -> current file position, an integer\n");

PyDoc_STRVAR(StringIO_truncate_doc,
"truncate([size]) -> int.  Truncate the file to at most size bytes.\n"
"\n"
"Size defaults to the current file position, as returned by tell().\n"
"Returns the new size.  Imply an absolute seek to the position size.");

PyDoc_STRVAR(StringIO_close_doc,
"close() -> None.  Does nothing.");

PyDoc_STRVAR(StringIO_seek_doc,
"seek(pos, whence=0) -> int.  Change stream position.\n"
"\n"
"Seek to byte offset pos relative to position indicated by whence:\n"
"     0  Start of stream (the default).  pos should be >= 0;\n"
"     1  Current position - pos must be 0;\n"
"     2  End of stream - pos must be 0.\n"
"Returns the new absolute position.");

PyDoc_STRVAR(StringIO_write_doc,
"write(str) -> int.  Write string str to file.\n"
"\n"
"Return the number of characters written.");

PyDoc_STRVAR(StringIO_writelines_doc,
"writelines(sequence_of_strings) -> None.  Write the strings to the file.\n"
"\n"
"Note that newlines are not added.  The sequence can be any iterable object\n"
"producing strings. This is equivalent to calling write() for each string.");

PyDoc_STRVAR(generic_true_doc, "Always True.");


static PyGetSetDef StringIO_getsetlist[] = {
    {"closed", (getter)stringio_get_closed, NULL,
     "True if the file is closed"},
    {0},            /* sentinel */
};

static struct PyMethodDef StringIO_methods[] = {
    {"readable",   (PyCFunction)generic_true, METH_NOARGS,
     generic_true_doc},
    {"seekable",   (PyCFunction)generic_true, METH_NOARGS,
     generic_true_doc},
    {"writable",   (PyCFunction)generic_true, METH_NOARGS,
     generic_true_doc},
    {"flush",      (PyCFunction)stringio_flush, METH_NOARGS,
     StringIO_flush_doc},
    {"getvalue",   (PyCFunction)stringio_getvalue, METH_VARARGS,
     StringIO_getval_doc},
    {"isatty",     (PyCFunction)stringio_isatty, METH_NOARGS,
     StringIO_isatty_doc},
    {"read",       (PyCFunction)stringio_read, METH_VARARGS,
     StringIO_read_doc},
    {"readline",   (PyCFunction)stringio_readline, METH_VARARGS,
     StringIO_readline_doc},
    {"readlines",  (PyCFunction)stringio_readlines, METH_VARARGS,
     StringIO_readlines_doc},
    {"tell",       (PyCFunction)stringio_tell, METH_NOARGS,
     StringIO_tell_doc},
    {"truncate",   (PyCFunction)stringio_truncate, METH_VARARGS,
     StringIO_truncate_doc},
    {"close",      (PyCFunction)stringio_close, METH_NOARGS,
     StringIO_close_doc},
    {"seek",       (PyCFunction)stringio_seek, METH_VARARGS,
     StringIO_seek_doc},
    {"write",      (PyCFunction)stringio_write, METH_O,
     StringIO_write_doc},
    {"writelines", (PyCFunction)stringio_writelines, METH_O,
     StringIO_writelines_doc},
    {NULL, NULL}        /* sentinel */
};


static PyTypeObject StringIO_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_stringio.StringIO",                      /*tp_name*/
    sizeof(StringIOObject),                    /*tp_basicsize*/
    0,                                         /*tp_itemsize*/
    (destructor)StringIO_dealloc,              /*tp_dealloc*/
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
    StringIO_doc,                              /*tp_doc*/
    0,                                         /*tp_traverse*/
    0,                                         /*tp_clear*/
    0,                                         /*tp_richcompare*/
    0,                                         /*tp_weaklistoffset*/
    PyObject_SelfIter,                         /*tp_iter*/
    (iternextfunc)stringio_iternext,           /*tp_iternext*/
    StringIO_methods,                          /*tp_methods*/
    0,                                         /*tp_members*/
    StringIO_getsetlist,                       /*tp_getset*/
    0,                                         /*tp_base*/
    0,                                         /*tp_dict*/
    0,                                         /*tp_descr_get*/
    0,                                         /*tp_descr_set*/
    0,                                         /*tp_dictoffset*/
    0,                                         /*tp_init*/
    0,                                         /*tp_alloc*/
    StringIO_new,                              /*tp_new*/
};

PyMODINIT_FUNC
init_stringio(void)
{
    PyObject *m;

    if (PyType_Ready(&StringIO_Type) < 0)
        return;
    m = Py_InitModule3("_stringio", NULL, module_doc);
    if (m == NULL)
        return;
    Py_INCREF(&StringIO_Type);
    PyModule_AddObject(m, "StringIO", (PyObject *)&StringIO_Type);
}
