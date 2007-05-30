#include "Python.h"

PyDoc_STRVAR(module_doc,
"A fast implementation of StringIO.");

typedef struct {
	PyObject_HEAD
	Py_UNICODE *buf;
	Py_ssize_t pos, string_size;
	Py_ssize_t buf_size;
} StringIOObject;

static PyTypeObject StringIO_Type;

/* The initial size of the buffer of StringIO objects */
#define BUFSIZE 128

static PyObject *
err_closed(void)
{
	PyErr_SetString(PyExc_ValueError, "I/O operation on closed file");
	return NULL;
}

/* Internal routine to get a line. Returns the number of bytes read. */
static Py_ssize_t
get_line(StringIOObject *self, Py_UNICODE **output)
{
	Py_UNICODE *n;
	const Py_UNICODE *str_end;
	Py_ssize_t l;

	/* XXX: Should we ckeck here if the object is closed,
	   for thread-safety? */
	assert(self->buf != NULL);
	str_end = self->buf + self->string_size;

	/* Move to the end of the line, up to the end of the string, s. */
	for (n = self->buf + self->pos; n < str_end && *n != '\n'; n++);

	/* Skip the newline character */
	if (n < str_end)
		n++;

	/* Get the length from the current position to the end of the line. */
	l = n - (self->buf + self->pos);
	*output = self->buf + self->pos;

	assert(self->pos + l < PY_SSIZE_T_MAX);
	assert(l >= 0);
	self->pos += l;

	return l;
}

/* Internal routine for writing a string of bytes to the buffer of a StringIO
   object. Returns the number of bytes wrote. */
static Py_ssize_t
write_bytes(StringIOObject *self, const Py_UNICODE *c, Py_ssize_t l)
{
	Py_ssize_t newl;

	assert(self->buf != NULL);

	newl = self->pos + l;
	if (newl >= self->buf_size) {
		self->buf_size *= 2;
		if (self->buf_size <= newl) {
			assert(newl + 1 < PY_SSIZE_T_MAX);
			self->buf_size = newl + 1;
		}

		PyMem_Resize(self->buf, Py_UNICODE, self->buf_size);
		if (self->buf == NULL) {
			PyErr_SetString(PyExc_MemoryError, "Out of memory");
			PyMem_Del(self->buf);
			self->buf_size = self->pos = 0;
			return -1;
		}
	}

	memcpy(self->buf + self->pos, c, l);

	assert(self->pos + l < PY_SSIZE_T_MAX);
	self->pos += l;

	if (self->string_size < self->pos) {
		self->string_size = self->pos;
	}

	return l;
}


static PyObject *
string_io_get_closed(StringIOObject *self)
{
	PyObject *result = Py_False;

	if (self->buf == NULL)
		result = Py_True;

	Py_INCREF(result);
	return result;
}

static PyObject *
string_io_flush(StringIOObject *self)
{
	if (self->buf == NULL)
		return err_closed();

	Py_RETURN_NONE;
}

static PyObject *
string_io_getvalue(StringIOObject *self)
{
	if (self->buf == NULL)
		return err_closed();

	return PyUnicode_FromUnicode(self->buf, self->string_size);
}

static PyObject *
string_io_isatty(StringIOObject *self)
{
	if (self->buf == NULL)
		return err_closed();

	Py_RETURN_FALSE;
}

static PyObject *
string_io_tell(StringIOObject *self)
{
	if (self->buf == NULL)
		return err_closed();

	return PyInt_FromSsize_t(self->pos);
}

static PyObject *
string_io_read(StringIOObject *self, PyObject *args)
{
	Py_ssize_t l, n = -1;
	Py_UNICODE *output;

	if (self->buf == NULL)
		return err_closed();

	if (!PyArg_ParseTuple(args, "|n:read", &n))
		return NULL;

	/* adjust invalid sizes */
	l = self->string_size - self->pos;
	if (n < 0 || n > l) {
		n = l;
		if (n < 0)
			n = 0;
	}

	output = self->buf + self->pos;
	self->pos += n;

	return PyUnicode_FromUnicode(output, n);
}

static PyObject *
string_io_readline(StringIOObject *self, PyObject *args)
{
	Py_ssize_t n, m = -1;
	Py_UNICODE *output;

	if (self->buf == NULL)
		return err_closed();

	if (!PyArg_ParseTuple(args, "|i:readline", &m))
		return NULL;

	n = get_line(self, &output);

	if (m >= 0 && m < n) {
		m = n - m;
		n -= m;
		self->pos -= m;
	}

	return PyUnicode_FromUnicode(output, n);
}

