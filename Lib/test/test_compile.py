"""Test a variety of compilation edge cases.

Several ways to assign to __debug__
-----------------------------------

>>> __debug__ = 1
Traceback (most recent call last):
 ...
SyntaxError: can not assign to __debug__ (<string>, line 1)

>>> import __debug__
Traceback (most recent call last):
 ...
SyntaxError: can not assign to __debug__ (<string>, line 1)

>>> try:
...     1
... except MemoryError, __debug__:
...     pass
Traceback (most recent call last):
 ...
SyntaxError: can not assign to __debug__ (<string>, line 2)

Shouldn't that be line 3?

>>> def __debug__():
...     pass
Traceback (most recent call last):
 ...
SyntaxError: can not assign to __debug__ (<string>, line 1)

Not sure what the next few lines is trying to test.

>>> import __builtin__
>>> prev = __builtin__.__debug__
>>> setattr(__builtin__, "__debug__", "sure")
>>> setattr(__builtin__, "__debug__", prev)

Parameter passing
-----------------

>>> def f(a = 0, a = 1):
...     pass
Traceback (most recent call last):
 ...
SyntaxError: duplicate argument 'a' in function definition (<string>, line 1)

>>> def f(a):
...     global a
...     a = 1
Traceback (most recent call last):
 ...
SyntaxError: name 'a' is local and global

XXX How hard would it be to get the location in the error?

>>> def f(a=1, (b, c)):
...     pass
Traceback (most recent call last):
 ...
SyntaxError: non-default argument follows default argument (<string>, line 1)


Details of SyntaxError object
-----------------------------

>>> 1+*3
Traceback (most recent call last):
 ...
SyntaxError: invalid syntax

In this case, let's explore the details fields of the exception object.
>>> try:
...     compile("1+*3", "filename", "exec")
... except SyntaxError, err:
...     pass
>>> err.filename, err.lineno, err.offset
('filename', 1, 3)
>>> err.text, err.msg
('1+*3', 'invalid syntax')

Complex parameter passing
-------------------------

>>> def comp_params((a, b)):
...     print a, b
>>> comp_params((1, 2))
1 2

>>> def comp_params((a, b)=(3, 4)):
...     print a, b
>>> comp_params((1, 2))
1 2
>>> comp_params()
3 4

>>> def comp_params(a, (b, c)):
...     print a, b, c
>>> comp_params(1, (2, 3))
1 2 3

>>> def comp_params(a=2, (b, c)=(3, 4)):
...     print a, b, c
>>> comp_params(1, (2, 3))
1 2 3
>>> comp_params()
2 3 4

"""

from test.test_support import verbose, TestFailed, run_doctest

# It takes less space to deal with bad float literals using a helper
# function than it does with doctest.  The doctest needs to include
# the exception every time.

def expect_error(s):
    try:
        eval(s)
        raise TestFailed("%r accepted" % s)
    except SyntaxError:
        pass

expect_error("2e")
expect_error("2.0e+")
expect_error("1e-")
expect_error("3-4e/21")

if verbose:
    print "testing compile() of indented block w/o trailing newline"

s = """
if 1:
    if 2:
        pass"""
compile(s, "<string>", "exec")


if verbose:
    print "testing literals with leading zeroes"

def expect_same(test_source, expected):
    got = eval(test_source)
    if got != expected:
        raise TestFailed("eval(%r) gave %r, but expected %r" %
                         (test_source, got, expected))

expect_error("077787")
expect_error("0xj")
expect_error("0x.")
expect_error("0e")
expect_same("0777", 511)
expect_same("0777L", 511)
expect_same("000777", 511)
expect_same("0xff", 255)
expect_same("0xffL", 255)
expect_same("0XfF", 255)
expect_same("0777.", 777)
expect_same("0777.0", 777)
expect_same("000000000000000000000000000000000000000000000000000777e0", 777)
expect_same("0777e1", 7770)
expect_same("0e0", 0)
expect_same("0000E-012", 0)
expect_same("09.5", 9.5)
expect_same("0777j", 777j)
expect_same("00j", 0j)
expect_same("00.0", 0)
expect_same("0e3", 0)
expect_same("090000000000000.", 90000000000000.)
expect_same("090000000000000.0000000000000000000000", 90000000000000.)
expect_same("090000000000000e0", 90000000000000.)
expect_same("090000000000000e-0", 90000000000000.)
expect_same("090000000000000j", 90000000000000j)
expect_error("090000000000000")  # plain octal literal w/ decimal digit
expect_error("080000000000000")  # plain octal literal w/ decimal digit
expect_error("000000000000009")  # plain octal literal w/ decimal digit
expect_error("000000000000008")  # plain octal literal w/ decimal digit
expect_same("000000000000007", 7)
expect_same("000000000000008.", 8.)
expect_same("000000000000009.", 9.)

# Verify treatment of unary minus on negative numbers SF bug #660455
import warnings
warnings.filterwarnings("ignore", "hex/oct constants", FutureWarning)
warnings.filterwarnings("ignore", "hex.* of negative int", FutureWarning)
# XXX Of course the following test will have to be changed in Python 2.4
# This test is in a <string> so the filterwarnings() can affect it
import sys
all_one_bits = '0xffffffff'
if sys.maxint != 2147483647:
    all_one_bits = '0xffffffffffffffff'
exec """
expect_same(all_one_bits, -1)
expect_same("-" + all_one_bits, 1)
"""

def test_main(verbose=None):
    from test import test_compile
    run_doctest(test_compile, verbose)
