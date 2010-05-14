#include "Python.h"

#include "gtest/gtest.h"
#include "pytest.h"

namespace {

class ThreadTest : public ::testing::Test {
    PythonSetupTeardown setup_teardown_;
};

TEST_F(ThreadTest, InitThreadsAcrossFinalize)
{
    PyEval_InitThreads();

    Py_Finalize();
    // Make it more likely that we get a different address for the
    // PyThreadState in the new interpreter.
    void *p = malloc(sizeof(PyThreadState));
    Py_Initialize();
    free(p);

    PyEval_InitThreads();
    Py_BEGIN_ALLOW_THREADS;  // Used to crash.
    Py_END_ALLOW_THREADS;
}

}  // anonymous namespace
