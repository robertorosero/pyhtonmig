#include "Python.h"

#include "bg_thread.h"
#include "pythread.h"

#ifdef WITH_LLVM

// Support structs:

PyCondition::PyCondition(PyThread_type_lock lock)
    : lock_(lock)
{}
PyCondition::~PyCondition()
{
    assert(this->waiters_.empty() &&
           "Destroyed condition variable with waiting threads");
}

void PyCondition::Wait()
{
    // TODO: Allocating a new python-lock on every wait is moderately
    // expensive.  We should add a real condition variable to
    // pythreads or create a freelist for these python-locks.
    const PyThread_type_lock semaphore = PyThread_allocate_lock();
    PyThread_acquire_lock(semaphore, WAIT_LOCK);
    this->waiters_.push_back(semaphore);
    const PyThread_type_lock this_lock = this->lock_;
    PyThread_release_lock(this_lock);
    // After here, 'this' may have been destroyed.
    // Block until another thread notifies us.
    PyThread_acquire_lock(semaphore, WAIT_LOCK);
    PyThread_free_lock(semaphore);
    PyThread_acquire_lock(this_lock, WAIT_LOCK);
}

void PyCondition::Notify(size_t to_notify)
{
    to_notify = std::min<size_t>(to_notify, waiters_.size());
    for (size_t i = 0; i < to_notify; ++i) {
        PyThread_type_lock waiter_semaphore = waiters_.front();
        waiters_.pop_front();
        PyThread_release_lock(waiter_semaphore);
    }
}

void PyCondition::NotifyAll()
{
    Notify(waiters_.size());
}

PyMonotonicEvent::PyMonotonicEvent()
    : lock_(PyThread_allocate_lock()),
      cond_(lock_),
      was_set_(false)
{}
PyMonotonicEvent::~PyMonotonicEvent()
{
    PyThread_free_lock(lock_);
}

void PyMonotonicEvent::Wait()
{
    PyLockGuard g(this->lock_);
    while (!this->was_set_) {
        this->cond_.Wait();
    }
}

void PyMonotonicEvent::Set()
{
    PyLockGuard g(this->lock_);
    this->was_set_ = true;
    this->cond_.NotifyAll();
}


// Actual background thread code:

PyBackgroundThread::PyBackgroundThread()
    : lock_(PyThread_allocate_lock()), cond_(lock_),
      running_(false), exiting_(false), tstate_(NULL),
      interpreter_state_(PyThreadState_GET()->interp),
      unpause_event_(PyThread_allocate_lock())
{
}

PyBackgroundThread::~PyBackgroundThread()
{
    this->Terminate();
    PyThread_free_lock(this->unpause_event_);
#ifdef Py_WITH_INSTRUMENTATION
    fprintf(stderr, "Compilation thread statistics:\n");
    fprintf(stderr, "compile jobs completed: %d\n", CompileJob::compile_count);
    fprintf(stderr, "compile jobs skipped at thread termination: %d\n",
            CompileJob::skipped_compile_count);
#endif
}

// Assumes this->lock_ is held.
void
PyBackgroundThread::OutputFinishedJob(PyBackgroundJob *job)
{
    // Put the result of the action on the output queue.  This takes
    // ownership of the job.
    this->back2fore_queue_.push_back(job);

    // Alert anybody who might be blocked waiting for this job to finish.  This
    // is done under the same lock as the output queue, so that anyone waiting
    // can call this->ApplyFinishedJobs(Py_BLOCK) to make the job take effect.
    if (job->ready_to_apply_) {
        job->ready_to_apply_->Set();
    }

    // Tell the eval loop too.
    _PyEval_SetBackgroundJobAvailable(true);
}

void
PyBackgroundThread::Run()
{
    // Create a new thread state.
    assert(this->tstate_ == NULL);
    this->tstate_ = PyThreadState_New(this->interpreter_state_);

    PyLockGuard lg(this->lock_);
    // Consume and run jobs from the input queue until Terminate is called.
    // Then consume jobs until there aren't any left and exit.
    while (true) {
        while (!this->exiting_ && this->fore2back_queue_.empty()) {
            this->cond_.Wait();
        }
        const bool exiting = this->exiting_;
        if (exiting && this->fore2back_queue_.empty()) {
            break;
        }
        PyBackgroundJob *job = this->fore2back_queue_.front();
        this->fore2back_queue_.pop_front();

        {
            PyUnlockGuard ug(this->lock_);
            // Don't hold the lock while running the job, so other jobs can be
            // submitted without blocking.
            job->Run(exiting);
        }
        this->OutputFinishedJob(job);
    }
    PyThreadState_Clear(this->tstate_);
    PyThreadState_Delete(this->tstate_);
    this->tstate_ = NULL;
    this->running_ = false;
    this->cond_.NotifyAll();
}

