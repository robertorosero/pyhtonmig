#include "Python.h"
#include "pythread.h"
#include "bg_thread.h"

#include "gtest/gtest.h"
#include "pytest.h"

#include <deque>

namespace {

class BgThreadTest : public ::testing::Test {
    PythonSetupTeardown setup_teardown_;
};

TEST_F(BgThreadTest, StartStop)
{
    PyThreadState *tstate = PyThreadState_GET();
    PyBackgroundThread thread;
    EXPECT_EQ(tstate, PyThreadState_GET());
    thread.Start();
    EXPECT_EQ(tstate, PyThreadState_GET());
    thread.Terminate();
    EXPECT_EQ(tstate, PyThreadState_GET());
}

struct TestJob : public PyBackgroundJob {
    TestJob(int &phases_run)
        : phases_run_(phases_run)
    {
    }

    virtual void Run(bool shutdown)
    {
        phases_run_++;
    }

    virtual void Apply()
    {
        phases_run_++;
    }

private:
    int &phases_run_;
};

TEST_F(BgThreadTest, PhasesRun)
{
    PyBackgroundThread thread;
    thread.Start();
    int phases_run = 0;
    thread.RunJobAndWait(new TestJob(phases_run));
    EXPECT_EQ(1, phases_run);
    thread.ApplyFinishedJobs(Py_BLOCK);
    EXPECT_EQ(2, phases_run);
    thread.Terminate();
}

TEST_F(BgThreadTest, EvalLoopAppliesJobs)
{
    PyBackgroundThread thread;
    // Tell the eval loop which PyBackgroundThread to listen to.
    PyThreadState_GET()->interp->background_thread = &thread;
    thread.Start();
    int phases_run = 0;
    thread.RunJobAndWait(new TestJob(phases_run));
    EXPECT_EQ(1, phases_run);
    PyRun_SimpleString("for i in range(3): i += 1");
    EXPECT_EQ(2, phases_run);
    thread.Terminate();
    // Don't free the background thread in PyInterpreterState
    // destruction though.
    PyThreadState_GET()->interp->background_thread = NULL;
}

TEST_F(BgThreadTest, RunAndApplyAppliesJobs)
{
    PyBackgroundThread thread;
    thread.Start();
    int phases_run = 0;
    thread.RunJobAndApply(new TestJob(phases_run));
    EXPECT_EQ(2, phases_run);
    thread.Terminate();
}

TEST_F(BgThreadTest, TerminationWaitsForAndAppliesActiveJobs)
{
    PyBackgroundThread thread;
    thread.Start();
    int phases_run = 0;
    thread.RunJob(new TestJob(phases_run));
    thread.Terminate();
    EXPECT_EQ(2, phases_run);
}

TEST_F(BgThreadTest, AfterTerminationJobsRunImmediately)
{
    PyBackgroundThread thread;
    thread.Start();
    thread.Terminate();
    int phases_run = 0;
    thread.RunJob(new TestJob(phases_run));
    EXPECT_EQ(2, phases_run);
}

TEST_F(BgThreadTest, HotFunctionStartsThread)
{
    PyRun_SimpleString("def hot(): pass\nfor _ in range(10**4 + 1): hot()");
    EXPECT_TRUE(NULL != PyThreadState_GET()->interp->background_thread);
}

}  // anonymous namespace
