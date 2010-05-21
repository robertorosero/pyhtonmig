/* -*- C++ -*- */
#ifndef Py_BG_THREAD_H
#define Py_BG_THREAD_H

/* This header defines a class used to run jobs in a background
   thread, with the GIL unlocked.  The thread is accessible from
   either C or C++, but it is implemented in C++.  The interface and
   implementation assume a single background thread (and, of course,
   one thread at a time on the GIL end). */

typedef struct PyBackgroundThread PyBackgroundThread;
typedef struct PyBackgroundJob PyBackgroundJob;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Whether or not we should block until a background job is finished.  */
typedef enum {
    Py_NO_BLOCK,
    Py_BLOCK
} Py_ShouldBlock;

/* Creates and starts a background thread if none was running and
   saves it into the PyInterpreterState.  Then hands it a job to
   run. */
PyAPI_FUNC(int) PyBackgroundThread_RunJob(
    PyInterpreterState*, PyBackgroundJob*);

/* Returns true if the background thread has been disabled after
 * forking.  This function must be false to call the other functions
 * here. */
#define PyBackgroundThread_Disabled(thread) ((uintptr_t)(thread) & 1)

/* Simple extern "C" wrappers for the compile thread methods.  */
PyAPI_FUNC(PyBackgroundThread*) PyBackgroundThread_New(void);
PyAPI_FUNC(void) PyBackgroundThread_Free(PyBackgroundThread*);
PyAPI_FUNC(void) PyBackgroundThread_Pause(PyBackgroundThread*);
PyAPI_FUNC(void) PyBackgroundThread_Unpause(PyBackgroundThread*);
PyAPI_FUNC(void) PyBackgroundThread_DisableAfterFork(PyInterpreterState*);
PyAPI_FUNC(int) PyBackgroundThread_ApplyFinishedJobs(
    PyBackgroundThread*, Py_ShouldBlock);

/* Returns a PyBackgroundJob that does nothing. */
PyAPI_FUNC(PyBackgroundJob*) PyBackgroundThread_NewDummyJob(void);

#ifdef __cplusplus
}

#include "pythread.h"
#include <deque>

// Acquires its lock argument within a scope.
struct PyLockGuard {
    PyLockGuard(PyThread_type_lock lock) : lock_(lock)
    {
        PyThread_acquire_lock(this->lock_, WAIT_LOCK);
    }
    ~PyLockGuard()
    {
        PyThread_release_lock(this->lock_);
    }
private:
    const PyThread_type_lock lock_;
};

// Releases its lock argument within a scope.  The lock must be held when the
// scope is entered, of course.
struct PyUnlockGuard {
    PyUnlockGuard(PyThread_type_lock lock) : lock_(lock)
    {
        PyThread_release_lock(this->lock_);
    }
    ~PyUnlockGuard()
    {
        PyThread_acquire_lock(this->lock_, WAIT_LOCK);
    }
private:
    const PyThread_type_lock lock_;
};

// Acts roughly like a posix condition variable.  This class takes the lock as a
// constructor argument and requires that it be held on entry to Wait(),
// Notify(), and NotifyAll().
struct PyCondition {
    PyCondition(PyThread_type_lock lock);
    // All threads blocked in this->Wait() must have been notified by the time
    // 'this' is destroyed.
    ~PyCondition();

    // Blocks until another thread calls Notify or NotifyAll.  The associated
    // lock must be held on entry and will be released and reacquired.
    void Wait();

    // Wakes up 'to_notify' threads blocked in this->Wait().  If less than that
    // many threads are blocked, wakes all blocked threads.  Unlike Posix
    // condition variables, the associated lock must be held.
    void Notify(size_t to_notify = 1);

    // Wakes up all threads blocked in this->Wait().  Unlike Posix condition
    // variables, the associated lock must be held.
    void NotifyAll();

private:
    const PyThread_type_lock lock_;
    std::deque<PyThread_type_lock> waiters_;
};

// Provides a boolean variable 'was_set' that can transition from false to true
// at most one time, and a way to wait until it turns true.  There is no way to
// change 'was_set' back to false because most uses would require extra locking
// around the reset to make sure notifications aren't lost.
struct PyMonotonicEvent {
    PyMonotonicEvent();
    ~PyMonotonicEvent();

    // Waits for 'was_set' to become true.
    void Wait();
    // Sets 'was_set' to true.
    void Set();

private:
    const PyThread_type_lock lock_;
    PyCondition cond_;
    bool was_set_;
};

// This is the interface for background jobs.  Each job has two virtual methods:
// a Run method, and an Apply method.
//
// The Run method runs on the background thread, with the GIL unlocked.  It may
// acquire the GIL, but that kinda defeats the point.  All Run methods execute
// serially, in the same order they were submitted to the background thread.
//
// The Apply method will called from some foreground thread that holds the GIL.
// The eval loop applies jobs periodically, and Python code waiting for a
// particular job to finish may apply other jobs too.  When a Job's Apply method
// returns, the system deletes the job.
struct PyBackgroundJob {
    PyBackgroundJob() : ready_to_apply_(NULL) {}
    virtual ~PyBackgroundJob() {}

