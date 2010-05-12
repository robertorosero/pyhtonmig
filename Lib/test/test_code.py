"""This module includes tests of the code object representation.

>>> def f(x):
...     def g(y):
...         return x + y
...     return g
...

>>> dump(f.__code__)
name: f
argcount: 1
kwonlyargcount: 0
names: ()
varnames: ('x', 'g')
cellvars: ('x',)
freevars: ()
nlocals: 2
flags: 3
consts: ('None', '<code object g>')

>>> dump(f(4).__code__)
name: g
argcount: 1
kwonlyargcount: 0
names: ()
varnames: ('y',)
cellvars: ()
freevars: ('x',)
nlocals: 1
flags: 19
consts: ('None',)

>>> def h(x, y):
...     a = x + y
...     b = x - y
...     c = a * b
...     return c
...

>>> dump(h.__code__)
name: h
argcount: 2
kwonlyargcount: 0
names: ()
varnames: ('x', 'y', 'a', 'b', 'c')
cellvars: ()
freevars: ()
nlocals: 5
flags: 67
consts: ('None',)

>>> def attrs(obj):
...     print(obj.attr1)
...     print(obj.attr2)
...     print(obj.attr3)

>>> dump(attrs.__code__)
name: attrs
argcount: 1
kwonlyargcount: 0
names: ('print', 'attr1', 'attr2', 'attr3')
varnames: ('obj',)
cellvars: ()
freevars: ()
nlocals: 1
flags: 67
consts: ('None',)

>>> def optimize_away():
...     'doc string'
...     'not a docstring'
...     53
...     0x53

>>> dump(optimize_away.__code__)
name: optimize_away
argcount: 0
kwonlyargcount: 0
names: ()
varnames: ()
cellvars: ()
freevars: ()
nlocals: 0
flags: 67
consts: ("'doc string'", 'None')

>>> def keywordonly_args(a,b,*,k1):
...     return a,b,k1
...

>>> dump(keywordonly_args.__code__)
name: keywordonly_args
argcount: 2
kwonlyargcount: 1
names: ()
varnames: ('a', 'b', 'k1')
cellvars: ()
freevars: ()
nlocals: 3
flags: 67
consts: ('None',)

"""

import unittest
import weakref
import _testcapi


def consts(t):
    """Yield a doctest-safe sequence of object reprs."""
    for elt in t:
        r = repr(elt)
        if r.startswith("<code object"):
            yield "<code object %s>" % elt.co_name
        else:
            yield r

def dump(co):
    """Print out a text representation of a code object."""
    for attr in ["name", "argcount", "kwonlyargcount", "names", "varnames",
                 "cellvars", "freevars", "nlocals", "flags"]:
        print("%s: %s" % (attr, getattr(co, "co_" + attr)))
    print("consts:", tuple(consts(co.co_consts)))


def new_code(function_name, def_string, globals_dict=None):
    """Compiles function_name, defined in def_string into a new code object.

    Compiles and runs def_string in a temporary namespace, with the specified
    globals dict if any, and returns the function named 'function_name' out of
    that namespace.

    This allows us to track things that change in a code object as it's called
    repeatedly.  Simply defining a local function would re-use the same code
    object for each function

    """
    namespace = {}
    if globals_dict is None:
        globals_dict = {}
    exec(def_string, globals_dict, namespace)
    return namespace[function_name]


class CodeTest(unittest.TestCase):

    def test_newempty(self):
        co = _testcapi.code_newempty("filename", "funcname", 15)
        self.assertEquals(co.co_filename, "filename")
        self.assertEquals(co.co_name, "funcname")
        self.assertEquals(co.co_firstlineno, 15)


HOTNESS_CALL = 10
HOTNESS_ITER = 1

@unittest.skipUnless(hasattr(new_code.__code__, "co_hotness"),
                     "Only applies with LLVM compiled in.")
