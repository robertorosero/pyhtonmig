""" Things to protect (and thus test) against:
* Importing
    + built-ins
    + .pyc/.pyo
    + extension modules
* File access
    + open()
* Evil methods
    + __del__
    + __str__ (for exceptions)
    + properties
    + something that raises SystemExit

"""
import interpreter

import unittest
from test import test_support
import sys
import __builtin__
import StringIO

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

test_builtins_contains = """
import __builtin__
_return.append(__builtin__.__dict__.__contains__("%s"))
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

    """Test interpreter.Interpreter().builtins() ."""

    def test_get(self):
        # Test the getting of 'builtins'.
        builtins = self.interp.builtins()
        self.failUnless(isinstance(builtins, dict))
        self.failUnless('object' in builtins)

    def test_remove(self):
        # Make sure that something can be removed from built-ins.
        builtins = self.interp.builtins()
        del builtins['open']
        _return = []
        builtins['_return'] = _return
        self.interp.execute(test_builtins_contains % 'open')
        self.failUnless(not _return[-1])

    def test_add_remove(self):
        # Make sure you can add to the built-ins and then remove the same
        # object.
        self.interp.builtins()['test'] = 'test'
        _return = []
        self.interp.builtins()['_return'] = _return
        self.interp.execute(test_builtins_contains % 'test')
        self.failUnless(_return[-1])
        del self.interp.builtins()['test']
        self.interp.execute(test_builtins_contains % 'test')
        self.failUnless(not _return[-1])
        
    def test_copied(self):
        # Make sure built-ins are unique per interpreter.
        master_id = id(__builtin__.__dict__)
        _return = []
        self.interp.builtins()['_return'] = _return
        self.interp.execute('import __builtin__;'
                            '_return.append(id(__builtin__.__dict__))')
        self.failUnless(_return[-1] != master_id)
        
                            
class ModulesTests(BaseInterpTests):

    """Test interpreter.Interpreter().modules ."""

    def test_get(self):
        # Make sure a dict is returned.
        modules = self.interp.modules
        self.failUnless(isinstance(modules, dict))
        master_id = id(modules)
        _return = []
        self.interp.builtins()['_return'] = _return
        self.interp.execute('import sys; _return.append(id(sys.modules))')
        self.failUnlessEqual(_return[-1], master_id)

    def test_set(self):
        # Make sure setting 'modules' can be done and has proper type checking.
        self.interp.modules = {}
        self.failUnlessRaises(TypeError, setattr, self.interp.modules, [])

    def test_mutation(self):
        # If a module is added, make sure importing uses that module.
        self.interp.modules['test1'] = 'test1'
        _return = []
        self.interp.builtins()['_return'] = _return
        self.interp.execute('import test1; _return.append(test1)')
        self.failUnlessEqual(_return[-1], 'test1')
        self.interp.modules['test1'] = 'test2'
        self.interp.execute('import test1; _return.append(test1)')
        self.failUnlessEqual(_return[-1], 'test2')

    def test_deleting(self):
        # Make sure that a module is re-imported if it is removed.
        self.failUnless('token' not in self.interp.modules)
        self.interp.execute('import token')
        del self.interp.modules['token']
        self.interp.execute('import token')
        
    def test_replacing(self):
        # Replacing with a new dict should work.
        new_modules = self.interp.modules.copy()
        self.interp.modules = new_modules
        _return = []
        self.interp.builtins()['_return'] = _return
        self.interp.execute('import sys; _return.append(id(sys.modules))')
        self.failUnlessEqual(id(new_modules), _return[-1])

    def test_fresh(self):
        # Make sure that imported Python modules are new instances.
        import token
        token.test = 'test'
        _return = []
        self.interp.builtins()['_return'] = _return
        self.interp.execute("import token;"
                            "_return.append(hasattr(token, 'test'))")
        self.failUnless(not _return[-1])
        
    def test_not_cached(self):
        # Make sure that 'modules' dict is not cached.
        builtin = self.interp.modules['__builtin__']
        main = self.interp.modules['__main__']
        self.interp.execute("import token")
        self.interp.modules = {}
        self.interp.modules['__builtin__'] = builtin
        self.interp.modules['__main__'] = main
        self.interp.execute("import token")
        self.failUnless('token' in self.interp.modules)
        

class SysDictTests(BaseInterpTests):

    """Test interpreter.Interpreter().sys_dict ."""

    def test_get(self):
        # Make sure a dict is returned.
        sys_dict = self.interp.sys_dict()
        self.failUnless(isinstance(sys_dict, dict))
        self.failUnless('version' in sys_dict)

    def test_mutating(self):
        # Changes to the dict should be reflected in the interpreter.
        sys_dict = self.interp.sys_dict()
        sys_dict['version'] = 'test'
        interp_return = []
        self.interp.builtins()['to_return'] = interp_return
        self.interp.execute(test_sys_changed)
        self.failUnless(interp_return[-1])

    def test_deletion(self):
        # Make sure removing a value raises the proper exception when accessing
        # through the 'sys' module.
        del self.interp.sys_dict()['version']
        stdout, stderr = self.interp.redirect_output()
        self.failUnlessRaises(RuntimeError, self.interp.execute,
                              'import sys; sys.version')
        self.failUnless(self.interp.exc_matches(AttributeError))

    def test_copied(self):
        # sys_dict should be unique per interpreter (including mutable data
        # structures).
        sys_version = sys.version
        self.interp.sys_dict()['version'] = 'test'
        reload(sys)
        self.failUnlessEqual(sys.version, sys_version)
        _return = []
        self.interp.builtins()['_return'] = _return
        self.interp.execute("import sys; _return.append(sys.version)")
        self.failUnlessEqual(_return[-1], 'test')
        

class InputOutputTests(BaseInterpTests):

    """Test stdin/stdout/stderr fiddling."""
    
    def test_redirect_output_defaults(self):
        # Make sure it sets stdout and stderr.
        stdout, stderr = self.interp.redirect_output()
        self.interp.execute("print 'test'")
        self.failUnlessEqual("test\n", stdout.getvalue())
        self.failUnless(not stderr.getvalue())
        self.interp.execute(r"import sys; sys.stderr.write('test\n')")
        self.failUnlessEqual('test\n', stderr.getvalue())
        
    def test_redirect_output_arguments(self):
        # Test passing in arguments to redirect_output().
        new_stdout = StringIO.StringIO()
        new_stderr = StringIO.StringIO()
        used_stdout, used_stderr = self.interp.redirect_output(new_stdout,
                                                                new_stderr)
        self.failUnless(used_stdout is new_stdout)
        self.failUnless(used_stderr is new_stderr)
        self.failUnlessRaises(RuntimeError, self.interp.execute, '=')
        self.failUnlessEqual(new_stderr.getvalue(), used_stderr.getvalue())
        self.interp.execute("print 'test'")
        self.failUnlessEqual(new_stdout.getvalue(), used_stdout.getvalue())
        
        
class ExceptionsTests(BaseInterpTests):

    """Test exception usage."""
    
    def test_execute_failure(self):
        # RuntimeError should be raised when something goes bad in execute().
        stdout, stderr = self.interp.redirect_output()
        self.failUnlessRaises(RuntimeError, self.interp.execute, "=")
        
    def test_exc_matches(self):
        # Test exc_matches().
        stdout, stderr = self.interp.redirect_output()
        self.failUnlessRaises(RuntimeError, self.interp.execute, '=')
        self.failUnless(self.interp.exc_matches(SyntaxError))
        self.failUnless(not self.interp.exc_matches(TypeError))
        
    def test_exception_cleared(self):
        # No exception should be set after a successful execution.
        stdout, stderr = self.interp.redirect_output()
        self.failUnlessRaises(RuntimeError, self.interp.execute, '=')
        self.interp.execute('2 + 3')
        self.failUnlessRaises(LookupError, self.interp.exc_matches, Exception)
        
    def test_multiple_exc_checks(self):
        # Be able to check the exception multiple times.
        stdout, stderr = self.interp.redirect_output()
        self.failUnlessRaises(RuntimeError, self.interp.execute, '=')
        for x in range(2):
            self.failUnless(self.interp.exc_matches(SyntaxError))
            
    def test_SystemExit_safe(self):
        # Raising SystemExit should not cause the process to exit.
        self.failUnlessRaises(RuntimeError, self.interp.execute,
                                "raise SystemExit")
        self.failUnless(self.interp.exc_matches(SystemExit))
        self.failUnlessRaises(RuntimeError, self.interp.execute, "exit()")
        self.failUnless(self.interp.exc_matches(SystemExit))
        

def test_main():
    test_support.run_unittest(
            BasicInterpreterTests,
            BuiltinsTests,
            ModulesTests,
            SysDictTests,
            InputOutputTests,
            ExceptionsTests,
    )


#if __name__ == '__main__':
    #test_main()
