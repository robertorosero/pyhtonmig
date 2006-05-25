
/*
 *    BaseException
 */
typedef struct {
        PyObject_HEAD
        PyObject *args;
        PyObject *message;
} BaseExceptionObject;

static int
BaseException_init(BaseExceptionObject *self, PyObject *args, PyObject *kwds)
{
    /* we just want to store off the args as they are */
    self->args = args;
    Py_INCREF(args);

    if (PySequence_Length(args) == 1)
        self->message = PySequence_GetItem(args, 0);
    else
	self->message = PyString_FromString("");

    return 0;
}

static void
BaseException_dealloc(BaseExceptionObject *self)
{
    Py_DECREF(self->args);
    Py_DECREF(self->message);
}


static PyObject *
BaseException_str(BaseExceptionObject *self)
{
    PyObject *out;

    switch (PySequence_Size(self->args)) {
    case 0:
        out = PyString_FromString("");
        break;
    case 1:
    {
	PyObject *tmp = PySequence_GetItem(self->args, 0);
	if (tmp) {
	    out = PyObject_Str(tmp);
	    Py_DECREF(tmp);
	}
	else
	    out = NULL;
	break;
    }
    case -1:
        PyErr_Clear();
        /* Fall through */
    default:
        out = PyObject_Str(self->args);
        break;
    }

    return out;
}


#ifdef Py_USING_UNICODE
static PyObject *
BaseException_unicode(PyObject *self, PyObject *args)
{
	Py_ssize_t args_len;

	if (!PyArg_ParseTuple(args, "O:__unicode__", &self))
		return NULL;

	args_len = PySequence_Size(self->args);
	if (args_len < 0) {
		return NULL;
	}

	if (args_len == 0) {
		return PyUnicode_FromUnicode(NULL, 0);
	}
	else if (args_len == 1) {
		PyObject *temp = PySequence_GetItem(self->args, 0);
		PyObject *unicode_obj;

		if (!temp) {
			return NULL;
		}
		unicode_obj = PyObject_Unicode(temp);
		Py_DECREF(temp);
		return unicode_obj;
	}
	else {
		PyObject *unicode_obj = PyObject_Unicode(self->args);

		return unicode_obj;
	}
}
#endif /* Py_USING_UNICODE */

static PyObject *
BaseException_repr(PyObject *self)
{
	Py_ssize_t args_len;
	PyObject *repr_suffix;
	PyObject *repr;
	
	if (!PyArg_ParseTuple(args, "O:__repr__", &self))
		return NULL;

	args_len = PySequence_Length(self->args);
	if (args_len < 0) {
		return NULL;
	}

	if (args_len == 0) {
		repr_suffix = PyString_FromString("()");
		if (!repr_suffix)
			return NULL;
	}
	else {
		PyObject *args_repr = PyObject_Repr(self->args);
		if (!args_repr)
			return NULL;

		repr_suffix = args_repr;
	}

	repr = PyString_FromString(self->ob_type->tp_name);
	if (!repr) {
		Py_DECREF(repr_suffix);
		return NULL;
	}

	PyString_ConcatAndDel(&repr, repr_suffix);
	return repr;
}

static PyObject *
BaseException_getitem(PyObject *self, PyObject *args)
{
    PyObject *out;
    PyObject *index;

    if (!PyArg_ParseTuple(args, "OO:__getitem__", &self, &index))
        return NULL;

    return PyObject_GetItem(self->args, index);
}

static PySequenceMethods BaseException_as_sequence = {
	0 sq_length;
	0 sq_concat;
	0 sq_repeat;
	BaseException_getitem sq_item;
	0 sq_slice;
	0 sq_ass_item;
	0 sq_ass_slice;
	0 sq_contains;
	0 sq_inplace_concat;
	0 sq_inplace_repeat;
};

static PyMemberDef BaseException_members[] = {
        {"args", T_OBJECT, offsetof(BaseExceptionObject, args), 0, "exception arguments"},
        {"message", T_OBJECT, offsetof(BaseExceptionObject, message), 0, "exception message"},
        {NULL}  /* Sentinel */
};

static PyMethodDef BaseException_methods[] = {
   {"__unicode__", (PyCFunction)BaseException_unicode, METH_O },
   {NULL, NULL, 0, NULL},
};

static PyTypeObject BaseExceptionType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "exceptions.BaseException",          /*tp_name*/
    sizeof(BaseExceptionObject),      /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    BaseException_dealloc,     /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    (reprfunc)BaseException_repr,     /*tp_repr*/
    0,                         /*tp_as_number*/
    &BaseException_as_sequence, /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    (reprfunc)BaseException_str,       /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /*tp_flags*/
    "Common base class for all exceptions", /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    BaseException_methods,            /* tp_methods */
    BaseException_members,            /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)BaseException_init,     /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};

