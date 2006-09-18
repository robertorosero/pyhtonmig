"""XXX Use case tests:
    * cannot open files
        - test removal of open() as well as change to bz2.
    * block imports
        - built-ins
        - .pyc/.pyo
        - extension modules

"""
import interpreter

import unittest
from test import test_support
import sys
import __builtin__

simple_stmt = """
while True:
    2 + 3
    break
"""

test_sys_changed = """
import sys
if sys.version == 'test':
    to_return.append(True)
else:
    to_return.append(False)
"""

class BaseInterpTests(unittest.TestCase):

    def setUp(self):
        """Create a new interpreter."""

        self.interp = interpreter.Interpreter()


class BasicInterpreterTests(BaseInterpTests):

    def test_basic_expression(self):
        # Make sure that execucuting a simple expression does not die.
        self.interp.execute('2 + 3')

    def test_basic_statement(self):
        # Make sure executing a statement does not die.
        self.interp.execute(simple_stmt)


class BuiltinsTests(BaseInterpTests):

    """Test interpreter.Interpreter().builtins ."""

    def test_get(self):
        # Test the getting of 'builtins'.
        builtins = self.interp.builtins
        self.failUnless(isinstance(builtins, dict))
        self.failUnless('object' in builtins)

    def test_set(self):
        # Make sure setting 'builtins' can be done and has proper type
        # checking.
        self.interp.builtins = {}
        self.failUnlessRaises(TypeError, setattr, self.interp, 'builtins', [])

    def test_remove(self):
        # Make sure that something can be removed from built-ins.
        # XXX
        pass

    def test_add_remove(self):
        # Make sure you can add to the built-ins and then remove the same
        # object.
        self.interp.builtins['test'] = 'test'
        self.interp.execute('test')
        del self.interp.builtins['test']
        # XXX self.failUnlessRaises(XXX, interp.execute, 'test')


    def test_empty_builtins(self):
        # Make sure that emptying the built-ins truly make them non-existent.
        self.interp.builtins = {}
        # XXX self.failUnlessRaises(XXX, interp.execute, 'object')

    def test_copied(self):
        # Make sure built-ins are unique per interpreter.
        self.failUnless(self.interp.builtins is not __builtin__.__dict__)

        try:
            __builtin__.__dict__['test1'] = 'test1'
            self.failUnless('test1' not in self.interp.builtins)
        finally:
            del __builtin__.__dict__['test1']
        self.interp.builtins['test2'] = 'test2'
        self.failUnless('test2' not in __builtin__.__dict__)


class ModulesTests(BaseInterpTests):

    """Test interpreter.Interpreter().modules ."""

    def test_get(self):
        # Make sure a dict is returned.
        modules = self.interp.modules
        self.failUnless(isinstance(modules, dict))

    def test_set(self):
        # Make sure setting 'modules' can be done and has proper type checking.
        self.interp.modules = {}
        self.failUnlessRaises(TypeError, setattr, self.interp.modules, [])

    def test_mutation(self):
        # If a module is swapped for another one, make sure imports actually
        # use the new module entry.
        # XXX
        pass

    def test_adding(self):
        # If a module is added, make sure importing uses that module.
        # XXX
        pass

    def test_deleting(self):
        # Make sure that a module is re-imported if it is removed.
        # XXX
        pass

    def test_fresh(self):
        # Make sure that import Python modules are new instances.
        import token
        self.interp.execute("import token")
        self.failUnless(self.interp.modules['token'] is not token)


class SysDictTests(BaseInterpTests):

    """Test interpreter.Interpreter().sys_dict ."""

    def test_get(self):
        # Make sure a dict is returned.
        sys_dict = self.interp.sys_dict
        self.failUnless(isinstance(sys_dict, dict))
        self.failUnless('version' in sys_dict)

    def test_set(self):
        # Make sure sys_dict can be set to a new dict.
        sys_dict_copy = self.interp.sys_dict.copy()
        self.interp.sys_dict = sys_dict_copy
        self.failUnlessRaises(TypeError, setattr, self.interp, 'sys_dict', [])

    def test_mutating(self):
        # Changes to the dict should be reflected in the interpreter.
        sys_dict = self.interp.sys_dict
        sys_dict['version'] = 'test'
        interp_return = []
        self.interp.builtins['to_return'] = interp_return
        self.interp.execute(test_sys_changed)
        self.failUnless(interp_return[0])

    def test_deletion(self):
        # Make sure removing a value raises the proper exception when accessing
        # through the 'sys' module.
        del self.interp.sys_dict['version']
        # XXX self.failUnlessRaises(XXX, self.interp.execute,
        #                           'import sys; sys.version')

    def test_copied(self):
        # sys_dict should be unique per interpreter (including mutable data
        # structures).
        sys_dict = self.interp.sys_dict
        sys_dict['version'] = 'test'
        self.failUnless(sys.version != 'test')
        # XXX check mutable data structures
        sys_dict.setdefault('argv', []).append('test')
        self.failUnless(sys.argv[-1] != 'test')
        sys_dict['path'].append('test')
        self.failUnless(sys.path[-1] != 'test')


class PlaceHolder(BaseInterpTests):

    """Hold some tests that will need to pass at some point."""

    def test_file_restrictions(self):
        # You cannot open a file.
        del self.interp.builtins['open']
        try:
            self.interp.execute("open(%s, 'w')" % test_support.TESTFN)
        finally:
            try:
                os.remove(test_support.TESTFN)
            except OSError:
                pass
        # XXX bz2 module protection.

    def test_import_builtin(self):
        # Block importing built-in modules.
        pass

    def test_import_pyc(self):
        # Block importing .pyc files.
        pass

    def test_import_extensions(self):
        # Block importing extension modules.
        pass


def test_main():
    test_support.run_unittest(
            BasicInterpreterTests,
            BuiltinsTests,
            ModulesTests,
            SysDictTests,
    )


if __name__ == '__main__':
    test_main()
