"""
Test script for doctest.
"""

from test import test_support
import doctest

def test_Example(): r"""
Unit tests for the `Example` class.

Example is a simple container class that holds a source code string,
an expected output string, and a line number (within the docstring):

    >>> example = doctest.Example('print 1', '1\n', 0)
    >>> (example.source, example.want, example.lineno)
    ('print 1', '1\n', 0)

The `source` string should end in a newline iff the source spans more
than one line:

    >>> # Source spans a single line: no terminating newline.
    >>> e = doctest.Example('print 1', '1\n', 0)
    >>> e = doctest.Example('print 1\n', '1\n', 0)
    Traceback (most recent call last):
    AssertionError
    
    >>> # Source spans multiple lines: require terminating newline.
    >>> e = doctest.Example('print 1;\nprint 2\n', '1\n2\n', 0)
    >>> e = doctest.Example('print 1;\nprint 2', '1\n2\n', 0)
    Traceback (most recent call last):
    AssertionError

The `want` string should be terminated by a newline, unless it's the
empty string:

    >>> e = doctest.Example('print 1', '1\n', 0)
    >>> e = doctest.Example('print 1', '1', 0)
    Traceback (most recent call last):
    AssertionError
    >>> e = doctest.Example('print', '', 0)
"""

def test_DocTest(): r"""
Unit tests for the `DocTest` class.

DocTest is a collection of examples, extracted from a docstring, along
with information about where the docstring comes from (a name,
filename, and line number).  The docstring is parsed by the `DocTest`
constructor:

    >>> docstring = '''
    ...     >>> print 12
    ...     12
    ...
    ... Non-example text.
    ...
    ...     >>> print 'another\example'
    ...     another
    ...     example
    ... '''
    >>> test = doctest.DocTest(docstring, 'some_test', 'some_file', 20)
    >>> print test
    <DocTest some_test from some_file:20 (2 examples)>
    >>> len(test.examples)
    2
    >>> e1, e2 = test.examples
    >>> (e1.source, e1.want, e1.lineno)
    ('print 12', '12\n', 1)
    >>> (e2.source, e2.want, e2.lineno)
    ("print 'another\\example'", 'another\nexample\n', 6)

Source information (name, filename, and line number) is available as
attributes on the doctest object:

    >>> (test.name, test.filename, test.lineno)
    ('some_test', 'some_file', 20)

The line number of an example within its containing file is found by
adding the line number of the example and the line number of its
containing test:

    >>> test.lineno + e1.lineno
    21
    >>> test.lineno + e2.lineno
    26

If the docstring contains inconsistant leading whitespace in the
expected output of an example, then `DocTest` will raise a ValueError:

    >>> docstring = r'''
    ...       >>> print 'bad\nindentation'
    ...       bad
    ...     indentation
    ...     '''
    >>> doctest.DocTest(docstring, 'some_test', 'filename', 0)
    Traceback (most recent call last):
    ValueError: line 3 of the docstring for some_test has inconsistent leading whitespace: '    indentation'

If the docstring contains inconsistent leading whitespace on
continuation lines, then `DocTest` will raise a ValueError:

    >>> docstring = r'''
    ...       >>> print ('bad indentation',
    ...     ...          2)
    ...       ('bad', 'indentation')
    ...     '''
    >>> doctest.DocTest(docstring, 'some_test', 'filename', 0)
    Traceback (most recent call last):
    ValueError: line 2 of the docstring for some_test has inconsistent leading whitespace: '    ...          2)'

If there's no blnak space after a PS1 prompt ('>>>'), then `DocTest`
will raise a ValueError:

    >>> docstring = '>>>print 1\n1'
    >>> doctest.DocTest(docstring, 'some_test', 'filename', 0)
    Traceback (most recent call last):
    ValueError: line 0 of the docstring for some_test lacks blanks after >>>: '>>>print 1'
"""

