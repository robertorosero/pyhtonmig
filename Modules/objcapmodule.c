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


static PyMethodDef module_methods[] = {
    {"subclasses", (PyCFunction)object_subclasses, METH_O, "XXX"},
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