#define SimpleExtendsException(EXCBASE, EXCNAME, EXCDOC) \
static PyTypeObject EXCNAMEType = { \
    PyObject_HEAD_INIT(NULL) \
    0, \
    "exceptions.EXCNAME", \
    sizeof(BaseExceptionObject), \
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, \
    EXCDOC, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, &EXCBASE, \
    0, 0, 0, 0, 0, 0, 0,\
};

#define ComplexExtendsException(EXCBASE, EXCNAME, EXCDEALLOC, EXCMETHODS, EXCMEMBERS, EXCINIT, EXCNEW, EXCSTR, EXCDOC) \
static PyTypeObject EXCNAMEType = { \
    PyObject_HEAD_INIT(NULL) \
    0, \
    "exceptions.EXCNAME", \
    sizeof(EXCNAMEObject), \
    0, EXCDEALLOC, 0, 0, 0, 0, 0, 0, 0, 0, 0, EXCSTR, 0, 0, 0, \
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, \
    EXCDOC, \
    0, 0, 0, 0, 0, 0, EXCMETHODS, EXCMEMBERS, 0, &EXCBASE, \
    0, 0, 0, 0, EXCINIT, 0, EXCNEW,\
};


/*
 *    Exception extends BaseException
 */
SimpleExtendsException(BaseExceptionType, Exception, "Common base class for all non-exit exceptions.")


/*
 *    StandardError extends Exception
 */
SimpleExtendsException(ExceptionType, StandardError,
"Base class for all standard Python exceptions that do not represent"
"interpreter exiting.");


/*
 *    TypeError extends StandardError
 */
SimpleExtendsException(StandardErrorType, TypeError, "Inappropriate argument type.");


/*
 *    StopIteration extends Exception
 */
SimpleExtendsException(ExceptionType, StopIteration, "Signal the end from iterator.next().");


/*
 *    GeneratorExit extends Exception
 */
SimpleExtendsException(ExceptionType, GeneratorExit, "Request that a generator exit.");


/*
 *    SystemExit extends BaseException
 */
typedef struct {
        PyObject_HEAD
        PyObject *args;
        PyObject *message;
        PyObject *code;
} SystemExitObject;

static int
SystemExit_init(SystemExitObject *self, PyObject *args, PyObject *kwds)
{
    if (BaseException_init((BaseExceptionObject *)self, args, kwds) == -1)
        return -1;

    if (PySequence_Length(args) == 1)
        self->code = PySequence_GetItem(args, 0);
    else {
	self->code = Py_None;
        Py_INCREF(Py_None);
    }

    return 0;
}

static void
SystemExit_dealloc(SystemExitObject *self)
{
    BaseException_dealloc((BaseExceptionObject *)self);
    Py_DECREF(self->code);
}

static PyMemberDef SystemExit_members[] = {
        {"args", T_OBJECT, offsetof(SystemExitObject, args), 0, "exception arguments"},
        {"message", T_OBJECT, offsetof(SystemExitObject, message), 0, "exception message"},
        {"code", T_OBJECT, offsetof(SystemExitObject, code), 0, "exception code"},
        {NULL}  /* Sentinel */
};

ComplexExtendsException(BaseExceptionType, SystemExit, SystemExit_dealloc, 0, SystemExit_members, SystemExit_init, 0, "Request to exit from the interpreter.");


/*
 *    KeyboardInterrupt extends BaseException
 */
SimpleExtendsException(BaseExceptionType, KeyboardInterrupt, "Program interrupted by user.");


/*
 *    ImportError extends StandardError
 */
SimpleExtendsException(StandardErrorType, ImportError, "Import can't find module, or can't find name in module.");


/*
 *    EnvironmentError extends StandardError
 */
typedef struct {
        PyObject_HEAD
        PyObject *args;
        PyObject *message;
        PyObject *errno;
        PyObject *strerror;
        PyObject *filename;
} EnvironmentErrorObject;

