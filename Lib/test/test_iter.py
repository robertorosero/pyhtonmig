from test_support import verify

class iter_filter:
    def __init__(self, pred, it):
        self.pred = pred
        self.it = it
    def __iter__(self):
        return self
    def next(self):
        while 1:
            value = self.it.next()
            if self.pred(value):
                return value

class iter_map:
    def __init__(self, func, *its):
        assert len(its) >= 1 # We're not testing this, it's just a requirement
        self.func = func
        self.its = its
    def __iter__(self):
        return self
    def next(self):
        # This doesn't emulate map() exactly, it stops with the shortest
        return self.func(*[it.next() for it in self.its])

def test_iter_filter():
    a = "The quick brown fox jumps over the lazy dog"
    def pred(x): return x != ' '
    expected = list(filter(pred, a))
    outcome = [x for x in iter_filter(pred, iter(a))]
    verify(outcome==expected, outcome)
    print "iter_filter() works"

def test_iter_map():
    a = "The quick brown fox jumps over the lazy dog"
    def func(x): return x.upper()
    expected = list(map(func, a))
    outcome = [x for x in iter_map(func, iter(a))]
    verify(outcome==expected, outcome)
    print "iter_map() works"

def test_dict_iter():
    a = {}
    for i in range(20):
        for j in range(20):
            verify((i+j in a) == a.has_key(i+j))
            if i+j not in a:
                a[i+j] = i+j
    print "if key in dict works"
    keys = []
    for k in a:
        keys.append(k)
    verify(keys == a.keys())
    print "for key in dict works"

def test_file_iter():
    f = open("test_iter.py")
    data = f.readlines()
    f.seek(0)
    L = [line for line in f]
    f.close()
    verify(L == data, L)
    print "for line in file works"

test_iter_filter()
test_iter_map()
test_dict_iter()
test_file_iter()
