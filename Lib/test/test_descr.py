# Test descriptor-related enhancements

from test_support import verify, verbose
from copy import deepcopy

def testunop(a, res, expr="len(a)", meth="__len__"):
    if verbose: print "checking", expr
    dict = {'a': a}
    verify(eval(expr, dict) == res)
    t = type(a)
    m = getattr(t, meth)
    verify(m == t.__dict__[meth])
    verify(m(a) == res)
    bm = getattr(a, meth)
    verify(bm() == res)

def testbinop(a, b, res, expr="a+b", meth="__add__"):
    if verbose: print "checking", expr
    dict = {'a': a, 'b': b}
    verify(eval(expr, dict) == res)
    t = type(a)
    m = getattr(t, meth)
    verify(m == t.__dict__[meth])
    verify(m(a, b) == res)
    bm = getattr(a, meth)
    verify(bm(b) == res)

def testternop(a, b, c, res, expr="a[b:c]", meth="__getslice__"):
    if verbose: print "checking", expr
    dict = {'a': a, 'b': b, 'c': c}
    verify(eval(expr, dict) == res)
    t = type(a)
    m = getattr(t, meth)
    verify(m == t.__dict__[meth])
    verify(m(a, b, c) == res)
    bm = getattr(a, meth)
    verify(bm(b, c) == res)

def testsetop(a, b, res, stmt="a+=b", meth="__iadd__"):
    if verbose: print "checking", stmt
    dict = {'a': deepcopy(a), 'b': b}
    exec stmt in dict
    verify(dict['a'] == res)
    t = type(a)
    m = getattr(t, meth)
    verify(m == t.__dict__[meth])
    dict['a'] = deepcopy(a)
    m(dict['a'], b)
    verify(dict['a'] == res)
    dict['a'] = deepcopy(a)
    bm = getattr(dict['a'], meth)
    bm(b)
    verify(dict['a'] == res)

def testset2op(a, b, c, res, stmt="a[b]=c", meth="__setitem__"):
    if verbose: print "checking", stmt
    dict = {'a': deepcopy(a), 'b': b, 'c': c}
    exec stmt in dict
    verify(dict['a'] == res)
    t = type(a)
    m = getattr(t, meth)
    verify(m == t.__dict__[meth])
    dict['a'] = deepcopy(a)
    m(dict['a'], b, c)
    verify(dict['a'] == res)
    dict['a'] = deepcopy(a)
    bm = getattr(dict['a'], meth)
    bm(b, c)
    verify(dict['a'] == res)

def testset3op(a, b, c, d, res, stmt="a[b:c]=d", meth="__setslice__"):
    if verbose: print "checking", stmt
    dict = {'a': deepcopy(a), 'b': b, 'c': c, 'd': d}
    exec stmt in dict
    verify(dict['a'] == res)
    t = type(a)
    m = getattr(t, meth)
    verify(m == t.__dict__[meth])
    dict['a'] = deepcopy(a)
    m(dict['a'], b, c, d)
    verify(dict['a'] == res)
    dict['a'] = deepcopy(a)
    bm = getattr(dict['a'], meth)
    bm(b, c, d)
    verify(dict['a'] == res)

def lists():
    if verbose: print "Testing list operations..."
    testbinop([1], [2], [1,2], "a+b", "__add__")
    testbinop([1,2,3], 2, 1, "b in a", "__contains__")
    testbinop([1,2,3], 4, 0, "b in a", "__contains__")
    testbinop([1,2,3], 1, 2, "a[b]", "__getitem__")
    testternop([1,2,3], 0, 2, [1,2], "a[b:c]", "__getslice__")
    testsetop([1], [2], [1,2], "a+=b", "__iadd__")
    testsetop([1,2], 3, [1,2,1,2,1,2], "a*=b", "__imul__")
    testunop([1,2,3], 3, "len(a)", "__len__")
    testbinop([1,2], 3, [1,2,1,2,1,2], "a*b", "__mul__")
    testbinop([1], [2], [2,1], "b+a", "__radd__")
    testbinop([1,2], 3, [1,2,1,2,1,2], "b*a", "__rmul__")
    testset2op([1,2], 1, 3, [1,3], "a[b]=c", "__setitem__")
    testset3op([1,2,3,4], 1, 3, [5,6], [1,5,6,4], "a[b:c]=d", "__setslice__")

def dicts():
    if verbose: print "Testing dict operations..."
    testbinop({1:2}, {2:1}, -1, "cmp(a,b)", "__cmp__")
    testbinop({1:2,3:4}, 1, 1, "b in a", "__contains__")
    testbinop({1:2,3:4}, 2, 0, "b in a", "__contains__")
    testbinop({1:2,3:4}, 1, 2, "a[b]", "__getitem__")
    d = {1:2,3:4}
    l1 = []
    for i in d.keys(): l1.append(i)
    l = []
    for i in iter(d): l.append(i)
    verify(l == l1)
    l = []
    for i in d.__iter__(): l.append(i)
    verify(l == l1)
    l = []
    for i in dictionary.__iter__(d): l.append(i)
    verify(l == l1)
    testunop({1:2,3:4}, 2, "len(a)", "__len__")
    testunop({1:2,3:4}, "{3: 4, 1: 2}", "repr(a)", "__repr__")
    testset2op({1:2,3:4}, 2, 3, {1:2,2:3,3:4}, "a[b]=c", "__setitem__")

