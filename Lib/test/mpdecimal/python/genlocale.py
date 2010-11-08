#
# Copyright (c) 2008-2010 Stefan Krah. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#


#
# Very extensive test that comes close to brute force testing
# all format string combinations containing either a thousands
# separator or the 'n' specifier.
#
# Usage:  python3.2 genlocale.py | ../runtest -
#


from formathelper import *


# Generate random format strings, no 'n' specifier
# [[fill]align][sign][#][0][width][.precision][type]
def _gen_format_sep():
    for align in ('', '<', '>', '=', '^'):
        for fill in ('', 'x'):
            if align == '': fill = ''
            for sign in ('', '+', '-', ' '):
                for zeropad in ('', '0'):
                    if align != '': zeropad = ''
                    for width in ['']+[str(y) for y in range(1, 15)]+['101']:
                        for prec in ['']+['.'+str(y) for y in range(15)]:
                            # for type in ('', 'E', 'e', 'G', 'g', 'F', 'f', '%'):
                            type = random.choice(('', 'E', 'e', 'G', 'g', 'F', 'f', '%'))
                            yield ''.join((fill, align, sign, zeropad, width, ',', prec, type))


# Generate random format strings with 'n' specifier
# [[fill]align][sign][#][0][width][.precision][type]
def _gen_format_locale():
    for align in ('', '<', '>', '=', '^'):
        for fill in ('', 'x'):
            if align == '': fill = ''
            for sign in ('', '+', '-', ' '):
                for zeropad in ('', '0'):
                    if align != '': zeropad = ''
                    for width in ['']+[str(y) for y in range(1, 20)]+['101']:
                        for prec in ['']+['.'+str(y) for y in range(1, 20)]:
                            yield ''.join((fill, align, sign, zeropad, width, prec, 'n'))


# Generate random format strings with a unicode fill character
# [[fill]align][sign][#][0][width][.precision][type]
def randf(fill):
    active = sorted(random.sample(range(5), random.randrange(6)))
    s = ''
    s += str(fill)
    s += random.choice('<>=^')
    have_align = 1
    for elem in active:
        if elem == 0: # sign
            s += random.choice('+- ')
        elif elem == 1: # width
            s += str(random.randrange(1, 100))
        elif elem == 2: # thousands separator
            s += ','
        elif elem == 3: # prec
            s += '.'
            # decimal.py does not support prec=0
            s += str(random.randrange(1, 100))
        elif elem == 4:
            if 2 in active: c = 'EeGgFf%'
            else: c = 'EeGgFfn%'
            s += random.choice(c)
    return s

# Generate random format strings with random locale setting
# [[fill]align][sign][#][0][width][.precision][type]
def rand_locale():
    try:
        loc = random.choice(locale_list)
        locale.setlocale(locale.LC_ALL, loc)
    except locale.Error as err:
        pass
    active = sorted(random.sample(range(5), random.randrange(6)))
    s = ''
    have_align = 0
    for elem in active:
        if elem == 0: # fill+align
            s += chr(random.randrange(32, 128))
            s += random.choice('<>=^')
            have_align = 1
        elif elem == 1: # sign
            s += random.choice('+- ')
        elif elem == 2 and not have_align: # zeropad
            s += '0'
        elif elem == 3: # width
            s += str(random.randrange(1, 100))
        elif elem == 4: # prec
            s += '.'
            # decimal.py does not support prec=0
            s += str(random.randrange(1, 100))
    s += 'n'
    return s


if __name__ == '__main__':

    testno = 0
    print("rounding: half_even")

    if not unicode_chars:
        unicode_chars = gen_unicode_chars()

    # unicode fill character test
    for x in range(10):
        for fill in unicode_chars:
            intpart = fracpart = ''
            while (not intpart) and (not fracpart):
                intpart = random.choice(integers)
                fracpart = random.choice(integers)
            s = ''.join((random.choice(('', '-')), intpart, '.', fracpart))
            fmt = randf(fill)
            testno += 1
            printit(testno, s, fmt, 'utf-8')

    # thousands separator test
    for fmt in _gen_format_sep():
        for s in un_incr_digits(15, 384, 30):
            testno += 1
        for sign in ('', '-'):
            for intpart in integers:
                for fracpart in integers:
                    if (not intpart) and (not fracpart):
                        continue
                    s = ''.join((sign, intpart, '.', fracpart))
                    testno += 1
                    printit(testno, s, fmt)
        for s in numbers:
            testno += 1
            printit(testno, s, fmt)
        for x in range(100):
            s = randdec(20, 425)
            testno += 1
            printit(testno, s, fmt)
        for x in range(100):
            s = randint(20)
            testno += 1
            printit(testno, s, fmt)

    # locale test
    for loc in locale_list:
        try:
            locale.setlocale(locale.LC_ALL, loc)
        except locale.Error as err:
            sys.stderr.write("%s: %s\n" % (loc, err))
            continue
        print("locale: %s" % loc)
        for fmt in _gen_format_locale():
            for sign in ('', '-'):
                intpart = fracpart = ''
                while (not intpart) and (not fracpart):
                    intpart = random.choice(integers)
                    fracpart = random.choice(integers)
                s = ''.join((sign, intpart, '.', fracpart))
                testno += 1
                printit(testno, s, fmt)
            for s in random.sample(numbers, 3):
                testno += 1
                printit(testno, s, fmt)
            getcontext().prec = 300
            for x in range(10):
                s = randdec(20, 425000000)
                testno += 1
                printit(testno, s, fmt)
