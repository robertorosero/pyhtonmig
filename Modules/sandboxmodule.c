#include "Python.h"

#ifdef __cplusplus
extern "C" {
#endif

PyMODINIT_FUNC
initsandbox(void)
{
    PyObject *module;

    module = Py_InitModule3("sandbox", NULL,
	    "Provide a sandbox to safely execute Python code.");
    if (module == NULL)
	return;
}

#ifdef __cplusplus
}
#endif