static int
EnvironmentError_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *subslice = NULL;

    if (BaseException_init((BaseExceptionObject *)self, args, kwds) == -1)
        return -1;

    self->errno = Py_None;
    Py_INCREF(Py_None);
    self->strerror = Py_None;
    Py_INCREF(Py_None);
    self->filename = Py_None;
    Py_INCREF(Py_None);

    switch (PySequence_Size(args)) {
    case 3:
	/* Where a function has a single filename, such as open() or some
	 * of the os module functions, PyErr_SetFromErrnoWithFilename() is
	 * called, giving a third argument which is the filename.  But, so
	 * that old code using in-place unpacking doesn't break, e.g.:
	 *
	 * except IOError, (errno, strerror):
	 *
	 * we hack args so that it only contains two items.  This also
	 * means we need our own __str__() which prints out the filename
	 * when it was supplied.
	 */
	self->errno = PySequence_GetItem(args, 0);
        if (!self->errno) goto finally;
	self->strerror = PySequence_GetItem(args, 1);
        if (!self->strerror) goto finally;
	self->filename = PySequence_GetItem(args, 2);
        if (!self->filename) goto finally;

        subslice = PySequence_GetSlice(args, 0, 2);
	if (!subslice)
	    goto finally;

        Py_DECREF(self->args);  /* drop old ref set by base class init */
        self->args = subslice;
	return 0;

    case 2:
	/* Used when PyErr_SetFromErrno() is called and no filename
	 * argument is given.
	 */
	self->errno = PySequence_GetItem(args, 0);
        if (!self->errno) goto finally;
	self->strerror = PySequence_GetItem(args, 1);
        if (!self->strerror) goto finally;
	return 0;

    case -1:
	PyErr_Clear();
	return 0;
    }

  finally:
    Py_XDECREF(subslice);
    Py_XDECREF(self->errno);
    Py_XDECREF(self->strerror);
    Py_XDECREF(self->filename);
    return -1;
}

static void
EnvironmentError_dealloc(EnvironmentErrorObject *self)
{
    BaseException_dealloc((BaseExceptionObject *)self);
    Py_DECREF(self->errno);
    Py_DECREF(self->strerror);
    Py_DECREF(self->filename);
}

static PyObject *
EnvironmentError_str(PyObject *self)
{
    PyObject *rtnval = NULL;

    if (filename != Py_None) {
	PyObject *fmt = PyString_FromString("[Errno %s] %s: %s");
	PyObject *repr = PyObject_Repr(self->filename);
	PyObject *tuple = PyTuple_New(3);

	if (!fmt || !repr || !tuple) {
	    Py_XDECREF(fmt);
	    Py_XDECREF(repr);
	    Py_XDECREF(tuple);
	    return NULL;
	}

	PyTuple_SET_ITEM(tuple, 0, serrno);
	PyTuple_SET_ITEM(tuple, 1, strerror);
	PyTuple_SET_ITEM(tuple, 2, repr);

	rtnval = PyString_Format(fmt, tuple);

	Py_DECREF(fmt);
	Py_DECREF(tuple);
	/* already freed because tuple owned only reference */
	serrno = NULL;
	strerror = NULL;
    }
    else if (PyObject_IsTrue(self->errno) && PyObject_IsTrue(self->strerror)) {
	PyObject *fmt = PyString_FromString("[Errno %s] %s");
	PyObject *tuple = PyTuple_New(2);

	if (!fmt || !tuple) {
	    Py_XDECREF(fmt);
	    Py_XDECREF(tuple);
	    goto finally;
	}

	PyTuple_SET_ITEM(tuple, 0, self->errno);
	PyTuple_SET_ITEM(tuple, 1, self->strerror);

	rtnval = PyString_Format(fmt, tuple);

	Py_DECREF(fmt);
	Py_DECREF(tuple);
    }
    else
	rtnval = BaseException_str_(self);

    return rtnval;
}

static PyMemberDef EnvironmentError_members[] = {
        {"args", T_OBJECT, offsetof(EnvironmentErrorObject, args), 0, "exception arguments"},
        {"message", T_OBJECT, offsetof(EnvironmentErrorObject, message), 0, "exception message"},
        {"errno", T_OBJECT, offsetof(EnvironmentErrorObject, errno), 0, "exception code"},
        {"strerror", T_OBJECT, offsetof(EnvironmentErrorObject, strerror), 0, "exception code"},
        {"filename", T_OBJECT, offsetof(EnvironmentErrorObject, filename), 0, "exception code"},
        {NULL}  /* Sentinel */
};

ComplexExtendsException(StandardErrorType, EnvironmentError, EnvironmentError_dealloc, 0, EnvironmentError_members, EnvironmentError_init, EnvironmentError_str, "Base class for I/O related errors.");


/*
 *    IOError extends EnvironmentError
 */
SimpleExtendsException(EnvironmentErrorType, IOError, "I/O operation failed.");


/*
 *    OSError extends EnvironmentError
 */
