
#ifndef Py_COMPILE_H
#define Py_COMPILE_H

#include "code.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Public interface */
struct _node; /* Declare the existence of this type */
PyAPI_FUNC(PyCodeObject *) PyNode_Compile(struct _node *, const char *);

/* Future feature support */

typedef struct {
    int ff_features;      /* flags set by future statements */
    int ff_lineno;        /* line number of last future statement */
} PyFutureFeatures;

enum PyCompilationMode {
    PyCompilationMode_Exec_Module = 0,
    PyCompilationMode_Eval_Expression = 1,
    PyCompilationMode_Single_Interactive = 2,
};

#define FUTURE_NESTED_SCOPES "nested_scopes"
#define FUTURE_GENERATORS "generators"
#define FUTURE_DIVISION "division"
#define FUTURE_ABSOLUTE_IMPORT "absolute_import"
#define FUTURE_WITH_STATEMENT "with_statement"
#define FUTURE_PRINT_FUNCTION "print_function"
#define FUTURE_UNICODE_LITERALS "unicode_literals"
#define FUTURE_BARRY_AS_BDFL "barry_as_FLUFL"

struct _mod; /* Declare the existence of this type */
PyAPI_FUNC(enum PyCompilationMode) PyAST_CompilationModeFromStartToken(int);

PyAPI_FUNC(PyCodeObject *) PyAST_Compile(struct _mod *, const char *,
                                         PyCompilerFlags *, PyArena *, enum PyCompilationMode);
PyAPI_FUNC(PyFutureFeatures *) PyFuture_FromAST(struct _mod *, const char *);

PyAPI_FUNC(void) _PyOptimizer_Init(void);


#ifdef __cplusplus
}
#endif
#endif /* !Py_COMPILE_H */
