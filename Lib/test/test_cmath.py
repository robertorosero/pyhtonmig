from test.test_support import run_unittest
from test.test_math import parse_testfile, test_file
import unittest
import os, sys
import cmath, math

class CMathTests(unittest.TestCase):
    # list of all functions in cmath
    test_functions = [getattr(cmath, fname) for fname in [
            'acos', 'acosh', 'asin', 'asinh', 'atan', 'atanh',
            'cos', 'cosh', 'exp', 'log', 'log10', 'sin', 'sinh',
            'sqrt', 'tan', 'tanh']]
    # test first and second arguments independently for 2-argument log
    test_functions.append(lambda x : cmath.log(x, 1729. + 0j))
    test_functions.append(lambda x : cmath.log(14.-27j, x))

    def setUp(self):
        self.test_values = open(test_file)

    def tearDown(self):
        self.test_values.close()

    def rAssertAlmostEqual(self, a, b, rel_eps = 2e-15, abs_eps = 5e-323):
        """Check that two floating-point numbers are almost equal."""

        # test passes if either the absolute error or the relative
        # error is sufficiently small.  The defaults amount to an
        # error of between 9 ulps and 19 ulps on an IEEE-754 compliant
        # machine.

        try:
            absolute_error = abs(b-a)
        except OverflowError:
            pass
        else:
            if absolute_error <= max(abs_eps, rel_eps * abs(a)):
                return
        self.fail("%s and %s are not sufficiently close" % (repr(a), repr(b)))

    def test_constants(self):
        e_expected = 2.71828182845904523536
        pi_expected = 3.14159265358979323846
        self.rAssertAlmostEqual(cmath.pi, pi_expected, 9,
            "cmath.pi is %s; should be %s" % (cmath.pi, pi_expected))
        self.rAssertAlmostEqual(cmath.e,  e_expected, 9,
            "cmath.e is %s; should be %s" % (cmath.e, e_expected))

    def test_user_object(self):
        # Test automatic calling of __complex__ and __float__ by cmath
        # functions

        # some random values to use as test values; we avoid values
        # for which any of the functions in cmath is undefined
        # (i.e. 0., 1., -1., 1j, -1j) or would cause overflow
        cx_arg = 4.419414439 + 1.497100113j
        flt_arg = -6.131677725

        # a variety of non-complex numbers, used to check that
        # non-complex return values from __complex__ give an error
        non_complexes = ["not complex", 1, 5L, 2., None,
                         object(), NotImplemented]

        # Now we introduce a variety of classes whose instances might
        # end up being passed to the cmath functions

        # usual case: new-style class implementing __complex__
        class MyComplex(object):
            def __init__(self, value):
                self.value = value
            def __complex__(self):
                return self.value

        # old-style class implementing __complex__
        class MyComplexOS:
            def __init__(self, value):
                self.value = value
            def __complex__(self):
                return self.value

        # classes for which __complex__ raises an exception
        class SomeException(Exception):
            pass
        class MyComplexException(object):
            def __complex__(self):
                raise SomeException
        class MyComplexExceptionOS:
            def __complex__(self):
                raise SomeException

        # some classes not providing __float__ or __complex__
        class NeitherComplexNorFloat(object):
            pass
        class NeitherComplexNorFloatOS:
            pass
        class MyInt(object):
            def __int__(self): return 2
            def __long__(self): return 2L
            def __index__(self): return 2
        class MyIntOS:
            def __int__(self): return 2
            def __long__(self): return 2L
            def __index__(self): return 2

        # other possible combinations of __float__ and __complex__
        # that should work
        class FloatAndComplex(object):
            def __float__(self):
                return flt_arg
            def __complex__(self):
                return cx_arg
        class FloatAndComplexOS:
            def __float__(self):
                return flt_arg
            def __complex__(self):
                return cx_arg
        class JustFloat(object):
            def __float__(self):
                return flt_arg
        class JustFloatOS:
            def __float__(self):
                return flt_arg

        for f in self.test_functions:
            # usual usage
            self.assertEqual(f(MyComplex(cx_arg)), f(cx_arg))
            self.assertEqual(f(MyComplexOS(cx_arg)), f(cx_arg))
            # other combinations of __float__ and __complex__
            self.assertEqual(f(FloatAndComplex()), f(cx_arg))
            self.assertEqual(f(FloatAndComplexOS()), f(cx_arg))
            self.assertEqual(f(JustFloat()), f(flt_arg))
            self.assertEqual(f(JustFloatOS()), f(flt_arg))
            # TypeError should be raised for classes not providing
            # either __complex__ or __float__, even if they provide
            # __int__, __long__ or __index__.  An old-style class
            # currently raises AttributeError instead of a TypeError;
            # this could be considered a bug.
            self.assertRaises(TypeError, f, NeitherComplexNorFloat())
            self.assertRaises(TypeError, f, MyInt())
            self.assertRaises(Exception, f, NeitherComplexNorFloatOS())
            self.assertRaises(Exception, f, MyIntOS())
            # non-complex return value from __complex__ -> TypeError
            for bad_complex in non_complexes:
                self.assertRaises(TypeError, f, MyComplex(bad_complex))
                self.assertRaises(TypeError, f, MyComplexOS(bad_complex))
            # exceptions in __complex__ should be propagated correctly
            self.assertRaises(SomeException, f, MyComplexException())
            self.assertRaises(SomeException, f, MyComplexExceptionOS())

    def test_input_type(self):
        # ints and longs should be acceptable inputs to all cmath
        # functions, by virtue of providing a __float__ method
        for f in self.test_functions:
            for arg in [2, 2L, 2.]:
                self.assertEqual(f(arg), f(arg.__float__()))

        # but strings should give a TypeError
        for f in self.test_functions:
            for arg in ["a", "long_string", "0", "1j", ""]:
                self.assertRaises(TypeError, f, arg)

    def test_cmath_matches_math(self):
        # check that corresponding cmath and math functions are equal
        # for floats in the appropriate range

        # test_values in (0, 1)
        test_values = [0.01, 0.1, 0.2, 0.5, 0.9, 0.99]

        # test_values for functions defined on [-1., 1.]
        unit_interval = test_values + [-x for x in test_values] + \
            [0., 1., -1.]

        # test_values for log, log10, sqrt
        positive = test_values + [1.] + [1./x for x in test_values]
        nonnegative = [0.] + positive

        # test_values for functions defined on the whole real line
        real_line = [0.] + positive + [-x for x in positive]

        test_functions = {
            'acos' : unit_interval,
            'asin' : unit_interval,
            'atan' : real_line,
            'cos' : real_line,
            'cosh' : real_line,
            'exp' : real_line,
            'log' : positive,
            'log10' : positive,
            'sin' : real_line,
            'sinh' : real_line,
            'sqrt' : nonnegative,
            'tan' : real_line,
            'tanh' : real_line}

        for fn, values in test_functions.items():
            float_fn = getattr(math, fn)
            complex_fn = getattr(cmath, fn)
            for v in values:
                z = complex_fn(v)
                self.rAssertAlmostEqual(float_fn(v), z.real)
                self.assertEqual(0., z.imag)

        # test two-argument version of log with various bases
        for base in [0.5, 2., 10.]:
            for v in positive:
                z = cmath.log(v, base)
                self.rAssertAlmostEqual(math.log(v, base), z.real)
                self.assertEqual(0., z.imag)

    def test_specific_values(self):
        if not float.__getformat__("double").startswith("IEEE"):
            return
        for id, fn, ar, ai, er, ei in parse_testfile(test_file):
            arg = complex(ar, ai)
            expected = complex(er, ei)
            function = getattr(cmath, fn)
            actual = function(arg)

            if fn=='log':
                # for the real part of the log function, we allow an
                # absolute error of up to 2e-15.
                self.rAssertAlmostEqual(expected.real, actual.real,
                                        abs_eps = 2e-15)
            else:
                self.rAssertAlmostEqual(expected.real, actual.real)
            self.rAssertAlmostEqual(expected.imag, actual.imag)

def test_main():
    run_unittest(CMathTests)

if __name__ == "__main__":
    test_main()