static PyObject *
string_io_readlines(StringIOObject *self, PyObject *args)
{
	Py_ssize_t n, hint = 0, length = 0;
	PyObject *result, *line;
	Py_UNICODE *output;

	if (self->buf == NULL)
		return err_closed();

	if (!PyArg_ParseTuple(args, "|i:readlines", &hint))
		return NULL;

	result = PyList_New(0);
	if (!result)
		return NULL;

	while (1) {
		n = get_line(self, &output);

		if (n == 0)
			break;
		line = PyUnicode_FromUnicode(output, n);
		if (!line)
			goto err;
		if (PyList_Append(result, line) == -1) {
			Py_DECREF(line);
			goto err;
		}
		Py_DECREF(line);
		length += n;
		if (hint > 0 && length >= hint)
			break;
	}
	return result;
      err:
	Py_DECREF(result);
	return NULL;
}

static PyObject *
string_io_truncate(StringIOObject *self, PyObject *args)
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

	Py_RETURN_NONE;
}

static PyObject *
string_io_iternext(StringIOObject *self)
{
	Py_UNICODE *next;
	Py_ssize_t n;

	if (self->buf == NULL)
		return err_closed();

	n = get_line(self, &next);

	if (!next)
		return NULL;
	if (n == 0)
		return NULL;

	return PyUnicode_FromUnicode(next, n);
}

static PyObject *
string_io_seek(StringIOObject *self, PyObject *args)
{
	Py_ssize_t position;
	int mode = 0;

	if (self->buf == NULL)
		return err_closed();

	if (!PyArg_ParseTuple(args, "n|i:seek", &position, &mode))
		return NULL;

	if (mode == 2) {
		position += self->string_size;
	}
	else if (mode == 1) {
		position += self->pos;
	}

	if (position > self->buf_size) {
		self->buf_size *= 2;
		if (self->buf_size <= position)
			self->buf_size = position + 1;

		PyMem_Resize(self->buf, Py_UNICODE, self->buf_size);
		if (self->buf == NULL) {
			PyMem_Del(self->buf);
			self->buf_size = self->pos = 0;
			return PyErr_NoMemory();
		}
	}
	else if (position < 0)
		position = 0;

	self->pos = position;

	while (--position >= self->string_size)
		self->buf[position] = 0;

	Py_RETURN_NONE;
}

static PyObject *
string_io_write(StringIOObject *self, PyObject *args)
{
	const Py_UNICODE *c;
	Py_ssize_t l;

	if (self->buf == NULL)
		return err_closed();

	if (!PyArg_ParseTuple(args, "u#:write", &c, &l))
		return NULL;

	if (write_bytes(self, c, l) == -1)
		return NULL;

	Py_RETURN_NONE;
}

