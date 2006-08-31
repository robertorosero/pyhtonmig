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



static PyMethodDef module_methods[] = {
    {"subclasses", (PyCFunction)object_subclasses, METH_O, object_subclass_doc},
    {"file_init", (PyCFunction)file_init, METH_VARARGS | METH_KEYWORDS,
	file_init_doc},
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
