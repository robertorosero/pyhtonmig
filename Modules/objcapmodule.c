/*
    Store changes done to implement object-capabilities.
*/

#include "Python.h"

PyDoc_STRVAR(module_doc,
"XXX Placeholder for code removed for object-capabilities reasons.\n\
");


/*
   Return the subclasses of a class.

   Moved so that object does not expose *all* new-style classes to *every*
   interpreter.  Otherwise would invert direction of knowledge about
   inheritance tree.
*/
static PyObject *
object_subclasses(PyObject* self, PyTypeObject *type)
{
    PyObject *list, *raw, *ref;
    Py_ssize_t i, n;

    if (!PyType_Check(type)) {
	PyErr_SetString(PyExc_TypeError,
			    "argument must be a type or subclass thereof");
	return NULL;
    }
    list = PyList_New(0);
    if (list == NULL)
	return NULL;
    raw = type->tp_subclasses;
    if (raw == NULL)
	return list;
    assert(PyList_Check(raw));
    n = PyList_GET_SIZE(raw);
    for (i = 0; i < n; i++) {
	ref = PyList_GET_ITEM(raw, i);
	assert(PyWeakref_CheckRef(ref));
	ref = PyWeakref_GET_OBJECT(ref);
	if (ref != Py_None) {
	    if (PyList_Append(list, ref) < 0) {
		Py_DECREF(list);
		return NULL;
	    }
	}
    }
    return list;
}

PyDoc_STRVAR(object_subclass_doc,
"subclasses(object) -> return a list of subclasses.\n\
Originally object.__subclasses__().");

/*
   Initialize a file object.

   Need to strip out file instance and then pass remaining arguments to
   _PyFile_Init().
*/
static PyObject *
file_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *file_ins = NULL;
    static char *kwlist[] = {"file_instance", "name", "mode", "buffering", 0};
    PyObject *name = NULL;
    PyObject *mode = NULL;
    PyObject *buffering = NULL;
    PyObject *init_args = NULL;
    Py_ssize_t arg_count = 1;
    Py_ssize_t arg_pos = 0;
    int ret;

    /* Unpack all arguments. */
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO|OO:file_init", kwlist,
		&file_ins, &name, &mode, &buffering))
	return NULL;

    /* Figure out how many arguments to _PyFile_Init() we have. */
    if (mode)
	arg_count += 1;
    if (buffering)
	arg_count += 1;

    /* Construct a new argument tuple for _PyFile_Init() sans file instance. */
    init_args = PyTuple_New(arg_count);
    PyTuple_SET_ITEM(init_args, arg_pos++, name);
    Py_INCREF(name);
    if (mode) {
	Py_INCREF(mode);
	PyTuple_SET_ITEM(init_args, arg_pos++, mode);
    }
    if (buffering) {
	Py_INCREF(buffering);
	PyTuple_SET_ITEM(init_args, arg_pos++, buffering);
    }
    
    /* Initialize file instance. */
    Py_INCREF(file_ins);
    ret = _PyFile_Init(file_ins, init_args, NULL);
    Py_DECREF(file_ins);
    Py_DECREF(init_args);

    if (ret < 0)
	return NULL;

    Py_RETURN_NONE;
}


PyDoc_VAR(file_init_doc) =
PyDoc_STR(
"file_init(file_instance, name[, mode[, buffering]]) -> None\n"
"\n"
"Initialize a file object.  The mode can be 'r', 'w' or 'a' for reading (default),\n"
"writing or appending.  The file itself will be created if it doesn't exist\n"
"when opened for writing or appending; it will be truncated when\n"
"opened for writing.  Add a 'b' to the mode for binary files.\n"
"Add a '+' to the mode to allow simultaneous reading and writing.\n"
"If the buffering argument is given, 0 means unbuffered, 1 means line\n"
"buffered, and larger numbers specify the buffer size.\n"
)
PyDoc_STR(
"Add a 'U' to mode to open the file for input with universal newline\n"
"support.  Any line ending in the input file will be seen as a '\\n'\n"
"in Python.  Also, a file so opened gains the attribute 'newlines';\n"
"the value for this attribute is one of None (no newline read yet),\n"
"'\\r', '\\n', '\\r\\n' or a tuple containing all the newline types seen.\n"
"\n"
"'U' cannot be combined with 'w' or '+' mode.\n"
);


/* Helper for code_new: return a shallow copy of a tuple that is
   guaranteed to contain exact strings, by converting string subclasses
   to exact strings and complaining if a non-string is found. */
