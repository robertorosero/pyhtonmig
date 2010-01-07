#
# Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
# Licensed to PSF under a Contributor Agreement.
#


#
# Test formatting using random format strings. This must be run
# in a UFT-8 terminal.
#
# Usage:  python3.2 genrandformat.py | ../runtest -
#


from formathelper import *
print("rounding: half_even")


testno = 0
for x in range(1000):
    for sign in ('', '-'):
        intpart = fracpart = ''
        while (not intpart) and (not fracpart):
            intpart = random.choice(integers)
            fracpart = random.choice(integers)
        s = ''.join((sign, intpart, '.', fracpart))
        fmt = rand_format(rand_unicode())
        testno += 1
        printit(testno, s, fmt, 'utf-8')
    for s in un_incr_digits(15, 384, 30):
        fmt = rand_format(rand_unicode())
        testno += 1
        printit(testno, s, fmt, 'utf-8')
