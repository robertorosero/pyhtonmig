
/* Thread and interpreter state structures and their interfaces */

#include "Python.h"

/* --------------------------------------------------------------------------
CAUTION

Always use malloc() and free() directly in this file.  A number of these
functions are advertised as safe to call when the GIL isn't held, and in
a debug build Python redirects (e.g.) PyMem_NEW (etc) to Python's debugging
obmalloc functions.  Those aren't thread-safe (they rely on the GIL to avoid
the expense of doing their own locking).
-------------------------------------------------------------------------- */

#ifdef HAVE_DLOPEN
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#ifndef RTLD_LAZY
#define RTLD_LAZY 1
#endif
#endif


#ifdef WITH_THREAD
#include "pythread.h"
static PyThread_type_lock head_mutex = NULL; /* Protects interp->tstate_head */
#define HEAD_INIT() (void)(head_mutex || (head_mutex = PyThread_allocate_lock()))
#define HEAD_LOCK() PyThread_acquire_lock(head_mutex, WAIT_LOCK)
#define HEAD_UNLOCK() PyThread_release_lock(head_mutex)

#ifdef __cplusplus
extern "C" {
#endif

/* The single PyInterpreterState used by this process'
   GILState implementation
*/
static PyInterpreterState *autoInterpreterState = NULL;
static int autoTLSkey = 0;
#else
#define HEAD_INIT() /* Nothing */
#define HEAD_LOCK() /* Nothing */
#define HEAD_UNLOCK() /* Nothing */
#endif

static PyInterpreterState *interp_head = NULL;

PyThreadState *_PyThreadState_Current = NULL;
PyThreadFrameGetter _PyThreadState_GetFrame = NULL;

#ifdef WITH_THREAD
static void _PyGILState_NoteThreadState(PyThreadState* tstate);
#endif


PyInterpreterState *
PyInterpreterState_New(void)
{
	PyInterpreterState *interp = (PyInterpreterState *)
				     malloc(sizeof(PyInterpreterState));

	if (interp != NULL) {
		HEAD_INIT();
		interp->modules = NULL;
		interp->sysdict = NULL;
		interp->builtins = NULL;
		interp->tstate_head = NULL;
		interp->codec_search_path = NULL;
		interp->codec_search_cache = NULL;
		interp->codec_error_registry = NULL;
#ifdef HAVE_DLOPEN
#ifdef RTLD_NOW
                interp->dlopenflags = RTLD_NOW;
#else
		interp->dlopenflags = RTLD_LAZY;
#endif
#endif
#ifdef WITH_TSC
		interp->tscdump = 0;
#endif
#ifdef Py_MEMORY_CAP
		interp->mem_cap = 0;
		interp->mem_usage = 0;
#endif

		HEAD_LOCK();
		interp->next = interp_head;
		interp_head = interp;
		HEAD_UNLOCK();
	}

	return interp;
}


void
PyInterpreterState_Clear(PyInterpreterState *interp)
{
	PyThreadState *p;
	HEAD_LOCK();
	for (p = interp->tstate_head; p != NULL; p = p->next)
		PyThreadState_Clear(p);
	HEAD_UNLOCK();
	Py_CLEAR(interp->codec_search_path);
	Py_CLEAR(interp->codec_search_cache);
	Py_CLEAR(interp->codec_error_registry);
	Py_CLEAR(interp->modules);
	Py_CLEAR(interp->sysdict);
	Py_CLEAR(interp->builtins);
}


static void
zapthreads(PyInterpreterState *interp)
{
	PyThreadState *p;
	/* No need to lock the mutex here because this should only happen
	   when the threads are all really dead (XXX famous last words). */
	while ((p = interp->tstate_head) != NULL) {
		PyThreadState_Delete(p);
	}
}


void
PyInterpreterState_Delete(PyInterpreterState *interp)
{
	PyInterpreterState **p;
	zapthreads(interp);
	HEAD_LOCK();
	for (p = &interp_head; ; p = &(*p)->next) {
		if (*p == NULL)
			Py_FatalError(
				"PyInterpreterState_Delete: invalid interp");
		if (*p == interp)
			break;
	}
	if (interp->tstate_head != NULL)
		Py_FatalError("PyInterpreterState_Delete: remaining threads");
	*p = interp->next;
	HEAD_UNLOCK();
	free(interp);
}

#ifdef Py_MEMORY_CAP
/*
   Get the interpreter state from a PyThreadState after checking to make sure
   it is safe to do so based on initialization of the interpreter.
*/
PyInterpreterState *
PyInterpreterState_SafeGet(void)
{
    PyThreadState *tstate = NULL;

    if (!Py_IsInitialized() || !_PyThreadState_Current)
	return NULL;

    tstate = PyThreadState_GET();
    if (!tstate)
	return NULL;

    return tstate->interp;
}

int
PyInterpreterState_SetMemoryCap(PyInterpreterState *interp, PY_LONG_LONG cap)
{
    if (cap < 0) {
	PyErr_SetString(PyExc_ValueError, "memory cap must be >= 0");
	return 0;
    }

    if (cap < interp->mem_usage) {
	PyErr_SetString(PyExc_ValueError, "new memory cap too small for "
					    "current memory usage");	
	return 0;
    }

    interp->mem_cap = cap;

    return 1;
}

/*
   Raise the current allocation of memory on the interpreter by 'increase'.
   If it the allocation pushes the total memory usage past the memory cap,
   return a false value.
*/
int
PyInterpreterState_RaiseMemoryUsage(PyInterpreterState *interp, size_t increase)
{
    size_t original_mem_usage = 0;

    if (increase < 0)
	Py_FatalError("can only increase memory usage by a positive value");

    if (!interp->mem_cap)
	return 1;

    /* Watch out for integer overflow. */
    original_mem_usage = interp->mem_usage;
    interp->mem_usage += (PY_LONG_LONG)increase;
    if (interp->mem_usage < original_mem_usage) {
	interp->mem_usage = original_mem_usage;
	PyErr_NoMemory();
	return 0;
    }
    
    if (interp->mem_usage > interp->mem_cap) {
	interp->mem_usage = original_mem_usage;
	PyErr_NoMemory();
	return 0;
    }

    return 1;
}

/*
   Lower the current memory allocation.
   If lowered to below zero, push back up to zero.
*/
void
PyInterpreterState_LowerMemoryUsage(PyInterpreterState *interp, size_t decrease)
{
    if (decrease < 0)
	Py_FatalError("must specify memory usage reduction by a positive number");

    interp->mem_usage -= (PY_LONG_LONG)decrease;
    if (interp->mem_usage < 0)
	interp->mem_usage = 0;
}
#endif /* Py_MEMORY_CAP */

/* Default implementation for _PyThreadState_GetFrame */
static struct _frame *
threadstate_getframe(PyThreadState *self)
{
	return self->frame;
}

PyThreadState *
PyThreadState_New(PyInterpreterState *interp)
{
	PyThreadState *tstate = (PyThreadState *)malloc(sizeof(PyThreadState));

	if (_PyThreadState_GetFrame == NULL)
		_PyThreadState_GetFrame = threadstate_getframe;

	if (tstate != NULL) {
		tstate->interp = interp;

		tstate->frame = NULL;
		tstate->recursion_depth = 0;
		tstate->tracing = 0;
		tstate->use_tracing = 0;
		tstate->tick_counter = 0;
		tstate->gilstate_counter = 0;
		tstate->async_exc = NULL;
#ifdef WITH_THREAD
		tstate->thread_id = PyThread_get_thread_ident();
#else
		tstate->thread_id = 0;
#endif

		tstate->dict = NULL;

		tstate->curexc_type = NULL;
		tstate->curexc_value = NULL;
		tstate->curexc_traceback = NULL;

		tstate->exc_type = NULL;
		tstate->exc_value = NULL;
		tstate->exc_traceback = NULL;

		tstate->c_profilefunc = NULL;
		tstate->c_tracefunc = NULL;
		tstate->c_profileobj = NULL;
		tstate->c_traceobj = NULL;

#ifdef WITH_THREAD
		_PyGILState_NoteThreadState(tstate);
#endif

		HEAD_LOCK();
		tstate->next = interp->tstate_head;
		interp->tstate_head = tstate;
		HEAD_UNLOCK();
	}

	return tstate;
}


void
PyThreadState_Clear(PyThreadState *tstate)
{
	if (Py_VerboseFlag && tstate->frame != NULL)
		fprintf(stderr,
		  "PyThreadState_Clear: warning: thread still has a frame\n");

	Py_CLEAR(tstate->frame);

	Py_CLEAR(tstate->dict);
	Py_CLEAR(tstate->async_exc);

	Py_CLEAR(tstate->curexc_type);
	Py_CLEAR(tstate->curexc_value);
	Py_CLEAR(tstate->curexc_traceback);

	Py_CLEAR(tstate->exc_type);
	Py_CLEAR(tstate->exc_value);
	Py_CLEAR(tstate->exc_traceback);

	tstate->c_profilefunc = NULL;
	tstate->c_tracefunc = NULL;
	Py_CLEAR(tstate->c_profileobj);
	Py_CLEAR(tstate->c_traceobj);
}


/* Common code for PyThreadState_Delete() and PyThreadState_DeleteCurrent() */
static void
tstate_delete_common(PyThreadState *tstate)
{
	PyInterpreterState *interp;
	PyThreadState **p;
	if (tstate == NULL)
		Py_FatalError("PyThreadState_Delete: NULL tstate");
	interp = tstate->interp;
	if (interp == NULL)
		Py_FatalError("PyThreadState_Delete: NULL interp");
	HEAD_LOCK();
	for (p = &interp->tstate_head; ; p = &(*p)->next) {
		if (*p == NULL)
			Py_FatalError(
				"PyThreadState_Delete: invalid tstate");
		if (*p == tstate)
			break;
	}
	*p = tstate->next;
	HEAD_UNLOCK();
	free(tstate);
}


void
PyThreadState_Delete(PyThreadState *tstate)
{
	if (tstate == _PyThreadState_Current)
		Py_FatalError("PyThreadState_Delete: tstate is still current");
	tstate_delete_common(tstate);
#ifdef WITH_THREAD
	if (autoTLSkey && PyThread_get_key_value(autoTLSkey) == tstate)
		PyThread_delete_key_value(autoTLSkey);
#endif /* WITH_THREAD */
}


#ifdef WITH_THREAD
void
PyThreadState_DeleteCurrent()
{
	PyThreadState *tstate = _PyThreadState_Current;
	if (tstate == NULL)
		Py_FatalError(
			"PyThreadState_DeleteCurrent: no current tstate");
	_PyThreadState_Current = NULL;
	tstate_delete_common(tstate);
	if (autoTLSkey && PyThread_get_key_value(autoTLSkey) == tstate)
		PyThread_delete_key_value(autoTLSkey);
	PyEval_ReleaseLock();
}
#endif /* WITH_THREAD */


PyThreadState *
PyThreadState_Get(void)
{
	if (_PyThreadState_Current == NULL)
		Py_FatalError("PyThreadState_Get: no current thread");

	return _PyThreadState_Current;
}


PyThreadState *
PyThreadState_Swap(PyThreadState *newts)
{
	PyThreadState *oldts = _PyThreadState_Current;

	_PyThreadState_Current = newts;
	/* It should not be possible for more than one thread state
	   to be used for a thread.  Check this the best we can in debug
	   builds.
	*/
#if defined(Py_DEBUG) && defined(WITH_THREAD)
	if (newts) {
		PyThreadState *check = PyGILState_GetThisThreadState();
		if (check && check->interp == newts->interp && check != newts)
			Py_FatalError("Invalid thread state for this thread");
	}
#endif
	return oldts;
}

/* An extension mechanism to store arbitrary additional per-thread state.
   PyThreadState_GetDict() returns a dictionary that can be used to hold such
   state; the caller should pick a unique key and store its state there.  If
   PyThreadState_GetDict() returns NULL, an exception has *not* been raised
   and the caller should assume no per-thread state is available. */

PyObject *
PyThreadState_GetDict(void)
{
	if (_PyThreadState_Current == NULL)
		return NULL;

	if (_PyThreadState_Current->dict == NULL) {
		PyObject *d;
		_PyThreadState_Current->dict = d = PyDict_New();
		if (d == NULL)
			PyErr_Clear();
	}
	return _PyThreadState_Current->dict;
}


/* Asynchronously raise an exception in a thread.
   Requested by Just van Rossum and Alex Martelli.
   To prevent naive misuse, you must write your own extension
   to call this.  Must be called with the GIL held.
   Returns the number of tstates modified; if it returns a number
   greater than one, you're in trouble, and you should call it again
   with exc=NULL to revert the effect.  This raises no exceptions. */

int
PyThreadState_SetAsyncExc(long id, PyObject *exc) {
	PyThreadState *tstate = PyThreadState_GET();
	PyInterpreterState *interp = tstate->interp;
	PyThreadState *p;
	int count = 0;
	HEAD_LOCK();
	for (p = interp->tstate_head; p != NULL; p = p->next) {
		if (p->thread_id != id)
			continue;
		Py_CLEAR(p->async_exc);
		Py_XINCREF(exc);
		p->async_exc = exc;
		count += 1;
	}
	HEAD_UNLOCK();
	return count;
}


/* Routines for advanced debuggers, requested by David Beazley.
   Don't use unless you know what you are doing! */

PyInterpreterState *
PyInterpreterState_Head(void)
{
	return interp_head;
}

PyInterpreterState *
PyInterpreterState_Next(PyInterpreterState *interp) {
	return interp->next;
}

PyThreadState *
PyInterpreterState_ThreadHead(PyInterpreterState *interp) {
	return interp->tstate_head;
}

PyThreadState *
PyThreadState_Next(PyThreadState *tstate) {
	return tstate->next;
}


/* Python "auto thread state" API. */
#ifdef WITH_THREAD

/* Keep this as a static, as it is not reliable!  It can only
   ever be compared to the state for the *current* thread.
   * If not equal, then it doesn't matter that the actual
     value may change immediately after comparison, as it can't
     possibly change to the current thread's state.
   * If equal, then the current thread holds the lock, so the value can't
     change until we yield the lock.
*/
static int
PyThreadState_IsCurrent(PyThreadState *tstate)
{
	/* Must be the tstate for this thread */
	assert(PyGILState_GetThisThreadState()==tstate);
	/* On Windows at least, simple reads and writes to 32 bit values
	   are atomic.
	*/
	return tstate == _PyThreadState_Current;
}

/* Internal initialization/finalization functions called by
   Py_Initialize/Py_Finalize
*/
void
_PyGILState_Init(PyInterpreterState *i, PyThreadState *t)
{
	assert(i && t); /* must init with valid states */
	autoTLSkey = PyThread_create_key();
	autoInterpreterState = i;
	assert(PyThread_get_key_value(autoTLSkey) == NULL);
	assert(t->gilstate_counter == 0);

	_PyGILState_NoteThreadState(t);
}

void
_PyGILState_Fini(void)
{
	PyThread_delete_key(autoTLSkey);
	autoTLSkey = 0;
	autoInterpreterState = NULL;;
}

/* When a thread state is created for a thread by some mechanism other than
   PyGILState_Ensure, it's important that the GILState machinery knows about
   it so it doesn't try to create another thread state for the thread (this is
   a better fix for SF bug #1010677 than the first one attempted).
*/
void
_PyGILState_NoteThreadState(PyThreadState* tstate)
{
	/* If autoTLSkey is 0, this must be the very first threadstate created
	   in Py_Initialize().  Don't do anything for now (we'll be back here
	   when _PyGILState_Init is called). */
	if (!autoTLSkey) 
		return;
	
	/* Stick the thread state for this thread in thread local storage.

	   The only situation where you can legitimately have more than one
	   thread state for an OS level thread is when there are multiple
	   interpreters, when:
	       
	       a) You shouldn't really be using the PyGILState_ APIs anyway,
	          and:

	       b) The slightly odd way PyThread_set_key_value works (see
	          comments by its implementation) means that the first thread
	          state created for that given OS level thread will "win",
	          which seems reasonable behaviour.
	*/
	if (PyThread_set_key_value(autoTLSkey, (void *)tstate) < 0)
		Py_FatalError("Couldn't create autoTLSkey mapping");

	/* PyGILState_Release must not try to delete this thread state. */
	tstate->gilstate_counter = 1;
}

/* The public functions */
PyThreadState *
PyGILState_GetThisThreadState(void)
{
	if (autoInterpreterState == NULL || autoTLSkey == 0)
		return NULL;
	return (PyThreadState *)PyThread_get_key_value(autoTLSkey);
}

PyGILState_STATE
PyGILState_Ensure(void)
{
	int current;
	PyThreadState *tcur;
	/* Note that we do not auto-init Python here - apart from
	   potential races with 2 threads auto-initializing, pep-311
	   spells out other issues.  Embedders are expected to have
	   called Py_Initialize() and usually PyEval_InitThreads().
	*/
	assert(autoInterpreterState); /* Py_Initialize() hasn't been called! */
	tcur = (PyThreadState *)PyThread_get_key_value(autoTLSkey);
	if (tcur == NULL) {
		/* Create a new thread state for this thread */
		tcur = PyThreadState_New(autoInterpreterState);
		if (tcur == NULL)
			Py_FatalError("Couldn't create thread-state for new thread");
		/* This is our thread state!  We'll need to delete it in the
		   matching call to PyGILState_Release(). */
		tcur->gilstate_counter = 0;
		current = 0; /* new thread state is never current */
	}
	else
		current = PyThreadState_IsCurrent(tcur);
	if (current == 0)
		PyEval_RestoreThread(tcur);
	/* Update our counter in the thread-state - no need for locks:
	   - tcur will remain valid as we hold the GIL.
	   - the counter is safe as we are the only thread "allowed"
	     to modify this value
	*/
	++tcur->gilstate_counter;
	return current ? PyGILState_LOCKED : PyGILState_UNLOCKED;
}

void
PyGILState_Release(PyGILState_STATE oldstate)
{
	PyThreadState *tcur = (PyThreadState *)PyThread_get_key_value(
                                                                autoTLSkey);
	if (tcur == NULL)
		Py_FatalError("auto-releasing thread-state, "
		              "but no thread-state for this thread");
	/* We must hold the GIL and have our thread state current */
	/* XXX - remove the check - the assert should be fine,
	   but while this is very new (April 2003), the extra check
	   by release-only users can't hurt.
	*/
	if (! PyThreadState_IsCurrent(tcur))
		Py_FatalError("This thread state must be current when releasing");
	assert(PyThreadState_IsCurrent(tcur));
	--tcur->gilstate_counter;
	assert(tcur->gilstate_counter >= 0); /* illegal counter value */

	/* If we're going to destroy this thread-state, we must
	 * clear it while the GIL is held, as destructors may run.
	 */
	if (tcur->gilstate_counter == 0) {
		/* can't have been locked when we created it */
		assert(oldstate == PyGILState_UNLOCKED);
		PyThreadState_Clear(tcur);
		/* Delete the thread-state.  Note this releases the GIL too!
		 * It's vital that the GIL be held here, to avoid shutdown
		 * races; see bugs 225673 and 1061968 (that nasty bug has a
		 * habit of coming back).
		 */
		PyThreadState_DeleteCurrent();
	}
	/* Release the lock if necessary */
	else if (oldstate == PyGILState_UNLOCKED)
		PyEval_SaveThread();
}

#ifdef __cplusplus
}
#endif

#endif /* WITH_THREAD */


