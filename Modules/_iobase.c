#define PY_SSIZE_T_CLEAN
#include "Python.h"
#include "structmember.h"
#include "_iomodule.h"

/*
 * IOBase class, an abstract class
 */

PyDoc_STRVAR(IOBase_doc,
    "The abstract base class for all I/O classes, acting on streams of\n"
    "bytes. There is no public constructor.\n"
    "\n"
    "This class provides dummy implementations for many methods that\n"
    "derived classes can override selectively; the default implementations\n"
    "represent a file that cannot be read, written or seeked.\n"
    "\n"
    "Even though IOBase does not declare read, readinto, or write because\n"
    "their signatures will vary, implementations and clients should\n"
    "consider those methods part of the interface. Also, implementations\n"
    "may raise a IOError when operations they do not support are called.\n"
    "\n"
    "The basic type used for binary data read from or written to a file is\n"
    "bytes. bytearrays are accepted too, and in some cases (such as\n"
    "readinto) needed. Text I/O classes work with str data.\n"
    "\n"
    "Note that calling any method (even inquiries) on a closed stream is\n"
    "undefined. Implementations may raise IOError in this case.\n"
    "\n"
    "IOBase (and its subclasses) support the iterator protocol, meaning\n"
    "that an IOBase object can be iterated over yielding the lines in a\n"
    "stream.\n"
    "\n"
    "IOBase also supports the :keyword:`with` statement. In this example,\n"
    "fp is closed after the suite of the with statment is complete:\n"
    "\n"
    "with open('spam.txt', 'r') as fp:\n"
    "    fp.write('Spam and eggs!')\n");

/* Use this macro whenever you want to check the internal `closed` status
   of the IOBase object rather than the virtual `closed` attribute as returned
   by whatever subclass. */

#define IS_CLOSED(self) \
    PyObject_HasAttrString(self, "__IOBase_closed")

/* Internal methods */
static PyObject *
IOBase_unsupported(const char *message)
{
    PyErr_SetString(PyIOExc_UnsupportedOperation, message);
    return NULL;
}

/* Positionning */

PyDoc_STRVAR(IOBase_seek_doc,
    "Change stream position.\n"
    "\n"
    "Change the stream position to byte offset offset. offset is\n"
    "interpreted relative to the position indicated by whence.  Values\n"
    "for whence are:\n"
    "\n"
    "* 0 -- start of stream (the default); offset should be zero or positive\n"
    "* 1 -- current stream position; offset may be negative\n"
    "* 2 -- end of stream; offset is usually negative\n"
    "\n"
    "Return the new absolute position.");

static PyObject *
IOBase_seek(PyObject *self, PyObject *args)
{
    return IOBase_unsupported("seek");
}

PyDoc_STRVAR(IOBase_tell_doc,
             "Return current stream position.");

static PyObject *
IOBase_tell(PyObject *self, PyObject *args)
{
    return PyObject_CallMethod(self, "seek", "ii", 0, 1);
}

PyDoc_STRVAR(IOBase_truncate_doc,
    "Truncate file to size bytes.\n"
    "\n"
    "Size defaults to the current IO position as reported by tell().  Return\n"
    "the new size.");

static PyObject *
IOBase_truncate(PyObject *self, PyObject *args)
{
    return IOBase_unsupported("seek");
}

/* Flush and close methods */

PyDoc_STRVAR(IOBase_flush_doc,
    "Flush write buffers, if applicable.\n"
    "\n"
    "This is not implemented for read-only and non-blocking streams.\n");

static PyObject *
IOBase_flush(PyObject *self, PyObject *args)
{
    /* XXX Should this return the number of bytes written??? */
    Py_RETURN_NONE;
}

PyDoc_STRVAR(IOBase_close_doc,
    "Flush and close the IO object.\n"
    "\n"
    "This method has no effect if the file is already closed.\n");

static int
IOBase_closed(PyObject *self)
{
    PyObject *res;
    int closed;
    /* This gets the derived attribute, which is *not* __IOBase_closed
       in most cases! */
    res = PyObject_GetAttr(self, _PyIO_str_closed);
    closed = PyObject_IsTrue(res);
    Py_DECREF(res);
    return closed;
}

static PyObject *
IOBase_closed_get(PyObject *self, void *context)
{
    return PyBool_FromLong(IS_CLOSED(self));
}


PyObject *
_PyIOBase_checkClosed(PyObject *self, PyObject *args)
{
    if (IOBase_closed(self)) {
        PyErr_SetString(PyExc_ValueError, "I/O operation on closed file.");
        return NULL;
    }
    if (args == Py_True)
        return Py_None;
    else
        Py_RETURN_NONE;
}

