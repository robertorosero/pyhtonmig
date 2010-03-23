/// Tests for code in Objects/object.c.
#include "Python.h"

#include "gtest/gtest.h"
#include "pytest.h"

// Grab common setup/teardown routines.
class ObjectTest : public ::testing::Test {
    PythonSetupTeardown setup_teardown_;
};


// Test passing NULL to PyObject_Str. This should produce a "<NULL>" string.
TEST_F(ObjectTest, PyObject_Str_NullArg)
{
    PyObject *str = PyObject_Str(NULL);
    ASSERT_TRUE(str != NULL);

    EXPECT_EQ(0, PyUnicode_CompareWithASCIIString(str, "<NULL>"));

    Py_DECREF(str);
}