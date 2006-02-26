#ifndef Py_AST_H
#define Py_AST_H
#ifdef __cplusplus
extern "C" {
#endif

PyAPI_FUNC(PyObject*) PyAST_FromNode(const node *, PyCompilerFlags *flags,
				  const char *);

/* Singletons for load contexts */
extern PyObject *_PyAST_Load, *_PyAST_Store, *_PyAST_Del, 
		*_PyAST_AugLoad, *_PyAST_AugStore, *_PyAST_Param;
int _PyAST_Init(void);

#ifdef __cplusplus
}
#endif
#endif /* !Py_AST_H */