/* XXX: IOBase thinks it has to maintain its own internal state in
   `__IOBase_closed` and call flush() by itself, but it is redundant with
   whatever behaviour a non-trivial derived class will implement. */

static PyObject *
IOBase_close(PyObject *self, PyObject *args)
{
    PyObject *res;

    if (IS_CLOSED(self))
        Py_RETURN_NONE;

    res = PyObject_CallMethodObjArgs(self, _PyIO_str_flush, NULL);
    PyObject_SetAttrString(self, "__IOBase_closed", Py_True);
    if (res == NULL) {
        /* If flush() fails, just give up */
        if (PyErr_ExceptionMatches(PyExc_IOError))
            PyErr_Clear();
        else
            return NULL;
    }
    Py_XDECREF(res);
    Py_RETURN_NONE;
}

static PyObject *
IOBase_del(PyObject *self, PyObject *args)
{
    PyObject *res = NULL;
    res = PyObject_CallMethodObjArgs(self, _PyIO_str_close, NULL);
    if (res == NULL) {
        /* At program exit time, it's possible that globals have already been
         *  deleted, and then the close() call might fail.  Since there's
         *  nothing we can do about such failures and they annoy the end
         *  users, we suppress the traceback.
         *
         * XXX: this function can be called at other times and what if the
         * error is genuine?
         */
        PyErr_Clear();
    }
    Py_XDECREF(res);
    Py_RETURN_NONE;
}

/* Inquiry methods */

PyDoc_STRVAR(IOBase_seekable_doc,
    "Return whether object supports random access.\n"
    "\n"
    "If False, seek(), tell() and truncate() will raise IOError.\n"
    "This method may need to do a test seek().");

static PyObject *
IOBase_seekable(PyObject *self, PyObject *args)
{
    Py_RETURN_FALSE;
}

PyObject *
_PyIOBase_checkSeekable(PyObject *self, PyObject *args)
{
    PyObject *res  = PyObject_CallMethodObjArgs(self, _PyIO_str_seekable, NULL);
    if (res == NULL)
        return NULL;
    if (res != Py_True) {
        Py_CLEAR(res);
        PyErr_SetString(PyExc_IOError, "File or stream is not seekable.");
    }
    if (args == Py_True) {
        Py_DECREF(res);
    }
    return res;
}

PyDoc_STRVAR(IOBase_readable_doc,
    "Return whether object was opened for reading.\n"
    "\n"
    "If False, read() will raise IOError.");

static PyObject *
IOBase_readable(PyObject *self, PyObject *args)
{
    Py_RETURN_FALSE;
}

/* May be called with any object */
PyObject *
_PyIOBase_checkReadable(PyObject *self, PyObject *args)
{
    PyObject *res  = PyObject_CallMethodObjArgs(self, _PyIO_str_readable, NULL);
    if (res == NULL)
        return NULL;
    if (res != Py_True) {
        Py_CLEAR(res);
        PyErr_SetString(PyExc_IOError, "File or stream is not readable.");
    }
    if (args == Py_True) {
        Py_DECREF(res);
    }
    return res;
}

PyDoc_STRVAR(IOBase_writable_doc,
    "Return whether object was opened for writing.\n"
    "\n"
    "If False, read() will raise IOError.");

static PyObject *
IOBase_writable(PyObject *self, PyObject *args)
{
    Py_RETURN_FALSE;
}

/* May be called with any object */
PyObject *
_PyIOBase_checkWritable(PyObject *self, PyObject *args)
{
    PyObject *res  = PyObject_CallMethodObjArgs(self, _PyIO_str_writable, NULL);
    if (res == NULL)
        return NULL;
    if (res != Py_True) {
        Py_CLEAR(res);
        PyErr_SetString(PyExc_IOError, "File or stream is not writable.");
    }
    if (args == Py_True) {
        Py_DECREF(res);
    }
    return res;
}

/* Context manager */

static PyObject *
IOBase_enter(PyObject *self, PyObject *args)
{
    if (_PyIOBase_checkClosed(self, Py_True) == NULL)
        return NULL;

    Py_INCREF(self);
    return self;
}

static PyObject *
IOBase_exit(PyObject *self, PyObject *args)
{
    return PyObject_CallMethodObjArgs(self, _PyIO_str_close, NULL);
}

/* Lower-level APIs */

/* XXX Should these be present even if unimplemented? */

