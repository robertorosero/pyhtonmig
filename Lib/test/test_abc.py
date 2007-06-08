# Copyright 2007 Google, Inc. All Rights Reserved.
# Licensed to PSF under a Contributor Agreement.

"""Unit tests for abc.py."""

import sys
import unittest
from test import test_support

import abc


class TestABC(unittest.TestCase):

    def test_abstractmethod_basics(self):
        @abc.abstractmethod
        def foo(self): pass
        self.assertEqual(foo.__isabstractmethod__, True)
        def bar(self): pass
        self.assertEqual(hasattr(bar, "__isabstractmethod__"), False)

    def test_abstractmethod_integration(self):
        class C(metaclass=abc.ABCMeta):
            @abc.abstractmethod
            def foo(self): pass  # abstract
            def bar(self): pass  # concrete
        self.assertEqual(C.__abstractmethods__, {"foo"})
        self.assertRaises(TypeError, C)  # because foo is abstract
        class D(C):
            def bar(self): pass  # concrete override of concrete
        self.assertEqual(D.__abstractmethods__, {"foo"})
        self.assertRaises(TypeError, D)  # because foo is still abstract
        class E(D):
            def foo(self): pass
        self.assertEqual(E.__abstractmethods__, set())
        E()  # now foo is concrete, too
        class F(E):
            @abc.abstractmethod
            def bar(self): pass  # abstract override of concrete
        self.assertEqual(F.__abstractmethods__, {"bar"})
        self.assertRaises(TypeError, F)  # because bar is abstract now

    def test_registration_basics(self):
        class A(metaclass=abc.ABCMeta):
            pass
        class B:
            pass
        b = B()
        self.assertEqual(issubclass(B, A), False)
        self.assertEqual(isinstance(b, A), False)
        A.register(B)
        self.assertEqual(issubclass(B, A), True)
        self.assertEqual(isinstance(b, A), True)
        class C(B):
            pass
        c = C()
        self.assertEqual(issubclass(C, A), True)
        self.assertEqual(isinstance(c, A), True)

    def test_registration_builtins(self):
        class A(metaclass=abc.ABCMeta):
            pass
        A.register(int)
        self.assertEqual(isinstance(42, A), True)
        self.assertEqual(issubclass(int, A), True)
        class B(A):
            pass
        B.register(basestring)
        self.assertEqual(isinstance("", A), True)
        self.assertEqual(issubclass(str, A), True)

    def test_registration_edge_cases(self):
        class A(metaclass=abc.ABCMeta):
            pass
        A.register(A)  # should pass silently
        class A1(A):
            pass
        self.assertRaises(RuntimeError, A1.register, A)  # cycles not allowed
        class B:
            pass
        A1.register(B)  # ok
        A1.register(B)  # should pass silently
        class C(A):
            pass
        A.register(C)  # should pass silently
        self.assertRaises(RuntimeError, C.register, A)  # cycles not allowed
        C.register(B)  # ok

    def test_registration_transitiveness(self):
        class A(metaclass=abc.ABCMeta):
            pass
        self.failUnless(issubclass(A, A))
        class B(metaclass=abc.ABCMeta):
            pass
        self.failIf(issubclass(A, B))
        self.failIf(issubclass(B, A))
        class C(metaclass=abc.ABCMeta):
            pass
        A.register(B)
        class B1(B):
            pass
        self.failUnless(issubclass(B1, A))
        class C1(C):
            pass
        B1.register(C1)
        self.failIf(issubclass(C, B))
        self.failIf(issubclass(C, B1))
        self.failUnless(issubclass(C1, A))
        self.failUnless(issubclass(C1, B))
        self.failUnless(issubclass(C1, B1))
        C1.register(int)
        class MyInt(int):
            pass
        self.failUnless(issubclass(MyInt, A))
        self.failUnless(isinstance(42, A))


def test_main():
    test_support.run_unittest(TestABC)


if __name__ == "__main__":
    unittest.main()
