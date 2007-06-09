__all__ = ['deque', 'defaultdict', 'NamedTuple',
           'Hashable', 'Iterable', 'Iterator', 'Sized', 'Container',
           ]

from _collections import deque, defaultdict
from operator import itemgetter as _itemgetter
import sys as _sys
from abc import abstractmethod as _abstractmethod, ABCMeta as _ABCMeta

def NamedTuple(typename, s):
    """Returns a new subclass of tuple with named fields.

    >>> Point = NamedTuple('Point', 'x y')
    >>> Point.__doc__           # docstring for the new class
    'Point(x, y)'
    >>> p = Point(11, y=22)     # instantiate with positional args or keywords
    >>> p[0] + p[1]             # works just like the tuple (11, 22)
    33
    >>> x, y = p                # unpacks just like a tuple
    >>> x, y
    (11, 22)
    >>> p.x + p.y               # fields also accessable by name
    33
    >>> p                       # readable __repr__ with name=value style
    Point(x=11, y=22)

    """

    field_names = s.split()
    if not ''.join([typename] + field_names).replace('_', '').isalnum():
        raise ValueError('Type names and field names can only contain alphanumeric characters and underscores')
    argtxt = ', '.join(field_names)
    reprtxt = ', '.join('%s=%%r' % name for name in field_names)
    template = '''class %(typename)s(tuple):
        '%(typename)s(%(argtxt)s)'
        __slots__ = ()
        def __new__(cls, %(argtxt)s):
            return tuple.__new__(cls, (%(argtxt)s,))
        def __repr__(self):
            return '%(typename)s(%(reprtxt)s)' %% self
    ''' % locals()
    for i, name in enumerate(field_names):
        template += '\n        %s = property(itemgetter(%d))\n' % (name, i)
    m = dict(itemgetter=_itemgetter)
    exec(template, m)
    result = m[typename]
    if hasattr(_sys, '_getframe'):
        result.__module__ = _sys._getframe(1).f_globals['__name__']
    return result


class _OneTrickPony(metaclass=_ABCMeta):

    @classmethod
    def __instancecheck__(cls, x):
        return issubclass(x.__class__, cls)

    @classmethod
    def register(cls, C):
        raise TypeError("class %s doesn't allow registration of subclasses" %
                        cls.__name__)


class Hashable(_OneTrickPony):

    @_abstractmethod
    def __hash__(self):
        return 0

    @classmethod
    def __subclasscheck__(cls, C):
        for B in C.__mro__:
            if "__hash__" in B.__dict__:
                return B.__dict__["__hash__"] is not None
        return False


class Iterable(_OneTrickPony):

    @_abstractmethod
    def __iter__(self):
        while False:
            yield None

    @classmethod
    def __subclasscheck__(cls, C):
        return any("__iter__" in B.__dict__ or "__getitem__" in B.__dict__
                   for B in C.__mro__)


class Iterator(_OneTrickPony):

    @_abstractmethod
    def __next__(self):
        raise StopIteration

    def __iter__(self):
        return self

    @classmethod
    def __subclasscheck__(cls, C):
        return any("__next__" in B.__dict__ for B in C.__mro__)


class Sized(_OneTrickPony):

    @_abstractmethod
    def __len__(self):
        return 0

    @classmethod
    def __subclasscheck__(cls, C):
        return any("__len__" in B.__dict__ for B in C.__mro__)


class Container(_OneTrickPony):

    @_abstractmethod
    def __contains__(self, x):
        return False

    @classmethod
    def __subclasscheck__(cls, C):
        return any("__contains__" in B.__dict__ for B in C.__mro__)


if __name__ == '__main__':
    # verify that instances are pickable
    from cPickle import loads, dumps
    Point = NamedTuple('Point', 'x y')
    p = Point(x=10, y=20)
    assert p == loads(dumps(p))

    import doctest
    TestResults = NamedTuple('TestResults', 'failed attempted')
    print(TestResults(*doctest.testmod()))
