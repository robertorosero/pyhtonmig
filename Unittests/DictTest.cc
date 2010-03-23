#include "Python.h"

#include "gtest/gtest.h"
#include "pytest.h"

// Value-parameterized testing, gtest-style. See
// http://code.google.com/p/googletest/wiki/GoogleTestAdvancedGuide for more.
class DictIterationTest : public ::testing::TestWithParam<int> {
    PythonSetupTeardown setup_teardown_;
};

TEST_P(DictIterationTest, TestIteration)
{
    Py_ssize_t pos = 0, iterations = 0;
    int i;
    int count = this->GetParam();
    PyObject *v, *k;

    PyObject *dict = PyDict_New();
    ASSERT_TRUE(dict != NULL);

    for (i = 0; i < count; i++) {
        v = PyLong_FromLong(i);
        EXPECT_GE(PyDict_SetItem(dict, v, v), 0);
        Py_DECREF(v);
    }

    while (PyDict_Next(dict, &pos, &k, &v)) {
        iterations++;

        i = PyLong_AS_LONG(v) + 1;
        PyObject *o = PyLong_FromLong(i);
        EXPECT_TRUE(o != NULL);
        EXPECT_GE(PyDict_SetItem(dict, k, o), 0);

        Py_DECREF(o);
    }

    Py_DECREF(dict);

    EXPECT_EQ(count, iterations) << "dict iteration went wrong";
}

INSTANTIATE_TEST_CASE_P(DictIterationTest,
                        DictIterationTest,
                        ::testing::Range(0, 200, /* step= */ 10));
