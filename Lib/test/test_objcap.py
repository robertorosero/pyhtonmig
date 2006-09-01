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


class CodeNewTests(unittest.TestCase):

    """Test code_new()."""

    def test_all(self):
        # Originally taken from test_new.py .

        def f(a): pass

        c = f.func_code
        argcount = c.co_argcount
        nlocals = c.co_nlocals
        stacksize = c.co_stacksize
        flags = c.co_flags
        codestring = c.co_code
        constants = c.co_consts
        names = c.co_names
        varnames = c.co_varnames
        filename = c.co_filename
        name = c.co_name
        firstlineno = c.co_firstlineno
        lnotab = c.co_lnotab
        freevars = c.co_freevars
        cellvars = c.co_cellvars

        d = objcap.code_new(argcount, nlocals, stacksize, flags, codestring,
                     constants, names, varnames, filename, name,
                     firstlineno, lnotab, freevars, cellvars)

        # test backwards-compatibility version with no freevars or cellvars
        d = objcap.code_new(argcount, nlocals, stacksize, flags, codestring,
                     constants, names, varnames, filename, name,
                     firstlineno, lnotab)

        try: # this used to trigger a SystemError
            d = objcap.code_new(-argcount, nlocals, stacksize, flags, codestring,
                         constants, names, varnames, filename, name,
                         firstlineno, lnotab)
        except ValueError:
            pass
        else:
            raise test_support.TestFailed(
                    "negative co_argcount didn't trigger an exception")

        try: # this used to trigger a SystemError
            d = objcap.code_new(argcount, -nlocals, stacksize, flags, codestring,
                         constants, names, varnames, filename, name,
                         firstlineno, lnotab)
        except ValueError:
            pass
        else:
            raise test_support.TestFailed(
                    "negative co_nlocals didn't trigger an exception")

        try: # this used to trigger a Py_FatalError!
            d = objcap.code_new(argcount, nlocals, stacksize, flags, codestring,
                         constants, (5,), varnames, filename, name,
                         firstlineno, lnotab)
        except TypeError:
            pass
        else:
            raise TestFailed, "non-string co_name didn't trigger an exception"

        # new.code used to be a way to mutate a tuple...
        class S(str): pass
        t = (S("ab"),)
        d = objcap.code_new(argcount, nlocals, stacksize, flags, codestring,
                     constants, t, varnames, filename, name,
                     firstlineno, lnotab)
        self.failUnless(type(t[0]) is S, "eek, tuple changed under us!")



def test_main():
    test_support.run_unittest(
        ObjectSubclasses,
        FileInitTests,
        CodeNewTests,
    )


if __name__ == '__main__':
    test_main()
