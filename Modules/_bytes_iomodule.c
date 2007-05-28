#include "Python.h"

typedef struct {
	PyObject_HEAD
	char *buf;
	Py_ssize_t pos, string_size;
	Py_ssize_t buf_size;
} BytesIOObject;

static PyTypeObject BytesIO_Type;


static PyObject *
err_closed(void)
{
	PyErr_SetString(PyExc_ValueError, "I/O operation on closed file");
	return NULL;
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

static PyObject *
bytes_io_flush(BytesIOObject *self)
{
	if (self->buf == NULL)
		return err_closed();

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
bytes_io_getvalue(BytesIOObject *self)
{
	if (self->buf == NULL)
		return err_closed();

	return PyString_FromStringAndSize(self->buf, self->string_size);
}

static PyObject *
bytes_io_isatty(BytesIOObject *self)
{
	if (self->buf == NULL)
		return err_closed();

	Py_INCREF(Py_False);
	return Py_False;
}

static PyObject *
bytes_io_read(BytesIOObject *self, PyObject *args)
{
	Py_ssize_t l, n = -1;
	char *output = NULL;

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

	return PyString_FromStringAndSize(output, n);
}

static PyObject *
bytes_io_readline(BytesIOobject *self, PyObject *args)
{
	char *n, *s;
	int m = -1;
	Py_ssize_t l;
	char *output;

	if (self->buf == NULL)
		return err_closed();

	if (args) {
		if (!PyArg_ParseTuple(args, "|i:readline", &m))
			return NULL;
	}

	/* move to the end of the line, upto the end of the string, s */
	for (n = self->buf + self->pos,
	     s = self->buf + self->string_size;
	     n < s && *n != '\n'; n++);

	/* skip the newline character */
	if (n < s)
		n++;

	/* get the length from the current position to the end of the line */
	l = n - (self->buf + self->pos);
	output = self->buf + self->pos;

	assert(self->pos + l < INT_MAX);  /* XXX: Is this really needed? */
	self->pos += l;

	if (m >= 0 && m < l) {
		m = l - m;
		l -= m;
		self->pos -= m;
	}

	return PyString_FromStringAndSize(output, l);
}

static PyObject *
IO_readlines(IOobject *self, PyObject *args)
{
	int n;
	char *output;
	PyObject *result, *line;
	int hint = 0, length = 0;

	if (!PyArg_ParseTuple(args, "|i:readlines", &hint))
		return NULL;

	result = PyList_New(0);
	if (!result)
		return NULL;

	while (1) {
		if ((n = IO_creadline((PyObject *) self, &output)) < 0)
			goto err;
		if (n == 0)
			break;
		line = PyString_FromStringAndSize(output, n);
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

PyDoc_STRVAR(IO_readlines__doc__, "readlines() -- Read all lines");

static PyObject *
IO_reset(IOobject *self, PyObject *unused)
{

	if (!IO__opencheck(self))
		return NULL;

	self->pos = 0;

	Py_INCREF(Py_None);
	return Py_None;
}

PyDoc_STRVAR(IO_reset__doc__,
"reset() -- Reset the file position to the beginning");

static PyObject *
IO_tell(IOobject *self, PyObject *unused)
{

	if (!IO__opencheck(self))
		return NULL;

	return PyInt_FromSsize_t(self->pos);
}

PyDoc_STRVAR(IO_tell__doc__, "tell() -- get the current position.");

static PyObject *
IO_truncate(IOobject *self, PyObject *args)
{
	Py_ssize_t pos = -1;

	if (!IO__opencheck(self))
		return NULL;
	if (!PyArg_ParseTuple(args, "|n:truncate", &pos))
		return NULL;

	if (PyTuple_Size(args) == 0) {
		/* No argument passed, truncate to current position */
		pos = self->pos;
	}

	if (pos < 0) {
		errno = EINVAL;
		PyErr_SetFromErrno(PyExc_IOError);
		return NULL;
	}

	if (self->string_size > pos)
		self->string_size = pos;
	self->pos = self->string_size;

	Py_INCREF(Py_None);
	return Py_None;
}

PyDoc_STRVAR(IO_truncate__doc__,
"truncate(): truncate the file at the current position.");

static PyObject *
IO_iternext(Iobject *self)
{
	PyObject *next;
	next = IO_readline((IOobject *) self, NULL);
	if (!next)
		return NULL;
	if (!PyString_GET_SIZE(next)) {
		Py_DECREF(next);
		PyErr_SetNone(PyExc_StopIteration);
		return NULL;
	}
	return next;
}


/* Read-write object methods */

static PyObject *
O_seek(Oobject *self, PyObject *args)
{
	Py_ssize_t position;
	int mode = 0;

	if (!IO__opencheck(IOOOBJECT(self)))
		return NULL;
	if (!PyArg_ParseTuple(args, "n|i:seek", &position, &mode))
		return NULL;

	if (mode == 2) {
		position += self->string_size;
	}
	else if (mode == 1) {
		position += self->pos;
	}

	if (position > self->buf_size) {
		char *newbuf;
		self->buf_size *= 2;
		if (self->buf_size <= position)
			self->buf_size = position + 1;
		newbuf = (char *) realloc(self->buf, self->buf_size);
		if (!newbuf) {
			free(self->buf);
			self->buf = 0;
			self->buf_size = self->pos = 0;
			return PyErr_NoMemory();
		}
		self->buf = newbuf;
	}
	else if (position < 0)
		position = 0;

	self->pos = position;

	while (--position >= self->string_size)
		self->buf[position] = 0;

	Py_INCREF(Py_None);
	return Py_None;
}

PyDoc_STRVAR(O_seek__doc__,
"seek(position)       -- set the current position\n"
"seek(position, mode) -- mode 0: absolute; 1: relative; 2: relative to EOF");

static int
O_cwrite(PyObject *self, const char *c, Py_ssize_t l)
{
	Py_ssize_t newl;
	Oobject *oself;
	char *newbuf;

	if (!IO__opencheck(IOOOBJECT(self)))
		return -1;
	oself = (Oobject *) self;

	newl = oself->pos + l;
	if (newl >= oself->buf_size) {
		oself->buf_size *= 2;
		if (oself->buf_size <= newl) {
			assert(newl + 1 < INT_MAX);
			oself->buf_size = (int) (newl + 1);
		}
		newbuf = (char *) realloc(oself->buf, oself->buf_size);
		if (!newbuf) {
			PyErr_SetString(PyExc_MemoryError, "out of memory");
			free(oself->buf);
			oself->buf = 0;
			oself->buf_size = oself->pos = 0;
			return -1;
		}
		oself->buf = newbuf;
	}

	memcpy(oself->buf + oself->pos, c, l);

	assert(oself->pos + l < INT_MAX);
	oself->pos += (int) l;

	if (oself->string_size < oself->pos) {
		oself->string_size = oself->pos;
	}

	return (int) l;
}

static PyObject *
O_write(Oobject *self, PyObject *args)
{
	char *c;
	int l;

	if (!PyArg_ParseTuple(args, "t#:write", &c, &l))
		return NULL;

	if (O_cwrite((PyObject *) self, c, l) < 0)
		return NULL;

	Py_INCREF(Py_None);
	return Py_None;
}

PyDoc_STRVAR(O_write__doc__,
"write(s) -- Write a string to the file"
"\n\nNote (hack:) writing None resets the buffer");

static PyObject *
O_close(Oobject *self, PyObject *unused)
{
	if (self->buf != NULL)
		free(self->buf);
	self->buf = NULL;

	self->pos = self->string_size = self->buf_size = 0;

	Py_INCREF(Py_None);
	return Py_None;
}

PyDoc_STRVAR(O_close__doc__, "close(): explicitly release resources held.");

static PyObject *
O_writelines(Oobject *self, PyObject *args)
{
	PyObject *it, *s;

	it = PyObject_GetIter(args);
	if (it == NULL)
		return NULL;
	while ((s = PyIter_Next(it)) != NULL) {
		Py_ssize_t n;
		char *c;
		if (PyString_AsStringAndSize(s, &c, &n) == -1) {
			Py_DECREF(it);
			Py_DECREF(s);
			return NULL;
		}
		if (O_cwrite((PyObject *) self, c, n) == -1) {
			Py_DECREF(it);
			Py_DECREF(s);
			return NULL;
		}
		Py_DECREF(s);
	}

	Py_DECREF(it);

	/* See if PyIter_Next failed */
	if (PyErr_Occurred())
		return NULL;

	Py_RETURN_NONE;
}

PyDoc_STRVAR(O_writelines__doc__,
"writelines(sequence_of_strings) -> None.  Write the strings to the file.\n"
"\n"
"Note that newlines are not added.  The sequence can be any iterable object\n"
"producing strings. This is equivalent to calling write() for each string.");


static struct PyMethodDef O_methods[] = {
	/* Common methods: */
	{"flush", (PyCFunction) IO_flush, METH_NOARGS, IO_flush__doc__},
	{"getvalue", (PyCFunction) IO_getval, METH_VARARGS, IO_getval__doc__},
	{"isatty", (PyCFunction) IO_isatty, METH_NOARGS, IO_isatty__doc__},
	{"read", (PyCFunction) IO_read, METH_VARARGS, IO_read__doc__},
	{"readline", (PyCFunction) IO_readline, METH_VARARGS,
	 IO_readline__doc__},
	{"readlines", (PyCFunction) IO_readlines, METH_VARARGS,
	 IO_readlines__doc__},
	{"reset", (PyCFunction) IO_reset, METH_NOARGS, IO_reset__doc__},
	{"tell", (PyCFunction) IO_tell, METH_NOARGS, IO_tell__doc__},
	{"truncate", (PyCFunction) IO_truncate, METH_VARARGS,
	 IO_truncate__doc__},

	/* Read-write BytesIO specific  methods: */
	{"close", (PyCFunction) O_close, METH_NOARGS, O_close__doc__},
	{"seek", (PyCFunction) O_seek, METH_VARARGS, O_seek__doc__},
	{"write", (PyCFunction) O_write, METH_VARARGS, O_write__doc__},
	{"writelines", (PyCFunction) O_writelines, METH_O,
	 O_writelines__doc__},
	{NULL, NULL}		/* sentinel */
};

static void
O_dealloc(Oobject *self)
{
	if (self->buf != NULL)
		free(self->buf);
	PyObject_Del(self);
}


PyDoc_STRVAR(Otype__doc__, "Simple type for output to strings.");

static PyTypeObject Otype = {
	PyObject_HEAD_INIT(NULL)
	0,			/*ob_size */
	"_bytes_io.StringO",	/*tp_name */
	sizeof(Oobject),	/*tp_basicsize */
	0,			/*tp_itemsize */
	/* methods */
	(destructor) O_dealloc,	/*tp_dealloc */
	0,			/*tp_print */
	0,			/*tp_getattr */
	0,			/*tp_setattr */
	0,			/*tp_compare */
	0,			/*tp_repr */
	0,			/*tp_as_number */
	0,			/*tp_as_sequence */
	0,			/*tp_as_mapping */
	0,			/*tp_hash */
	0,			/*tp_call */
	0,			/*tp_str */
	0,			/*tp_getattro */
	0,			/*tp_setattro */
	0,			/*tp_as_buffer */
	Py_TPFLAGS_DEFAULT,	/*tp_flags */
	Otype__doc__,		/*tp_doc */
	0,			/*tp_traverse */
	0,			/*tp_clear */
	0,			/*tp_richcompare */
	0,			/*tp_weaklistoffset */
	PyObject_SelfIter,	/*tp_iter */
	(iternextfunc) IO_iternext,	/*tp_iternext */
	O_methods,		/*tp_methods */
	0,			/*tp_members */
	file_getsetlist,	/*tp_getset */
};

static PyObject *
newOobject(int size)
{
	Oobject *self;

	self = PyObject_New(Oobject, &Otype);
	if (self == NULL)
		return NULL;
	self->pos = 0;
	self->string_size = 0;

	self->buf = (char *) malloc(size);
	if (!self->buf) {
		PyErr_SetString(PyExc_MemoryError, "out of memory");
		self->buf_size = 0;
		Py_DECREF(self);
		return NULL;
	}

	self->buf_size = size;
	return (PyObject *) self;
}

/* End of code for StringO objects */
/* -------------------------------------------------------- */

static PyObject *
I_close(Iobject *self, PyObject *unused)
{
	Py_XDECREF(self->pbuf);
	self->pbuf = NULL;
	self->buf = NULL;

	self->pos = self->string_size = 0;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
I_seek(Iobject *self, PyObject *args)
{
	Py_ssize_t position;
	int mode = 0;

	if (!IO__opencheck(IOOOBJECT(self)))
		return NULL;
	if (!PyArg_ParseTuple(args, "n|i:seek", &position, &mode))
		return NULL;

	if (mode == 2)
		position += self->string_size;
	else if (mode == 1)
		position += self->pos;

	if (position < 0)
		position = 0;

	self->pos = position;

	Py_INCREF(Py_None);
	return Py_None;
}

static struct PyMethodDef I_methods[] = {
	/* Common methods: */
	{"flush", (PyCFunction) IO_flush, METH_NOARGS, IO_flush__doc__},
	{"getvalue", (PyCFunction) IO_getval, METH_VARARGS, IO_getval__doc__},
	{"isatty", (PyCFunction) IO_isatty, METH_NOARGS, IO_isatty__doc__},
	{"read", (PyCFunction) IO_read, METH_VARARGS, IO_read__doc__},
	{"readline", (PyCFunction) IO_readline, METH_VARARGS,
	 IO_readline__doc__},
	{"readlines", (PyCFunction) IO_readlines, METH_VARARGS,
	 IO_readlines__doc__},
	{"reset", (PyCFunction) IO_reset, METH_NOARGS, IO_reset__doc__},
	{"tell", (PyCFunction) IO_tell, METH_NOARGS, IO_tell__doc__},
	{"truncate", (PyCFunction) IO_truncate, METH_VARARGS,
	 IO_truncate__doc__},

	/* Read-only BytesIO specific  methods: */
	{"close", (PyCFunction) I_close, METH_NOARGS, O_close__doc__},
	{"seek", (PyCFunction) I_seek, METH_VARARGS, O_seek__doc__},
	{NULL, NULL}
};

static void
I_dealloc(Iobject *self)
{
	Py_XDECREF(self->pbuf);
	PyObject_Del(self);
}

PyDoc_STRVAR(Itype__doc__,
	     "Simple type for treating strings as input file streams");

static PyTypeObject Itype = {
	PyObject_HEAD_INIT(NULL)
	      0,		/*ob_size */
	"_bytes_io.StringI",	/*tp_name */
	sizeof(Iobject),	/*tp_basicsize */
	0,			/*tp_itemsize */
	/* methods */
	(destructor) I_dealloc,	/*tp_dealloc */
	0,			/*tp_print */
	0,			/* tp_getattr */
	0,			/*tp_setattr */
	0,			/*tp_compare */
	0,			/*tp_repr */
	0,			/*tp_as_number */
	0,			/*tp_as_sequence */
	0,			/*tp_as_mapping */
	0,			/*tp_hash */
	0,			/*tp_call */
	0,			/*tp_str */
	0,			/* tp_getattro */
	0,			/* tp_setattro */
	0,			/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,	/* tp_flags */
	Itype__doc__,		/* tp_doc */
	0,			/* tp_traverse */
	0,			/* tp_clear */
	0,			/* tp_richcompare */
	0,			/* tp_weaklistoffset */
	PyObject_SelfIter,	/* tp_iter */
	(iternextfunc) IO_iternext,	/* tp_iternext */
	I_methods,		/* tp_methods */
	0,			/* tp_members */
	file_getsetlist,	/* tp_getset */
};

static PyObject *
newIobject(PyObject *s)
{
	Iobject *self;
	char *buf;
	Py_ssize_t size;

	if (PyObject_AsCharBuffer(s, (const char **) &buf, &size) != 0)
		return NULL;

	self = PyObject_New(Iobject, &Itype);
	if (!self)
		return NULL;
	Py_INCREF(s);
	self->buf = buf;
	self->string_size = size;
	self->pbuf = s;
	self->pos = 0;

	return (PyObject *) self;
}

/* End of code for StringI objects */
/* -------------------------------------------------------- */

static PyObject *
IO_BytesIO(PyObject *self, PyObject *args)
{
	PyObject *s = 0;

	if (!PyArg_UnpackTuple(args, "BytesIO", 0, 1, &s))
		return NULL;

	if (s)
		return newIobject(s);
	return newOobject(128);
}

PyDoc_STRVAR(BytesIO_BytesIO_doc,
"BytesIO([s]) -> Return a BytesIO stream for reading or writing");

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
"Return an empty string at EOF.\n");

PyDoc_STRVAR(BytesIO_readline_doc,
"readline([size]) -> next line from the file, as a string.\n"
"\n"
"Retain newline.  A non-negative size argument limits the maximum\n"
"number of bytes to return (an incomplete line may be returned then).\n"
"Return an empty string at EOF.\n");

/* List of methods defined in the module */
static struct PyMethodDef IO_methods[] = {
	{"BytesIO", IO_BytesIO, METH_VARARGS, IO_BytesIO_doc},
	{NULL, NULL}		/* sentinel */
};

static PyGetSetDef bytes_io_getsetlist[] = {
	{"closed", (getter) IO_get_closed, NULL, "True if the file is closed"},
	{0},
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
