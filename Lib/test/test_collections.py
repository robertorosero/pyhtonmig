"""Unit tests for collections.py."""

import unittest
from test import test_support
from collections import NamedTuple
from collections import Hashable, Iterable, Iterator, Sized, Container


class TestNamedTuple(unittest.TestCase):

    def test_factory(self):
        Point = NamedTuple('Point', 'x y')
        self.assertEqual(Point.__name__, 'Point')
        self.assertEqual(Point.__doc__, 'Point(x, y)')
        self.assertEqual(Point.__slots__, ())
        self.assertEqual(Point.__module__, __name__)
        self.assertEqual(Point.__getitem__, tuple.__getitem__)
        self.assertRaises(ValueError, NamedTuple, 'abc%', 'def ghi')
        self.assertRaises(ValueError, NamedTuple, 'abc', 'def g%hi')
        NamedTuple('Point0', 'x1 y2')   # Verify that numbers are allowed in names

    def test_instance(self):
        Point = NamedTuple('Point', 'x y')
        p = Point(11, 22)
        self.assertEqual(p, Point(x=11, y=22))
        self.assertEqual(p, Point(11, y=22))
        self.assertEqual(p, Point(y=22, x=11))
        self.assertEqual(p, Point(*(11, 22)))
        self.assertEqual(p, Point(**dict(x=11, y=22)))
        self.assertRaises(TypeError, Point, 1)                              # too few args
        self.assertRaises(TypeError, Point, 1, 2, 3)                        # too many args
        self.assertRaises(TypeError, eval, 'Point(XXX=1, y=2)', locals())   # wrong keyword argument
        self.assertRaises(TypeError, eval, 'Point(x=1)', locals())          # missing keyword argument
        self.assertEqual(repr(p), 'Point(x=11, y=22)')
        self.assert_('__dict__' not in dir(p))                              # verify instance has no dict
        self.assert_('__weakref__' not in dir(p))

    def test_tupleness(self):
        Point = NamedTuple('Point', 'x y')
        p = Point(11, 22)

        self.assert_(isinstance(p, tuple))
        self.assertEqual(p, (11, 22))                                       # matches a real tuple
        self.assertEqual(tuple(p), (11, 22))                                # coercable to a real tuple
        self.assertEqual(list(p), [11, 22])                                 # coercable to a list
        self.assertEqual(max(p), 22)                                        # iterable
        self.assertEqual(max(*p), 22)                                       # star-able
        x, y = p
        self.assertEqual(p, (x, y))                                         # unpacks like a tuple
        self.assertEqual((p[0], p[1]), (11, 22))                            # indexable like a tuple
        self.assertRaises(IndexError, p.__getitem__, 3)

        self.assertEqual(p.x, x)
        self.assertEqual(p.y, y)
        self.assertRaises(AttributeError, eval, 'p.z', locals())


class TestABCs(unittest.TestCase):

    def test_Hashable(self):
        # Check some non-hashables
        non_samples = [bytes(), list(), set(), dict()]
        for x in non_samples:
            self.failIf(isinstance(x, Hashable), repr(x))
            self.failIf(issubclass(type(x), Hashable), repr(type(x)))
        # Check some hashables
        samples = [None,
                   int(), float(), complex(),
                   str(), unicode(),
                   tuple(), frozenset(),
                   int, list, object, type,
                   ]
        for x in samples:
            self.failUnless(isinstance(x, Hashable), repr(x))
            self.failUnless(issubclass(type(x), Hashable), repr(type(x)))
        self.assertRaises(TypeError, Hashable)
        # Check direct subclassing
        class H(Hashable):
            def __hash__(self):
                return super(H, self).__hash__()
        self.assertEqual(hash(H()), 0)
        # Check registration is disabled
        class C:
            def __hash__(self):
                return 0
        self.assertRaises(TypeError, Hashable.register, C)

    def test_Iterable(self):
        non_samples = [None, 42, 3.14, 1j]
        for x in non_samples:
            self.failIf(isinstance(x, Iterable), repr(x))
            self.failIf(issubclass(type(x), Iterable), repr(type(x)))
        samples = [bytes(), str(), unicode(),
                   tuple(), list(), set(), frozenset(), dict(),
                   ]
        for x in samples:
            self.failUnless(isinstance(x, Iterable), repr(x))
            self.failUnless(issubclass(type(x), Iterable), repr(type(x)))

    def test_Iterator(self):
        non_samples = [None, 42, 3.14, 1j, b"", "", u"", (), [], {}, set()]
        for x in non_samples:
            self.failIf(isinstance(x, Iterator), repr(x))
            self.failIf(issubclass(type(x), Iterator), repr(type(x)))
        samples = [iter(bytes()), iter(str()), iter(unicode()),
                   iter(tuple()), iter(list()), iter(dict()),
                   iter(set()), iter(frozenset()),
                   ]
        for x in samples:
            self.failUnless(isinstance(x, Iterator), repr(x))
            self.failUnless(issubclass(type(x), Iterator), repr(type(x)))

    def test_Sized(self):
        non_samples = [None, 42, 3.14, 1j]
        for x in non_samples:
            self.failIf(isinstance(x, Sized), repr(x))
            self.failIf(issubclass(type(x), Sized), repr(type(x)))
        samples = [bytes(), str(), unicode(),
                   tuple(), list(), set(), frozenset(), dict(),
                   ]
        for x in samples:
            self.failUnless(isinstance(x, Sized), repr(x))
            self.failUnless(issubclass(type(x), Sized), repr(type(x)))

    def test_Container(self):
        non_samples = [None, 42, 3.14, 1j]
        for x in non_samples:
            self.failIf(isinstance(x, Container), repr(x))
            self.failIf(issubclass(type(x), Container), repr(type(x)))
        samples = [bytes(), str(), unicode(),
                   tuple(), list(), set(), frozenset(), dict(),
                   ]
        for x in samples:
            self.failUnless(isinstance(x, Container), repr(x))
            self.failUnless(issubclass(type(x), Container), repr(type(x)))


def test_main(verbose=None):
    import collections as CollectionsModule
    test_classes = [TestNamedTuple, TestABCs]
    test_support.run_unittest(*test_classes)
    test_support.run_doctest(CollectionsModule, verbose)

if __name__ == "__main__":
    test_main(verbose=True)
