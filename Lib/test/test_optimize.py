import dis
import re
import sys
from io import StringIO
import unittest
import ast
import types

def disassemble(func):
    f = StringIO()
    tmp = sys.stdout
    sys.stdout = f
    dis.dis(func)
    sys.stdout = tmp
    result = f.getvalue()
    f.close()
    return result

def dis_single(line):
    return disassemble(compile(line, '', 'single'))

def compile_fn(src, fnname='f'):
    """
    Compile src to a Module, return an (ast.FunctionDef, ast.Module) pair
    where the former is the named function
    """
    node = ast.parse(src)
    #print(ast.dump(node))
    for f in node.body:
        if isinstance(f, ast.FunctionDef):
            if f.name == fnname:
                return f, node
    raise ValueError('Could not find function: %r' % fnname)

def get_code_for_fn(co, fnname):
    for const in co.co_consts:
        if isinstance(const, types.CodeType):
            if const.co_name == fnname:
                return const
    raise ValueError('code for %r not found' % fnname)


class TestFramework(unittest.TestCase):
    # Ensure that invoking the optimizer is working:
    def test_eval(self):
        self.assertEqual(eval('42'), 42)

class TestInlining(unittest.TestCase):
    def assertIsInlinable(self, src, fnname='f'):
        from __optimizer__ import fn_is_inlinable
        fn, mod = compile_fn(src)
        self.assert_(fn_is_inlinable(fn, mod),
                     msg='Unexpectedly not inlinable: %s\n%r' % (src, ast.dump(fn)))

    def assertIsNotInlinable(self, src, fnname='f'):
        from __optimizer__ import fn_is_inlinable
        fn, mod = compile_fn(src)
        self.assert_(not fn_is_inlinable(fn, mod),
                     msg='Unexpectedly inlinable: %s\n%r' % (src, ast.dump(fn)))

    def compile_to_code(self, src, fnname):
        # Compile the given source code, returning the code object for the given function name
        co = compile(src, 'test_optimize.py', 'exec')
        return get_code_for_fn(co, fnname)

    def test_simple_inlining(self):
        src = (
'''
def function_to_be_inlined(x, y, z):
    # this currently needs to exist to enable the inliner
    pass
sep = '-'
def f(x, y, z):
    return (2 * x * y) + (sep * z)
def callsite():
    f('foo', 3, 16)
''')
        self.assertIsInlinable(src)
        callsite = self.compile_to_code(src, 'callsite')
        asm = disassemble(callsite)
        self.assertIn('JUMP_IF_SPECIALIZABLE', asm)
        self.assertIn("('foofoofoofoofoofoo')", asm)
        self.assertIn("(sep)", asm)



    def test_copy_propagation(self):
        src = (
'''
def function_to_be_inlined(a, b, c, d):
    return(a + b + c + d)
def callsite():
    print(function_to_be_inlined('fee', 'fi', 'fo', 'fum'))
''')
        callsite = self.compile_to_code(src, 'callsite')
        asm = disassemble(callsite)
        # print(asm)
        self.assertIn('JUMP_IF_SPECIALIZABLE', asm)
        self.assertIn("('feefifofum')", asm)

    def test_keyword_args(self):
        src = (
'''
def f(x, y, z=42):
    return 42
''')
        self.assertIsNotInlinable(src)

    def test_star_args(self):
        src = ('''
def f(x, y, *args):
    return [x, y] + args
''')
        self.assertIsNotInlinable(src)


    def test_kwargs(self):
        src = (
'''
def f(x, y, **kwargs):
    return kwargs
''')
        self.assertIsNotInlinable(src)

    def test_decorators(self):
        src = (
'''
@login_required
def f(x, y, z):
    return 42
''')
        self.assertIsNotInlinable(src)

    def test_imports(self):
        src = (
'''
def f(x, y, z):
    import os
    return 42
''')
        self.assertIsNotInlinable(src)

    def test_generators(self):
        src = (
'''
def f(x, y, z):
    yield 42
''')
        self.assertIsNotInlinable(src)

    def test_complex_return_logic(self):
        # Only simple use of "return" for now
        src = (
'''
def f(x, y, z):
    if x:
        return 42
''')
        self.assertIsNotInlinable(src)


    def test_free_and_cell_variables(self):
        # No free/cell variables
        src = (
'''
def f(x, y, z):
    def g(p, q):
        return x*p + y*q + z
    return g
''')
        self.assertIsNotInlinable(src)

    def test_reassignment_of_function_name(self):
        src = (
'''
def f(x):
    return 42
f = len
''')
        self.assertIsNotInlinable(src)



    def not_test_simple(self):
        src = '''
my_global = 34

def function_to_be_inlined(x, y, z):
    return (2 * x * y) + z + my_global
print(function_to_be_inlined(3, 4, 5))
'''
        asm = disassemble(src)
        print(asm)
        # FIXME: verify that the inlined callsite actually works!

    def test_double(self):
        src = '''
def function_to_be_inlined():
    pass
def f(x, y, z):
    return (2 * x * y) + z
def g(x, y, z):
    return (f(x, y, 3 * z)
            - f(x + 1, y * 5, z -2))
def h(x, y):
    return g(x, y, 0)
'''
        co = compile(src, 'test_optimize.py', 'exec')
        g = get_code_for_fn(co, 'g')
        h = get_code_for_fn(co, 'h')
        g_asm = disassemble(g)
        h_asm = disassemble(h)
        #print(disassemble(co))
        #print('\n\ng')
        #print(g_asm)
        self.assertIn('JUMP_IF_SPECIALIZABLE', g_asm)
        #print('\n\nh')
        #print(h_asm)
        self.assertIn('JUMP_IF_SPECIALIZABLE', h_asm)

    def test_ignore_implicit_return(self):
        src = '''
my_global = 34
def function_to_be_inlined(x, y, z):
    print(2 * x * y) + z + my_global
def callsite():
    function_to_be_inlined(3, 4, 5)
'''
        callsite = self.compile_to_code(src, 'callsite')
        asm = disassemble(callsite)
        #print(asm)
        # FIXME: actually test it

    def test_call_call(self):
        # seen in copy.py:
        src = '''
def _copy_with_constructor(x):
    return type(x)(x)
'''
        fn = self.compile_to_code(src, '_copy_with_constructor')
        asm = disassemble(fn)

    def test_global(self):
        # Taken from tempfile.py:
        src = '''
tempdir = None

def gettempdir():
    """Accessor for tempfile.tempdir."""
    global tempdir
    if tempdir is None:
        _once_lock.acquire()
        try:
            if tempdir is None:
                tempdir = _get_default_tempdir()
        finally:
            _once_lock.release()
    return tempdir

def callsite():
    return gettempdir()
'''
        fn = self.compile_to_code(src, 'callsite')
        asm = disassemble(fn)
        #print(asm)
        self.assertIn('JUMP_IF_SPECIALIZABLE', asm)
        self.assertIn('(tempdir)', asm)

    def test_del_local(self):
        # Adapted from tempfile.py:
        src = (
'''
def f():
    fd = _os.open(filename, _bin_openflags, 0o600)
    fp = _io.open(fd, 'wb')
    fp.write(b'blat')
    fp.close()
    _os.unlink(filename)
    del fp, fd
    return dir
''')
        fn = self.compile_to_code(src, 'f')
        asm = disassemble(fn)

    def test_inplace(self):
        src = (
'''
def inplace(self):
    a = foo()
    a *= 42
''')
        fn = self.compile_to_code(src, 'inplace')
        asm = disassemble(fn)
        # FIXME: Ensure that the initial write to "a" isn't optimized away:
        #print(asm)

    def test_use_implicit_return(self):
        pass # TODO

    def test_predicates(self):
        src = '''
def function_to_be_inlined():
    pass

def is_crunchy(path):
    return True

def find_file(filename, std_dirs, paths):
    for dir in std_dirs:
        if is_crunchy(dir):
            f = os.path.join(sysroot, dir[1:], filename)
    return None
'''
        fn = self.compile_to_code(src, 'find_file')
        asm = disassemble(fn)
        self.assertIn('JUMP_IF_SPECIALIZABLE', asm)

    def test_useless_args(self):
        src = '''
def function_to_be_inlined():
    pass

def foo(a, b, c):
    bar(a, b)

def bar(a, b):
    baz(a)

def baz(a):
    pass

foo(1, 2, 3)
'''
        asm = disassemble(src)
        #print(asm)
        # FIXME

    def test_simple_recursion(self):
        src = '''
def function_to_be_inlined():
    return function_to_be_inlined()
'''
        fn = self.compile_to_code(src, 'function_to_be_inlined')
        asm = disassemble(fn)
        #print(asm)

    def test_mutual_recursion(self):
        src = '''
def function_to_be_inlined():
    return function_to_be_inlined()

def f():
    return g()

def g():
    return f()
'''
        fn = self.compile_to_code(src, 'f')
        asm = disassemble(fn)
        #print(asm)