class HotnessTest(unittest.TestCase):

    def setUp(self):
        self.while_loop = new_code("while_loop", """
def while_loop(x):
  while x > 0:
    x = x - 1
""")

    def test_new_code_has_0_hotness(self):
        self.assertEquals(self.while_loop.__code__.co_hotness, 0)

    def test_call_adds_10_hotness(self):
        self.while_loop(0)
        self.assertEquals(self.while_loop.__code__.co_hotness, HOTNESS_CALL)
        self.while_loop(0)
        self.assertEquals(self.while_loop.__code__.co_hotness, 2 * HOTNESS_CALL)

        list(map(self.while_loop, [0]))  # Don't go through fast_function.
        self.assertEquals(self.while_loop.__code__.co_hotness, 3 * HOTNESS_CALL)

        kwargs = new_code("kwargs", """
def kwargs(**kwa):
  return kwa
""")
        self.assertEquals(kwargs.__code__.co_hotness, 0)
        kwargs(a=3, b=4)  # Also doesn't go through fast_function.
        self.assertEquals(kwargs.__code__.co_hotness, HOTNESS_CALL)

    def test_iteration_adds_1_hotness(self):
        self.while_loop(1)
        self.assertEquals(self.while_loop.__code__.co_hotness,
                          HOTNESS_CALL + HOTNESS_ITER)
        self.while_loop(36)
        self.assertEquals(self.while_loop.__code__.co_hotness,
                          2 * HOTNESS_CALL + 37 * HOTNESS_ITER)

        for_loop = new_code("for_loop", """
def for_loop():
  for x in range(17):
    pass
""")
        self.assertEquals(for_loop.__code__.co_hotness, 0)
        for_loop()
        self.assertEquals(for_loop.__code__.co_hotness,
                          HOTNESS_CALL + 17 * HOTNESS_ITER)

    def test_nested_for_loop_hotness(self):
        # Verify our understanding of how the hotness model deals with nested
        # for loops. This can be confusing, and we don't want to change it
        # accidentally.
        foo = new_code("foo", """
def foo():
    for x in range(50):
        for y in range(70):
            pass
""")
        self.assertEqual(foo.__code__.co_hotness, 0)
        foo()
        self.assertEqual(foo.__code__.co_hotness,
                         HOTNESS_CALL + HOTNESS_ITER * 3500 +
                         HOTNESS_ITER * 50)

    def test_for_loop_jump_threading_hotness(self):
        # Regression test: the bytecode peephole optimizer does some limited
        # jump threading, which caused problems for one earlier attempt at
        # tuning the hotness model.
        foo = new_code("foo", """
def foo():
    for x in range(30):
        if x % 2:  # Alternate between the two branches
            x = 8  # Nonsense
""")
        self.assertEqual(foo.__code__.co_hotness, 0)
        foo()

        hotness = HOTNESS_CALL + HOTNESS_ITER * 30

    def test_early_for_loop_exit_hotness(self):
        # Make sure we understand how the hotness model counts early exits from
        # for loops.
        foo = new_code("foo", """
def foo():
    for x in range(1000):
        return True
""")
        self.assertEqual(foo.__code__.co_hotness, 0)
        foo()

        # Note that we don't count the loop in any way, since we never take
        # a loop backedge.
        self.assertEqual(foo.__code__.co_hotness, HOTNESS_CALL)

    def test_generator_hotness(self):
        foo = new_code("foo", """
def foo():
    yield 5
    yield 6
""")
        # Generator object created, but not run yet.  This counts as the call.
        l = foo()
        self.assertEqual(foo.__code__.co_hotness, HOTNESS_CALL)

        next(l)  # Enter the generator.  This is not a call.
        self.assertEqual(foo.__code__.co_hotness, HOTNESS_CALL)
        next(l)  # Neither is this.
        self.assertEqual(foo.__code__.co_hotness, HOTNESS_CALL)



class CodeWeakRefTest(unittest.TestCase):

    def test_basic(self):
        # Create a code object in a clean environment so that we know we have
        # the only reference to it left.
        namespace = {}
        exec("def f(): pass", globals(), namespace)
        f = namespace["f"]
        del namespace

        self.called = False
        def callback(code):
            self.called = True

        # f is now the last reference to the function, and through it, the code
        # object.  While we hold it, check that we can create a weakref and
        # deref it.  Then delete it, and check that the callback gets called and
        # the reference dies.
        coderef = weakref.ref(f.__code__, callback)
        self.assertTrue(bool(coderef()))
        del f
        self.assertFalse(bool(coderef()))
        self.assertTrue(self.called)


def test_main(verbose=None):
    from test.support import run_doctest, run_unittest
    from test import test_code
    run_doctest(test_code, verbose)
    run_unittest(CodeTest, HotnessTest, CodeWeakRefTest)


if __name__ == "__main__":
    test_main()