SimpleExtendsException(EnvironmentErrorType, OSError, "OS system call failed.");


/*
 *    WindowsError extends OSError
 */
#ifdef MS_WINDOWS
#include "errmap.h"

typedef struct {
        PyObject_HEAD
        PyObject *args;
        PyObject *message;
        PyObject *errno;
        PyObject *strerror;
        PyObject *filename;
        PyObject *winerror;
} WindowsErrorObject;

static int
WindowsError_init(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *o_errcode = NULL;
	long posix_errno;

	if (EnvironmentError_init(self, args, kwds) == -1 )
            return -1;

	/* Set errno to the POSIX errno, and winerror to the Win32
	   error code. */
	errcode = PyInt_AsLong(self->errno);
	if (!errcode == -1 && PyErr_Occurred())
		goto failed;
	posix_errno = winerror_to_errno(errcode);

	self->winerror = self->errno;

	o_errcode = PyInt_FromLong(posix_errno);
	if (!o_errcode)
		goto failed;

	self->errno = o_errcode;

        return 0;
failed:
	/* Could not set errno. */
	Py_XDECREF(o_errcode);
	return -1;
}


static PyObject *
WindowsError_str(PyObject *self)
{
    PyObject *repr = NULL;
    PyObject *fmt = NULL;
    PyObject *tuple = NULL;
    PyObject *rtnval = NULL;

    if (self->filename != Py_None) {
	fmt = PyString_FromString("[Error %s] %s: %s");
	repr = PyObject_Repr(self->filename);
	if (!fmt || !repr)
	    goto finally;

	tuple = PyTuple_Pack(3, sellf->errno, self->strerror, repr);
	if (!tuple)
	    goto finally;

	rtnval = PyString_Format(fmt, tuple);
    }
    else if (PyObject_IsTrue(self->errno) && PyObject_IsTrue(self->strerror)) {
	fmt = PyString_FromString("[Error %s] %s");
	if (!fmt)
	    goto finally;

	tuple = PyTuple_Pack(2, self->errno, self->strerror);
	if (!tuple)
	    goto finally;

	rtnval = PyString_Format(fmt, tuple);
    }
    else
	rtnval = EnvironmentError_str(self);

  finally:
    Py_XDECREF(filename);
    Py_XDECREF(serrno);
    Py_XDECREF(strerror);
    Py_XDECREF(repr);
    Py_XDECREF(fmt);
    Py_XDECREF(tuple);
    return rtnval;
}

static PyMemberDef WindowsError_members[] = {
        {"args", T_OBJECT, offsetof(WindowsErrorObject, args), 0, "exception arguments"},
        {"message", T_OBJECT, offsetof(WindowsErrorObject, message), 0, "exception message"},
        {"errno", T_OBJECT, offsetof(WindowsErrorObject, errno), 0, "exception code"},
        {"strerror", T_OBJECT, offsetof(WindowsErrorObject, strerror), 0, "exception code"},
        {"filename", T_OBJECT, offsetof(WindowsErrorObject, filename), 0, "exception code"},
        {"winerror", T_OBJECT, offsetof(WindowsErrorObject, winerror), 0, "windows exception code"},
        {NULL}  /* Sentinel */
};

ComplexExtendsException(OSErrorType, WindowsError, EnvironmentError_dealloc, 0, WindowsError_members, WindowsError_init, WindowsError_str, "MS-Windows OS system call failed.");

#endif /* MS_WINDOWS */


/*
 *    VMSError extends OSError (I think)
 */
#ifdef __VMS
SimpleExtendsException(OSErrorType, VMSError, "OpenVMS OS system call failed.");
#endif


/*
 *    EOFError extends StandardError
 */
SimpleExtendsException(StandardErrorType, EOFError, "Read beyond end of file.");


/*
 *    RuntimeError extends StandardError
 */
SimpleExtendsException(StandardErrorType, RuntimeError, "Unspecified run-time error.");


/*
 *    NotImplementedError extends RuntimeError
 */
SimpleExtendsException(RuntimeErrorType, NotImplementedError, "Method or function hasn't been implemented yet.");

/*
 *    NameError extends StandardError
 */
SimpleExtendsException(StandardErrorType, NameError, "Name not found globally.");

/*
 *    UnboundLocalError extends NameError
 */
SimpleExtendsException(NameErrorType, UnboundLocalError, "Local name referenced but not bound to a value.");

/*
 *    AttributeError extends StandardError
 */
SimpleExtendsException(StandardErrorType, AttributeError, "Attribute not found.");


