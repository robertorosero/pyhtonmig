#
# Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
# Licensed to PSF under a Contributor Agreement.
#


#
# Grammar from http://speleotrove.com/decimal/daconvs.html
#
# sign           ::=  '+' | '-'
# digit          ::=  '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' |
#                    '8' | '9'
# indicator      ::=  'e' | 'E'
# digits         ::=  digit [digit]...
# decimal-part   ::=  digits '.' [digits] | ['.'] digits
# exponent-part  ::=  indicator [sign] digits
# infinity       ::=  'Infinity' | 'Inf'
# nan            ::=  'NaN' [digits] | 'sNaN' [digits]
# numeric-value  ::=  decimal-part [exponent-part] | infinity
# numeric-string ::=  [sign] numeric-value | [sign] nan
#


import random, sys


def sign():
    if random.randrange(2):
        if random.randrange(2): return '+'
        return ''
    return '-'

def indicator():
    return "eE"[random.randrange(2)]

def digits(maxprec):
    if maxprec == 0: return ''
    return str(random.randrange(10**maxprec))

def dot():
    if random.randrange(2): return '.'
    return ''

def decimal_part(maxprec):
    if random.randrange(100) > 60: # integers
        return digits(maxprec)
    if random.randrange(2):
        intlen = random.randrange(1, maxprec+1)
        fraclen = maxprec-intlen
        intpart = digits(intlen)
        fracpart = digits(fraclen)
        return ''.join((intpart, '.', fracpart))
    else:
        return ''.join((dot(), digits(maxprec)))

def expdigits(maxexp):
    return str(random.randrange(maxexp))

def exponent_part(maxexp):
    return ''.join((indicator(), sign(), expdigits(maxexp)))

def infinity():
    if random.randrange(2): return 'Infinity'
    return 'Inf'

def nan():
    d = ''
    if random.randrange(2):
        d = digits(random.randrange(99));
    if random.randrange(2):
        return ''.join(('NaN', d))
    else:
        return ''.join(('sNaN', d))

def numeric_value(maxprec, maxexp):
    if random.randrange(100) > 90:
        return infinity()
    exp_part = ''
    if random.randrange(100) > 60:
        exp_part = exponent_part(maxexp)
    return ''.join((decimal_part(maxprec), exp_part))

def numeric_string(maxprec, maxexp):
    if random.randrange(100) > 95:
        return ''.join((sign(), nan()))
    else:
        return ''.join((sign(), numeric_value(maxprec, maxexp)))

def randdec(maxprec, maxexp):
    return numeric_string(maxprec, maxexp)

def randint(maxprec):
    return digits(maxprec)

def rand_adjexp(maxprec, maxadjexp):
    d = digits(maxprec)
    maxexp = maxadjexp-len(d)+1
    if maxexp == 0: maxexp = 1
    exp = str(random.randrange(maxexp-2*(abs(maxexp)), maxexp))
    return ''.join((sign(), d, 'E', exp))


def ndigits(n):
    if n < 1: return 0
    return random.randrange(10**(n-1), 10**n)

def randtuple(maxprec, maxexp):
    n = random.randrange(100)
    sign = (0,1)[random.randrange(1)]
    coeff = ndigits(maxprec)
    if n >= 95:
        coeff = ()
        exp = 'F'
    elif n >= 85:
        coeff = tuple(map(int, str(ndigits(maxprec))))
        exp = "nN"[random.randrange(1)]
    else:
        coeff = tuple(map(int, str(ndigits(maxprec))))
        exp = random.randrange(-maxexp, maxexp)
    return (sign, coeff, exp)


def from_triple(sign, coeff, exp):
    return ''.join((str(sign*coeff), indicator(), str(exp)))


# Close to 10**n
def un_close_to_pow10(prec, maxexp, itertns=None):
    if itertns is None:
        lst = range(prec+30)
    else:
        lst = random.sample(range(prec+30), itertns)
    nines = [10**n - 1 for n in lst]
    pow10 = [10**n for n in lst]
    for coeff in nines:
        yield coeff
        yield -coeff
        yield from_triple(1, coeff, random.randrange(2*maxexp))
        yield from_triple(-1, coeff, random.randrange(2*maxexp))
    for coeff in pow10:
        yield coeff
        yield -coeff

