import unittest
from test import test_support

def _make_complex_mro_bases():
    class C(object):
        pass
    class D(C):
        pass
    class E(C):
        pass
    class F(D,E):
        pass
    class G(object):
        pass
    class H(G):
        pass
    return C,D,E,F,G,H

class SimpleUpdatesTestCases(unittest.TestCase):
    def test_instance(self):
        class C(object): 
            pass
        x = C()
        self.failIf(hasattr(x,'foo'), "x doesn't have a foo yet")
        x.foo = 1
        self.failUnless(x.foo == 1, "x.foo should be 1")
        C.foo = 'spam'
        self.failUnless(x.foo == 1, "x.foo should override C.foo")
        del C.foo
        self.failUnless(x.foo == 1, "x.foo should still be 1")
        x.foo = 'eggs'
        self.failUnless(x.foo == 'eggs', "x.foo should be updated")
        del x.foo
        self.failIf(hasattr(x,'foo'), "Shouldn't be caching foo")
        x.foo = 'monty'
        self.failUnless(x.foo == 'monty', "x.foo should be 'monty'")
        self.failUnless(x.foo == 'monty', 
                "x.foo should correctly cache 'monty'")
    def test_class(self):
        class C(object):
            pass
        self.failIf(hasattr(C,'foo'), "C doesn't have a foo yet")
        C.foo = 1
        self.failUnless(C.foo == 1, "C.foo should be 1")
        x = C()
        self.failUnless(x.foo == 1, "x.foo should be C.foo")
        self.failUnless(x.foo == 1, "x.foo should correctly cache C.foo")
        C.foo = 'spam'
        self.failUnless(x.foo == 'spam', "x should get C.foo update")
        del C.foo
        self.failIf(hasattr(x,'foo'), "x shouldn't cache foo")
        self.failIf(hasattr(C,'foo'), "C shouldn't cache foo")
    def test_single_inheritance1(self):
        class C(object):
            pass
        class D(C):
            pass
        self.failIf(hasattr(D,'foo'), "D shouldn't have foo yet")
        C.foo = 1
        self.failUnless(D.foo == 1, "D.foo should be C.foo")
        x = D()
        self.failUnless(x.foo == 1, "x.foo should be C.foo")
        del C.foo
        self.failIf(hasattr(D,'foo'), "D shouldn't cache foo")
    def test_single_inheritance2(self):
        class C(object):
            pass
        C.foo = 1
        class D(C):
            pass
        self.failUnless(D.foo == 1, "D.foo should be C.foo")
        D.foo = 'spam'
        self.failUnless(D.foo == 'spam', 
                "D.foo should override C.foo")
        del D.foo
        x = D()
        x.foo = 'eggs'
        self.failUnless(x.foo == 'eggs', 
                "x.foo should override C.foo")
        self.failIf(D.foo == 'eggs',
                "D.foo should be C.foo, not x.foo")
    def test_multiple_inheritance(self):
        class C(object):
            def plugh(self):
                return 'dragon'
        class D(object):
            def plugh(self):
                return 'dwarf'
            def maze(self):
                return 'twisty'
        class E(C,D):
            pass
        self.failIf(hasattr(E,'foo'), "E shouldn't have foo yet")
        E.foo = 1
        self.failUnless(E.foo == 1, "E.foo should be 1")
        self.failIf(hasattr(C,'foo'), "C shouldn't have foo")
        self.failIf(hasattr(D,'foo'), "D shouldn't have foo")
        C.foo = 'spam'
        self.failUnless(E.foo == 1, "E.foo should be 1")
        D.foo = 'eggs'
        self.failUnless(E.foo == 1, "E.foo should be 1")
        x = E()
        self.failUnless(x.foo == 1, "x.foo should be E.foo")
        x.foo = 'bar'
        self.failUnless(x.foo == 'bar', "x.foo should be 'bar'")
        del x.foo
        del E.foo
        self.failUnless(x.foo == 'spam', "x.foo should be C.foo")
        self.failUnless(E.foo == 'spam', "E.foo should be C.foo")
        del C.foo
        self.failUnless(x.foo == 'eggs', "x.foo should be D.foo")
        self.failUnless(E.foo == 'eggs', "E.foo should be D.foo")
        del D.foo
        self.failIf(hasattr(x,'foo'), "x shouldn't cache foo")
        self.failIf(hasattr(E,'foo'), "E shouldn't cache foo")
        D.xyzzy = 123
        self.failIf(hasattr(C,'xyzzy'), "C shouldn't have D.xyzzy")
        self.failUnless(E.xyzzy == 123, "E.xyzzy should be D.xyzzy")
        self.failUnless(x.xyzzy == 123, "x.xyzzy should be D.xyzzy")
        self.failUnless(x.plugh() == 'dragon', 
                "x.plugh() should call C.plugh()")
        self.failUnless(x.plugh() == 'dragon', 
                "x.plugh() should call C.plugh()")
        self.failUnless(x.maze() == 'twisty',
                "x.maze() should call D.maze()")
        def maze(self):
            return 'passage'
        C.maze = maze
        self.failUnless(x.maze() == 'passage',
                "x.maze() should call C.maze()")
        def maze(self):
            return 'twisty passage'
        import new
        x.maze = new.instancemethod(maze, x, C)
        self.failUnless(x.maze() == 'twisty passage',
            "x.maze() should call x.maze()")
    def test_complex_mro1(self):
        C,D,E,F,G,H = _make_complex_mro_bases()
        class J(F,H):
            pass
        C.foo = 1
        x = F()
        self.failUnless(x.foo == 1, "x.foo should be C.foo")
        y = J()
        self.failUnless(y.foo == 1, "y.foo should be C.foo")
        z = H()
        self.failIf(hasattr(z,'foo'), "z should not have C.foo")
        G.foo = 'spam'
        self.failIf(y.foo == 'spam', "G.foo should not mask C.foo")
    def test_complex_mro2(self):
        C,D,E,F,G,H = _make_complex_mro_bases()
        class J(H,F):
            pass
        C.foo = 1
        x = J()
        self.failUnless(x.foo == 1, "x.foo should be C.foo")
        G.foo = 'spam'
        self.failUnless(x.foo == 'spam', "G.foo should mask C.foo")

class DescriptorsTestCases(unittest.TestCase):
    pass

class ClassicClassesTestCases(unittest.TestCase):
    pass

class TestCacheDirectly(unittest.TestCase):
    pass

def test_main():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(SimpleUpdatesTestCases))
    suite.addTest(unittest.makeSuite(DescriptorsTestCases))
    suite.addTest(unittest.makeSuite(ClassicClassesTestCases))
    test_support.run_suite(suite)

if __name__=='__main__':
    test_main()