PyDoc_STRVAR(IOBase_fileno_doc,
    "Returns underlying file descriptor if one exists.\n"
    "\n"
    "An IOError is raised if the IO object does not use a file descriptor.\n");

static PyObject *
IOBase_fileno(PyObject *self, PyObject *args)
{
    return IOBase_unsupported("fileno");
}

PyDoc_STRVAR(IOBase_isatty_doc,
    "Return whether this is an 'interactive' stream.\n"
    "\n"
    "Return False if it can't be determined.\n");

static PyObject *
IOBase_isatty(PyObject *self, PyObject *args)
{
    if (_PyIOBase_checkClosed(self, Py_True) == NULL)
        return NULL;
    Py_RETURN_FALSE;
}

/* Readline(s) and writelines */

PyDoc_STRVAR(IOBase_readline_doc,
    "Read and return a line from the stream.\n"
    "\n"
    "If limit is specified, at most limit bytes will be read.\n"
    "\n"
    "The line terminator is always b'\n' for binary files; for text\n"
    "files, the newlines argument to open can be used to select the line\n"
    "terminator(s) recognized.\n");

static PyObject *
IOBase_readline(PyObject *self, PyObject *args)
{
    /* For backwards compatibility, a (slowish) readline(). */

    Py_ssize_t limit = -1;
    int has_peek = 0;
    PyObject *buffer, *result;
    Py_ssize_t old_size = -1;

    if (!PyArg_ParseTuple(args, "|n:readline", &limit)) {
        return NULL;
    }

    if (_PyIOBase_checkClosed(self, Py_True) == NULL)
        return NULL;

    if (PyObject_HasAttrString(self, "peek"))
        has_peek = 1;

    buffer = PyByteArray_FromStringAndSize(NULL, 0);
    if (buffer == NULL)
        return NULL;

    while (limit < 0 || Py_SIZE(buffer) < limit) {
        Py_ssize_t nreadahead = 1;
        PyObject *b;

        if (has_peek) {
            PyObject *readahead = PyObject_CallMethod(self, "peek", "i", 1);
            if (readahead == NULL)
                goto fail;
            if (!PyBytes_Check(readahead)) {
                PyErr_Format(PyExc_IOError,
                             "peek() should have returned a bytes object, "
                             "not '%.200s'", Py_TYPE(readahead)->tp_name);
                Py_DECREF(readahead);
                goto fail;
            }
            if (PyBytes_GET_SIZE(readahead) > 0) {
                Py_ssize_t n = 0;
                const char *buf = PyBytes_AS_STRING(readahead);
                if (limit >= 0) {
                    do {
                        if (n >= PyBytes_GET_SIZE(readahead) || n >= limit)
                            break;
                        if (buf[n++] == '\n')
                            break;
                    } while (1);
                }
                else {
                    do {
                        if (n >= PyBytes_GET_SIZE(readahead))
                            break;
                        if (buf[n++] == '\n')
                            break;
                    } while (1);
                }
                nreadahead = n;
            }
            Py_DECREF(readahead);
        }

        b = PyObject_CallMethod(self, "read", "n", nreadahead);
        if (b == NULL)
            goto fail;
        if (!PyBytes_Check(b)) {
            PyErr_Format(PyExc_IOError,
                         "read() should have returned a bytes object, "
                         "not '%.200s'", Py_TYPE(b)->tp_name);
            Py_DECREF(b);
            goto fail;
        }
        if (PyBytes_GET_SIZE(b) == 0) {
            Py_DECREF(b);
            break;
        }

        old_size = PyByteArray_GET_SIZE(buffer);
        PyByteArray_Resize(buffer, old_size + PyBytes_GET_SIZE(b));
        memcpy(PyByteArray_AS_STRING(buffer) + old_size,
               PyBytes_AS_STRING(b), PyBytes_GET_SIZE(b));

        Py_DECREF(b);

        if (PyByteArray_AS_STRING(buffer)[PyByteArray_GET_SIZE(buffer) - 1] == '\n')
            break;
    }

    result = PyBytes_FromStringAndSize(PyByteArray_AS_STRING(buffer),
                                       PyByteArray_GET_SIZE(buffer));
    Py_DECREF(buffer);
    return result;
  fail:
    Py_DECREF(buffer);
    return NULL;
}

static PyObject *
IOBase_iter(PyObject *self)
{
    if (_PyIOBase_checkClosed(self, Py_True) == NULL)
        return NULL;

    Py_INCREF(self);
    return self;
}

