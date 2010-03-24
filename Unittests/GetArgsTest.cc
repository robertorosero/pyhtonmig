/// Tests for Python/getargs.c.
#include "Python.h"

#include "gtest/gtest.h"
#include "pytest.h"

// Grab common setup/teardown routines.
class GetArgsTest : public ::testing::Test {
    PythonSetupTeardown setup_teardown_;
};


// Test that formats can begin with '|'. See issue #4720.
TEST_F(GetArgsTest, EmptyFormat)
{
    PyObject *tuple = PyTuple_New(0);
    PyObject *dict = PyDict_New();
    ASSERT_TRUE(tuple != NULL);
    ASSERT_TRUE(dict != NULL);

    // Test PyArg_ParseTuple.
    EXPECT_GE(PyArg_ParseTuple(tuple, "|:EmptyFormat"), 0);

    // Same test, for PyArg_ParseTupleAndKeywords.
    static char *kwlist[] = {NULL};
    int result = PyArg_ParseTupleAndKeywords(tuple, dict,
                                             "|:EmptyFormat",
                                             kwlist);
    EXPECT_GE(result, 0);

    Py_XDECREF(tuple);
    Py_XDECREF(dict);
}


// Unicode strings should be accepted for the s and z format codes.
TEST_F(GetArgsTest, FormatCodes_s_And_z)
{
    PyObject *tuple, *obj;
    char *value;

    tuple = PyTuple_New(1);
    ASSERT_TRUE(tuple != NULL);

    obj = PyUnicode_Decode("t\xeate", strlen("t\xeate"), "latin-1", NULL);
    ASSERT_TRUE(obj != NULL);

    PyTuple_SET_ITEM(tuple, 0, obj);

    // These two blocks used to raise a TypeError:
    // "argument must be string without null bytes, not str"
    EXPECT_GE(PyArg_ParseTuple(tuple, "s:FormatCodes_s_And_z", &value), 0);
    EXPECT_GE(PyArg_ParseTuple(tuple, "z:FormatCodes_s_And_z", &value), 0);

    Py_XDECREF(tuple);
}


// Issue #7414: for PyArg_ParseTupleAndKeywords, 'C' code wasn't being
// skipped properly in skipitem().
TEST_F(GetArgsTest, FormatCode_C)
{
    int a = 0, b = 0, result;
    char *kwlist[] = {"a", "b", NULL};
    PyObject *tuple = NULL, *dict = NULL, *b_str;

    tuple = PyTuple_New(0);
    dict = PyDict_New();
    b_str = PyUnicode_FromString("b");
    ASSERT_TRUE(tuple != NULL);
    ASSERT_TRUE(dict != NULL);
    ASSERT_TRUE(b_str != NULL);

    result = PyDict_SetItemString(dict, "b", b_str);
    Py_XDECREF(b_str);
    EXPECT_GE(result, 0);

    result = PyArg_ParseTupleAndKeywords(tuple, dict, "|CC", kwlist, &a, &b);
    EXPECT_GE(result, 0);
    EXPECT_EQ(0, a) << "C format code not skipped properly";
    EXPECT_EQ('b', b) << "C format code returned wrong value";

    Py_XDECREF(dict);
    Py_XDECREF(tuple);
}

// Test the u and u# codes for PyArg_ParseTuple.
TEST_F(GetArgsTest, FormatCode_u)
{
    PyObject *tuple, *obj;
    Py_UNICODE *value;
    Py_ssize_t len;

    tuple = PyTuple_New(1);
    ASSERT_TRUE(tuple != NULL);

    obj = PyUnicode_Decode("test", strlen("test"), "ascii", NULL);
    ASSERT_TRUE(obj != NULL);

    PyTuple_SET_ITEM(tuple, 0, obj);

    value = 0;
    EXPECT_GE(PyArg_ParseTuple(tuple, "u:FormatCode_u", &value), 0);
    EXPECT_EQ(value, PyUnicode_AS_UNICODE(obj));

    value = 0;
    EXPECT_GE(PyArg_ParseTuple(tuple, "u#:FormatCode_u", &value, &len), 0);
    EXPECT_EQ(value, PyUnicode_AS_UNICODE(obj));
    EXPECT_EQ(len, PyUnicode_GET_SIZE(obj));

    Py_DECREF(tuple);
}

