#!/usr/bin/env python

#
# Copyright (C) 2001-2010 Python Software Foundation. All Rights Reserved.
# Modified and extended by Stefan Krah.
#


import time
from math import log, ceil
import decimal
import cdecimal
from gmpy import mpf, mpz, set_minprec


Decimal = decimal.Decimal
Cdecimal = cdecimal.Decimal


# Pi function from the decimal.py documentation
def pi_native(prec=None):
    """native float"""
    lasts, t, s, n, na, d, da = 0, 3.0, 3, 1, 0, 0, 24
    while s != lasts:
        lasts = s
        n, na = n+na, na+8
        d, da = d+da, da+32
        t = (t * n) / d
        s += t
    return s

def pi_decimal(prec):
    """Decimal"""
    decimal.getcontext().prec = prec
    lasts, t, s, n, na, d, da = Decimal(0), Decimal(3), Decimal(3), Decimal(1), Decimal(0), Decimal(0), Decimal(24)
    while s != lasts:
        lasts = s
        n, na = n+na, na+8
        d, da = d+da, da+32
        t = (t * n) / d
        s += t
    return s

def pi_Cdecimal(prec):
    """cdecimal.Cdecimal"""
    cdecimal.getcontext().prec = prec
    lasts, t, s, n, na, d, da = Cdecimal(0), Cdecimal(3), Cdecimal(3), Cdecimal(1), Cdecimal(0), Cdecimal(0), Cdecimal(24)
    while s != lasts:
        lasts = s
        n, na = n+na, na+8
        d, da = d+da, da+32
        t = (t * n) / d
        s += t
    return s

def pi_mpf(prec):
    """gmp.mpf"""
    set_minprec(prec)
    lasts, t, s, n, na, d, da = mpf(0), mpf(3), mpf(3), mpf(1), mpf(0), mpf(0), mpf(24)
    while s != lasts:
        lasts = s
        n, na = n+na, na+8
        d, da = d+da, da+32
        t = (t * n) / d
        s += t
    return s


def factorial(n, m):
    if (n > m):
        return factorial(m, n)
    elif m == 0:
        return 1
    elif n == m:
        return n
    else:
        return factorial(n, (n+m)//2) * factorial((n+m)//2 + 1, m)


d = {
    "native":  pi_native,
    "cdecimal": pi_Cdecimal,
    "gmpy":    pi_mpf,
    "decimal": pi_decimal,
}


def prec_to_bits(prec):
    return ceil(prec * log(10) / log(2))



print("\n# ======================================================================")
print("#                   Calculating pi, 10000 iterations")
print("# ======================================================================\n")

for prec in [9, 19, 38, 100]:
    print("\nPrecision: %d decimal digits\n" % prec)
    for name in ["native", "cdecimal", "gmpy", "decimal"]:
        if prec > 19 and (name == "native" or name == "decimal"):
            continue
        p = prec
        if name == "gmpy":
            p = int(prec_to_bits(prec))
        func = d[name]
        start = time.time()
        for i in range(10000):
            x = func(p)
        print("%s:" % name)
        print("result: %s" % str(x))
        print("time: %fs\n" % (time.time()-start))


print("\n# ======================================================================")
print("#                               Factorial")
print("# ======================================================================\n")

cdecimal.getcontext().prec = cdecimal.MAX_PREC

for n in [100000, 1000000, 10000000, 100000000]:

    print("n = %d\n" % n)

    start_calc = time.time()
    x = factorial(Cdecimal(n), 0)
    end_calc = time.time()
    start_conv = time.time()
    sx = str(x)
    end_conv = time.time()
    print("cdecimal:")
    print("calculation time: %fs" % (end_calc-start_calc))
    print("conversion time: %fs\n" % (end_conv-start_conv))

    start_calc = time.time()
    y = factorial(mpz(n), 0)
    end_calc = time.time()
    start_conv = time.time()
    sy = str(y)
    end_conv =  time.time()

    print("gmpy:")
    print("calculation time: %fs" % (end_calc-start_calc))
    print("conversion time: %fs\n\n" % (end_conv-start_conv))

    assert(sx == sy)