# Close to 10**n
def bin_close_to_pow10(prec, maxexp, itertns=None):
    if itertns is None:
        lst = range(prec+30)
    else:
        lst = random.sample(range(prec+30), itertns)
    nines = [10**n - 1 for n in lst]
    pow10 = [10**n for n in lst]
    for coeff in nines:
        yield coeff, 1
        yield -coeff, -1
        yield 1, coeff
        yield -1, -coeff
        yield from_triple(1, coeff, random.randrange(2*maxexp)), 1
        yield from_triple(-1, coeff, random.randrange(2*maxexp)), -1
        yield 1, from_triple(1, coeff, -random.randrange(2*maxexp))
        yield -1, from_triple(-1, coeff, -random.randrange(2*maxexp))
    for coeff in pow10:
        yield coeff, -1
        yield -coeff, 1
        yield 1, -coeff
        yield -coeff, 1

# Close to 1:
def close_to_one_greater(prec, emax, emin):
    rprec = 10**prec
    return ''.join(("1.", '0'*random.randrange(prec),
                   str(random.randrange(rprec))))

def close_to_one_less(prec, emax, emin):
    rprec = 10**prec
    return ''.join(("0.9", '9'*random.randrange(prec),
                   str(random.randrange(rprec))))

# Close to 0:
def close_to_zero_greater(prec, emax, emin):
    rprec = 10**prec
    return ''.join(("0.", '0'*random.randrange(prec),
                   str(random.randrange(rprec))))

def close_to_zero_less(prec, emax, emin):
    rprec = 10**prec
    return ''.join(("-0.", '0'*random.randrange(prec),
                   str(random.randrange(rprec))))

# Close to emax:
def close_to_emax_less(prec, emax, emin):
    rprec = 10**prec
    return ''.join(("9.", '9'*random.randrange(prec),
                   str(random.randrange(rprec)), "E", str(emax)))

def close_to_emax_greater(prec, emax, emin):
    rprec = 10**prec
    return ''.join(("1.", '0'*random.randrange(prec),
                   str(random.randrange(rprec)), "E", str(emax+1)))

# Close to emin:
def close_to_emin_greater(prec, emax, emin):
    rprec = 10**prec
    return ''.join(("1.", '0'*random.randrange(prec),
                   str(random.randrange(rprec)), "E", str(emin)))

def close_to_emin_less(prec, emax, emin):
    rprec = 10**prec
    return ''.join(("9.", '9'*random.randrange(prec),
                   str(random.randrange(rprec)), "E", str(emin-1)))

# Close to etiny:
def close_to_etiny_greater(prec, emax, emin):
    rprec = 10**prec
    etiny = emin - (prec - 1)
    return ''.join(("1.", '0'*random.randrange(prec),
                   str(random.randrange(rprec)), "E", str(etiny)))

def close_to_etiny_less(prec, emax, emin):
    rprec = 10**prec
    etiny = emin - (prec - 1)
    return ''.join(("9.", '9'*random.randrange(prec),
                   str(random.randrange(rprec)), "E", str(etiny-1)))


def close_to_min_etiny_greater(prec, max_prec, min_emin):
    rprec = 10**prec
    etiny = min_emin - (max_prec - 1)
    return ''.join(("1.", '0'*random.randrange(prec),
                   str(random.randrange(rprec)), "E", str(etiny)))

def close_to_min_etiny_less(prec, max_prec, min_emin):
    rprec = 10**prec
    etiny = min_emin - (max_prec - 1)
    return ''.join(("9.", '9'*random.randrange(prec),
                   str(random.randrange(rprec)), "E", str(etiny-1)))


close_funcs = [
  close_to_one_greater, close_to_one_less, close_to_zero_greater,
  close_to_zero_less, close_to_emax_less, close_to_emax_greater,
  close_to_emin_greater, close_to_emin_less, close_to_etiny_greater,
  close_to_etiny_less, close_to_min_etiny_greater, close_to_min_etiny_less
]


def un_close_numbers(prec, emax, emin, itertns=None):
    if itertns is None:
        itertns = 1000
    for i in range(itertns):
        for func in close_funcs:
            yield func(prec, emax, emin)

