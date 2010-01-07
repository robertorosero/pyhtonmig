#
# Valgrind claims a possible leak indicating that the signal-dicts
# of the context are not freed properly. The tests below do not
# show any leaks.
#


from cdecimal import *


for i in xrange(10000000):
    c = Context()
    c.prec = 9
    setcontext(c)


for i in xrange(10000000):
    c = Context()
    d = getcontext().copy()
    del(c)
    del(d)
