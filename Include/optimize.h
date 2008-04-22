#ifndef Py_OPTIMIZE_H
#define Py_OPTIMIZE_H

#ifdef __cplusplus
extern "C" {
#endif

PyAPI_FUNC(int) PyAST_Optimize(mod_ty* mod_ptr, PyArena* arena);

#ifdef __cplusplus
};
#endif

#endif