    // This method is called in the background thread.  It must be careful not
    // to access Python data structures since it doesn't hold the GIL, and to
    // lock any data it needs to share.  'shutting_down' is true if Terminate
    // has been called.
    virtual void Run(bool shutting_down) = 0;

    // This method applies the result of the background job in a foreground
    // thread.  It's called while holding the GIL.  If any Python exceptions are
    // set when Apply returns, they'll be printed as if they escaped a Python
    // thread.
    virtual void Apply() = 0;

private:
    friend struct PyBackgroundThread;
    // If ready_to_apply_ is not null, the background thread will Set it when a
    // subsequent ApplyFinishedJobs will apply it.  This is only set by
    // PyBackgroundThread::RunJobAndWait(); users don't have access.
    PyMonotonicEvent *ready_to_apply_;
};

// This class encapsulates all state required by the background thread.
// Instances must be created and all public methods must be called while holding
// the GIL.
struct PyBackgroundThread {
public:
    PyBackgroundThread();
    ~PyBackgroundThread();

    // This method starts the background thread.  Calling it multiple times has
    // no effect.
    void Start();

    // Terminate the background thread.  This will block until it terminates,
    // which may be after it finishes a single compile job.  After the thread
    // has has stopped, all of the public methods below will run the job in the
    // foreground or do nothing.  This method must be called before destroying
    // the thread object or the PyGlobalLlvmData object.  Calling this method
    // when the thread is already stopped has no effect.
    void Terminate();

    // Pause the background thread.  Puts a job on the queue that will cause the
    // background thread to acquire the unpause event lock twice.  This call
    // must be paired with an Unpause call from the same thread.
    //
    // While the background thread is paused, it will run new jobs in the
    // foreground, as if it had been terminated.  This means that if we fork
    // while paused, the child process won't send jobs off to nowhere allows us
    // to fork while paused and avoid creating a new thread in the child process
    // or depending on state
    void Pause();

    // Unpause the background thread.  This releases the unpause_event_ lock,
    // allowing the background thread to proceed.  This call must be paired with
    // a Pause call from the same thread.
    void Unpause();

    // Helper method that puts a job on the queue.  If the queue is closed, it
    // calls the RunAfterShutdown method of the job and returns -1.  This
    // method takes ownership of the job.
    int RunJob(PyBackgroundJob *job);

    // Helper method that puts a job on the queue and waits for it to be
    // finished.  This method takes ownership of the job.
    void RunJobAndWait(PyBackgroundJob *job);

    // Helper method that calls RunJobAndWait and then applies all finished
    // jobs.  This method takes ownership of the job.
    void RunJobAndApply(PyBackgroundJob *job);

    // Apply the results of any JIT compilation done in the background thread.
    // If block is PY_NO_BLOCK, then this function returns non-zero immediately
    // if it cannot acquire the lock on the output job queue.  Otherwise it
    // returns 0.  The thread that calls this must hold the GIL.
    int ApplyFinishedJobs(Py_ShouldBlock block);

    PyThreadState *getThreadState() { return this->tstate_; }

private:
    // Guards member variables.
    const PyThread_type_lock lock_;
    // Lets the background thread wait for something to do.  Guarded by
    // this->lock_.
    PyCondition cond_;

    // This is the queue transferring jobs that need to be Run from the
    // foreground (GIL-holding threads) to the background thread.  Guarded by
    // this->lock_.
    std::deque<PyBackgroundJob*> fore2back_queue_;

    // This is the queue transferring finished jobs that need to be Applied from
    // the background thread to the foreground.  It is read periodically when
    // the Python ticker expires, or when a thread waits for a job to complete.
    // Guarded by this->lock_.
    std::deque<PyBackgroundJob*> back2fore_queue_;

    // Indicates whether the thread is running.  Guarded by this->lock_.
    bool running_;

    // True if a thread has called Terminate().  Guarded by this->lock_.
    bool exiting_;

    // The thread state for the background thread.  We don't use any of this
    // information, but we have to have an interpreter state to swap in when we
    // acquire the GIL.  Many Python calls assume that there is always a valid
    // thread state from which to get the interpreter state, so we must provide
    // one.  Set once at the beginning of Run
    PyThreadState *tstate_;

    // The interpreter state corresponding to this background thread.  We cache
    // this value because it is not safe to call PyThreadState_GET() from a
    // background thread.
    PyInterpreterState *const interpreter_state_;

    // A lock that allows us to pause and unpause the background thread without
    // destroying and recreating the underlying OS-level thread.
    PyThread_type_lock unpause_event_;


    // -- Functions called by the background thread --

    // This static method is the background compilation thread entry point.  It
    // simply wraps calling ((PyBackgroundThread*)thread)->Run() so we can pass
    // it as a function pointer to PyThread_start_new_thread.
    static void Bootstrap(void *thread);

    // This method reads jobs from the queue and runs them in a loop.  It
    // terminates when it reads a JIT_EXIT job from the queue.  This method
    // must be called from the background thread, and the caller should not
    // hold the GIL.
    void Run();

    // Puts a finished job on the output queue and notifies anyone waiting for
    // it that it is now on the output queue.  This method takes ownership of
    // the job.  This method must be called from the background thread, and the
    // caller should not hold the GIL.
    void OutputFinishedJob(PyBackgroundJob *job);
};

#endif /* __cplusplus */

#endif /* Py_BG_THREAD_H */