binops = {
    'add': '+',
    'sub': '-',
    'mul': '*',
    'div': '/',
    'mod': '%',
    'divmod': 'divmod',
    'pow': '**',
    'lshift': '<<',
    'rshift': '>>',
    'and': '&',
    'xor': '^',
    'or': '|',
    'cmp': 'cmp',
    'lt': '<',
    'le': '<=',
    'eq': '==',
    'ne': '!=',
    'gt': '>',
    'ge': '>=',
    }

for name, expr in binops.items():
    if expr.islower():
        expr = expr + "(a, b)"
    else:
        expr = 'a %s b' % expr
    binops[name] = expr

unops = {
    'pos': '+',
    'neg': '-',
    'abs': 'abs',
    'invert': '~',
    'int': 'int',
    'long': 'long',
    'float': 'float',
    'oct': 'oct',
    'hex': 'hex',
    }

for name, expr in unops.items():
    if expr.islower():
        expr = expr + "(a)"
    else:
        expr = '%s a' % expr
    unops[name] = expr

def numops(a, b):
    dict = {'a': a, 'b': b}
    for name, expr in binops.items():
        name = "__%s__" % name
        if hasattr(a, name):
            res = eval(expr, dict)
            testbinop(a, b, res, expr, name)
    for name, expr in unops.items():
        name = "__%s__" % name
        if hasattr(a, name):
            res = eval(expr, dict)
            testunop(a, res, expr, name)

def ints():
    if verbose: print "Testing int operations..."
    numops(100, 3)

def longs():
    if verbose: print "Testing long operations..."
    numops(100L, 3L)

def floats():
    if verbose: print "Testing float operations..."
    numops(100.0, 3.0)

def spamlists():
    if verbose: print "Testing spamlist operations..."
    import copy, spam
    def spamlist(l, memo=None):
        import spam
        sl = spam.list()
        for i in l: sl.append(i)
        return sl
    # This is an ugly hack:
    copy._deepcopy_dispatch[spam.SpamListType] = spamlist

    testbinop(spamlist([1]), spamlist([2]), spamlist([1,2]), "a+b", "__add__")
    testbinop(spamlist([1,2,3]), 2, 1, "b in a", "__contains__")
    testbinop(spamlist([1,2,3]), 4, 0, "b in a", "__contains__")
    testbinop(spamlist([1,2,3]), 1, 2, "a[b]", "__getitem__")
    testternop(spamlist([1,2,3]), 0, 2, spamlist([1,2]),
               "a[b:c]", "__getslice__")
    testsetop(spamlist([1]), spamlist([2]), spamlist([1,2]),
              "a+=b", "__iadd__")
    testsetop(spamlist([1,2]), 3, spamlist([1,2,1,2,1,2]), "a*=b", "__imul__")
    testunop(spamlist([1,2,3]), 3, "len(a)", "__len__")
    testbinop(spamlist([1,2]), 3, spamlist([1,2,1,2,1,2]), "a*b", "__mul__")
    testbinop(spamlist([1]), spamlist([2]), spamlist([2,1]), "b+a", "__radd__")
    testbinop(spamlist([1,2]), 3, spamlist([1,2,1,2,1,2]), "b*a", "__rmul__")
    testset2op(spamlist([1,2]), 1, 3, spamlist([1,3]), "a[b]=c", "__setitem__")
    testset3op(spamlist([1,2,3,4]), 1, 3, spamlist([5,6]),
               spamlist([1,5,6,4]), "a[b:c]=d", "__setslice__")

def spamdicts():
    if verbose: print "Testing spamdict operations..."
    import copy, spam
    def spamdict(d, memo=None):
        import spam
        sd = spam.dict()
        for k, v in d.items(): sd[k] = v
        return sd
    # This is an ugly hack:
    copy._deepcopy_dispatch[spam.SpamDictType] = spamdict

    testbinop(spamdict({1:2}), spamdict({2:1}), -1, "cmp(a,b)", "__cmp__")
    testbinop(spamdict({1:2,3:4}), 1, 1, "b in a", "__contains__")
    testbinop(spamdict({1:2,3:4}), 2, 0, "b in a", "__contains__")
    testbinop(spamdict({1:2,3:4}), 1, 2, "a[b]", "__getitem__")
    d = spamdict({1:2,3:4})
    l1 = []
    for i in d.keys(): l1.append(i)
    l = []
    for i in iter(d): l.append(i)
    verify(l == l1)
    l = []
    for i in d.__iter__(): l.append(i)
    verify(l == l1)
    l = []
    for i in type(spamdict({})).__iter__(d): l.append(i)
    verify(l == l1)
    testunop(spamdict({1:2,3:4}), 2, "len(a)", "__len__")
    testunop(spamdict({1:2,3:4}), "{3: 4, 1: 2}", "repr(a)", "__repr__")
    testset2op(spamdict({1:2,3:4}), 2, 3, spamdict({1:2,2:3,3:4}),
               "a[b]=c", "__setitem__")

