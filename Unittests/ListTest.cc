#include "Python.h"

#include "gtest/gtest.h"
#include "pytest.h"

// Grab common setup/teardown routines.
class ListTest : public ::testing::Test {
    PythonSetupTeardown setup_teardown_;
};

// Regression test for http://bugs.python.org/issue232008 (segfault in
// PyList_Reverse).
TEST_F(ListTest, Bug232008)
{
    int i;
    const int list_size = 30;

    PyObject *list = PyList_New(list_size);
    ASSERT_TRUE(list != NULL);

    // list = range(list_size)
    for (i = 0; i < list_size; ++i) {
        PyObject *an_int = PyLong_FromLong(i);
        EXPECT_TRUE(an_int != NULL);
        PyList_SET_ITEM(list, i, an_int);
    }

    // list.reverse(), via PyList_Reverse().
    i = PyList_Reverse(list);  // Should not blow up!
    EXPECT_EQ(0, i);

    // Check that list == range(29, -1, -1) now.
    for (i = 0; i < list_size; ++i) {
        PyObject *an_int = PyList_GET_ITEM(list, i);
        EXPECT_EQ(list_size - 1 - i, PyLong_AS_LONG(an_int))
                << "reverse screwed up";
    }
    Py_DECREF(list);
}