static PyObject *
IOBase_iternext(PyObject *self)
{
    PyObject *line = PyObject_CallMethodObjArgs(self, _PyIO_str_readline, NULL);

    if (line == NULL)
        return NULL;

    if (PyObject_Size(line) == 0) {
        Py_DECREF(line);
        return NULL;
    }

    return line;
}

PyDoc_STRVAR(IOBase_readlines_doc,
    "Return a list of lines from the stream.\n"
    "\n"
    "hint can be specified to control the number of lines read: no more\n"
    "lines will be read if the total size (in bytes/characters) of all\n"
    "lines so far exceeds hint.");

static PyObject *
IOBase_readlines(PyObject *self, PyObject *args)
{
    Py_ssize_t hint = -1, length = 0;
    PyObject *hintobj = Py_None, *result;

    if (!PyArg_ParseTuple(args, "|O:readlines", &hintobj)) {
        return NULL;
    }
    if (hintobj != Py_None) {
        hint = PyNumber_AsSsize_t(hintobj, PyExc_ValueError);
        if (hint == -1 && PyErr_Occurred())
            return NULL;
    }

    result = PyList_New(0);
    if (result == NULL)
        return NULL;

    if (hint <= 0) {
        PyObject *ret = PyObject_CallMethod(result, "extend", "O", self);
        if( ret == NULL) {
            Py_DECREF(result);
            return NULL;
        }
        Py_DECREF(ret);
        return result;
    }

    while (1) {
        PyObject *line = PyIter_Next(self);
        if (line == NULL) {
            if (PyErr_Occurred()) {
                Py_DECREF(result);
                return NULL;
            }
            else
                break; /* SopIteration raised */
        }

        if (PyList_Append(result, line) < 0) {
            Py_DECREF(line);
            Py_DECREF(result);
            return NULL;
        }
        length += PyObject_Size(line);
        Py_DECREF(line);

        if (length > hint)
            break;
    }
    return result;
}

static PyObject *
IOBase_writelines(PyObject *self, PyObject *args)
{
    PyObject *lines, *iter, *res;

    if (!PyArg_ParseTuple(args, "O:writelines", &lines)) {
        return NULL;
    }

    if (_PyIOBase_checkClosed(self, Py_True) == NULL)
        return NULL;

    iter = PyObject_GetIter(lines);
    if (iter == NULL)
        return NULL;

    while (1) {
        PyObject *line = PyIter_Next(iter);
        if(line == NULL) {
            if (PyErr_Occurred()) {
                Py_DECREF(iter);
                return NULL;
            }
            else
                break; /* Stop Iteration */
        }

        res = PyObject_CallMethodObjArgs(self, _PyIO_str_write, line, NULL);
        Py_DECREF(line);
        if (res == NULL) {
            Py_DECREF(iter);
            return NULL;
        }
        Py_DECREF(res);
    }
    Py_RETURN_NONE;
}

static PyMethodDef IOBase_methods[] = {
    {"seek", IOBase_seek, METH_VARARGS, IOBase_seek_doc},
    {"tell", IOBase_tell, METH_NOARGS, IOBase_tell_doc},
    {"truncate", IOBase_truncate, METH_VARARGS, IOBase_truncate_doc},
    {"flush", IOBase_flush, METH_NOARGS, IOBase_flush_doc},
    {"close", IOBase_close, METH_NOARGS, IOBase_close_doc},

    {"seekable", IOBase_seekable, METH_NOARGS, IOBase_seekable_doc},
    {"readable", IOBase_readable, METH_NOARGS, IOBase_readable_doc},
    {"writable", IOBase_writable, METH_NOARGS, IOBase_writable_doc},

    {"_checkClosed",   _PyIOBase_checkClosed, METH_NOARGS},
    {"_checkSeekable", _PyIOBase_checkSeekable, METH_NOARGS},
    {"_checkReadable", _PyIOBase_checkReadable, METH_NOARGS},
    {"_checkWritable", _PyIOBase_checkWritable, METH_NOARGS},

    {"fileno", IOBase_fileno, METH_NOARGS, IOBase_fileno_doc},
    {"isatty", IOBase_isatty, METH_NOARGS, IOBase_isatty_doc},

    {"__del__", IOBase_del, METH_NOARGS},
    {"__enter__", IOBase_enter, METH_NOARGS},
    {"__exit__", IOBase_exit, METH_VARARGS},

    {"readline", IOBase_readline, METH_VARARGS, IOBase_readline_doc},
    {"readlines", IOBase_readlines, METH_VARARGS, IOBase_readlines_doc},
    {"writelines", IOBase_writelines, METH_VARARGS},

    {NULL, NULL}
};