#ifdef HAVE_LONG_LONG
// Test the L code for PyArg_ParseTuple. This should deliver a PY_LONG_LONG
// for both long and int arguments.
TEST_F(GetArgsTest, FormatCode_L)
{
    PY_LONG_LONG value;

    PyObject *tuple = PyTuple_New(1);
    ASSERT_TRUE(tuple != NULL);

    PyObject *num = PyLong_FromLong(42);
    ASSERT_TRUE(num != NULL);

    PyTuple_SET_ITEM(tuple, 0, num);

    value = -1;
    EXPECT_GE(PyArg_ParseTuple(tuple, "L:FormatCode_L", &value), 0);
    EXPECT_EQ(42, value) << "L code returned wrong value for long 42";

    Py_DECREF(num);
    num = PyLong_FromLong(42);
    EXPECT_TRUE(num != NULL);

    PyTuple_SET_ITEM(tuple, 0, num);

    value = -1;
    EXPECT_GE(PyArg_ParseTuple(tuple, "L:FormatCode_L", &value), 0);
    EXPECT_EQ(42, value) << "L code returned wrong value for int 42";

    Py_DECREF(tuple);
}
#endif  // HAVE_LONG_LONG


// Test the K code for PyArg_ParseTuple.
TEST_F(GetArgsTest, FormatCode_K)
{
    unsigned long value;

    PyObject *tuple = PyTuple_New(1);
    ASSERT_TRUE(tuple != NULL);

    // A number larger than ULONG_MAX even on 64-bit platforms.
    PyObject *num = PyLong_FromString("FFFFFFFFFFFFFFFFFFFFFFFF", NULL, 16);
    ASSERT_TRUE(num != NULL);

    value = PyLong_AsUnsignedLongMask(num);
    EXPECT_EQ(ULONG_MAX, value) << "wrong value for long 0xFFF...FFF";

    PyTuple_SET_ITEM(tuple, 0, num);

    value = 0;
    EXPECT_GE(PyArg_ParseTuple(tuple, "k:FormatCode_k", &value), 0);
    EXPECT_EQ(ULONG_MAX, value) << "k code gave wrong value for 0xFFF...FFF";

    Py_DECREF(num);
    num = PyLong_FromString("-FFFFFFFF000000000000000042", NULL, 16);
    ASSERT_TRUE(num != NULL);

    value = PyLong_AsUnsignedLongMask(num);
    EXPECT_EQ((unsigned long)-0x42, value)
            << "wrong value for long -0xFFF..000042";

    PyTuple_SET_ITEM(tuple, 0, num);

    value = 0;
    EXPECT_GE(PyArg_ParseTuple(tuple, "k:FormatCode_k", &value), 0);
    EXPECT_EQ((unsigned long)-0x42, value)
            << "k code returned wrong value for long -0xFFF..000042";

    Py_DECREF(tuple);
}


// Test the Z and Z# codes for PyArg_ParseTuple.
TEST_F(GetArgsTest, FormatCode_Z)
{
    PyObject *tuple = PyTuple_New(2);
    ASSERT_TRUE(tuple != NULL);

    PyObject *obj = PyUnicode_FromString("test");
    PyTuple_SET_ITEM(tuple, 0, obj);
    Py_INCREF(Py_None);
    PyTuple_SET_ITEM(tuple, 1, Py_None);

    // Swap values on purpose.
    Py_UNICODE *value1 = NULL;
    Py_UNICODE *value2 = PyUnicode_AS_UNICODE(obj);

    /* Test Z for both values. */
    EXPECT_GE(PyArg_ParseTuple(tuple, "ZZ:FormatCode_Z", &value1, &value2), 0);
    EXPECT_EQ(value1, PyUnicode_AS_UNICODE(obj))
            << "Z code returned wrong value for 'test'";
    EXPECT_TRUE(value2 == NULL) << "Z code returned wrong value for None";

    value1 = NULL;
    value2 = PyUnicode_AS_UNICODE(obj);
    Py_ssize_t len1 = -1;
    Py_ssize_t len2 = -1;

    // Test Z# for both values.
    EXPECT_GE(PyArg_ParseTuple(tuple, "Z#Z#:FormatCode_Z", &value1, &len1,
                               &value2, &len2), 0);
    EXPECT_EQ(PyUnicode_AS_UNICODE(obj), value1);
    EXPECT_EQ(PyUnicode_GET_SIZE(obj), len1);
    EXPECT_TRUE(value2 == NULL);
    EXPECT_EQ(0, len2);

    Py_DECREF(tuple);

}