int
PyBackgroundThread::RunJob(PyBackgroundJob *job)
{
    PyEval_AssertLockHeld();

    {
        PyLockGuard g(this->lock_);
        if (this->running_) {
            this->fore2back_queue_.push_back(job);
            this->cond_.NotifyAll();
            return 0;
        }
    }

    // If the background thread has terminated, it can't race with the code in
    // any job that expects to run there.  So we just do exactly what the
    // background thread would have done.
    job->Run(/*shutting_down=*/true);

    // Rather than pushing it on to the output queue, since we already hold
    // the GIL, we apply the job and delete it here.
    job->Apply();
    delete job;
    return -1;
}

void
PyBackgroundThread::RunJobAndWait(PyBackgroundJob *job)
{
    // Create a lock for the job, acquire it, and put the job on the queue.
    assert(job->ready_to_apply_ == NULL && "Job is already being waited for?");
    PyMonotonicEvent ready;
    job->ready_to_apply_ = &ready;
    if (this->RunJob(job) < 0) {
        // If we couldn't put it on the queue, don't wait for it, because it's
        // already done.
        return;
    }

    // Try to acquire the lock again to wait until the lock is released by the
    // background thread.  This may take awhile, so we release the GIL while we
    // wait.
    Py_BEGIN_ALLOW_THREADS;
    ready.Wait();
    job->ready_to_apply_ = NULL;
    // When we get control back, reacquire the GIL.
    Py_END_ALLOW_THREADS;
}

void
PyBackgroundThread::RunJobAndApply(PyBackgroundJob *job)
{
    this->RunJobAndWait(job);
    // Call this to make sure all compilations take effect before we return.
    this->ApplyFinishedJobs(Py_BLOCK);
}

int
PyBackgroundThread::ApplyFinishedJobs(Py_ShouldBlock block)
{
    if (!PyThread_acquire_lock(this->lock_, block == Py_BLOCK)) {
        assert(block == Py_NO_BLOCK &&
               "We should only fail to acquire the lock "
               "in a non-blocking application attempt.");
        return -1;  // We couldn't acquire the lock, so we'll try again later.
    }
    std::deque<PyBackgroundJob*> queue;
    // Take all of the elements out of the background->foreground queue.
    queue.swap(this->back2fore_queue_);
    // Tell the eval loop that the jobs are taken care of. We have to
    // do this under the lock so we don't clobber any new jobs that
    // finish after we release the lock.
    _PyEval_SetBackgroundJobAvailable(false);
    // Then release the lock so Apply doesn't deadlock, and the background
    // thread can keep running jobs.
    PyThread_release_lock(this->lock_);

    for (std::deque<PyBackgroundJob*>::const_iterator
             it = queue.begin(), end = queue.end(); it != end; ++it) {
        PyBackgroundJob *job = *it;
        job->Apply();
        delete job;
    }
    return 0;
}

void
PyBackgroundThread::Terminate()
{
    Py_BEGIN_ALLOW_THREADS;
    {
        PyLockGuard g(this->lock_);
        this->exiting_ = true;
        this->cond_.NotifyAll();
        while (this->running_) {
            this->cond_.Wait();
        }
    }
    Py_END_ALLOW_THREADS;
    this->ApplyFinishedJobs(Py_BLOCK);
}

void
PyBackgroundThread::Bootstrap(void *thread)
{
    ((PyBackgroundThread*)thread)->Run();
}

void
PyBackgroundThread::Start()
{
    if (this->running_) {
        return;
    }
    this->running_ = true;

    PyEval_InitThreads(); /* Start the interpreter's thread-awareness */
    PyThread_start_new_thread(Bootstrap, this);
}

// A PyBackgroundJob that pauses the background thread in preparation for
// forking.  This works by acquiring the PyBackgroundThread::unpause_event_ lock
// twice.
struct PauseJob : public PyBackgroundJob {
    PauseJob(PyThread_type_lock bg_thread_lock,
             PyThread_type_lock unpause_event,
             PyMonotonicEvent &paused)
        : bg_thread_lock_(bg_thread_lock),
          unpause_event_(unpause_event),
          paused_(&paused)
    {}