def bin_close_numbers(prec, emax, emin, itertns=None):
    if itertns is None:
        itertns = 1000
    for i in range(itertns):
        for func1 in close_funcs:
            for func2 in close_funcs:
                yield func1(prec, emax, emin), func2(prec, emax, emin)
        for func in close_funcs:
            yield randdec(prec, emax), func(prec, emax, emin)
            yield func(prec, emax, emin), randdec(prec, emax)

def tern_close_numbers(prec, emax, emin, itertns):
    if itertns is None:
        itertns = 1000
    for i in range(itertns):
        for func1 in close_funcs:
            for func2 in close_funcs:
                for func3 in close_funcs:
                    yield (func1(prec, emax, emin), func2(prec, emax, emin),
                           func3(prec, emax, emin))
        for func in close_funcs:
            yield (randdec(prec, emax), func(prec, emax, emin),
                   func(prec, emax, emin))
            yield (func(prec, emax, emin), randdec(prec, emax),
                   func(prec, emax, emin))
            yield (func(prec, emax, emin), func(prec, emax, emin),
                   randdec(prec, emax))
        for func in close_funcs:
            yield (randdec(prec, emax), randdec(prec, emax),
                   func(prec, emax, emin))
            yield (randdec(prec, emax), func(prec, emax, emin),
                   randdec(prec, emax))
            yield (func(prec, emax, emin), randdec(prec, emax),
                   randdec(prec, emax))


# If itertns == None, test all digit lengths up to prec + 30
def un_incr_digits(prec, maxexp, itertns):
    if itertns is None:
        lst = range(prec+30)
    else:
        lst = random.sample(range(prec+30), itertns)
    for m in lst:
        yield from_triple(1, ndigits(m), 0)
        yield from_triple(-1, ndigits(m), 0)
        yield from_triple(1, ndigits(m), random.randrange(maxexp))
        yield from_triple(-1, ndigits(m), random.randrange(maxexp))

# If itertns == None, test all digit lengths up to prec + 30
# Also output decimals im tuple form.
def un_incr_digits_tuple(prec, maxexp, itertns):
    if itertns is None:
        lst = range(prec+30)
    else:
        lst = random.sample(range(prec+30), itertns)
    for m in lst:
        yield from_triple(1, ndigits(m), 0)
        yield from_triple(-1, ndigits(m), 0)
        yield from_triple(1, ndigits(m), random.randrange(maxexp))
        yield from_triple(-1, ndigits(m), random.randrange(maxexp))
        # test from tuple
        yield (0, tuple(map(int, str(ndigits(m)))), 0)
        yield (1, tuple(map(int, str(ndigits(m)))), 0)
        yield (0, tuple(map(int, str(ndigits(m)))), random.randrange(maxexp))
        yield (1, tuple(map(int, str(ndigits(m)))), random.randrange(maxexp))

# If itertns == None, test all combinations of digit lengths up to prec + 30
def bin_incr_digits(prec, maxexp, itertns):
    if itertns is None:
        lst1 = range(prec+30)
        lst2 = range(prec+30)
    else:
        lst1 = random.sample(range(prec+30), itertns)
        lst2 = random.sample(range(prec+30), itertns)
    for m in lst1:
        x = from_triple(1, ndigits(m), 0)
        yield x, x
        x = from_triple(-1, ndigits(m), 0)
        yield x, x
        x = from_triple(1, ndigits(m), random.randrange(maxexp))
        yield x, x
        x = from_triple(-1, ndigits(m), random.randrange(maxexp))
        yield x, x
    for m in lst1:
        for n in lst2:
            x = from_triple(1, ndigits(m), 0)
            y = from_triple(1, ndigits(n), 0)
            yield x, y
            x = from_triple(-1, ndigits(m), 0)
            y = from_triple(1, ndigits(n), 0)
            yield x, y
            x = from_triple(1, ndigits(m), 0)
            y = from_triple(-1, ndigits(n), 0)
            yield x, y
            x = from_triple(-1, ndigits(m), 0)
            y = from_triple(-1, ndigits(n), 0)
            yield x, y
            x = from_triple(1, ndigits(m), random.randrange(maxexp))
            y = from_triple(1, ndigits(n), random.randrange(maxexp))
            yield x, y
            x = from_triple(-1, ndigits(m), random.randrange(maxexp))
            y = from_triple(1, ndigits(n), random.randrange(maxexp))
            yield x, y
            x = from_triple(1, ndigits(m), random.randrange(maxexp))
            y = from_triple(-1, ndigits(n), random.randrange(maxexp))
            yield x, y
            x = from_triple(-1, ndigits(m), random.randrange(maxexp))
            y = from_triple(-1, ndigits(n), random.randrange(maxexp))
            yield x, y


