import objcap
import unittest
from test import test_support

class TestClass(object):
    pass

class ObjectSubclasses(unittest.TestCase):

    """Test object.__subclasses__() move."""

    def test_removal(self):
        # Make sure __subclasses__ is no longer on 'object'.
        self.failUnless(not hasattr(object, '__subclasses__'))

    def test_argument(self):
        # Make sure only proper arguments are accepted.
        self.failUnlessRaises(TypeError, objcap.subclasses, object())

    def test_result(self):
        # Check return values.
        self.failUnlessEqual(objcap.subclasses(TestClass), [])
        self.failUnless(objcap.subclasses(object))


class FileInitTests(unittest.TestCase):

    """Test removal of file type initializer and addition of file_init()."""

    def test_removal(self):
        # Calling constructor with any arguments should fail.
        self.failUnlessRaises(TypeError, file, test_support.TESTFN, 'w')

    def test_file_subclassing(self):
        # Should still be possible to subclass 'file'.

        class FileSubclass(file):
            pass

        self.failUnless(FileSubclass())

    def test_file_init(self):
        # Should be able to use file_init() to initialize instances of 'file'.
        ins = file()
        try:
            objcap.file_init(ins, test_support.TESTFN, 'w')
        finally:
            ins.close()

        ins = file()
        try:
            objcap.file_init(ins, test_support.TESTFN)
        finally:
            ins.close()

        ins = file()
        try:
            objcap.file_init(ins, test_support.TESTFN, 'r', 0)
        finally:
            ins.close()


def test_main():
    test_support.run_unittest(
        ObjectSubclasses,
        FileInitTests,
    )


if __name__ == '__main__':
    test_main()