def function_to_be_inlined():
    return 'I am the original implementation'

def new_version():
    return 'I am the new version'

def call_inlinable_function():
    return function_to_be_inlined()

class TestRebinding(unittest.TestCase):

    def test_rebinding(self):
        # "call_inlinable_function" should have an inlined copy
        # of "function_to_be_inlined":
        asm = disassemble(call_inlinable_function)
        #print(asm)
        # Should have logic for detecting if it can specialize:
        self.assertIn('JUMP_IF_SPECIALIZABLE', asm)
        self.assertIn('(__saved__function_to_be_inlined)', asm)
        # Should have inlined constant value:
        self.assertIn("('I am the original implementation')", asm)

        # Try calling it:
        self.assertEquals(call_inlinable_function(),
                          'I am the original implementation')

        # Now rebind the wrapped function.
        # The easy way to do this:
        #   function_to_be_inlined = new_version
        # doesn't work: the optimizer spots it, and turns off inlining
        # for "function_to_be_inlined" throughout this module
        # Similarly, using "globals()" directly is spotted.

        __builtins__.globals()['function_to_be_inlined'] = new_version

        # Verify that we get the correct result:
        self.assertEquals(call_inlinable_function(),
                          'I am the new version')
        # FIXME; should also check bytecode instrumentation to verify that we
        # took the generalized implementation




def test_main(verbose=None):
    import sys
    from test import support
    test_classes = (TestFramework, TestInlining, TestRebinding)
    support.run_unittest(*test_classes)

    # verify reference counting
    if verbose and hasattr(sys, "gettotalrefcount"):
        import gc
        counts = [None] * 5
        for i in range(len(counts)):
            support.run_unittest(*test_classes)
            gc.collect()
            counts[i] = sys.gettotalrefcount()
        print(counts)

if __name__ == "__main__":
    test_main(verbose=False)
    pass
