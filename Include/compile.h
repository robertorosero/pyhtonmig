#ifndef Py_COMPILE_H
#define Py_COMPILE_H
#ifdef __cplusplus
//extern "C" {
#endif

/* Public interface */
struct _node; /* Declare the existence of this type */
DL_IMPORT(PyCodeObject *) PyNode_Compile(struct _node *, char *);

/* Future feature support */

typedef struct {
    int ff_found_docstring;
    int ff_last_lineno;
    int ff_features;
} PyFutureFeatures;

#define FUTURE_NESTED_SCOPES "nested_scopes"
#define FUTURE_GENERATORS "generators"
#define FUTURE_DIVISION "division"

struct _mod; /* Declare the existence of this type */
DL_IMPORT(PyCodeObject *) PyAST_Compile(struct _mod *, const char *,
					PyCompilerFlags *);
DL_IMPORT(PyFutureFeatures *) PyFuture_FromAST(struct _mod *, const char *);

#define DEFAULT_BLOCK_SIZE 32
#define DEFAULT_BLOCKS 8

struct instr {
	int i_opcode;
	int i_oparg;
	PyObject *i_arg;
};

struct basicblock {
	size_t b_iused;
	size_t b_ialloc;
	int next;
	struct instr b_instr[DEFAULT_BLOCK_SIZE];
};

#ifdef __cplusplus
}
#endif
#endif /* !Py_COMPILE_H */
