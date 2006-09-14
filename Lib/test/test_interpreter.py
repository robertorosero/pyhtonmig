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

    def test_effect(self):
        # Make sure that setting 'builtin's actually affects the built-in
        # namespace.
        self.interp.builtins['msg'] = "hello"
        self.interp.execute("msg")
        self.interp.execute("import __builtin__; __builtin__.__dict__['test1'] = 'test1'")
        self.failUnless('test1' in self.interp.builtins)
        del self.interp.builtins['object']
        #self.failUnlessRaises(XXX, interp.execute, 'object')

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

    def test_effect(self):
        # Make sure mutating 'modules' has proper result.
        # XXX Insert value in 'module', have sub-interpreter set into
        # __builtin__, and check that same object.
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

    def test_effect(self):
        # Changes to the dict should be reflected in the interpreter.
        sys_dict = self.interp.sys_dict
        sys_dict['version'] = 'test'
        interp_return = []
        self.interp.builtins['to_return'] = interp_return
        self.interp.execute(test_sys_changed)
        self.failUnless(interp_return[0])

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


def test_main():
    test_support.run_unittest(
            BasicInterpreterTests,
            BuiltinsTests,
            ModulesTests,
            SysDictTests,
    )


if __name__ == '__main__':
    test_main()
