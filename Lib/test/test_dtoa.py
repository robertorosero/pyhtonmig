import random
import struct
import unittest
import math
import decimal
import test.support
import sys

def bitfield(n, a, b):
    """Extract bits a through b - 1 (inclusive) of an integer n."""
    return (n >> a) & ((1 << b - a) - 1)

# Some utility functions for manipulating Decimal instances.
# XXX These are *slow*.

def decimal_to_triple(d):
    """Express a finite Decimal instance as a triple (s, c, e) of
    integers, with value (-1)**s * c * 10**e."""
    sign, digits, exp = d.as_tuple()
    return sign, int(''.join(map(str, digits))), exp

def decimal_from_triple(sign, coefficient, exponent):
    """Create a Decimal instance from a triple; inverse of
    decimal_to_triple."""
    return decimal.Decimal((sign,
                            tuple(map(int, list(str(coefficient)))),
                            exponent))

def next_fixed(d):
    """Next decimal.Decimal up with the same exponent."""
    s, c, e = decimal_to_triple(d)
    return decimal_from_triple(s, c+1, e)

def next_floating(d):
    """Next decimal.Decimal up with the same coefficient length."""
    s, c, e = decimal_to_triple(d)
    # if c is one less than a power of 10...
    strc = str(c)
    if strc == '9' * len(strc):
        return decimal_from_triple(s, (c + 1) // 10, e + 1)
    else:
        return decimal_from_triple(s, c + 1, e)

class MockFloat(object):
    """Mock float class, implemented using integer arithmetic.

    Provides integer and Decimal-based formatting operations, for the
    purposes of comparing with true float str, repr and formatting.

    This class models IEEE 754 binary64 floating-point, except that:

       - NaNs and infinities aren't represented, and
       - There's no upper bound on the exponent.

    Subnormals and signed zeros are supported.

    """
    # parameters of the system float format
    MIN_EXP = sys.float_info.min_exp
    MANT_DIG = sys.float_info.mant_dig
    SPECIAL_EXP = sys.float_info.max_exp - MIN_EXP + 2
    TOTAL_BITS = MANT_DIG + SPECIAL_EXP.bit_length()

    def __new__(cls, x):
        if isinstance(x, tuple):
            # create from triple (s, m, e), representing the value (-1)**s * m
            # * 2**e.  The triple is required to be normalized, in the sense
            # that:
            #
            #   * e >= MIN_EXP - MANT_DIG
            #   * m.bit_length() <= MANT_DIG
            #   * either e == MIN_EXP - MANT_DIG or m.bit_length() == MANT_DIG
            self = object.__new__(cls)
            self.sign, self.coefficient, self.exponent = x
            if not self.is_normalized():
                raise ValueError("unnormalized MockFloat instance")
            return self
        elif isinstance(x, float):
            return cls.from_float(x)
        elif isinstance(x, decimal.Decimal):
            return cls.from_decimal(x)
        else:
            raise TypeError("Don't know how to construct a MockFloat "
                            "from an object of type {}".format(type(x)))

    def __bool__(self):
        return bool(self.coefficient)

    def __eq__(self, other):
        return (self.sign == other.sign and
                self.coefficient == other.coefficient and
                self.exponent == other.exponent)

    def __repr__(self):
        if not self:
            return '-0.0' if self.sign else '0.0'

        sign, coeff, exp = decimal_to_triple(self.to_shortest_decimal())
        digits = str(coeff)
        k = len(str(coeff)) + exp
        if k <= -4 or k > 16:
            # Use scientific notation.
            res = '{}{}e{:+03d}'.format(digits[:1],
                                        '.' + digits[1:] if digits[1:] else '',
                                        k - 1)
        else:
            # Non-scientific notation; may need to pad with zeros on either the
            # left or the right of the digit string.
            dotpos = exp + len(digits)
            if dotpos <= 0:
                res = '0.' + '0'*-dotpos + digits
            elif dotpos >= len(digits):
                res = digits + '0'*(dotpos-len(digits)) + '.0'
            else:
                res = digits[:dotpos] + '.' + digits[dotpos:]
        return '-' + res if sign else res

    def is_normalized(self):
        m, e = self.coefficient, self.exponent
        return (m >= 0 and
                e >= self.MIN_EXP - self.MANT_DIG and
                m.bit_length() <= self.MANT_DIG and
                (e == self.MIN_EXP - self.MANT_DIG or
                 m.bit_length() == self.MANT_DIG))

    @classmethod
    def from_float(cls, x):
        """Convert a finite float to an equivalent MockFloat."""

        x_bits = struct.pack('<d', x)
        n = int.from_bytes(x_bits, 'little')

        # Break into fields: significand m, exponent e, and sign s.
        coeff = bitfield(n, 0, cls.MANT_DIG - 1)
        exp = bitfield(n, cls.MANT_DIG - 1, cls.TOTAL_BITS - 1)
        sign = bitfield(n, cls.TOTAL_BITS - 1, cls.TOTAL_BITS)
        if exp == cls.SPECIAL_EXP:
            # NaNs and infinities.
            raise ValueError("from_float expects a finite value, "
                             "not an infinity or nan.")

        elif exp == 0:
            # Zeros and subnormal values.
            return cls((sign, coeff, cls.MIN_EXP - cls.MANT_DIG + exp))
        else:
            # Normal values.
            return cls((sign, coeff + (1 << cls.MANT_DIG - 1),
                        cls.MIN_EXP - cls.MANT_DIG - 1 + exp))

    @classmethod
    def from_decimal(cls, x):
        """Convert a finite Decimal instance to the nearest MockFloat."""

        s, n, d = decimal_to_triple(x)
        # Express abs(x) as an integer fraction, a / b
        a, b = n * 10**max(d, 0), 10**max(0, -d)
        if a:
            # Identify exponent e such that 2**(e-1) <= a / b < 2**e.
            # The difference 'e_test' between the bit lengths of a and b
            # gives a value that's either correct, or one too small.
            e_test = a.bit_length() - b.bit_length()
            scaled_a = a >> e_test if e_test >= 0 else a << -e_test
            e = e_test if scaled_a < b else e_test + 1
        else:
            e = cls.MIN_EXP

        # Adjust e to give a result with MANT_DIG bits of precision for normal
        # numbers, or such that e == MIN_EXP - MANT_DIG for subnormals.
        e = max(e, cls.MIN_EXP) - cls.MANT_DIG

        # Now approximate a / b by number of the form m * 2**e.  For rounding
        # purposes, we compute an extra 2 bits for m,  hence the '2' in '2-e'.
        p2 = 2 - e
        a, b = a << max(p2, 0), b << max(0, -p2)
        m, r = divmod(a, b)
        # Absorb any remainder into the last bit of m, then find the nearest
        # integer to m / 4, using round-half-to-even.
        if r:
            m |= 1
        m = (m >> 2) + (bool(m & 2) and bool(m & 5))
        if m.bit_length() == cls.MANT_DIG + 1:
            m //= 2
            e += 1

        return cls((s, m, e))

    def decimal_exponent(self):
        """The unique integer d such that 10**(d-1) <= abs(self) < 10**d.

        If self is zero, there's no such integer; a ValueError is raised in
        this case.

        """
        if not self:
            raise ValueError("decimal_exponent expects a nonzero argument")

        e = self.exponent
        # Express abs(self) as a fraction, a / b, and compute an approximation
        # to log10(abs(self)) by counting decimal digits in a and b.
        a, b = self.coefficient << max(e, 0), 1 << max(0, -e)
        d = len(str(a)) - len(str(b))
        # d is either correct, or one too small; compare a / 10**d
        # with b to find out which.
        scaled_a = a // 10**d if d >= 0 else a * 10**-d
        if scaled_a >= b:
            d += 1
        return d

    def to_decimal(self, d):
        """Convert to a decimal with a particular exponent d.

        Returns a pair x, r, where:

           x is the largest decimal with exponent d that doesn't exceed self.
           r is False if x is the *closest* decimal with exponent d to
              self, else True.

        """

        p2 = self.exponent - d + 2
        n, r = divmod(self.coefficient * 5**max(-d, 0) << max(p2, 0),
                      5**max(d, 0) << max(-p2, 0))
        if r:
            n |= 1
        round_up = bool(n & 2) and bool(n & 5)
        return decimal_from_triple(self.sign, n >> 2, d), round_up

    def to_fixed_precision(self, d):
        """Round self to the nearest integral multiple of 10**d."""
        n, r = self.to_decimal(d)
        return next_fixed(n) if r else n

    def to_significant_figures(self, f):
        """Round self to the given number of significant figures.

        self should be nonzero;  if it's zero a ValueError is raised.

        """
        if f < 1:
            raise ValueError("significant figures should be positive.")
        if not self:
            raise ValueError("Refusing to compute significant digits of 0.")

        d = self.decimal_exponent() - f
        n, r = self.to_decimal(d)
        return next_floating(n) if r else n

    def to_shortest_decimal(self):
        """Find shortest Decimal value that rounds back to the original."""

        if not self:
            raise ValueError("Refusing to compute shortest decimal for 0.")

        d = self.decimal_exponent() - 1
        while True:
            lower, round_up = self.to_decimal(d)
            upper = next_floating(lower)

            lower_ok = MockFloat(lower) == self
            upper_ok = MockFloat(upper) == self

            if upper_ok and (round_up or not lower_ok):
                return upper
            elif lower_ok:
                return lower
            d -= 1


class ReprTests(unittest.TestCase):
    def check_repr(self, x):
        """Compare float.__repr__ with MockFloat.__repr__."""
        expected = repr(MockFloat(x))
        got = repr(x)
        self.assertEqual(expected, got,
                         "Incorrect repr for {}: "
                         "expected {}, got {}".format(x, expected, got))

    def test_random(self):
        for _ in range(1000):
            # bit pattern corresponding to a random finite positive float
            bits = random.randrange(2047*2**52)
            x = struct.unpack('<d', struct.pack('<Q', bits))[0]
            self.check_repr(x)

    def test_particular(self):
        self.check_repr(0.0)
        self.check_repr(-0.0)
        self.check_repr(2.3)
        self.check_repr(4.788141995955901e+131)
        # case where it matters that we're using round half to even
        self.check_repr(1e23)

def test_main():
    test.support.run_unittest(ReprTests)

if __name__ == "__main__":
    test_main()
