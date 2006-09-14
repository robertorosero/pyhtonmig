"""XXX
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


def test_main():
    test_support.run_unittest(
            BasicInterpreterTests,
            BuiltinsTests,
            ModulesTests,
    )


if __name__ == '__main__':
    test_main()
