#include "Python.h"
#include <malloc.h>

/*
    Add accountability to memory allocation.

    The goal of these functions is to allow for all memory to be tracked based
    on how much is being (roughly) used, and for what.  

    The APIs that need to be covered are PyObject_New(), Pyobject_Malloc(),
    PyMem_Malloc(), the realloc/free mates, the macro variants, and the GC
    variants.

    * PyObject_New()/PyObject_NewVar()/PyObject_Del()
	Uses PyObject_T_*()
    * PyObject_NEW()/PyObject_NEW_VAR()/PyObject_DEL()
	Uses PyObject_T_*()
    * PyObject_MALLOC()/PyObject_REALLOC()/PyObject_FREE()
	Change over to PyObject_T_*()
    * PyObject_Malloc()/PyObject_Realloc()/PyObject_Free()
	XXX
    * PyObject_GC_New()/PyObject_GC_NewVar()/PyObject_GC_Del()
	Uses _PyObject_GC_TrackedMalloc()
    * _PyObject_GC_Malloc()
	Changed to _PyObject_GC_TrackedMalloc()
    * PyObject_GC_Resize()
	Uses PyObject_T_MALLOC()
    * PyMem_Malloc()/PyMem_Realloc()/PyMem_Free()
	XXX
    * PyMem_MALLOC()/PyMem_REALLOC()/PyMem_FREE()
	XXX
    * malloc()/realloc()/free()
	XXX

    In order to properly track memory usage, we must handle both memory handed
    out by pymalloc as well as memory from malloc().  For pymaloc, we need to
    first find out if pymalloc is managing the memory, and if that is true then
    how big of a chunk of memory was given for the pointer.
    For malloc(), we need to either use functions provided by the C library
    (for glibc, see
    http://www.gnu.org/software/libc/manual/html_node/Summary-of-Malloc.html).
*/

unsigned long Py_ProcessMemUsage = 0;

static const char *UNKNOWN_WHAT = "<unknown>";


/*
   Track an anonymous chunk of memory.
*/
int
PyObject_TrackMemory(const char *what, size_t nbytes)
{
    what = what ? what : UNKNOWN_WHAT;

    Py_ProcessMemUsage += nbytes;

    return 1;
}

/*
   Stop tracking an anonymous chunk of memory.
*/
int
PyObject_UntrackMemory(const char *what, size_t nbytes)
{
    what = what ? what : UNKNOWN_WHAT;

    Py_ProcessMemUsage -= nbytes;

    return 1;
}

/*
   Track the memory created by PyObject_Maloc().
*/
void *
PyObject_TrackedMalloc(const char *what, size_t nbytes)
{
    struct mallinfo before = mallinfo();
    void *allocated = NULL;
    size_t used = 0;

    allocated = PyObject_Malloc(nbytes);

    if (!allocated)
	return NULL;

    if (PyMalloc_ManagesMemory(allocated)) {
	used = PyMalloc_AllocatedSize(allocated);
    }
    else {
	used = mallinfo().uordblks - before.uordblks;
    }

    PyObject_TrackMemory(what, used);

    return allocated;
}

/*
   Track the realloc of memory cretaed by PyObject_Malloc().

   XXX really need to have reason string passed in?
*/
void *
PyObject_TrackedRealloc(const char *what, void *to_resize, size_t new_size)
{
    struct mallinfo before = mallinfo();
    void *allocated = NULL;
    size_t size_delta = 0;

    if (PyMalloc_ManagesMemory(to_resize)) {
	size_delta = PyMalloc_AllocatedSize(to_resize);
    }

    allocated = PyObject_Realloc(to_resize, new_size);

    if (!allocated)
	return NULL;

    if (PyMalloc_ManagesMemory(allocated)) {
	size_delta = PyMalloc_AllocatedSize(allocated) - size_delta;
    }
    else {
	size_delta = mallinfo().uordblks - before.uordblks;
    }

    PyObject_TrackMemory(what, size_delta);

    return allocated;
}

/*
   Untrack memory created by PyObject_Malloc().
*/
void
PyObject_TrackedFree(const char *what, void *to_free)
{
    struct mallinfo before = mallinfo();
    size_t freed = 0;

    if (PyMalloc_ManagesMemory(to_free)) {
	freed = PyMalloc_AllocatedSize(to_free);
    }

    PyObject_Free(to_free);

    if (!freed) {
	freed = before.uordblks - mallinfo().uordblks;
    }

    PyObject_UntrackMemory(what, freed);
}
