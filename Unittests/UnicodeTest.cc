/// Tests for Python's unicode support.
#include "Python.h"

#include "gtest/gtest.h"
#include "pytest.h"

// Grab common setup/teardown routines.
class UnicodeTest : public ::testing::Test {
    PythonSetupTeardown setup_teardown_;
};


// issue4122: Undefined reference to _Py_ascii_whitespace on Windows.
// Just use the macro and check that it compiles.
TEST_F(UnicodeTest, IsSpace)
{
    EXPECT_FALSE(Py_UNICODE_ISSPACE(25));
}


// Python strings ending with '\0' should not be equivalent to their C
// counterparts in PyUnicode_CompareWithASCIIString.
TEST_F(UnicodeTest, CompareWithAscii)
{
    PyObject *py_s = PyUnicode_FromStringAndSize("str\0", 4);
    ASSERT_TRUE(py_s != NULL);

    EXPECT_NE(0, PyUnicode_CompareWithASCIIString(py_s, "str"))
            << "Python string ending in NULL "
               "should not compare equal to c string.";

    Py_DECREF(py_s);
}