def randsign():
    return (1, -1)[random.randrange(2)]

# If itertns == None, test all combinations of digit lengths up to prec + 30
def tern_incr_digits(prec, maxexp, itertns):
    if itertns is None:
        lst1 = range(prec+30)
        lst2 = range(prec+30)
        lst3 = range(prec+30)
    else:
        lst1 = random.sample(range(prec+30), itertns)
        lst2 = random.sample(range(prec+30), itertns)
        lst3 = random.sample(range(prec+30), itertns)
    for m in lst1:
        for n in lst2:
            for p in lst3:
                x = from_triple(randsign(), ndigits(m), 0)
                y = from_triple(randsign(), ndigits(n), 0)
                z = from_triple(randsign(), ndigits(p), 0)
                yield x, y, z


# Tests for the 'logical' functions
def bindigits(prec):
    z = 0
    for i in range(prec):
        z += random.randrange(2) * 10**i
    return z

def logical_un_incr_digits(prec, itertns):
    if itertns is None:
        lst = range(prec+30)
    else:
        lst = random.sample(range(prec+30), itertns)
    for m in lst:
        yield from_triple(1, bindigits(m), 0)

def logical_bin_incr_digits(prec, itertns):
    if itertns is None:
        lst1 = range(prec+30)
        lst2 = range(prec+30)
    else:
        lst1 = random.sample(range(prec+30), itertns)
        lst2 = random.sample(range(prec+30), itertns)
    for m in lst1:
        x = from_triple(1, bindigits(m), 0)
        yield x, x
    for m in lst1:
        for n in lst2:
            x = from_triple(1, bindigits(m), 0)
            y = from_triple(1, bindigits(n), 0)
            yield x, y


py_major = sys.version_info[0]
if py_major == 2:
    def touni(c): return unichr(c)
else:
    def touni(c): return chr(c)

# Generate list of all unicode characters that are accepted
# as fill characters by decimal.py.
def gen_unicode_chars():
    from decimal import Decimal
    sys.stderr.write("\ngenerating unicode chars ... ")
    r = []
    for c in range(32, 0x110001):
        try:
            x = touni(c)
            try:
                x.encode('utf-8').decode()
                format(Decimal(0), x + '<19g')
                r.append(x)
            except:
                pass
        except ValueError:
            pass
    r.remove(touni(ord("'")))
    r.remove(touni(ord('"')))
    r.remove(touni(ord('\\')))
    sys.stderr.write("DONE\n\n")
    return r

unicode_chars = []
def rand_unicode():
    global unicode_chars
    if not unicode_chars:
        unicode_chars = gen_unicode_chars()
    return random.choice(unicode_chars)


# Generate random format strings
# [[fill]align][sign][#][0][width][.precision][type]
def rand_format(fill):
    active = sorted(random.sample(range(7), random.randrange(8)))
    have_align = 0
    s = ''
    for elem in active:
        if elem == 0: # fill+align
            s += fill
            s += random.choice('<>=^')
            have_align = 1
        elif elem == 1: # sign
            s += random.choice('+- ')
        elif elem == 2 and not have_align: # zeropad
            s += '0'
        elif elem == 3: # width
            s += str(random.randrange(1, 100))
        elif elem == 4: # thousands separator
            s += ','
        elif elem == 5: # prec
            s += '.'
            # decimal.py does not support prec=0
            s += str(random.randrange(1, 100))
        elif elem == 6:
            if 4 in active: c = 'EeGgFf%'
            else: c = 'EeGgFfn%'
            s += random.choice(c)
    return s
