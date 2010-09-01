
/* Support for dynamic loading of extension modules */

#include "Python.h"

/* ./configure sets HAVE_DYNAMIC_LOADING if dynamic loading of modules is
   supported on this platform. configure will then compile and link in one
   of the dynload_*.c files, as appropriate. We will call a function in
   those modules to get a function pointer to the module's init function.
*/
#ifdef HAVE_DYNAMIC_LOADING

#include "importdl.h"

extern dl_funcptr _PyImport_GetDynLoadFunc(const char *name,
                                           const char *shortname,
                                           const char *pathname, FILE *fp);



PyObject *
_PyImport_LoadDynamicModule(char *name, PyObject *path, FILE *fp)
{
    PyObject *m;
    PyObject *pathbytes;
    char *lastdot, *shortname, *packagecontext, *oldcontext;
    dl_funcptr p0;
    PyObject* (*p)(void);
    struct PyModuleDef *def;

    if ((m = _PyImport_FindExtension(name, path)) != NULL) {
        Py_INCREF(m);
        return m;
    }
    lastdot = strrchr(name, '.');
    if (lastdot == NULL) {
        packagecontext = NULL;
        shortname = name;
    }
    else {
        packagecontext = name;
        shortname = lastdot+1;
    }

    /* FIXME: pass path, not pathname, at least to the Windows implementation */
    pathbytes = PyUnicode_EncodeFSDefault(path);
    if (pathbytes == NULL)
        return NULL;
    p0 = _PyImport_GetDynLoadFunc(name, shortname, PyBytes_AsString(pathbytes), fp);
    Py_DECREF(pathbytes);
    p = (PyObject*(*)(void))p0;
    if (PyErr_Occurred())
        return NULL;
    if (p == NULL) {
        PyErr_Format(PyExc_ImportError,
           "dynamic module does not define init function (PyInit_%.200s)",
                     shortname);
        return NULL;
    }
    oldcontext = _Py_PackageContext;
    _Py_PackageContext = packagecontext;
    m = (*p)();
    _Py_PackageContext = oldcontext;
    if (m == NULL)
        return NULL;

    if (PyErr_Occurred()) {
        Py_DECREF(m);
        PyErr_Format(PyExc_SystemError,
                     "initialization of %s raised unreported exception",
                     shortname);
        return NULL;
    }

    /* Remember pointer to module init function. */
    def = PyModule_GetDef(m);
    def->m_base.m_init = p;

    /* Remember the filename as the __file__ attribute */
    Py_INCREF(path);
    if (PyModule_AddObject(m, "__file__", path) < 0) {
        PyErr_Clear(); /* Not important enough to report */
        Py_DECREF(path);
    }

    if (_PyImport_FixupExtension(m, name, path) < 0)
        return NULL;
    if (Py_VerboseFlag)
        PySys_FormatStderr(
            "import %s # dynamically loaded from %U\n",
            name, path);
    return m;
}

#endif /* HAVE_DYNAMIC_LOADING */
