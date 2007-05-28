/*
   Proof-of-concept application that embeds Python with security features
   turned on to prevent unmitigated access to resources.

   XXX See BRANCH_NOTES for what needs to be done.
*/
#include "Python.h"

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
    PyObject *module_names_list;
    PyObject *hidden_modules;
    PyObject *import_module;
    PyObject *import_callable;
    Py_ssize_t safe_builtins_count = 7;
    const char *safe_builtins_names[] = {"_ast", "_codecs", "_sre",
					  "_symtable", "_types", "errno",
					  "exceptions"};
    Py_ssize_t safe_frozen_count = 0;
    const char *safe_frozen_names[] = {};
    PyObject *safe_builtins_seq;
    PyObject *safe_frozen_seq;
    Py_ssize_t safe_extensions_count = 5;
    const char *safe_extensions_names[] = {"binascii", "cmath", "math",
					    "operator", "time"};
    PyObject *safe_extensions_seq;

    /* Initialize interpreter.  */
    Py_Initialize();

    /* Create lists of modules safe to import. */
    CREATE_SAFE_LIST(builtins);
    CREATE_SAFE_LIST(frozen);
    CREATE_SAFE_LIST(extensions);

    interp = PyThreadState_GET()->interp;

    /* Get importer from importlib. */
    import_module = PyImport_ImportModule("controlled_importlib");
    if (!import_module)
	    return 1;

    import_callable = PyObject_CallMethod(import_module,
		    "ControlledImport", "(O, O, O)",
		    safe_builtins_seq, safe_frozen_seq, safe_extensions_seq);
    if (!import_callable)
	    return 1;

    /* Store importlib importer somewhere. */
    PyDict_SetItemString(interp->sysdict, "import_", import_callable);
    /* Set __import__ to the import delegate defined in 'sys'. */
    PyDict_SetItemString(interp->builtins, "__import__",
		    PyDict_GetItemString(interp->sysdict,
			    "import_delegate"));

    Py_DECREF(import_module);
    Py_DECREF(import_callable);

    /* Clear out sys.modules.
       Some modules must be kept around in order for Python to function
       properly.

       * __builtin__
	   Lose this and Python will not run.
       * __main__
	   Current scope of execution.
       * exceptions
	   Safe to keep around.
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
    module_count = PyList_GET_SIZE(module_names_list);
    hidden_modules = PyDict_New();
    for (x=0; x < module_count; x+=1) {
	    char *module_name =
		    PyString_AS_STRING(
				    PyList_GET_ITEM(module_names_list, x));
	    /* Modules that *must* stay visible. */
	    if ((strcmp(module_name, "__builtin__") == 0) ||
			    (strcmp(module_name, "__main__") == 0) ||
			    (strcmp(module_name, "exceptions") == 0) ||
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
		    PyDict_SetItemString(hidden_modules, module_name,
				    module);
		    PyDict_DelItemString(interp->modules, module_name);
	    }
    }
    /* Store away modules that must stick around but should not be exposed;
       this is done for importlib's benefit.
       */
    PyDict_SetItemString(interp->modules, ".hidden", hidden_modules);

    PyDict_SetItemString(interp->sysdict, "modules", interp->modules);

    /* Clear out sys.path_importer_cache. */
    PyDict_Clear(PyDict_GetItemString(interp->sysdict,
			    "path_importer_cache"));

    /* Remove dangerous built-ins. */
    PyDict_DelItemString(interp->builtins, "execfile");
    PyDict_DelItemString(interp->builtins, "open");

  /* Use interpreter. */
    return_val = Py_Main(argc, argv);
  /* Tear down interpreter. */
    Py_Finalize();

    return return_val;
}