def pydicts():
    if verbose: print "Testing Python subclass of dict..."
    verify(issubclass(dictionary, dictionary))
    verify(isinstance({}, dictionary))
    d = dictionary()
    verify(d == {})
    verify(d.__class__ is dictionary)
    verify(isinstance(d, dictionary))
    class C(dictionary):
        state = -1
        def __init__(self, *a, **kw):
            if a:
                assert len(a) == 1
                self.state = a[0]
            if kw:
                for k, v in kw.items(): self[v] = k
        def __getitem__(self, key):
            return self.get(key, 0)
        def __setitem__(self, key, value):
            assert isinstance(key, type(0))
            dictionary.__setitem__(self, key, value)
        def setstate(self, state):
            self.state = state
        def getstate(self):
            return self.state
    verify(issubclass(C, dictionary))
    a1 = C(12)
    verify(a1.state == 12)
    a2 = C(foo=1, bar=2)
    verify(a2[1] == 'foo' and a2[2] == 'bar')
    a = C()
    verify(a.state == -1)
    verify(a.getstate() == -1)
    a.setstate(0)
    verify(a.state == 0)
    verify(a.getstate() == 0)
    a.setstate(10)
    verify(a.state == 10)
    verify(a.getstate() == 10)
    verify(a[42] == 0)
    a[42] = 24
    verify(a[42] == 24)
    if verbose: print "pydict stress test ..."
    N = 50
    for i in range(N):
        a[i] = C()
        for j in range(N):
            a[i][j] = i*j
    for i in range(N):
        for j in range(N):
            verify(a[i][j] == i*j)

def metaclass():
    if verbose: print "Testing __metaclass__..."
    global C
    class C:
        __metaclass__ = type(type(0))
        def __init__(self):
            self.__state = 0
        def getstate(self):
            return self.__state
        def setstate(self, state):
            self.__state = state
    a = C()
    verify(a.getstate() == 0)
    a.setstate(10)
    verify(a.getstate() == 10)

import sys
MT = type(sys)

def pymods():
    if verbose: print "Testing Python subclass of module..."
    global log
    log = []
    class MM(MT):
        def __init__(self):
            MT.__init__(self)
        def __getattr__(self, name):
            log.append(("getattr", name))
            return MT.__getattr__(self, name)
        def __setattr__(self, name, value):
            log.append(("setattr", name, value))
            MT.__setattr__(self, name, value)
        def __delattr__(self, name):
            log.append(("delattr", name))
            MT.__delattr__(self, name)
    a = MM()
    a.foo = 12
    x = a.foo
    del a.foo
    verify(log == [('getattr', '__init__'),
                   ('getattr', '__setattr__'),
                   ("setattr", "foo", 12),
                   ("getattr", "foo"),
                   ('getattr', '__delattr__'),
                   ("delattr", "foo")], log)

def multi():
    if verbose: print "Testing multiple inheritance..."
    global C
    class C(object):
        def __init__(self):
            self.__state = 0
        def getstate(self):
            return self.__state
        def setstate(self, state):
            self.__state = state
    a = C()
    verify(a.getstate() == 0)
    a.setstate(10)
    verify(a.getstate() == 10)
    class D(dictionary, C):
        def __init__(self):
            type({}).__init__(self)
            C.__init__(self)
    d = D()
    verify(d.keys() == [])
    d["hello"] = "world"
    verify(d.items() == [("hello", "world")])
    verify(d["hello"] == "world")
    verify(d.getstate() == 0)
    d.setstate(10)
    verify(d.getstate() == 10)
    verify(D.__mro__ == (D, dictionary, C, object))

def diamond():
    if verbose: print "Testing multiple inheritance special cases..."
    class A(object):
        def spam(self): return "A"
    verify(A().spam() == "A")
    class B(A):
        def boo(self): return "B"
        def spam(self): return "B"
    verify(B().spam() == "B")
    verify(B().boo() == "B")
    class C(A):
        def boo(self): return "C"
    verify(C().spam() == "A")
    verify(C().boo() == "C")
    class D(B, C): pass
    verify(D().spam() == "B")
    verify(D().boo() == "B")
    verify(D.__mro__ == (D, B, C, A, object))
    class E(C, B): pass
    verify(E().spam() == "B")
    verify(E().boo() == "C")
    verify(E.__mro__ == (E, C, B, A, object))
    class F(D, E): pass
    verify(F().spam() == "B")
    verify(F().boo() == "B")
    verify(F.__mro__ == (F, D, E, B, C, A, object))
    class G(E, D): pass
    verify(G().spam() == "B")
    verify(G().boo() == "C")
    verify(G.__mro__ == (G, E, D, C, B, A, object))

def all():
    lists()
    dicts()
    ints()
    longs()
    floats()
    spamlists()
    spamdicts()
    pydicts()
    metaclass()
    pymods()
    multi()
    diamond()

all()

if verbose: print "All OK"
