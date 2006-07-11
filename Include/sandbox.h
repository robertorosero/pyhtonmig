#ifdef PySandbox_SUPPORTED

#ifndef Py_SANDBOX_H
#define Py_SANDBOX_H
#ifdef __cplusplus
extern "C" {
#endif

struct _sandbox_state;  /* Forward */

typedef struct _sandbox_state {
    /* The memory cap and current usage. */
    size_t mem_cap;
    size_t mem_usage;

} PySandboxState;


/* Return the sandbox state struct. */
#define _PySandbox_GET() (PyThreadState_GET()->interp->sandbox_state)

/* Return true if sandboxing is turn on for the current interpreter. */
#define _PySandbox_Check() (_PySandbox_GET() != NULL)

/* Return true if memory caps are to be used.
   Assumes sandboxing is turned on. */
#define _PySandbox_IsMemCapped() (_PySandbox_GET()->mem_cap > 0)


/*
   Memory
*/

PyAPI_FUNC(int) PySandbox_SetMemoryCap(PyThreadState *, size_t);

PyAPI_FUNC(int) _PySandbox_AllowedMemoryAlloc(size_t);
/* Return for caller if memory allocation would exceed memory cap. */
#define PySandbox_AllowedMemoryAlloc(alloc, err_return) \
    if (!_PySandbox_AllowedMemoryAlloc(alloc)) return err_return

/* Lower memory usage. */
PyAPI_FUNC(void) PySandbox_AllowedMemoryFree(size_t);

#ifdef __cplusplus
}
#endif

#endif /* Py_SANDBOX_H */
#endif /* PySandbox_SUPPORTED */
