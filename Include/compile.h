#ifndef Py_COMPILE_H
#define Py_COMPILE_H
#ifdef __cplusplus
//extern "C" {
#endif

/* Public interface */
struct _node; /* Declare the existence of this type */
PyAPI_FUNC(PyCodeObject *) PyNode_Compile(struct _node *, const char *);
PyAPI_FUNC(PyCodeObject *) PyCode_New(
	int, int, int, int, PyObject *, PyObject *, PyObject *, PyObject *,
	PyObject *, PyObject *, PyObject *, PyObject *, int, PyObject *); 
        /* same as struct above */
PyAPI_FUNC(int) PyCode_Addr2Line(PyCodeObject *, int);

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

#define DEFAULT_BLOCK_SIZE 16
#define DEFAULT_BLOCKS 8
#define DEFAULT_CODE_SIZE 128
#define DEFAULT_LNOTAB_SIZE 16

struct instr {
	int i_jabs : 1;
	int i_jrel : 1;
	int i_hasarg : 1;
	unsigned char i_opcode;
	int i_oparg;
	int i_lineno;
};

struct basicblock {
	int b_iused;
	int b_ialloc;
	/* If b_next is non-zero, it is the block id of the next
	   block reached by normal control flow.
	   Since a valid b_next must always be > 0,
	   0 can be reserved to mean no next block. */
	int b_next;
	/* b_seen is used to perform a DFS of basicblocks. */
	int b_seen : 1;
	/* b_return is true if a RETURN_VALUE opcode is inserted. */
	int b_return : 1;
	struct instr b_instr[DEFAULT_BLOCK_SIZE];
};

#ifdef __cplusplus
}
#endif
#endif /* !Py_COMPILE_H */
