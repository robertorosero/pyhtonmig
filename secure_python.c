/*
   Proof-of-concept application that embeds Python with security features
   turned on to prevent unmitigated access to resources.
*/
#include "Python.h"

/* XXX Add error checking. */
#define CREATE_SAFE_LIST(kind)  \
	safe_##kind##_seq = PyTuple_New(safe_##kind##_count); \
        for (x = 0; x < safe_##kind##_count; x += 1) { \
		PyObject *module_name = \
			PyString_FromString(safe_##kind##_names[x]); \
		PyTuple_SetItem(safe_##kind##_seq, x, module_name); \
	}

extern PyObject *PyModule_GetWarningsModule(void);

int
main(int argc, char *argv[])
{
    int return_val;
    PyInterpreterState *interp;
    Py_ssize_t module_count, x;
    PyObject *obj;
    PyObject *module_names_list;
    PyObject *hidden_modules;
    PyObject *import_module;
    PyObject *import_callable;
    Py_ssize_t safe_builtins_count = 6;
    /* All whitelisted modules should be imported in the proper test file. */
    const char *safe_builtins_names[] = {"_ast", "_codecs", "_sre",
					  "_symtable", "_types", "errno"};
    Py_ssize_t safe_frozen_count = 0;
    const char *safe_frozen_names[] = {};
    PyObject *safe_builtins_seq;
    PyObject *safe_frozen_seq;
    Py_ssize_t safe_extensions_count = 19;
    /* All whitelisted modules should be imported in the proper test file. */
    const char *safe_extensions_names[] = {"_bisect", "_collections", "_csv",
					   "_functools", "_hashlib",
					   "_heapq", "_random",
					   "_struct", "_weakref",
					   "array",
					   "binascii", "cmath",
					   "datetime",
					   "itertools",
					   "math",
					   "operator",
					   "time", "unicodedata", "zlib"};
    PyObject *safe_extensions_seq;

    /* Initialize interpreter.  */
    Py_SetProgramName("secure_python.exe");
    Py_Initialize();

    interp = PyThreadState_GET()->interp;

    /* Clear sys.meta_path and sys.path_hooks.
       This needs to be done before importlib is called as it sets values in
       both attributes. */
    obj = PyDict_GetItemString(interp->sysdict, "meta_path");
    if (PyErr_Occurred()) {
	    fprintf(stderr, "Fetching sys.meta_path failed.\n");
	    return 1;
    }
    x = PyList_Size(obj);
    if (PyList_SetSlice(obj, 0, x, NULL) == -1) {
	    fprintf(stderr, "Clearing sys.meta_path failed.\n");
	    return 1;
    }

    obj = PyDict_GetItemString(interp->sysdict, "path_hooks");
    if (PyErr_Occurred()) {
	    fprintf(stderr, "Fetching sys.path_hooks failed.\n");
	    return 1;
    }
    x = PyList_Size(obj);
    if (PyList_SetSlice(obj, 0, x, NULL) == -1) {
	    fprintf(stderr, "Clearing sys.path_hooks failed.\n");
	    return 1;
    }

    /* Create lists of modules safe to import. */
    CREATE_SAFE_LIST(builtins);
    CREATE_SAFE_LIST(frozen);
    CREATE_SAFE_LIST(extensions);


    /* Get importer from importlib. */
    import_module = PyImport_ImportModule("controlled_importlib");
    if (!import_module) {
	    fprintf(stderr, "Import of controlled_importlib failed.\n");
	    return 1;
    }

    import_callable = PyObject_CallMethod(import_module,
		    "ControlledImport", "(O, O, O)",
		    safe_builtins_seq, safe_frozen_seq, safe_extensions_seq);
    if (!import_callable) {
	    fprintf(stderr, "Instantiating "
			    "controlled_importlib.ControlledImport failed.\n");
	    return 1;
    }

    /* Store importlib importer somewhere. */
    if (PyDict_SetItemString(interp->sysdict, "import_", import_callable) ==
		    -1) {
	    fprintf(stderr, "Setting sys.import_ failed.\n");
	    return 1;
    }
    /* Set __import__ to the import delegate defined in 'sys'. */
    if (PyDict_SetItemString(interp->builtins, "__import__",
		    PyDict_GetItemString(interp->sysdict,
			    "import_delegate")) == -1) {
	    fprintf(stderr, "Resetting __import__ failed.\n");
	    return 1;
    }

    Py_DECREF(import_module);
    Py_DECREF(import_callable);

    /* Clear out sys.modules.
       Some modules must be kept around in order for Python to function
       properly.

       * __builtin__
	   Lose this and Python will not run.
       * __main__
	   Current scope of execution.
       * encodings
	   Does dynamic import of encodings which requires globals() to
	   work; globals() fails when the module has been deleted.  Also
	   fails if you hide module because importing of submodules for
	   encodings no longer has the parent package.
       * codecs
	   Incremental codecs fail.
       * _codecs
	   Exposed by codecs.
       * warnings (cache in C code)
	   Warnings reset otherwise.  Requires 'sys' module to work.

       All other modules are kept in the '.hidden' dict in sys.modules for
       use by importlib.
     */
    /* Get the 'warnings' module cached away at the C level. */
    PyModule_GetWarningsModule();
    module_names_list = PyDict_Keys(interp->modules);
    if (!module_names_list) {
	    fprintf(stderr, "sys.modules.keys() failed.\n");
	    return 1;
    }
    module_count = PyList_GET_SIZE(module_names_list);
    hidden_modules = PyDict_New();
    if (!hidden_modules) {
	    fprintf(stderr, "Creating an empty dict for .hidden failed.\n");
	    return 1;
    }
    for (x=0; x < module_count; x+=1) {
	    char *module_name =
		    PyString_AS_STRING(
				    PyList_GET_ITEM(module_names_list, x));
	    /* Modules that *must* stay visible. */
	    if ((strcmp(module_name, "__builtin__") == 0) ||
			    (strcmp(module_name, "__main__") == 0) ||
			    (strcmp(module_name, "encodings") == 0) ||
			    (strcmp(module_name, "codecs") == 0) ||
			    (strcmp(module_name, "_codecs") == 0)) {
		    continue;
	    }
	    /* All other modules must be stored away for importlib. */
	    else {
		    PyObject *module =
			    PyDict_GetItemString(interp->modules,
					    module_name);
		    if (PyErr_Occurred()) {
			    fprintf(stderr, "Fetching %s from sys.modules "
					    "failed.\n", module_name);
			    return 1;
		    }
		    if (PyDict_SetItemString(hidden_modules, module_name,
				    module) == -1) {
			    fprintf(stderr, "Adding %s to .hidden failed.\n",
					    module_name);
			    return 1;
		    }
		    if (PyDict_DelItemString(interp->modules, module_name) ==
				    -1) {
			    fprintf(stderr, "Removing %s from sys.modules "
					    "failed.\n", module_name);
			    return 1;
		    }
	    }
    }
    /* Store away modules that must stick around but should not be exposed;
       this is done for importlib's benefit.
       */
    if (PyDict_SetItemString(interp->modules, ".hidden", hidden_modules) == -1)
    {
	    fprintf(stderr, "Adding .hidden to sys.modules failed.\n");
	    return 1;
    }

    //PyDict_SetItemString(interp->sysdict, "modules", interp->modules);

    /* Clear out sys.path_importer_cache. */
    PyDict_Clear(PyDict_GetItemString(interp->sysdict,
			    "path_importer_cache"));
    if (PyErr_Occurred()) {
	    fprintf(stderr, "An occurred while clearing "
			    "sys.path_importer_cache.\n");
	    return 1;
    }

    /* Remove dangerous built-ins. */
    if (PyDict_DelItemString(interp->builtins, "execfile") == -1) {
	    fprintf(stderr, "Removing execfile failed.\n");
	    return 1;
    }
    if (PyDict_DelItemString(interp->builtins, "open") == -1) {
	    fprintf(stderr, "Removing 'open' failed.\n");
	    return 1;
    }
    if (PyDict_DelItemString(interp->builtins, "SystemExit") == -1) {
	    fprintf(stderr, "Removing SystemExit failed.\n");
	    return 1;
    }

  /* Use interpreter. */
    return_val = Py_Main(argc, argv);
  /* Tear down interpreter. */
    Py_Finalize();

    return return_val;
}
