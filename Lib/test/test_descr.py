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

def testternop(a, b, c, res, expr="a]b:c]", meth="__getslice__"):
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

if verbose: print __name__, "OK"