static PyGetSetDef IOBase_getset[] = {
    {"closed", (getter)IOBase_closed_get, NULL, NULL},
    {0}
};


PyTypeObject PyIOBase_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "IOBase",                   /*tp_name*/
    0,                          /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    0,                          /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare */
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash */
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /*tp_flags*/
    IOBase_doc,                 /* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    IOBase_iter,                /* tp_iter */
    IOBase_iternext,            /* tp_iternext */
    IOBase_methods,             /* tp_methods */
    0,                          /* tp_members */
    IOBase_getset,              /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0,                          /* tp_init */
    0,                          /* tp_alloc */
    PyType_GenericNew,          /* tp_new */
    0,                          /* tp_free */
    0,                          /* tp_is_gc */
    0,                          /* tp_bases */
    0,                          /* tp_mro */
    0,                          /* tp_cache */
    0,                          /* tp_subclasses */
    0,                          /* tp_weaklist */
    0,                          /* tp_del */
};



/*
 * RawIOBase class, Inherits from IOBase.
 */
PyDoc_STRVAR(RawIOBase_doc,
             "Base class for raw binary I/O.");

/*
 * The read() method is implemented by calling readinto(); derived classes
 * that want to support read() only need to implement readinto() as a
 * primitive operation.  In general, readinto() can be more efficient than
 * read().
 *
 * (It would be tempting to also provide an implementation of readinto() in
 * terms of read(), in case the latter is a more suitable primitive operation,
 * but that would lead to nasty recursion in case a subclass doesn't implement
 * either.)
*/

static PyObject *
RawIOBase_read(PyObject *self, PyObject *args)
{
    Py_ssize_t n = -1;
    PyObject *b, *res;

    if (!PyArg_ParseTuple(args, "|n:read", &n)) {
        return NULL;
    }

    if (n < 0)
        return PyObject_CallMethod(self, "readall", NULL);

    /* TODO: allocate a bytes object directly instead and manually construct
       a writable memoryview pointing to it. */
    b = PyByteArray_FromStringAndSize(NULL, n);
    if (b == NULL)
        return NULL;

    res = PyObject_CallMethodObjArgs(self, _PyIO_str_readinto, b, NULL);
    if (res == NULL) {
        Py_DECREF(b);
        return NULL;
    }

    n = PyNumber_AsSsize_t(res, PyExc_ValueError);
    Py_DECREF(res);
    if (n == -1 && PyErr_Occurred()) {
        Py_DECREF(b);
        return NULL;
    }

    res = PyBytes_FromStringAndSize(PyByteArray_AsString(b), n);
    Py_DECREF(b);
    return res;
}


PyDoc_STRVAR(RawIOBase_readall_doc,
             "Read until EOF, using multiple read() call.");

static PyObject *
RawIOBase_readall(PyObject *self, PyObject *args)
{
    PyObject *b = NULL;
    Py_ssize_t cursize = 0;

    while (1) {
        Py_ssize_t length;
        PyObject *data = PyObject_CallMethod(self, "read",
                                             "i", DEFAULT_BUFFER_SIZE);

        if (!data) {
            Py_XDECREF(b);
            return NULL;
        }

        if (!PyBytes_Check(data)) {
            Py_XDECREF(b);
            Py_DECREF(data);
            PyErr_SetString(PyExc_TypeError, "read() should return bytes");
            return NULL;
        }

        length = Py_SIZE(data);

        if (b == NULL)
            b = data;
        else if (length != 0) {

            _PyBytes_Resize(&b, cursize + length);
            if (b == NULL) {
                Py_DECREF(data);
                return NULL;
            }

            memcpy(PyBytes_AS_STRING(b) + cursize,
                   PyBytes_AS_STRING(data), length);
            Py_DECREF(data);
        }

        if (length == 0)
            break;
    }

    return b;

}

static PyMethodDef RawIOBase_methods[] = {
    {"read", RawIOBase_read, METH_VARARGS},
    {"readall", RawIOBase_readall, METH_NOARGS, RawIOBase_readall_doc},
    {NULL, NULL}
};

PyTypeObject PyRawIOBase_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "RawIOBase",                /*tp_name*/
    0,                          /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    0,                          /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare */
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash */
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /*tp_flags*/
    RawIOBase_doc,              /* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    RawIOBase_methods,          /* tp_methods */
    0,                          /* tp_members */
    0,                          /* tp_getset */
    &PyIOBase_Type,             /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0,                          /* tp_init */
    0,                          /* tp_alloc */
    0,                          /* tp_new */
};