/*
 *    SyntaxError extends StandardError
 */

typedef struct {
        PyObject_HEAD
        PyObject *args;
        PyObject *message;
        PyObject *msg;
	PyObject *filename;
	PyObject *lineno;
	PyObject *offset;
	PyObject *text;
	PyObject *print_file_and_line;
} SyntaxErrorObject;

static int
SyntaxError_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *info = NULL;
    Py_ssize_t lenargs;

    if (BaseException_init(self, args, kwds) == -1 )
        return -1;

    self->msg = self->filename = self->lineno = self->offset =
        self->text = NULL;

    /* this is always None - yes, I know it doesn't seem to be used
       anywhere, but it was in the previous implementation */
    self->print_file_and_line = Py_None;
    Py_INCREF(Py_None);

    lenargs = PySequence_Size(args);
    if (lenargs >= 1) {
	PyObject *item0 = PySequence_GetItem(args, 0);
	if (!item0) goto finally;
	self->msg = item0;
    }
    if (lenargs == 2) {
        info = PySequence_GetItem(args, 1);
	if (!info) goto finally;
	self->filename = PySequence_GetItem(info, 0);
        if (!self->filename) goto finally;
        self->lineno = PySequence_GetItem(info, 1);
        if (!self->lineno) goto finally;
        self->offset = PySequence_GetItem(info, 2);
        if (!self->offset) goto finally;
        self->text = PySequence_GetItem(info, 3);
        if (!self->text) goto finally;
    }
    return 0;

  finally:
    Py_XDECREF(info);
    Py_XDECREF(self->msg);
    Py_XDECREF(self->filename);
    Py_XDECREF(self->lineno);
    Py_XDECREF(self->offset);
    Py_XDECREF(self->text);
    Py_XDECREF(self->print_file_and_line);
    return -1;
}


static void
SyntaxError_dealloc(SyntaxErrorObject *self)
{
    BaseException_dealloc((BaseExceptionObject *)self);
    Py_DECREF(self->code);
    Py_DECREF(self->msg);
    Py_DECREF(self->filename);
    Py_DECREF(self->lineno);
    Py_DECREF(self->offset);
    Py_DECREF(self->text);
    Py_DECREF(self->print_file_and_line);
}

/* This is called "my_basename" instead of just "basename" to avoid name
   conflicts with glibc; basename is already prototyped if _GNU_SOURCE is
   defined, and Python does define that. */
static char *
my_basename(char *name)
{
	char *cp = name;
	char *result = name;

	if (name == NULL)
		return "???";
	while (*cp != '\0') {
		if (*cp == SEP)
			result = cp + 1;
		++cp;
	}
	return result;
}


static PyObject *
SyntaxError_str(PyObject *self)
{
    PyObject *str;
    PyObject *result;

    str = PyObject_Str(self->msg);
    result = str;

    /* XXX -- do all the additional formatting with filename and
       lineno here */

    if (str != NULL && PyString_Check(str)) {
	int have_filename = 0;
	int have_lineno = 0;
	char *buffer = NULL;

        have_filename = PyString_Check(self->filename);
        have_lineno = PyInt_Check(self->lineno);

	if (have_filename || have_lineno) {
	    Py_ssize_t bufsize = PyString_GET_SIZE(str) + 64;
	    if (have_filename)
		bufsize += PyString_GET_SIZE(self->filename);

	    buffer = (char *)PyMem_MALLOC(bufsize);
	    if (buffer != NULL) {
		if (have_filename && have_lineno)
		    PyOS_snprintf(buffer, bufsize, "%s (%s, line %ld)",
                            PyString_AS_STRING(str),
                            my_basename(PyString_AS_STRING(self->filename)),
                            PyInt_AsLong(self->lineno));
		else if (have_filename)
		    PyOS_snprintf(buffer, bufsize, "%s (%s)",
                            PyString_AS_STRING(str),
                            my_basename(PyString_AS_STRING(self->filename)));
		else if (have_lineno)
		    PyOS_snprintf(buffer, bufsize, "%s (line %ld)",
                            PyString_AS_STRING(str),
                            PyInt_AsLong(self->lineno));

		result = PyString_FromString(buffer);
		PyMem_FREE(buffer);

		if (result == NULL)
		    result = str;
		else
		    Py_DECREF(str);
	    }
	}
    }
    return result;
}

ComplexExtendsException(StandardErrorType, SyntaxError, SyntaxError_dealloc, 0, SyntaxError_members, SyntaxError_init, SyntaxError_str, "Invalid syntax.");


/* WE'RE HALF WAY! WOO! */

