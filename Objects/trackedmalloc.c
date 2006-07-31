#include "Python.h"

/* Must come after Python.h for pyconfig.h value to be set. */
#ifdef Py_TRACK_MEMORY

#ifdef HAVE_MALLINFO
#include <malloc.h>
#endif

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
	Change over to PyObject_T_*()
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

XXX:
+ convert over all APIs.
+ add proper Py_TRACK_MEMORY #ifdef protections.
+ Raise an error during compilation if required functionality (e.g. mallinfo())
  not available for tracking memory, even if requested.
+ Completely convert ElementTree.
+ Add an adjust function to compliment track/untrack for realloc usage (allows
  for future usage of tracking active object counts)
*/

unsigned long Py_ProcessMemUsage = 0;

static const char *UNKNOWN_WHAT = "<unknown>";

struct mem_item {
    struct mem_item *next;
    const char *type;
    unsigned long using;
};

static struct mem_item mem_sentinel = {NULL, NULL, 0};
static struct mem_item *mem_head  = &mem_sentinel;
static Py_ssize_t mem_item_count = 0;


PyObject *
Py_MemoryUsage(PyObject *self, PyObject *ignore)
{
    struct mem_item *cur_mem = mem_head;
    PyObject *mem_dict = PyDict_New();
    Py_ssize_t x = 0;
    int int_result = 0;

    if (!mem_dict)
	return NULL;

    for (x=0; x < mem_item_count; x+=1) {
	cur_mem = cur_mem->next;
	PyObject *long_obj = PyLong_FromUnsignedLong(cur_mem->using);
	if (!long_obj)
	    return NULL;
	int_result = PyDict_SetItemString(mem_dict, cur_mem->type, long_obj);
	Py_DECREF(long_obj);
	
	if (int_result < 0)
	    return NULL;
    }

    return mem_dict;
}

/* XXX remove entries where memory usage is zero? */
static struct mem_item *
find_mem_entry(const char *what)
{
    struct mem_item *cur_mem = mem_head;

    what = what ? what : UNKNOWN_WHAT;

    while (cur_mem->next) {
	cur_mem = cur_mem->next;

	if (strcmp(what, cur_mem->type) == 0)
	    return cur_mem;
    }

    cur_mem->next = malloc(sizeof(struct mem_item));
    cur_mem = cur_mem->next;

    if (!cur_mem)
	return NULL;

    mem_item_count += 1;

    cur_mem->next = NULL;
    cur_mem->type = what;  /* XXX memcpy? */
    cur_mem->using = 0;

    return cur_mem;
}

/*
   Track an anonymous chunk of memory.
*/
int
PyObject_TrackMemory(const char *what, size_t nbytes)
{
    struct mem_item *mem_entry = find_mem_entry(what);

    if (!mem_entry)
	return 0;

    /* XXX check for overflow. */
    mem_entry->using += nbytes;
    Py_ProcessMemUsage += nbytes;

    return 1;
}

/*
   Stop tracking an anonymous chunk of memory.
*/
int
PyObject_UntrackMemory(const char *what, size_t nbytes)
{
    struct mem_item *mem_entry = find_mem_entry(what);

    if (!mem_entry)
	return 0;

    /* XXX check for hitting < 0. */
    mem_entry->using -= nbytes;
    Py_ProcessMemUsage -= nbytes;

    return 1;
}

/*
   Track the memory created by PyObject_Maloc().
*/
void *
PyObject_TrackedMalloc(const char *what, size_t nbytes)
{
#ifdef HAVE_MALLINFO
    struct mallinfo before = mallinfo();
#endif
    void *allocated = NULL;
    size_t used = 0;

    allocated = PyObject_Malloc(nbytes);

    if (!allocated)
	return NULL;

    if (PyMalloc_ManagesMemory(allocated)) {
	used = PyMalloc_AllocatedSize(allocated);
    }
    else {
#ifdef HAVE_MALLINFO
	used = mallinfo().uordblks - before.uordblks;
#endif
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
#ifdef HAVE_MALLINFO
    struct mallinfo before = mallinfo();
#endif
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
#ifdef HAVE_MALLINFO
	size_delta = mallinfo().uordblks - before.uordblks;
#endif
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
#ifdef HAVE_MALLINFO
    struct mallinfo before = mallinfo();
#endif
    size_t freed = 0;

    if (PyMalloc_ManagesMemory(to_free)) {
	freed = PyMalloc_AllocatedSize(to_free);
    }

    PyObject_Free(to_free);

    if (!freed) {
#ifdef HAVE_MALLINFO
	freed = before.uordblks - mallinfo().uordblks;
#endif
    }

    PyObject_UntrackMemory(what, freed);
}

#endif /* Py_TRACK_MEMORY */