static PyObject *
string_io_writelines(StringIOObject *self, PyObject *v)
{
	PyObject *it, *item;

	if (self->buf == NULL)
		return err_closed();

	it = PyObject_GetIter(v);
	if (it == NULL)
		return NULL;

	while ((item = PyIter_Next(it)) != NULL) {
		Py_ssize_t n;
		Py_UNICODE *c;
		if (PyString_AsStringAndSize(item, &c, &n) == -1) {
			Py_DECREF(it);
			Py_DECREF(item);
			return NULL;
		}
		Py_DECREF(item);

		if (write_bytes(self, c, n) == -1) {
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
string_io_close(StringIOObject *self)
{
	if (self->buf != NULL)
		PyMem_Del(self->buf);
	self->buf = NULL;

	self->pos = self->string_size = self->buf_size = 0;

	Py_RETURN_NONE;
}

static void
StringIO_dealloc(StringIOObject *op)
{
	if (op->buf != NULL)
		PyMem_Del(op->buf);

	op->ob_type->tp_free((PyObject *)op);
}

static PyObject *
StringIO_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	StringIOObject *self;
	const Py_UNICODE *buf;
	Py_ssize_t n = -1, size = BUFSIZE;

	assert(type != NULL && type->tp_alloc != NULL);

	if (!PyArg_ParseTuple(args, "|u#:StringIO", &buf, &n))
		return NULL;

	self = (StringIOObject *)type->tp_alloc(type, 0);

	if (self == NULL)
		return NULL;

	self->buf = PyMem_New(Py_UNICODE, size);

	/* These variables need to be initialized before attempting to write
	   anything to the object. */
	self->pos = 0;
	self->string_size = 0;

	if (n > 0) {
		if (write_bytes(self, buf, n) == -1)
			return NULL;
		self->pos = 0;
	}
	if (self->buf == NULL) {
		Py_DECREF(self);
		return PyErr_NoMemory();
	}
	self->buf_size = size;

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
"number of bytes to return (an incomplete line may be returned then).\n"
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
"truncate([size]) -> None.  Truncate the file to at most size bytes.\n"
"\n"
"Size defaults to the current file position, as returned by tell().\n"
"If the specified size exceeds the file's current size, the file\n"
"remains unchanged.");

PyDoc_STRVAR(StringIO_close_doc,
"close() -> None.  Close the file and release the resources held.");

PyDoc_STRVAR(StringIO_seek_doc,
"seek(offset[, whence]) -> None.  Set the file's current position.\n"
"\n"
"Argument offset is a byte count.  Optional argument whence defaults to\n"
"0 (offset from start of file, offset should be >= 0); other values are 1\n"
"(move relative to current position, positive or negative), and 2 (move\n"
"relative to end of file, usually negative).\n");

PyDoc_STRVAR(StringIO_write_doc,
"write(str) -> None.  Write string str to file.");

PyDoc_STRVAR(StringIO_writelines_doc,
"writelines(sequence_of_strings) -> None.  Write the strings to the file.\n"
"\n"
"Note that newlines are not added.  The sequence can be any iterable object\n"
"producing strings. This is equivalent to calling write() for each string.");


static PyGetSetDef StringIO_getsetlist[] = {
	{"closed", (getter) string_io_get_closed, NULL,
	 "True if the file is closed"},
	{0},			/* sentinel */
};

static struct PyMethodDef StringIO_methods[] = {
	{"flush",      (PyCFunction) string_io_flush, METH_NOARGS,
	 StringIO_flush_doc},
	{"getvalue",   (PyCFunction) string_io_getvalue, METH_VARARGS,
	 StringIO_getval_doc},
	{"isatty",     (PyCFunction) string_io_isatty, METH_NOARGS,
	 StringIO_isatty_doc},
	{"read",       (PyCFunction) string_io_read, METH_VARARGS,
	 StringIO_read_doc},
	{"readline",   (PyCFunction) string_io_readline, METH_VARARGS,
	 StringIO_readline_doc},
	{"readlines",  (PyCFunction) string_io_readlines, METH_VARARGS,
	 StringIO_readlines_doc},
	{"tell",       (PyCFunction) string_io_tell, METH_NOARGS,
	 StringIO_tell_doc},
	{"truncate",   (PyCFunction) string_io_truncate, METH_VARARGS,
	 StringIO_truncate_doc},
	{"close",      (PyCFunction) string_io_close, METH_NOARGS,
	 StringIO_close_doc},
	{"seek",       (PyCFunction) string_io_seek, METH_VARARGS,
	 StringIO_seek_doc},
	{"write",      (PyCFunction) string_io_write, METH_VARARGS,
	 StringIO_write_doc},
	{"writelines", (PyCFunction) string_io_writelines, METH_O,
	 StringIO_writelines_doc},
	{NULL, NULL}		/* sentinel */
};


static PyTypeObject StringIO_Type = {
	PyObject_HEAD_INIT(NULL)
	0,					/*ob_size*/
	"_string_io.StringIO",			/*tp_name*/
	sizeof(StringIOObject),			/*tp_basicsize*/
	0,					/*tp_itemsize*/
	/* methods */
	(destructor)StringIO_dealloc,		/*tp_dealloc*/
	0,					/*tp_print*/
	0,					/*tp_getattr*/
	0,					/*tp_setattr*/
	0,					/*tp_compare*/
	0,					/*tp_repr*/
	0,					/*tp_as_number*/
	0,					/*tp_as_sequence*/
	0,					/*tp_as_mapping*/
	0,					/*tp_hash*/
	0,					/*tp_call*/
	0,					/*tp_str*/
	0,					/*tp_getattro*/
	0,					/*tp_setattro*/
	0,					/*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
	StringIO_doc,				/*tp_doc*/
	0,					/*tp_traverse*/
	0,					/*tp_clear*/
	0,					/*tp_richcompare*/
	0,					/*tp_weaklistoffset*/
	PyObject_SelfIter,			/*tp_iter*/
	(iternextfunc)string_io_iternext,	/*tp_iternext*/
	StringIO_methods, 			/*tp_methods*/
	0,					/*tp_members*/
	StringIO_getsetlist,			/*tp_getset*/
	0,					/*tp_base*/
	0,					/*tp_dict*/
	0,					/*tp_descr_get*/
	0,					/*tp_descr_set*/
	0,					/*tp_dictoffset*/
	0,					/*tp_init*/
	0,					/*tp_alloc*/
	StringIO_new,				/*tp_new*/
	0,					/*tp_free*/
	0,					/*tp_is_gc*/
};

PyMODINIT_FUNC
init_string_io(void)
{
	PyObject *m;

	if (PyType_Ready(&StringIO_Type) < 0)
		return;
	m = Py_InitModule3("_string_io", NULL, module_doc);
	if (m == NULL)
		return;
	Py_INCREF(&StringIO_Type);
	PyModule_AddObject(m, "StringIO", (PyObject *)&StringIO_Type);
}
