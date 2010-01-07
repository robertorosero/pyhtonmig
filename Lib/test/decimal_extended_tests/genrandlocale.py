#
# For each possible locale setting, test formatting using random
# format strings.
#
# Usage:  python3.2 genrandlocale.py | ../runtest -
#


from formathelper import *
print("rounding: half_even")


testno = 0
for loc in locale_list:
    try:
        locale.setlocale(locale.LC_ALL, loc)
    except locale.Error as err:
        sys.stderr.write("%s: %s\n" % (loc, err))
        continue
    print("locale: %s" % loc)
    for sign in ('', '-'):
        intpart = fracpart = ''
        while (not intpart) and (not fracpart):
            intpart = random.choice(integers)
            fracpart = random.choice(integers)
        s = ''.join((sign, intpart, '.', fracpart))
        fmt = rand_format('x')
        testno += 1
        printit(testno, s, fmt)
    for s in un_incr_digits(15, 384, 30):
        fmt = rand_format('x')
        testno += 1
        printit(testno, s, fmt)
