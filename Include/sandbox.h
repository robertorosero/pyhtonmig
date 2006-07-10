#ifdef PySandbox_SUPPORTED

#ifndef Py_SANDBOX_H
#define Py_SANDBOX_H
#ifdef __cplusplus
extern "C" {
#endif

struct _sandbox_state;  /* Forward */

typedef struct _sandbox_state {
    PY_LONG_LONG mem_cap;

} PySandboxState;

/* Return true if sandboxing is turn on for the current interpreter. */
#define _PySandbox_Protected() (PyThreadState_GET()->interp->sandbox_state != NULL)

#ifdef __cplusplus
}
#endif

#endif /* Py_SANDBOX_H */
#endif /* PySandbox_SUPPORTED */
