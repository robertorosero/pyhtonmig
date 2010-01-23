# Copyright (c) 2010 Python Software Foundation.
# All rights reserved.


import sys
from test.support import import_fresh_module
# abuse sys.modules as a communication channel
sys.modules['have_cdecimal'] = True
m = import_fresh_module('test.decimal_tests')


def test_main(arith=False, verbose=None, todo_tests=None, debug=None):
    return m.test_main(arith, verbose, todo_tests, debug)


if __name__ == '__main__':
    import optparse
    p = optparse.OptionParser("test_cdecimal.py [--debug] [{--skip | test1 [test2 [...]]}]")
    p.add_option('--debug', '-d', action='store_true', help='shows the test number and context before each test')
    p.add_option('--skip',  '-s', action='store_true', help='skip over 90% of the arithmetic tests')
    (opt, args) = p.parse_args()

    if opt.skip:
        test_main(arith=False, verbose=True)
    elif args:
        test_main(arith=True, verbose=True, todo_tests=args, debug=opt.debug)
    else:
        test_main(arith=True, verbose=True)
