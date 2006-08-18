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


def test_main():
    test_support.run_unittest(
        ObjectSubclasses
    )


if __name__ == '__main__':
    test_main()