static PyObject*
validate_and_copy_tuple(PyObject *tup)
{
	PyObject *newtuple;
	PyObject *item;
	Py_ssize_t i, len;

	len = PyTuple_GET_SIZE(tup);
	newtuple = PyTuple_New(len);
	if (newtuple == NULL)
		return NULL;

	for (i = 0; i < len; i++) {
		item = PyTuple_GET_ITEM(tup, i);
		if (PyString_CheckExact(item)) {
			Py_INCREF(item);
		}
		else if (!PyString_Check(item)) {
			PyErr_Format(
				PyExc_TypeError,
				"name tuples must contain only "
				"strings, not '%.500s'",
				item->ob_type->tp_name);
			Py_DECREF(newtuple);
			return NULL;
		}
		else {
			item = PyString_FromStringAndSize(
				PyString_AS_STRING(item),
				PyString_GET_SIZE(item));
			if (item == NULL) {
				Py_DECREF(newtuple);
				return NULL;
			}
		}
		PyTuple_SET_ITEM(newtuple, i, item);
	}

	return newtuple;
}

PyDoc_STRVAR(code_doc,
"code(argcount, nlocals, stacksize, flags, codestring, constants, names,\n\
      varnames, filename, name, firstlineno, lnotab[, freevars[, cellvars]])\n\
\n\
Create a code object.  Not for the faint of heart.");

static PyObject *
code_new(PyTypeObject *type, PyObject *args, PyObject *kw)
{
	int argcount;
	int nlocals;
	int stacksize;
	int flags;
	PyObject *co = NULL;
	PyObject *code;
	PyObject *consts;
	PyObject *names, *ournames = NULL;
	PyObject *varnames, *ourvarnames = NULL;
	PyObject *freevars = NULL, *ourfreevars = NULL;
	PyObject *cellvars = NULL, *ourcellvars = NULL;
	PyObject *filename;
	PyObject *name;
	int firstlineno;
	PyObject *lnotab;

	if (!PyArg_ParseTuple(args, "iiiiSO!O!O!SSiS|O!O!:code",
			      &argcount, &nlocals, &stacksize, &flags,
			      &code,
			      &PyTuple_Type, &consts,
			      &PyTuple_Type, &names,
			      &PyTuple_Type, &varnames,
			      &filename, &name,
			      &firstlineno, &lnotab,
			      &PyTuple_Type, &freevars,
			      &PyTuple_Type, &cellvars))
		return NULL;

	if (argcount < 0) {
		PyErr_SetString(
			PyExc_ValueError,
			"code: argcount must not be negative");
		goto cleanup;
	}

	if (nlocals < 0) {
		PyErr_SetString(
			PyExc_ValueError,
			"code: nlocals must not be negative");
		goto cleanup;
	}

	ournames = validate_and_copy_tuple(names);
	if (ournames == NULL)
		goto cleanup;
	ourvarnames = validate_and_copy_tuple(varnames);
	if (ourvarnames == NULL)
		goto cleanup;
	if (freevars)
		ourfreevars = validate_and_copy_tuple(freevars);
	else
		ourfreevars = PyTuple_New(0);
	if (ourfreevars == NULL)
		goto cleanup;
	if (cellvars)
		ourcellvars = validate_and_copy_tuple(cellvars);
	else
		ourcellvars = PyTuple_New(0);
	if (ourcellvars == NULL)
		goto cleanup;

	co = (PyObject *)PyCode_New(argcount, nlocals, stacksize, flags,
				    code, consts, ournames, ourvarnames,
				    ourfreevars, ourcellvars, filename,
				    name, firstlineno, lnotab);
  cleanup:
	Py_XDECREF(ournames);
	Py_XDECREF(ourvarnames);
	Py_XDECREF(ourfreevars);
	Py_XDECREF(ourcellvars);
	return co;
}



static PyMethodDef module_methods[] = {
    {"subclasses", (PyCFunction)object_subclasses, METH_O, object_subclass_doc},
    {"file_init", (PyCFunction)file_init, METH_VARARGS | METH_KEYWORDS,
	file_init_doc},
    {"code_new", (PyCFunction)code_new, METH_VARARGS, code_doc},
    {NULL, NULL}
};

PyMODINIT_FUNC
initobjcap(void)
{
    PyObject *module;

    module = Py_InitModule3("objcap", module_methods, module_doc);
    if (!module)
	return;
}
