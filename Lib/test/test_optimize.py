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
    def _get_fn_single(mod, node, name):
        for f in node.body:
            if isinstance(f, (ast.ClassDef, ast.FunctionDef)):
                if f.name == name:
                    return f
        raise ValueError('Could not find name: %r' % name)
    mod = ast.parse(src)
    path = fnname.split('.')
    node = mod
    for name in path:
        node = _get_fn_single(mod, node, name)
    return node, mod

def get_code_for_fn(co, fnname):
    def _get_code_single(co, fnname):
        for const in co.co_consts:
            if isinstance(const, types.CodeType):
                if const.co_name == fnname:
                    return const
        raise ValueError('code for %r not found' % fnname)

    path = fnname.split('.')
    for name in path:
        co = _get_code_single(co, name)
    return co

class TestOptimization(unittest.TestCase):
    def assertHasLineWith(self, asm, items):
        def _has_items(line, items):
            for item in items:
                if item not in line:
                    return False
            return True

        for line in asm.splitlines():
            if _has_items(line, items):
                return
        raise ValueError('Did not find a line containing all of %r within %s'
                         % (items, asm))

    def test_custom_assert(self):
        with self.assertRaises(ValueError):
            self.assertHasLineWith('Hello world', ('foo', 'bar'))

class TestFramework(TestOptimization):
    # Ensure that invoking the optimizer is working:
    def test_eval(self):
        self.assertEqual(eval('42'), 42)

class TestInlining(TestOptimization):
    def assertIsInlinable(self, src, fnname='f'):
        from __optimizer__ import fn_is_inlinable
        fn, mod = compile_fn(src, fnname)
        self.assert_(fn_is_inlinable(fn, mod),
                     msg='Unexpectedly not inlinable: %s\n%r' % (src, ast.dump(fn)))

    def assertIsNotInlinable(self, src, fnname='f'):
        from __optimizer__ import fn_is_inlinable
        fn, mod = compile_fn(src, fnname)
        self.assert_(not fn_is_inlinable(fn, mod),
                     msg='Unexpectedly inlinable: %s\n%r' % (src, ast.dump(fn)))

    def compile_to_code(self, src, fnname):
        # Compile the given source code, returning the code object for the given function name
        co = compile(src, 'optimizable.py', 'exec')
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
        self.assertHasLineWith(asm, ('LOAD_GLOBAL', '(__internal__.saved.function_to_be_inlined)'))
        self.assertIn('JUMP_IF_SPECIALIZABLE', asm)
        self.assertHasLineWith(asm, ('LOAD_CONST', "('feefifofum')"))

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
        co = compile(src, 'optimizable.py', 'exec')
        g = get_code_for_fn(co, 'g')
        h = get_code_for_fn(co, 'h')
        co_asm = disassemble(co)
        g_asm = disassemble(g)
        h_asm = disassemble(h)
        # Verify co_asm:
        self.assertHasLineWith(co_asm,
                               ('STORE_GLOBAL', '(__internal__.saved.f)'))
        self.assertHasLineWith(co_asm,
                               ('STORE_GLOBAL', '(__internal__.saved.g)'))
        self.assertHasLineWith(co_asm,
                               ('STORE_GLOBAL', '(__internal__.saved.h)'))

        # Verify g_asm:
        self.assertHasLineWith(g_asm,
                               ('LOAD_GLOBAL', '(__internal__.saved.f)'))
        self.assertIn('JUMP_IF_SPECIALIZABLE', g_asm)

        # Verify h_asm:
        self.assertHasLineWith(h_asm,
                               ('LOAD_GLOBAL', '(__internal__.saved.g)'))
        self.assertHasLineWith(h_asm,
                               ('LOAD_GLOBAL', '(__internal__.saved.f)'))
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

    def test_method(self):
        src = '''
class Foo:
    def simple_method(self, a):
         return self.bar + a + self.baz

    def user_of_method(self):
         return self.simple_method(10)
'''

        # "Foo.simple_method" should be inlinable

        # Ensure that we're saving the inlinable function as a global for use
        # by JUMP_IF_SPECIALIZABLE at callsites:
        fn = self.compile_to_code(src, 'Foo')
        asm = disassemble(fn)
        self.assertHasLineWith(asm,
            ('STORE_GLOBAL', '(__internal__.saved.Foo.simple_method)'))
        #print(asm)

        self.assertIsInlinable(src, fnname='Foo.simple_method')
        fn = self.compile_to_code(src, 'Foo.user_of_method')
        asm = disassemble(fn)
        #print(asm)
        self.assertHasLineWith(asm,
                               ('LOAD_GLOBAL', '(__internal__.saved.Foo.simple_method)'))
        self.assertIn('JUMP_IF_SPECIALIZABLE', asm)




    def test_namespaces(self):
        src = '''
class Foo:
    def simple_method(self, a):
         return self.bar + a + self.baz
'''
        fn = self.compile_to_code(src, 'Foo.simple_method')
        asm = disassemble(fn)
        #from __optimizer__ import get_dotted_name


def function_to_be_inlined():
    return 'I am the original implementation'

def new_version():
    return 'I am the new version'

def call_inlinable_function():
    return function_to_be_inlined()

class TestRebinding(TestOptimization):

    def test_rebinding(self):
        # "call_inlinable_function" should have an inlined copy
        # of "function_to_be_inlined":
        asm = disassemble(call_inlinable_function)
        #print(asm)
        # Should have logic for detecting if it can specialize:
        self.assertIn('JUMP_IF_SPECIALIZABLE', asm)
        self.assertHasLineWith(asm,
                               ('LOAD_GLOBAL', '(__internal__.saved.function_to_be_inlined)'))
        # Should have inlined constant value:
        self.assertHasLineWith(asm,
                               ('LOAD_CONST', "('I am the original implementation')"))

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
