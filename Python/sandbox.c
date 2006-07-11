#include "Python.h" /* Must be defined before PySandbox_SUPPORTED check */

#ifdef PySandbox_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

/*
   Set the memory cap for a sandboxed interpreter.
*/
int
PySandbox_SetMemoryCap(PyThreadState *s_tstate, Py_ssize_t mem_cap)
{
    PySandboxState *sandbox_state = s_tstate->interp->sandbox_state;

    if (!sandbox_state)
	return 0;

    sandbox_state->mem_cap = mem_cap;

    return 1;
}

/*
   Verify that memory allocation is allowed.
*/
int
_PySandbox_AllowedMemoryAlloc(Py_ssize_t allocate)
{
    PySandboxState *sandbox_state = _PySandbox_GET();

    if (_PySandbox_Check() && _PySandbox_IsMemCapped()) {
	size_t orig_mem_usage = sandbox_state->mem_usage;

	sandbox_state->mem_usage += allocate;
	/* Watch out for integer overflow. */
	if ((sandbox_state->mem_cap < sandbox_state->mem_usage) ||
		(orig_mem_usage > sandbox_state->mem_usage)) {
	    sandbox_state -= allocate;
	    PyErr_SetString(PyExc_SandboxError, "memory allocation exceeded");
	    return 0;
	}
    }

    return 1;
}

/*
   Verify that freeing memory does not go past what was already used.
*/
void
PySandbox_AllowedMemoryFree(Py_ssize_t deallocate)
{
    PySandboxState *sandbox_state = _PySandbox_GET();

    if (_PySandbox_Check() && _PySandbox_IsMemCapped()) {
	sandbox_state->mem_usage -= deallocate;
	if (sandbox_state->mem_usage < 0)
	    sandbox_state->mem_usage = 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* PySandbox_SUPPORTED */