    // Acquire the unpause event lock so that we wait for it.
    virtual void Run(bool shut_down) {
        if (shut_down) {
            // If we try to pause the thread after its stopped, acquire the lock once,
            // so that it can be properly released later.
            PyThread_acquire_lock(this->unpause_event_, WAIT_LOCK);
            return;
        }

        // Prepare to block by acquiring the unpause event lock twice.
        assert(PyThread_acquire_lock(this->unpause_event_, NOWAIT_LOCK) &&
               "Unpause event lock was already acquired!");

        // Acquire the lock on the whole background thread so that no one can
        // use it while we're paused.
        PyThread_acquire_lock(this->bg_thread_lock_, WAIT_LOCK);

        // We need to notify whoever is waiting on this job before we block.
        this->paused_->Set();
        // The foreground thread may fork at any point after this.
        this->paused_ = NULL;

        // At this point, there is sort of a race between the thread that paused
        // the compilation thread and this next acquire.  In either case, after
        // the acquire we will hold the lock because only one pause job can be
        // processed at a time.

        // Block until we are unpaused.
        PyThread_acquire_lock(this->unpause_event_, WAIT_LOCK);

        // Leave the unpause event lock in the released state.
        PyThread_release_lock(this->unpause_event_);

        // Release the locks on the background thread.
        PyThread_release_lock(this->bg_thread_lock_);
    }

    virtual void Apply() {}

private:
    const PyThread_type_lock bg_thread_lock_;
    const PyThread_type_lock unpause_event_;
    PyMonotonicEvent *paused_;
};

void
PyBackgroundThread::Pause()
{
    PyMonotonicEvent paused;
    {
        PyLockGuard g(this->lock_);
        if (!this->running_)
            return;

        // Put the pause job at the front of the queue so we don't wait for the
        // other jobs to finish before pausing.
        this->fore2back_queue_.push_front(
            new PauseJob(this->lock_, this->unpause_event_, paused));
        this->cond_.NotifyAll();
    }

    // Wait for the pause job to actually start and acquire the locks it needs
    // to acquire.
    paused.Wait();

    // Note that we leak the pause job in the child process during a fork.  This
    // is OK because we leak all kinds of other things, like locks and objects
    // owned by other threads.
}

void
PyBackgroundThread::Unpause()
{
    PyThread_release_lock(this->unpause_event_);
}

extern "C" {
    void
    PyBackgroundThread_DisableAfterFork(PyInterpreterState *interp)
    {
        interp->background_thread =
            reinterpret_cast<PyBackgroundThread*>(
                reinterpret_cast<uintptr_t>(interp->background_thread) | 1);
    }

    void
    PyBackgroundThread_ReenableAfterFork(PyInterpreterState *interp)
    {
        // After forking, most of the data in the background thread is
        // in an unknown state.  We do, however, know that the thread
        // isn't actually running.  So we leak any memory owned by the
        // background thread, and reset the interpreter's pointer to
        // NULL, so anyone trying to use it will restart the thread.
        interp->background_thread = NULL;
    }

    PyBackgroundThread *
    PyBackgroundThread_New()
    {
        return new PyBackgroundThread;
    }

    void
    PyBackgroundThread_Free(PyBackgroundThread *bg_thread)
    {
        delete bg_thread;
    }

    void
    PyBackgroundThread_Pause(PyBackgroundThread *thread)
    {
        if (thread != NULL && !PyBackgroundThread_Disabled(thread))
            thread->Pause();
    }

    void
    PyBackgroundThread_Unpause(PyBackgroundThread *thread)
    {
        if (thread != NULL && !PyBackgroundThread_Disabled(thread))
            thread->Unpause();
    }

    int PyBackgroundThread_RunJob(PyInterpreterState *interp,
                                  PyBackgroundJob *job)
    {
        if (interp->background_thread == NULL) {
            interp->background_thread = new PyBackgroundThread;
            interp->background_thread->Start();
        }
        return interp->background_thread->RunJob(job);
    }

    int
    PyBackgroundThread_ApplyFinishedJobs(
        PyBackgroundThread *thread, Py_ShouldBlock block)
    {
        return thread->ApplyFinishedJobs(block);
    }

    PyBackgroundJob*
    PyBackgroundThread_NewDummyJob()
    {
        struct DummyJob : PyBackgroundJob {
            virtual void Run(bool shutting_down)
            {}
            virtual void Apply()
            {}
        };
        return new DummyJob;
    }
}

#endif // WITH_LLVM
