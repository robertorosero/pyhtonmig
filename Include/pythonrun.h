
/* Interfaces to parse and execute pieces of python code */

#ifndef Py_PYTHONRUN_H
#define Py_PYTHONRUN_H
#ifdef __cplusplus
extern "C" {
#endif

#define PyCF_MASK (CO_FUTURE_DIVISION)
#define PyCF_MASK_OBSOLETE (CO_GENERATOR_ALLOWED | CO_NESTED)

typedef struct {
	int cf_flags;  /* bitmask of CO_xxx flags relevant to future */
} PyCompilerFlags;

DL_IMPORT(void) Py_SetProgramName(char *);
DL_IMPORT(char *) Py_GetProgramName(void);

DL_IMPORT(void) Py_SetPythonHome(char *);
DL_IMPORT(char *) Py_GetPythonHome(void);

DL_IMPORT(void) Py_Initialize(void);
DL_IMPORT(void) Py_Finalize(void);
DL_IMPORT(int) Py_IsInitialized(void);
DL_IMPORT(PyThreadState *) Py_NewInterpreter(void);
DL_IMPORT(void) Py_EndInterpreter(PyThreadState *);

DL_IMPORT(int) PyRun_AnyFileFlags(FILE *, char *, PyCompilerFlags *);
DL_IMPORT(int) PyRun_AnyFileExFlags(FILE *, char *, int, PyCompilerFlags *);
DL_IMPORT(int) PyRun_SimpleStringFlags(char *, PyCompilerFlags *);
DL_IMPORT(int) PyRun_SimpleFileExFlags(FILE *, char *, int, PyCompilerFlags *);
DL_IMPORT(int) PyRun_InteractiveOneFlags(FILE *, char *, PyCompilerFlags *);
DL_IMPORT(int) PyRun_InteractiveLoopFlags(FILE *, char *, PyCompilerFlags *);

DL_IMPORT(struct _mod *) PyParser_ASTFromString(const char *, const char *, 
						int, int);
DL_IMPORT(struct _mod *) PyParser_ASTFromFile(FILE *, const char *, int, 
					      char *, char *, int);
DL_IMPORT(struct _node *) PyParser_SimpleParseString(char *, int);
DL_IMPORT(struct _node *) PyParser_SimpleParseFile(FILE *, char *, int);
DL_IMPORT(struct _node *) PyParser_SimpleParseStringFlags(const char *, int, 
							  int);
DL_IMPORT(struct _node *) PyParser_SimpleParseFileFlags(FILE *, char *,
							int, int);

DL_IMPORT(PyObject *) PyRun_StringFlags(char *, int, PyObject *, PyObject *,
					PyCompilerFlags *);

DL_IMPORT(PyObject *) PyRun_FileExFlags(FILE *, char *, int, PyObject *, 
					PyObject *, int, PyCompilerFlags *);

#define Py_CompileString(str, p, s) Py_CompileStringFlags(str, p, s, NULL)
DL_IMPORT(PyObject *) Py_CompileStringFlags(char *, char *, int,
					    PyCompilerFlags *);
DL_IMPORT(struct symtable *) Py_SymtableString(char *, char *, int);

DL_IMPORT(void) PyErr_Print(void);
DL_IMPORT(void) PyErr_PrintEx(int);
DL_IMPORT(void) PyErr_Display(PyObject *, PyObject *, PyObject *);

DL_IMPORT(int) Py_AtExit(void (*func)(void));

DL_IMPORT(void) Py_Exit(int);

DL_IMPORT(int) Py_FdIsInteractive(FILE *, char *);

/* Use macros for a bunch of old variants */
#define PyRun_String(str, s, g, l) PyRun_StringFlags(str, s, g, l, NULL)
#define PyRun_FileEx(f, p, s, g, l, c) \
        PyRun_FileExFlags(f, p, s, g, l, c, NULL)
#define PyRun_FileFlags(f, p, s, g, l, flags) \
        PyRun_FileExFlags(f, p, s, g, l, 0, flags)
#define PyRun_AnyFile(fp, name) PyRun_AnyFileExFlags(fp, name, 0, NULL)
#define PyRun_AnyFileEx(fp, name, closeit) \
	PyRun_AnyFileExFlags(fp, name, closeit, NULL)
#define PyRun_AnyFileFlags(fp, name, flags) \
	PyRun_AnyFileExFlags(fp, name, 0, flags)
#define PyRun_SimpleString(s, f) PyRunSimpleStringFlags(s, f, NULL)
#define PyRun_SimpleFile(f, p) PyRun_SimpleFileExFlags(f, p, 0, NULL)
#define PyRun_SimpleFileEx(f, p, c) PyRun_SimpleFileExFlags(f, p, c, NULL)
#define PyRun_InteractiveOne(f, p) PyRun_InteractiveOneFlags(f, p, NULL)
#define PyRun_InteractiveLoop(f, p) PyRun_InteractiveLoopFlags(f, p, NULL)
#define PyRun_File(fp, p, s, g, l) \
        PyRun_FileExFlags(fp, p, s, g, l, 0, NULL)
#define PyRun_FileEx(fp, p, s, g, l, c) \
        PyRun_FileExFlags(fp, p, s, g, l, c, NULL)
#define PyRun_FileFlags(fp, p, s, g, l, flags) \
        PyRun_FileExFlags(fp, p, s, g, l, 0, flags)

/* In getpath.c */
DL_IMPORT(char *) Py_GetProgramFullPath(void);
DL_IMPORT(char *) Py_GetPrefix(void);
DL_IMPORT(char *) Py_GetExecPrefix(void);
DL_IMPORT(char *) Py_GetPath(void);

/* In their own files */
DL_IMPORT(const char *) Py_GetVersion(void);
DL_IMPORT(const char *) Py_GetPlatform(void);
DL_IMPORT(const char *) Py_GetCopyright(void);
DL_IMPORT(const char *) Py_GetCompiler(void);
DL_IMPORT(const char *) Py_GetBuildInfo(void);

/* Internal -- various one-time initializations */
DL_IMPORT(PyObject *) _PyBuiltin_Init(void);
DL_IMPORT(PyObject *) _PySys_Init(void);
DL_IMPORT(void) _PyImport_Init(void);
DL_IMPORT(void) _PyExc_Init(void);

/* Various internal finalizers */
DL_IMPORT(void) _PyExc_Fini(void);
DL_IMPORT(void) _PyImport_Fini(void);
DL_IMPORT(void) PyMethod_Fini(void);
DL_IMPORT(void) PyFrame_Fini(void);
DL_IMPORT(void) PyCFunction_Fini(void);
DL_IMPORT(void) PyTuple_Fini(void);
DL_IMPORT(void) PyString_Fini(void);
DL_IMPORT(void) PyInt_Fini(void);
DL_IMPORT(void) PyFloat_Fini(void);
DL_IMPORT(void) PyOS_FiniInterrupts(void);

/* Stuff with no proper home (yet) */
DL_IMPORT(char *) PyOS_Readline(char *);
extern DL_IMPORT(int) (*PyOS_InputHook)(void);
extern DL_IMPORT(char) *(*PyOS_ReadlineFunctionPointer)(char *);

/* Stack size, in "pointers" (so we get extra safety margins
   on 64-bit platforms).  On a 32-bit platform, this translates
   to a 8k margin. */
#define PYOS_STACK_MARGIN 2048

#if defined(WIN32) && !defined(MS_WIN64) && defined(_MSC_VER)
/* Enable stack checking under Microsoft C */
#define USE_STACKCHECK
#endif

#ifdef USE_STACKCHECK
/* Check that we aren't overflowing our stack */
DL_IMPORT(int) PyOS_CheckStack(void);
#endif

/* Signals */
typedef void (*PyOS_sighandler_t)(int);
DL_IMPORT(PyOS_sighandler_t) PyOS_getsig(int);
DL_IMPORT(PyOS_sighandler_t) PyOS_setsig(int, PyOS_sighandler_t);


#ifdef __cplusplus
}
#endif
#endif /* !Py_PYTHONRUN_H */