def test_DocTestFinder(): r"""
Unit tests for the `DocTestFinder` class.

DocTestFinder is used to extract DocTests from an object's docstring
and the docstrings of its contained objects.  It can be used with
modules, functions, classes, methods, staticmethods, classmethods, and
properties.

For a function whose docstring contains examples, DocTestFinder.find()
will return a single test (for that function's docstring):

    >>> # Functions:
    >>> def double(v):
    ...     '''
    ...     >>> print double(22)
    ...     44
    ...     '''
    ...     return v+v
    
    >>> finder = doctest.DocTestFinder()
    >>> tests = finder.find(double)
    >>> print tests
    [<DocTest double from None:1 (1 example)>]
    >>> e = tests[0].examples[0]
    >>> print (e.source, e.want, e.lineno)
    ('print double(22)', '44\n', 1)

If an object has no docstring, then a test is not created for it:

    >>> def no_docstring(v):
    ...     pass
    >>> finder.find(no_docstring)
    []
    
If the function has a docstring with no examples, then a test with no
examples is returned.  (This lets `DocTestRunner` collect statistics
about which functions have no tests -- but is that useful?  And should
an empty test also be created when there's no docstring?)
    
    >>> def no_examples(v):
    ...     ''' no doctest examples '''
    >>> finder.find(no_examples)
    [<DocTest no_examples from None:1 (no examples)>]

For a class, DocTestFinder will create a test for the class's
docstring, and will recursively explore its contents, including
methods, classmethods, staticmethods, properties, and nested classes.

    >>> # A class:
    >>> class X:
    ...     '''
    ...     >>> print 1
    ...     1
    ...     '''
    ...     def __init__(self, val):
    ...         '''
    ...         >>> print X(12).get()
    ...         12
    ...         '''
    ...         self.val = val
    ...
    ...     def double(self):
    ...         '''
    ...         >>> print X(12).double().get()
    ...         24
    ...         '''
    ...         return X(self.val + self.val)
    ...
    ...     def get(self):
    ...         '''
    ...         >>> print X(-5).get()
    ...         -5
    ...         '''
    ...         return self.val
    ...
    ...     def a_staticmethod(v):
    ...         '''
    ...         >>> print X.a_staticmethod(10)
    ...         11
    ...         '''
    ...         return v+1
    ...     a_staticmethod = staticmethod(a_staticmethod)
    ...
    ...     def a_classmethod(cls, v):
    ...         '''
    ...         >>> print X.a_classmethod(10)
    ...         12
    ...         >>> print X(0).a_classmethod(10)
    ...         12
    ...         '''
    ...         return v+2
    ...     a_classmethod = classmethod(a_classmethod)
    ...
    ...     a_property = property(get, doc='''
    ...         >>> print x(22).a_property
    ...         22
    ...         ''')
    ...
    ...     class NestedClass:
    ...         '''
    ...         >>> x = X.NestedClass(5)
    ...         >>> y = x.square()
    ...         >>> print y.get()
    ...         25
    ...         '''
    ...         def __init__(self, val=0):
    ...             '''
    ...             >>> print X.NestedClass().get()
    ...             0
    ...             '''
    ...             self.val = val
    ...         def square(self):
    ...             X.NestedClass(self.val*self.val)
    ...         def get(self):
    ...             return self.val

    >>> finder = doctest.DocTestFinder()
    >>> tests = finder.find(X)
    >>> tests.sort()
    >>> for t in tests:
    ...     print '%4s %2s  %s' % (t.lineno, len(t.examples), t.name)
    None  1  X
    None  3  X.NestedClass
      96  1  X.NestedClass.__init__
       7  1  X.__init__
      40  2  X.a_classmethod
    None  1  X.a_property
      40  1  X.a_staticmethod
      40  1  X.double
      40  1  X.get

For a module, DocTestFinder will create a test for the class's
docstring, and will recursively explore its contents, including
functions, classes, and the `__test__` dictionary, if it exists:

    >>> # A module
    >>> import new
    >>> m = new.module('some_module')
    >>> def triple(val):
    ...     '''
    ...     >>> print tripple(11)
    ...     33
    ...     '''
    ...     return val*3
    >>> m.__dict__.update({
    ...     'double': double,
    ...     'X': X,
    ...     '__doc__': '''
    ...         Module docstring.
    ...             >>> print 'module'
    ...             module
    ...         ''',
    ...     '__test__': {
    ...         'd': '>>> print 6\n6\n>>> print 7\n7\n',
    ...         'c': triple}})
    
    >>> finder = doctest.DocTestFinder()
    >>> # The '(None)' is to prevent it from filtering out objects
    >>> # that were not defined in module m.
    >>> tests = finder.find(m, module='(None)')
    >>> tests.sort()
    >>> for t in tests:
    ...     print '%4s %2s  %s' % (t.lineno, len(t.examples), t.name)
       1  1  some_module
    None  1  some_module.X
    None  3  some_module.X.NestedClass
      57  1  some_module.X.NestedClass.__init__
       6  1  some_module.X.__init__
      35  2  some_module.X.a_classmethod
    None  1  some_module.X.a_property
      27  1  some_module.X.a_staticmethod
      13  1  some_module.X.double
      20  1  some_module.X.get
       1  1  some_module.c
    None  2  some_module.d
       1  1  some_module.double

If a single object is listed twice (under different names), then tests
will only be generated for it once:

    >>> class TwoNames:
    ...     def f(self):
    ...         '''
    ...         >>> print TwoNames().f()
    ...         f
    ...         '''
    ...         return 'f'
    ...
    ...     g = f # define an alias for f.

    >>> finder = doctest.DocTestFinder()
    >>> tests = finder.find(TwoNames)
    >>> print len(tests)
    1
    >>> print tests[0].name in ('TwoNames.f', 'TwoNames.g')
    True


    

"""

def test_main():
    # Check the doctest cases in doctest itself:
    test_support.run_doctest(doctest)
    # Check the doctest cases defined here:
    from test import test_doctest
    test_support.run_doctest(test_doctest, verbosity=0)

if __name__ == '__main__':
    test_main()
    
    
