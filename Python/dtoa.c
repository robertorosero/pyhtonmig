/****************************************************************
 *
 * The author of this software is David M. Gay.
 *
 * Copyright (c) 1991, 2000, 2001 by Lucent Technologies.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHOR NOR LUCENT MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 ***************************************************************/

/****************************************************************
 * This is dtoa.c by David M. Gay, downloaded from
 * http://www.netlib.org/fp/dtoa.c on April 15, 2009 and modified for
 * inclusion into the Python core by Mark E. T. Dickinson and Eric V. Smith.
 *
 * Please remember to check http://www.netlib.org/fp regularly (and especially
 * before any Python release) for bugfixes and updates.
 *
 * The major modifications from Gay's original code are as follows:
 *
 *  0. The original code has been specialized to Python's needs by removing
 *     many of the #ifdef'd sections.  In particular, code to support VAX and
 *     IBM floating-point formats, hex NaNs, hex floats, locale-aware
 *     treatment of the decimal point, and setting of the inexact flag have
 *     been removed.
 *
 *  1. We use PyMem_Malloc and PyMem_Free in place of malloc and free.
 *
 *  2. The public functions strtod, dtoa and freedtoa all now have
 *     a _Py_dg_ prefix.
 *
 *  3. Instead of assuming that PyMem_Malloc always succeeds, we thread
 *     PyMem_Malloc failures through the code.  The functions
 *
 *       Balloc, multadd, s2b, i2b, mult, pow5mult, lshift, diff, d2b
 *
 *     of return type *Bigint all return NULL to indicate a malloc failure.
 *     Similarly, rv_alloc and nrv_alloc (return type char *) return NULL on
 *     failure.  bigcomp now has return type int (it used to be void) and
 *     returns -1 on failure and 0 otherwise.  _Py_dg_dtoa returns NULL
 *     on failure.  _Py_dg_strtod indicates failure due to malloc failure
 *     by returning -1.0, setting errno=ENOMEM and *se to s00.
 *
 *  4. The static variable dtoa_result has been removed.  Callers of
 *     _Py_dg_dtoa are expected to call _Py_dg_freedtoa to free
 *     the memory allocated by _Py_dg_dtoa.
 *
 *  5. The code has been reformatted to better fit with Python's
 *     C style guide (PEP 7).
 *
 *  6. A bug in the memory allocation has been fixed: to avoid FREEing memory
 *     that hasn't been MALLOC'ed, private_mem should only be used when k <=
 *     Kmax.
 *
 *  7. _Py_dg_strtod has been modified so that it doesn't accept strings with
 *     leading whitespace.
 *
 ***************************************************************/

/* Please send bug reports for the original dtoa.c code to David M. Gay (dmg
 * at acm dot org, with " at " changed at "@" and " dot " changed to ".").
 * Please report bugs for this modified version using the Python issue tracker
 * (http://bugs.python.org). */

/* On a machine with IEEE extended-precision registers, it is
 * necessary to specify double-precision (53-bit) rounding precision
 * before invoking strtod or dtoa.  If the machine uses (the equivalent
 * of) Intel 80x87 arithmetic, the call
 *      _control87(PC_53, MCW_PC);
 * does this with many compilers.  Whether this or another call is
 * appropriate depends on the compiler; for this to work, it may be
 * necessary to #include "float.h" or another system-dependent header
 * file.
 */

/* strtod for IEEE-, VAX-, and IBM-arithmetic machines.
 *
 * This strtod returns a nearest machine number to the input decimal
 * string (or sets errno to ERANGE).  With IEEE arithmetic, ties are
 * broken by the IEEE round-even rule.  Otherwise ties are broken by
 * biased rounding (add half and chop).
 *
 * Inspired loosely by William D. Clinger's paper "How to Read Floating
 * Point Numbers Accurately" [Proc. ACM SIGPLAN '90, pp. 92-101].
 *
 * Modifications:
 *
 *      1. We only require IEEE, IBM, or VAX double-precision
 *              arithmetic (not IEEE double-extended).
 *      2. We get by with floating-point arithmetic in a case that
 *              Clinger missed -- when we're computing d * 10^n
 *              for a small integer d and the integer n is not too
 *              much larger than 22 (the maximum integer k for which
 *              we can represent 10^k exactly), we may be able to
 *              compute (d*10^k) * 10^(e-k) with just one roundoff.
 *      3. Rather than a bit-at-a-time adjustment of the binary
 *              result in the hard case, we use floating-point
 *              arithmetic to determine the adjustment to within
 *              one bit; only in really hard cases do we need to
 *              compute a second residual.
 *      4. Because of 3., we don't need a large table of powers of 10
 *              for ten-to-e (just some small tables, e.g. of 10^k
 *              for 0 <= k <= 22).
 */

/* Linking of Python's #defines to Gay's #defines starts here. */

#include "Python.h"

/* if PY_NO_SHORT_FLOAT_REPR is defined, then don't even try to compile
   the following code */
#ifndef PY_NO_SHORT_FLOAT_REPR

#include "float.h"

#define MALLOC PyMem_Malloc
#define FREE PyMem_Free

/* This code should also work for ARM mixed-endian format on little-endian
   machines, where doubles have byte order 45670123 (in increasing address
   order, 0 being the least significant byte). */
#ifdef DOUBLE_IS_LITTLE_ENDIAN_IEEE754
#  define IEEE_8087
#endif
#if defined(DOUBLE_IS_BIG_ENDIAN_IEEE754) ||  \
  defined(DOUBLE_IS_ARM_MIXED_ENDIAN_IEEE754)
#  define IEEE_MC68k
#endif
#if defined(IEEE_8087) + defined(IEEE_MC68k) != 1
#error "Exactly one of IEEE_8087 or IEEE_MC68k should be defined."
#endif

/* The code below assumes that the endianness of integers matches the
   endianness of the two 32-bit words of a double.  Check this. */
#if defined(WORDS_BIGENDIAN) && (defined(DOUBLE_IS_LITTLE_ENDIAN_IEEE754) || \
                                 defined(DOUBLE_IS_ARM_MIXED_ENDIAN_IEEE754))
#error "doubles and ints have incompatible endianness"
#endif

#if !defined(WORDS_BIGENDIAN) && defined(DOUBLE_IS_BIG_ENDIAN_IEEE754)
#error "doubles and ints have incompatible endianness"
#endif


#if defined(HAVE_UINT32_T) && defined(HAVE_INT32_T)
typedef PY_UINT32_T ULong;
typedef PY_INT32_T Long;
#else
#error "Failed to find an exact-width 32-bit integer type"
#endif

#if defined(HAVE_UINT64_T)
#define ULLong PY_UINT64_T
#else
#undef ULLong
#endif

#undef DEBUG
#ifdef Py_DEBUG
#define DEBUG
#endif

/* End Python #define linking */

#ifdef DEBUG
#define Bug(x) {fprintf(stderr, "%s\n", x); exit(1);}
#endif

#ifndef PRIVATE_MEM
#define PRIVATE_MEM 2304
#endif
#define PRIVATE_mem ((PRIVATE_MEM+sizeof(double)-1)/sizeof(double))
static double private_mem[PRIVATE_mem], *pmem_next = private_mem;

#ifdef __cplusplus
extern "C" {
#endif

typedef union { ULong L[2]; double d; } U;

#ifdef IEEE_8087
#define word0(x) (x)->L[1]
#define word1(x) (x)->L[0]
#define Uint(hi, lo) {{lo, hi}}
#else
#define word0(x) (x)->L[0]
#define word1(x) (x)->L[1]
#define Uint(hi, lo) {{hi, lo}}
#endif
#define dval(x) (x)->d

/* STRTOD_DIGLIM = 17 + 8.  17 is the number of decimal digits required to
   distinguish doubles; the extra 8 digits ensure that truncation to
   STRTOD_DIGLIM digits induces an error of at most 1e-8 ulps. */
#define STRTOD_DIGLIM 25

/* maximum permitted exponent value for strtod; exponents larger than
   MAX_ABS_EXP in absolute value get truncated to +-MAX_ABS_EXP.  MAX_ABS_EXP
   should fit into an int. */
#ifndef MAX_ABS_EXP
#define MAX_ABS_EXP 19999U
#endif

/* The following definition of Storeinc is appropriate for MIPS processors.
 * An alternative that might be better on some machines is
 * #define Storeinc(a,b,c) (*a++ = b << 16 | c & 0xffff)
 */
#if defined(IEEE_8087)
#define Storeinc(a,b,c) (((unsigned short *)a)[1] = (unsigned short)b,  \
                         ((unsigned short *)a)[0] = (unsigned short)c, a++)
#else
#define Storeinc(a,b,c) (((unsigned short *)a)[0] = (unsigned short)b,  \
                         ((unsigned short *)a)[1] = (unsigned short)c, a++)
#endif

/* #define P DBL_MANT_DIG */
/* Ten_pmax = floor(P*log(2)/log(5)) */
/* Bletch = (highest power of 2 < DBL_MAX_10_EXP) / 16 */
/* Quick_max = floor((P-1)*log(FLT_RADIX)/log(10) - 1) */
/* Int_max = floor(P*log(FLT_RADIX)/log(10) - 1) */

#define Exp_shift  20
#define Exp_shift1 20
#define Exp_msk1    0x100000
#define Exp_msk11   0x100000
#define Exp_mask  0x7ff00000
#define P 53
#define Nbits 53
#define Bias 1023
#define Emax 1023
#define Emin (-1022)
#define Etiny (-1074)  /* smallest denormal is 2**Etiny */
#define Exp_1  0x3ff00000
#define Exp_11 0x3ff00000
#define Ebits 11
#define Frac_mask  0xfffff
#define Frac_mask1 0xfffff
#define Ten_pmax 22
#define Big_10_exp 309      /* Values >= 10**Big_10_exp overflow. */
#define Tiny_10_exp -324    /* Values < 10**Tiny_10_exp underflow to zero. */
#define Bletch 0x10
#define Bndry_mask  0xfffff
#define Bndry_mask1 0xfffff
#define Sign_bit 0x80000000
#define Log2P 1
#define Tiny0 0
#define Tiny1 1
#define Quick_max 14
#define Int_max 14

/* Some double constants, defined via the union U to avoid the chicken-and-egg
   problem of relying on the compiler to do correctly-rounded string->double
   conversions. */
static U Dbl_min = Uint((Bias + Emin)*Exp_msk1, 0); /* 2.0 ** Emin */
static U Exp4P = Uint((Bias + 2*P)*Exp_msk1, 0);    /* 2.0**(2*P) */
static U Inf = Uint(Exp_mask, 0);

#ifndef Flt_Rounds
#ifdef FLT_ROUNDS
#define Flt_Rounds FLT_ROUNDS
#else
#define Flt_Rounds 1
#endif
#endif /*Flt_Rounds*/

#define Rounding Flt_Rounds

#define Big0 (Frac_mask1 | Exp_msk1*(DBL_MAX_EXP+Bias-1))
#define Big1 0xffffffff

/* struct BCinfo is used to pass information from _Py_dg_strtod to bigcomp */

typedef struct BCinfo BCinfo;
struct
BCinfo {
    Py_ssize_t nd, nd0;
    int e0, scale;
};

#define FFFFFFFF 0xffffffffUL

#define Kmax 7

/* struct Bigint is used to represent arbitrary-precision integers.  These
   integers are stored in sign-magnitude format, with the magnitude stored as
   an array of base 2**32 digits.  Bigints are always normalized: if x is a
   Bigint then x->wds >= 1, and either x->wds == 1 or x[wds-1] is nonzero.

   The Bigint fields are as follows:

     - next is a header used by Balloc and Bfree to keep track of lists
         of freed Bigints;  it's also used for the linked list of
         powers of 5 of the form 5**2**i used by pow5mult.
     - k indicates which pool this Bigint was allocated from
     - maxwds is the maximum number of words space was allocated for
       (usually maxwds == 2**k)
     - sign is 1 for negative Bigints, 0 for positive.  The sign is unused
       (ignored on inputs, set to 0 on outputs) in almost all operations
       involving Bigints: a notable exception is the diff function, which
       ignores signs on inputs but sets the sign of the output correctly.
     - wds is the actual number of significant words
     - x contains the vector of words (digits) for this Bigint, from least
       significant (x[0]) to most significant (x[wds-1]).
*/

struct
Bigint {
    struct Bigint *next;
    int k, maxwds, sign, wds;
    ULong x[1];
};

typedef struct Bigint Bigint;

#ifndef Py_USING_MEMORY_DEBUGGER

/* Memory management: memory is allocated from, and returned to, Kmax+1 pools
   of memory, where pool k (0 <= k <= Kmax) is for Bigints b with b->maxwds ==
   1 << k.  These pools are maintained as linked lists, with freelist[k]
   pointing to the head of the list for pool k.

   On allocation, if there's no free slot in the appropriate pool, MALLOC is
   called to get more memory.  This memory is not returned to the system until
   Python quits.  There's also a private memory pool that's allocated from
   in preference to using MALLOC.

   For Bigints with more than (1 << Kmax) digits (which implies at least 1233
   decimal digits), memory is directly allocated using MALLOC, and freed using
   FREE.

   XXX: it would be easy to bypass this memory-management system and
   translate each call to Balloc into a call to PyMem_Malloc, and each
   Bfree to PyMem_Free.  Investigate whether this has any significant
   performance on impact. */

static Bigint *freelist[Kmax+1];

/* Allocate space for a Bigint with up to 1<<k digits */

static Bigint *
Balloc(int k)
{
    int x;
    Bigint *rv;
    unsigned int len;

    if (k <= Kmax && (rv = freelist[k]))
        freelist[k] = rv->next;
    else {
        x = 1 << k;
        len = (sizeof(Bigint) + (x-1)*sizeof(ULong) + sizeof(double) - 1)
            /sizeof(double);
        if (k <= Kmax && pmem_next - private_mem + len <= PRIVATE_mem) {
            rv = (Bigint*)pmem_next;
            pmem_next += len;
        }
        else {
            rv = (Bigint*)MALLOC(len*sizeof(double));
            if (rv == NULL)
                return NULL;
        }
        rv->k = k;
        rv->maxwds = x;
    }
    rv->sign = rv->wds = 0;
    return rv;
}

/* Free a Bigint allocated with Balloc */

static void
Bfree(Bigint *v)
{
    if (v) {
        if (v->k > Kmax)
            FREE((void*)v);
        else {
            v->next = freelist[v->k];
            freelist[v->k] = v;
        }
    }
}

#else

/* Alternative versions of Balloc and Bfree that use PyMem_Malloc and
   PyMem_Free directly in place of the custom memory allocation scheme above.
   These are provided for the benefit of memory debugging tools like
   Valgrind. */

/* Allocate space for a Bigint with up to 1<<k digits */

static Bigint *
Balloc(int k)
{
    int x;
    Bigint *rv;
    unsigned int len;

    x = 1 << k;
    len = (sizeof(Bigint) + (x-1)*sizeof(ULong) + sizeof(double) - 1)
        /sizeof(double);

    rv = (Bigint*)MALLOC(len*sizeof(double));
    if (rv == NULL)
        return NULL;

    rv->k = k;
    rv->maxwds = x;
    rv->sign = rv->wds = 0;
    return rv;
}

/* Free a Bigint allocated with Balloc */

static void
Bfree(Bigint *v)
{
    if (v) {
        FREE((void*)v);
    }
}

#endif /* Py_USING_MEMORY_DEBUGGER */

#define Bcopy(x,y) memcpy((char *)&x->sign, (char *)&y->sign,   \
                          y->wds*sizeof(Long) + 2*sizeof(int))

/* Multiply a Bigint b by m and add a.  Either modifies b in place and returns
   a pointer to the modified b, or Bfrees b and returns a pointer to a copy.
   On failure, return NULL.  In this case, b will have been already freed. */

static Bigint *
multadd(Bigint *b, int m, int a)       /* multiply by m and add a */
{
    int i, wds;
#ifdef ULLong
    ULong *x;
    ULLong carry, y;
#else
    ULong carry, *x, y;
    ULong xi, z;
#endif
    Bigint *b1;

    wds = b->wds;
    x = b->x;
    i = 0;
    carry = a;
    do {
#ifdef ULLong
        y = *x * (ULLong)m + carry;
        carry = y >> 32;
        *x++ = (ULong)(y & FFFFFFFF);
#else
        xi = *x;
        y = (xi & 0xffff) * m + carry;
        z = (xi >> 16) * m + (y >> 16);
        carry = z >> 16;
        *x++ = (z << 16) + (y & 0xffff);
#endif
    }
    while(++i < wds);
    if (carry) {
        if (wds >= b->maxwds) {
            b1 = Balloc(b->k+1);
            if (b1 == NULL){
                Bfree(b);
                return NULL;
            }
            Bcopy(b1, b);
            Bfree(b);
            b = b1;
        }
        b->x[wds++] = (ULong)carry;
        b->wds = wds;
    }
    return b;
}

/* Convert a string s0 containing nd decimal digits (and possibly a decimal
   separator at position nd0, which is ignored) to a Bigint.  Returns NULL on
   failure.  Assumes that nd >= 1 and that the first digit is nonzero. */

static Bigint *
s2b(const char *s0, Py_ssize_t nd0, Py_ssize_t nd)
{
    Bigint *b;
    int k;
    Py_ssize_t i, x, y;

    x = 1 + (nd - 1) / 9;
    for (k = 0, y = 1; x > y; y <<= 1, k++) ;
    b = Balloc(k);
    if (b == NULL)
        return NULL;
    b->x[0] = 0;
    b->wds = 1;

    for (i = 0; i < nd; i++) {
        b = multadd(b, 10, s0[i < nd0 ? i : i + 1] - '0');
        if (b == NULL)
            return NULL;
    }
    return b;
}

/* count leading 0 bits in the 32-bit integer x. */

static int
hi0bits(ULong x)
{
    int k = 0;

    if (!(x & 0xffff0000)) {
        k = 16;
        x <<= 16;
    }
    if (!(x & 0xff000000)) {
        k += 8;
        x <<= 8;
    }
    if (!(x & 0xf0000000)) {
        k += 4;
        x <<= 4;
    }
    if (!(x & 0xc0000000)) {
        k += 2;
        x <<= 2;
    }
    if (!(x & 0x80000000)) {
        k++;
        if (!(x & 0x40000000))
            return 32;
    }
    return k;
}

/* count trailing 0 bits in the 32-bit integer y, and shift y right by that
   number of bits. */

static int
lo0bits(ULong *y)
{
    int k;
    ULong x = *y;

    if (x & 7) {
        if (x & 1)
            return 0;
        if (x & 2) {
            *y = x >> 1;
            return 1;
        }
        *y = x >> 2;
        return 2;
    }
    k = 0;
    if (!(x & 0xffff)) {
        k = 16;
        x >>= 16;
    }
    if (!(x & 0xff)) {
        k += 8;
        x >>= 8;
    }
    if (!(x & 0xf)) {
        k += 4;
        x >>= 4;
    }
    if (!(x & 0x3)) {
        k += 2;
        x >>= 2;
    }
    if (!(x & 1)) {
        k++;
        x >>= 1;
        if (!x)
            return 32;
    }
    *y = x;
    return k;
}

/* convert a small nonnegative integer to a Bigint */

static Bigint *
i2b(int i)
{
    Bigint *b;

    b = Balloc(1);
    if (b == NULL)
        return NULL;
    b->x[0] = i;
    b->wds = 1;
    return b;
}

/* multiply two Bigints.  Returns a new Bigint, or NULL on failure.  Ignores
   the signs of a and b. */

static Bigint *
mult(Bigint *a, Bigint *b)
{
    Bigint *c;
    int k, wa, wb, wc;
    ULong *x, *xa, *xae, *xb, *xbe, *xc, *xc0;
    ULong y;
#ifdef ULLong
    ULLong carry, z;
#else
    ULong carry, z;
    ULong z2;
#endif

    if ((!a->x[0] && a->wds == 1) || (!b->x[0] && b->wds == 1)) {
        c = Balloc(0);
        if (c == NULL)
            return NULL;
        c->wds = 1;
        c->x[0] = 0;
        return c;
    }

    if (a->wds < b->wds) {
        c = a;
        a = b;
        b = c;
    }
    k = a->k;
    wa = a->wds;
    wb = b->wds;
    wc = wa + wb;
    if (wc > a->maxwds)
        k++;
    c = Balloc(k);
    if (c == NULL)
        return NULL;
    for(x = c->x, xa = x + wc; x < xa; x++)
        *x = 0;
    xa = a->x;
    xae = xa + wa;
    xb = b->x;
    xbe = xb + wb;
    xc0 = c->x;
#ifdef ULLong
    for(; xb < xbe; xc0++) {
        if ((y = *xb++)) {
            x = xa;
            xc = xc0;
            carry = 0;
            do {
                z = *x++ * (ULLong)y + *xc + carry;
                carry = z >> 32;
                *xc++ = (ULong)(z & FFFFFFFF);
            }
            while(x < xae);
            *xc = (ULong)carry;
        }
    }
#else
    for(; xb < xbe; xb++, xc0++) {
        if (y = *xb & 0xffff) {
            x = xa;
            xc = xc0;
            carry = 0;
            do {
                z = (*x & 0xffff) * y + (*xc & 0xffff) + carry;
                carry = z >> 16;
                z2 = (*x++ >> 16) * y + (*xc >> 16) + carry;
                carry = z2 >> 16;
                Storeinc(xc, z2, z);
            }
            while(x < xae);
            *xc = carry;
        }
        if (y = *xb >> 16) {
            x = xa;
            xc = xc0;
            carry = 0;
            z2 = *xc;
            do {
                z = (*x & 0xffff) * y + (*xc >> 16) + carry;
                carry = z >> 16;
                Storeinc(xc, z, z2);
                z2 = (*x++ >> 16) * y + (*xc & 0xffff) + carry;
                carry = z2 >> 16;
            }
            while(x < xae);
            *xc = z2;
        }
    }
#endif
    for(xc0 = c->x, xc = xc0 + wc; wc > 0 && !*--xc; --wc) ;
    c->wds = wc;
    return c;
}

#ifndef Py_USING_MEMORY_DEBUGGER

/* p5s is a linked list of powers of 5 of the form 5**(2**i), i >= 2 */

static Bigint *p5s;

/* multiply the Bigint b by 5**k.  Returns a pointer to the result, or NULL on
   failure; if the returned pointer is distinct from b then the original
   Bigint b will have been Bfree'd.   Ignores the sign of b. */

static Bigint *
pow5mult(Bigint *b, int k)
{
    Bigint *b1, *p5, *p51;
    int i;
    static int p05[3] = { 5, 25, 125 };

    if ((i = k & 3)) {
        b = multadd(b, p05[i-1], 0);
        if (b == NULL)
            return NULL;
    }

    if (!(k >>= 2))
        return b;
    p5 = p5s;
    if (!p5) {
        /* first time */
        p5 = i2b(625);
        if (p5 == NULL) {
            Bfree(b);
            return NULL;
        }
        p5s = p5;
        p5->next = 0;
    }
    for(;;) {
        if (k & 1) {
            b1 = mult(b, p5);
            Bfree(b);
            b = b1;
            if (b == NULL)
                return NULL;
        }
        if (!(k >>= 1))
            break;
        p51 = p5->next;
        if (!p51) {
            p51 = mult(p5,p5);
            if (p51 == NULL) {
                Bfree(b);
                return NULL;
            }
            p51->next = 0;
            p5->next = p51;
        }
        p5 = p51;
    }
    return b;
}

#else

/* Version of pow5mult that doesn't cache powers of 5. Provided for
   the benefit of memory debugging tools like Valgrind. */

static Bigint *
pow5mult(Bigint *b, int k)
{
    Bigint *b1, *p5, *p51;
    int i;
    static int p05[3] = { 5, 25, 125 };

    if ((i = k & 3)) {
        b = multadd(b, p05[i-1], 0);
        if (b == NULL)
            return NULL;
    }

    if (!(k >>= 2))
        return b;
    p5 = i2b(625);
    if (p5 == NULL) {
        Bfree(b);
        return NULL;
    }

    for(;;) {
        if (k & 1) {
            b1 = mult(b, p5);
            Bfree(b);
            b = b1;
            if (b == NULL) {
                Bfree(p5);
                return NULL;
            }
        }
        if (!(k >>= 1))
            break;
        p51 = mult(p5, p5);
        Bfree(p5);
        p5 = p51;
        if (p5 == NULL) {
            Bfree(b);
            return NULL;
        }
    }
    Bfree(p5);
    return b;
}

#endif /* Py_USING_MEMORY_DEBUGGER */

/* shift a Bigint b left by k bits.  Return a pointer to the shifted result,
   or NULL on failure.  If the returned pointer is distinct from b then the
   original b will have been Bfree'd.   Ignores the sign of b. */

static Bigint *
lshift(Bigint *b, int k)
{
    int i, k1, n, n1;
    Bigint *b1;
    ULong *x, *x1, *xe, z;

    if (!k || (!b->x[0] && b->wds == 1))
        return b;

    n = k >> 5;
    k1 = b->k;
    n1 = n + b->wds + 1;
    for(i = b->maxwds; n1 > i; i <<= 1)
        k1++;
    b1 = Balloc(k1);
    if (b1 == NULL) {
        Bfree(b);
        return NULL;
    }
    x1 = b1->x;
    for(i = 0; i < n; i++)
        *x1++ = 0;
    x = b->x;
    xe = x + b->wds;
    if (k &= 0x1f) {
        k1 = 32 - k;
        z = 0;
        do {
            *x1++ = *x << k | z;
            z = *x++ >> k1;
        }
        while(x < xe);
        if ((*x1 = z))
            ++n1;
    }
    else do
             *x1++ = *x++;
        while(x < xe);
    b1->wds = n1 - 1;
    Bfree(b);
    return b1;
}

/* Do a three-way compare of a and b, returning -1 if a < b, 0 if a == b and
   1 if a > b.  Ignores signs of a and b. */

static int
cmp(Bigint *a, Bigint *b)
{
    ULong *xa, *xa0, *xb, *xb0;
    int i, j;

    i = a->wds;
    j = b->wds;
#ifdef DEBUG
    if (i > 1 && !a->x[i-1])
        Bug("cmp called with a->x[a->wds-1] == 0");
    if (j > 1 && !b->x[j-1])
        Bug("cmp called with b->x[b->wds-1] == 0");
#endif
    if (i -= j)
        return i;
    xa0 = a->x;
    xa = xa0 + j;
    xb0 = b->x;
    xb = xb0 + j;
    for(;;) {
        if (*--xa != *--xb)
            return *xa < *xb ? -1 : 1;
        if (xa <= xa0)
            break;
    }
    return 0;
}

/* Take the difference of Bigints a and b, returning a new Bigint.  Returns
   NULL on failure.  The signs of a and b are ignored, but the sign of the
   result is set appropriately. */

static Bigint *
diff(Bigint *a, Bigint *b)
{
    Bigint *c;
    int i, wa, wb;
    ULong *xa, *xae, *xb, *xbe, *xc;
#ifdef ULLong
    ULLong borrow, y;
#else
    ULong borrow, y;
    ULong z;
#endif

    i = cmp(a,b);
    if (!i) {
        c = Balloc(0);
        if (c == NULL)
            return NULL;
        c->wds = 1;
        c->x[0] = 0;
        return c;
    }
    if (i < 0) {
        c = a;
        a = b;
        b = c;
        i = 1;
    }
    else
        i = 0;
    c = Balloc(a->k);
    if (c == NULL)
        return NULL;
    c->sign = i;
    wa = a->wds;
    xa = a->x;
    xae = xa + wa;
    wb = b->wds;
    xb = b->x;
    xbe = xb + wb;
    xc = c->x;
    borrow = 0;
#ifdef ULLong
    do {
        y = (ULLong)*xa++ - *xb++ - borrow;
        borrow = y >> 32 & (ULong)1;
        *xc++ = (ULong)(y & FFFFFFFF);
    }
    while(xb < xbe);
    while(xa < xae) {
        y = *xa++ - borrow;
        borrow = y >> 32 & (ULong)1;
        *xc++ = (ULong)(y & FFFFFFFF);
    }
#else
    do {
        y = (*xa & 0xffff) - (*xb & 0xffff) - borrow;
        borrow = (y & 0x10000) >> 16;
        z = (*xa++ >> 16) - (*xb++ >> 16) - borrow;
        borrow = (z & 0x10000) >> 16;
        Storeinc(xc, z, y);
    }
    while(xb < xbe);
    while(xa < xae) {
        y = (*xa & 0xffff) - borrow;
        borrow = (y & 0x10000) >> 16;
        z = (*xa++ >> 16) - borrow;
        borrow = (z & 0x10000) >> 16;
        Storeinc(xc, z, y);
    }
#endif
    while(!*--xc)
        wa--;
    c->wds = wa;
    return c;
}

/* Convert a Bigint a to a double giving the value a / 2**(32 * a->wds).
   Error < 0.75 ulps.  This function is currently used only by ratio. */

static double
b2d(Bigint *a)
{
    int i, wds;
    double d;
    wds = a->wds;
    d = 0.0;
    for (i = wds > 3 ? wds - 3 : 0; i < wds; i++)
        d = (d + a->x[i]) * (1.0 / 4294967296.0);
    return d;
}

/* Convert a scaled double to a Bigint plus an exponent.  Similar to d2b,
   except that it accepts the scale parameter used in _Py_dg_strtod (which
   should be either 0 or 2*P), and the normalization for the return value is
   different (see below).  On input, d should be finite and nonnegative, and d
   / 2**scale should be exactly representable as an IEEE 754 double.

   Returns a Bigint b and an integer e such that

     dval(d) / 2**scale = b * 2**e.

   Unlike d2b, b is not necessarily odd: b and e are normalized so
   that either 2**(P-1) <= b < 2**P and e >= Etiny, or b < 2**P
   and e == Etiny.  This applies equally to an input of 0.0: in that
   case the return values are b = 0 and e = Etiny.

   The above normalization ensures that for all possible inputs d,
   2**e gives ulp(d/2**scale).

   Returns NULL on failure.
*/

static Bigint *
sd2b(U *d, int scale, int *e)
{
    Bigint *b;

    b = Balloc(1);
    if (b == NULL)
        return NULL;

    /* First construct b and e assuming that scale == 0. */
    b->wds = 2;
    b->x[0] = word1(d);
    b->x[1] = word0(d) & Frac_mask;
    *e = Etiny - 1 + (int)((word0(d) & Exp_mask) >> Exp_shift);
    if (*e < Etiny)
        *e = Etiny;
    else
        b->x[1] |= Exp_msk1;

    /* Now adjust for scale, provided that b != 0. */
    if (scale && (b->x[0] || b->x[1])) {
        *e -= scale;
        if (*e < Etiny) {
            scale = Etiny - *e;
            *e = Etiny;
            /* We can't shift more than P-1 bits without shifting out a 1. */
            assert(0 < scale && scale <= P - 1);
            if (scale >= 32) {
                /* The bits shifted out should all be zero. */
                assert(b->x[0] == 0);
                b->x[0] = b->x[1];
                b->x[1] = 0;
                scale -= 32;
            }
            if (scale) {
                /* The bits shifted out should all be zero. */
                assert(b->x[0] << (32 - scale) == 0);
                b->x[0] = (b->x[0] >> scale) | (b->x[1] << (32 - scale));
                b->x[1] >>= scale;
            }
        }
    }
    /* Ensure b is normalized. */
    if (!b->x[1])
        b->wds = 1;

    return b;
}

/* Convert a double to a Bigint plus an exponent.  Return NULL on failure.

   Given a finite nonzero double d, return an odd Bigint b and exponent *e
   such that fabs(d) = b * 2**e.  On return, *bbits gives the number of
   significant bits of b; that is, 2**(*bbits-1) <= b < 2**(*bbits).

   If d is zero, then b == 0, *e == -1010, *bbits = 0.
 */

static Bigint *
d2b(U *d, int *e, int *bits)
{
    Bigint *b;
    int de, k;
    ULong *x, y, z;
    int i;

    b = Balloc(1);
    if (b == NULL)
        return NULL;
    x = b->x;

    z = word0(d) & Frac_mask;
    word0(d) &= 0x7fffffff;   /* clear sign bit, which we ignore */
    if ((de = (int)(word0(d) >> Exp_shift)))
        z |= Exp_msk1;
    if ((y = word1(d))) {
        if ((k = lo0bits(&y))) {
            x[0] = y | z << (32 - k);
            z >>= k;
        }
        else
            x[0] = y;
        i =
            b->wds = (x[1] = z) ? 2 : 1;
    }
    else {
        k = lo0bits(&z);
        x[0] = z;
        i =
            b->wds = 1;
        k += 32;
    }
    if (de) {
        *e = de - Bias - (P-1) + k;
        *bits = P - k;
    }
    else {
        *e = de - Bias - (P-1) + 1 + k;
        *bits = 32*i - hi0bits(x[i-1]);
    }
    return b;
}

/* Compute the ratio of two Bigints, as a double.  The result may have an
   error of up to 3.5 ulps. */

static double
ratio(Bigint *a, Bigint *b)
{
    U da, db;
    int k;

#ifdef DEBUG
    if (!b->x[0] && b->wds == 1)
        Bug("zero divisor in ratio");
#endif

    dval(&da) = b2d(a);
    dval(&db) = b2d(b);
    k = a->wds - b->wds;
    if (k >= 0)
        word0(&da) += 32*Exp_msk1*k;
    else
        word0(&db) += 32*Exp_msk1*-k;
    return dval(&da) / dval(&db);
}

static const double
tens[] = {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
    1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
    1e20, 1e21, 1e22
};

static const double bigtens[] = { 1e16, 1e32, 1e64, 1e128, 1e256 };
static const double tinytens[] = { 1e-16, 1e-32, 1e-64, 1e-128, 1e-256 };
#define Scale_Bit 0x10
#define n_bigtens 5

#define ULbits 32
#define kshift 5
#define kmask 31


static int
dshift(Bigint *b, int p2)
{
    int rv = hi0bits(b->x[b->wds-1]) - 4;
    if (p2 > 0)
        rv -= p2;
    return rv & kmask;
}

/* special case of Bigint division.  The quotient is always in the range 0 <=
   quotient < 10, and on entry the divisor S is normalized so that its top 4
   bits (28--31) are zero and bit 27 is set. */

static int
quorem(Bigint *b, Bigint *S)
{
    int n;
    ULong *bx, *bxe, q, *sx, *sxe;
#ifdef ULLong
    ULLong borrow, carry, y, ys;
#else
    ULong borrow, carry, y, ys;
    ULong si, z, zs;
#endif

    n = S->wds;
#ifdef DEBUG
    /*debug*/ if (b->wds > n)
        /*debug*/       Bug("oversize b in quorem");
#endif
    if (b->wds < n)
        return 0;
    sx = S->x;
    sxe = sx + --n;
    bx = b->x;
    bxe = bx + n;
    q = *bxe / (*sxe + 1);      /* ensure q <= true quotient */
#ifdef DEBUG
    /*debug*/ if (q > 9)
        /*debug*/       Bug("oversized quotient in quorem");
#endif
    if (q) {
        borrow = 0;
        carry = 0;
        do {
#ifdef ULLong
            ys = *sx++ * (ULLong)q + carry;
            carry = ys >> 32;
            y = *bx - (ys & FFFFFFFF) - borrow;
            borrow = y >> 32 & (ULong)1;
            *bx++ = (ULong)(y & FFFFFFFF);
#else
            si = *sx++;
            ys = (si & 0xffff) * q + carry;
            zs = (si >> 16) * q + (ys >> 16);
            carry = zs >> 16;
            y = (*bx & 0xffff) - (ys & 0xffff) - borrow;
            borrow = (y & 0x10000) >> 16;
            z = (*bx >> 16) - (zs & 0xffff) - borrow;
            borrow = (z & 0x10000) >> 16;
            Storeinc(bx, z, y);
#endif
        }
        while(sx <= sxe);
        if (!*bxe) {
            bx = b->x;
            while(--bxe > bx && !*bxe)
                --n;
            b->wds = n;
        }
    }
    if (cmp(b, S) >= 0) {
        q++;
        borrow = 0;
        carry = 0;
        bx = b->x;
        sx = S->x;
        do {
#ifdef ULLong
            ys = *sx++ + carry;
            carry = ys >> 32;
            y = *bx - (ys & FFFFFFFF) - borrow;
            borrow = y >> 32 & (ULong)1;
            *bx++ = (ULong)(y & FFFFFFFF);
#else
            si = *sx++;
            ys = (si & 0xffff) + carry;
            zs = (si >> 16) + (ys >> 16);
            carry = zs >> 16;
            y = (*bx & 0xffff) - (ys & 0xffff) - borrow;
            borrow = (y & 0x10000) >> 16;
            z = (*bx >> 16) - (zs & 0xffff) - borrow;
            borrow = (z & 0x10000) >> 16;
            Storeinc(bx, z, y);
#endif
        }
        while(sx <= sxe);
        bx = b->x;
        bxe = bx + n;
        if (!*bxe) {
            while(--bxe > bx && !*bxe)
                --n;
            b->wds = n;
        }
    }
    return q;
}

/* Round a finite double x to the nearest integer (in double format), rounding
 * ties away from zero.  This function is unaffected by FPU rounding mode. */

static double
rnd(double x)
{
    U u;
    int exp;
    dval(&u) = x;

    /* Extract unbiased exponent. */
    exp = (int)((word0(&u) & Exp_mask) >> Exp_shift) - Bias;
    if (exp < 0) {
        /* Absolute value of x is at most 1.0; result is +-0.0 if exp < -1,
           and +-1.0 if exp == -1. */
        word0(&u) &= Sign_bit;
        if (exp == -1)
            word0(&u) |= Exp_1;
        word1(&u) = 0;
    }
    else if (exp <= P - 34) {
        /* Absolute value of x is in [1.0, 2**(P-33)).  The bit with value 0.5
           is bit (P - 34 - exp) in the upper word. */
        ULong mask = 1UL << ((P - 34) - exp);
        /* Zero out all bits with value < 0.5. */
        word1(&u) = 0;
        word0(&u) &= -mask;
        /* If bit with value 0.5 is set, round up. */
        if (word0(&u) & mask)
            word0(&u) += mask;
    }
    else if (exp <= P - 2) {
        /* Absolute value of x is in [2**(P-33), 2**(P-1)).  The bit with
           value 0.5 is bit (P - 2 - exp) in the lower word. */
        ULong mask = 1UL << ((P - 2) - exp);
        word1(&u) &= -mask;
        if (word1(&u) & mask) {
            word1(&u) += mask;
            if (word1(&u) < mask)
                /* word1 addition overflowed; add carry into word0. */
                ++word0(&u);
        }
    }
    /* If exp >= P-1 then abs(x) >= 2**(P-1) and x is already
       an integer, so there's nothing to do. */
    return dval(&u);
}

/* In strtod, the current estimate is stored in a pair (x, scale) consisting
   of a double x and an int scale, representing the value x / 2**scale.

   For a finite double x, sulp(x) returns 1 ulp(x), taking scale into account.
   That is, sulp(x, scale) / 2**scale == ulp(x / 2**scale), where ulp(x) is the
   difference between abs(x) and the next largest double.

   Logic in the strtod function ensures that the pair (x, scale) always
   satisfies the following two conditions:

   (0) x / 2**scale is an integral multiple of 2**(Emin - P + 1).  In other
       words, if x / 2**scale is at most DBL_MAX then it's exactly
       representable as a double.

   (1) Either x / 2**(P-1) >= 2**Emin, or scale >= P - 1.

   It follows from these conditions that on input, x is necessarily either
   normal or zero, and that sulp(x, scale) is always positive and normal.
   Moreover, if x is zero then scale >= P - 1.

*/

static double
sulp(U *x, int scale)
{
    U u;
    int e;

    /* Method: if 2**k <= abs(x / 2**scale) < 2**(k+1) then

         ulp(x / 2**scale) == max(2**Etiny, 2**k / 2**(P - 1)).

       We compute

          e = max(scale + 1, k + Bias + scale)

       Here k + Bias + scale corresponds exactly to the value given by the
       exponent bits of x.  From the conditions above, e > P - 1.  Setting u =
       2**(e - Bias - P + 1) gives

          u / 2**scale == 2**(e - Bias - P + 1 - scale)
                       == 2**max(2 - P - Bias, k - P + 1)
                       == max(2**Etiny, 2**k / 2**(P - 1)).
                       == ulp(x / 2**scale).

       The code below also works, unaltered, with x == +-0.0,
       returning u such that u / 2**scale == 2**Etiny.
    */

    e = (int)((word0(x) & Exp_mask) >> Exp_shift);
    if (e < scale + 1)
        e = scale + 1;

    assert(e > P - 1);
    word0(&u) = (ULong)(e - P + 1) << Exp_shift;
    word1(&u) = 0;
    return dval(&u);
}

/* Given a scaled double (as used in _Py_dg_strtod), find the next largest
   boundary, using the same scale.  If we're already on a boundary, return the
   next one up.

   A *boundary* is either 0.0, or a power of 2 that's at least 2**(Emin + 1);
   2**Emin is not considered a boundary, because the spacing of consecutive
   floats does not change at 2**Emin. */

double next_boundary(U *x, int scale) {
    U u;
    int e;

    e = (int)((word0(x) & Exp_mask) >> Exp_shift);

    if (e < scale + 1)
        e = scale + 1;
    word0(&u) = (ULong)(e + 1) << Exp_shift;
    word1(&u) = 0;
    return dval(&u);
}

double last_boundary(U *x, int scale) {
    U u;
    int e;

    e = (int)((word0(x) & Exp_mask) >> Exp_shift);
    if (e <= scale + 1)
        e = 0;
    word0(&u) = (ULong)e << Exp_shift;
    word1(&u) = 0;
    return dval(&u);
}

/* parse_numeric_string: Parse and validate a finite numeric string.

   Inputs:

      A NUL-terminated string s00 with an initial portion that represents a
      finite decimal value.  Valid numeric strings are described by the
      following pseudo-grammar:

         sign = '+' | '-'
         digit = '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'
         point = '.'
         indicator = 'e' | 'E'
         digits = digit [digits]
         significand = digits [point [digits]] | point digits
         exponent = indicator [sign] digits
         numeric-string = [sign] significand [exponent]

      NaNs, infinities and hex literals are not accepted, and leading
      whitespace is not permitted.

   Outputs:

      Any nonzero finite decimal value as above can be uniquely expressed in
      the form

         (-1)**sign * 0.<digits> * 10**exp

      where sign is 0 or 1, exp is an integer, and <digits> is a string of one
      ore more digits with both the first and last digit in the string nonzero.
      The following return values from parse_numeric_string provide access to
      these values:

         *sign gives the sign of the input: 1 for negative, 0 for positive.

         *pnd is the length of <digits>

         *ps0 is a pointer into the input string s00, and *pnd0 is an integer;
         their values are such that for each i in the range 0 <= i < *pnd, the
         ith digit <digits>[i] can be retrieved as:

            (*ps0)[i < *pnd0 ? i : i + 1]

         [ In the current implementation, *ps0 points just past the last
         leading zero in the input string, so will either point to a nonzero
         digit or to the decimal point.  *pnd0 gives the position of the
         decimal point, or of the end of the digit string if no decimal point
         is present, relative to ps0.  Note that *pnd0 may be larger than *pnd,
         or smaller than 0. ]

         *exp is the exponent in the formula above, clipped to lie in the range
         [INT_MIN, INT_MAX].

      For an input representing zero, *pnd will be 0.

      *se points at the first character past the largest initial sequence
      of s00 that gives a valid numeric string.  If the entire string
      s00 is a valid numeric string, *se will point to its terminating NUL
      character.  If no initial portion of s00 gives a valid numeric
      string then *se will be equal to s00.

      Returns 0 for a successful parse, and a nonzero value on failure.
*/

static int
parse_numeric_string(const char *s00, char **se, char **ps0,
                     Py_ssize_t *pnd, Py_ssize_t *pnd0,
                     int *exp, int *sign)
{
    char c;
    const char *s, *s0, *s1;
    int esign, rv;
    unsigned int dig;
    Py_ssize_t e, nd, nd0, nz;
    size_t abs_exp;

    s = s00;
    c = *s;

    /* Parse optional sign, if present. */
    *sign = 0;
    switch (c) {
    case '-':
        *sign = 1;
        /* no break */
    case '+':
        c = *++s;
    }

    /* Skip (and count) leading zeros. */
    s0 = s;
    while (c == '0')
        c = *++s;
    nz = s - s0;

    /* Parse any remaining digits before the point. */
    while ('0' <= c && c <= '9')
        c = *++s;
    nd0 = nd = s - s0;

    /* Parse decimal point and following digits. */
    if (c == '.') {
        c = *++s;
        s1 = s;
        /* If all digits so far are zeros, continue to count leading zeros. */
        if (nd == nz) {
            while (c == '0')
                c = *++s;
            nz += s - s1;
        }
        while ('0' <= c && c <= '9')
            c = *++s;
        nd += s - s1;
    }

    if (!nd) {
        /* No digits in the significand, so we've got an invalid numeric
           string and a parse failure. */
        rv = 1;
        *exp = 0; /* Silence a gcc 'may be used uninitialized' warning. */
        goto exit;
    }

    /* We've got at least one digit, so the string up to this point is a
       valid numeric string and the parse is successful. */
    rv = 0;
    s00 = s;

    /* Adjust s0, nd, nd0 for leading zeros. */
    s0 += nz;
    nd -= nz;
    nd0 -= nz;

    /* The ith digit of the significand can now be retrieved as s0[i < nd0 ? i
       : i + 1].  Discard any trailing zeros. */
    while (nd > 0 && s0[nd - 1 < nd0 ? nd - 1 : nd] == '0')
        nd--;

    /* Parse exponent. */
    esign = 0;
    abs_exp = 0U;
    if (c == 'e' || c == 'E') {
        c = *++s;

        /* Exponent sign. */
        esign = 0;
        switch (c) {
        case '-':
            esign = 1;
            /* no break */
        case '+':
            c = *++s;
        }

       /* Get absolute value abs_exp for the exponent; cap at SIZE_T_MAX.  */
        s1 = s;
        while ('0' <= c && c <= '9') {
            dig = (unsigned int)(c - '0');
            /* Safe version of: if (10U * abs_exp + dig > SIZE_T_MAX) {...} */
            if (abs_exp >= SIZE_T_MAX / 10U && (abs_exp > SIZE_T_MAX / 10U ||
                                                dig > SIZE_T_MAX % 10U)) {
                abs_exp = SIZE_T_MAX;
                break;
            }
            abs_exp = 10U * abs_exp + dig;
            c = *++s;
        }
        /* A valid exponent must have at least one digit. */
        if (s != s1)
            s00 = s;
    }

    /* Find e = nd0 + (-1)**esign * abs_exp, avoiding underflow/overflow and
       clipping the result to the range [PY_SSIZE_T_MIN, PY_SSIZE_T_MAX].
       Note that if the 'true' absolute exponent (i.e., without the SIZE_T_MAX
       cap) is greater than SIZE_T_MAX then either e >= PY_SSIZE_T_MAX or e <=
       PY_SSIZE_T_MIN, so we still end up doing the right thing here. */
    if (esign) {
        if (nd0 - (size_t)PY_SSIZE_T_MIN < abs_exp)
            e = PY_SSIZE_T_MIN;
        else
            e = nd0 - abs_exp;
    }
    else {
        if ((size_t)PY_SSIZE_T_MAX - nd0 < abs_exp)
            e = PY_SSIZE_T_MAX;
        else
            e = nd0 + abs_exp;
    }

    /* Clip further to [INT_MIN, INT_MAX]. */
    if (e < INT_MIN)
        *exp = INT_MIN;
    else if (e > INT_MAX)
        *exp = INT_MAX;
    else
        *exp = e;

  exit:
    if (se)
        *se = (char *)s00;
    *pnd = nd;
    *pnd0 = nd0;
    *ps0 = (char *)s0;
    return rv;
}

/* The bigcomp function handles some hard cases for strtod, for inputs
   with more than STRTOD_DIGLIM digits.  It's called once an initial
   estimate for the double corresponding to the input string has
   already been obtained by the code in _Py_dg_strtod.

   The bigcomp function is only called after _Py_dg_strtod has found a
   double value rv such that either rv or rv + 1ulp represents the
   correctly rounded value corresponding to the original string.  It
   determines which of these two values is the correct one by
   computing the decimal digits of rv + 0.5ulp and comparing them with
   the corresponding digits of s0.

   In the following, write dv for the absolute value of the number represented
   by the input string.

   Inputs:

     s0 points to the first significant digit of the input string.

     rv is a (possibly scaled) estimate for the closest double value to the
        value represented by the original input to _Py_dg_strtod.  If
        bc->scale is nonzero, then rv/2^(bc->scale) is the approximation to
        the input value.

     bc is a struct containing information gathered during the parsing and
        estimation steps of _Py_dg_strtod.  Description of fields follows:

        bc->e0, bc->nd and bc->nd0 correspond to the values returned by
           parse_numeric_string.

        bc->scale is the value used to scale rv to avoid doing arithmetic with
           subnormal values.  It's either 0 or 2*P (=106).

   Outputs:

     On successful exit, rv/2^(bc->scale) is the closest double to dv.

     Returns 0 on success, -1 on failure (e.g., due to a failed malloc call).
*/

static int
bigcomp(U *rv, const char *s0, BCinfo *bc)
{
    Bigint *b, *d;
    int b2, d2, dd, i, odd, p2, p5;
    Py_ssize_t nd, nd0;

    nd = bc->nd;
    nd0 = bc->nd0;
    p5 = bc->e0;
    b = sd2b(rv, bc->scale, &p2);
    if (b == NULL)
        return -1;

    /* record whether the lsb of rv/2^(bc->scale) is odd:  in the exact halfway
       case, this is used for round to even. */
    odd = b->x[0] & 1;

    /* left shift b by 1 bit and or a 1 into the least significant bit;
       this gives us b * 2**p2 = rv/2^(bc->scale) + 0.5 ulp. */
    b = lshift(b, 1);
    if (b == NULL)
        return -1;
    b->x[0] |= 1;
    p2--;

    p2 -= p5;
    d = i2b(1);
    if (d == NULL) {
        Bfree(b);
        return -1;
    }
    /* Arrange for convenient computation of quotients:
     * shift left if necessary so divisor has 4 leading 0 bits.
     */
    if (p5 > 0) {
        d = pow5mult(d, p5);
        if (d == NULL) {
            Bfree(b);
            return -1;
        }
    }
    else if (p5 < 0) {
        b = pow5mult(b, -p5);
        if (b == NULL) {
            Bfree(d);
            return -1;
        }
    }
    if (p2 > 0) {
        b2 = p2;
        d2 = 0;
    }
    else {
        b2 = 0;
        d2 = -p2;
    }
    i = dshift(d, d2);
    if ((b2 += i) > 0) {
        b = lshift(b, b2);
        if (b == NULL) {
            Bfree(d);
            return -1;
        }
    }
    if ((d2 += i) > 0) {
        d = lshift(d, d2);
        if (d == NULL) {
            Bfree(b);
            return -1;
        }
    }

    /* Compare s0 with b/d: set dd to -1, 0, or 1 according as s0 < b/d, s0 ==
     * b/d, or s0 > b/d.  Here the digits of s0 are thought of as representing
     * a number in the range [0.1, 1). */
    if (cmp(b, d) >= 0)
        /* b/d >= 1 */
        dd = -1;
    else {
        Py_ssize_t i = 0;
        for(;;) {
            b = multadd(b, 10, 0);
            if (b == NULL) {
                Bfree(d);
                return -1;
            }
            dd = s0[i < nd0 ? i : i+1] - '0' - quorem(b, d);
            i++;

            if (dd)
                break;
            if (!b->x[0] && b->wds == 1) {
                /* b/d == 0 */
                dd = i < nd;
                break;
            }
            if (!(i < nd)) {
                /* b/d != 0, but digits of s0 exhausted */
                dd = -1;
                break;
            }
        }
    }
    Bfree(b);
    Bfree(d);
    if (dd > 0 || (dd == 0 && odd))
        dval(rv) += sulp(rv, bc->scale);
    return 0;
}

/* Get current FPU rounding mode; same values as for FLT_ROUNDS
        -1 for indeterminate
         0 for rounding towards zero
         1 for rounding towards nearest (with halves rounded towards even)
         2 for rounding upward (towards positive infinity)
         3 for rounding downward (towards negative infinity)
*/

static int
get_rounding_mode(void)
{
    /* this may not work on all systems;  we should try a configure-time
       test. */
    return FLT_ROUNDS;
}

double
_Py_dg_strtod(const char *s00, char **se)
{
    int bbe, dsign, e, e1, e2, e5, error, i, k, scale, sign;
    Py_ssize_t nd, nd0;
    char *s0;
    double aadj, aadj_int, scalefac, ulp;
    U rv;
    Bigint *bb, *bd, *bs, *delta;
    BCinfo bc;

    error = parse_numeric_string(s00, se, &s0, &nd, &nd0, &e, &sign);
    if (error)
        goto parse_error;

    /* If all digits were zero, exit with return value +-0.0. */
    if (!nd) {
        dval(&rv) = 0.0;
        goto ret;
    }

    /* Catch cases of obvious overflow and underflow. */
    if (e > Big_10_exp)
        goto ovfl;
    else if (e <= Tiny_10_exp)
        goto undfl;

    /* Compute an initial approximation of the form rv * 10 ** e1, where rv is
     * the integer obtained from the first (up to) DBL_DIG digits of the
     * input. */
    k = nd <= DBL_DIG ? nd : DBL_DIG;
    dval(&rv) = 0.0;
    for (i = 0; i < k; i++)
        dval(&rv) = 10.0 * dval(&rv) + (s0[i < nd0 ? i : i + 1] - '0');
    e1 = e - k;

    /* If the input has at most DBL_DIG significant digits (that is, nd <=
     * DBL_DIG), then the represented value is exactly +/- rv * 10 ** e1.  In
     * this case, if e1 is small enough *and* the FPU rounding mode matches the
     * one we want, then we can compute a correctly rounded result with a
     * single floating-point multiplication or division. */
    if (get_rounding_mode() == 1) {
        if (nd <= DBL_DIG) {
            if (e1 >= 0) {
                if (e1 <= Ten_pmax) {
                    dval(&rv) *= tens[e1];
                    goto ret;
                }
                i = DBL_DIG - nd;
                if (e1 - i <= Ten_pmax) {
                    dval(&rv) *= tens[i];
                    dval(&rv) *= tens[e1 - i];
                    goto ret;
                }
            }
            else if (-e1 <= Ten_pmax) {
                dval(&rv) /= tens[-e1];
                goto ret;
            }
        }
    }

    /* The fast path above wasn't applicable, so at this point we have to do
     * things the hard way.  Here's an outline of the strategy.
     *
     *    (1) Compute a double approximation 'rv' to the true value of the
     *    input.  There are various sources of error in this approximation:
     *    it's based on the first (at most) DBL_DIG significant digits of the
     *    input, as computed above, so there's a truncation error involved; the
     *    floating-point arithmetic used to compute the approximation will also
     *    contribute error.  1000 ulps is a safe upper bound for the magnitude
     *    of the error, though it will be much smaller in most cases.
     *
     *    (2) Use Bigint arithmetic to compute an approximation 'aadj' to ulps
     *    difference between rv and the true value.  Again there are sources of
     *    error here: instead of computing with the true value, we compute with
     *    a value truncated to at most STRTOD_DIGLIM digits; this introduces an
     *    absolute error of at most 1e-8 to aadj.  Rounding error may
     *    contribute a further absolute error of 1e-10.
     *
     *    (3) Adjust rv based on the ulps difference computed above.  In most
     *    cases, the result will be unambiguous and we can return after this
     *    step.  But:
     *
     *    (4) If the ulps difference value computed in (2) indicates that the
     *    true value lies very nearly halfway between two representable
     *    doubles, then because of the uncertainty in aadj itself we need to
     *    more work.  In this case the decision is passed off to the 'bigcomp'
     *    function, which does a comparison with the original (not truncated)
     *    input value in order to figure out which way to round.
     *
     * If the value represented by the input is close to the limits of the
     * range of the double type (i.e., either very large or very small), then
     * it's convenient to using a scaling factor 'scalefac == 2**scale' to
     * ensure that rv is normal and not too close to the overflow boundary;
     * thus our approximation will actually be rv / 2**scale (or rv / scalefac)
     * rather than rv itself.
     */

    /* Get starting approximation, rv * 10**e1.  For small inputs, rv is
     * scaled by a factor of 2**(2*P); that is, we compute the value rv *
     * 10**e1 * 2**(2*P).  For large inputs, we scale in the opposite
     * direction, computing rv * 10**e1 / 2**(2*P). */
    scale = 0;
    scalefac = 1.0;
    if (e1 > 0) {
        dval(&rv) *= tens[e1 & 15];
        e1 >>= 4;
        if (e1 & Scale_Bit) {
            scale = -2*P;
            scalefac = 1.0 / dval(&Exp4P);
            dval(&rv) *= scalefac;
        }
        for(i = 0; e1 > 0; i++, e1 >>= 1)
            if (e1 & 1)
                dval(&rv) *= bigtens[i];
    }
    else if (e1 < 0) {
        e1 = -e1;
        dval(&rv) /= tens[e1 & 15];
        e1 >>= 4;
        if (e1 & Scale_Bit) {
            scale = 2*P;
            scalefac = dval(&Exp4P);
            dval(&rv) *= scalefac;
        }
        for(i = 0; e1 > 0; i++, e1 >>= 1)
            if (e1 & 1)
                dval(&rv) *= tinytens[i];
    }

    /* Some inequalities:

          -324 < e <= 309  (so 10**-324 <= dv < 10**309).
          1 <= k <= DBL_DIG (= 15).
          e = e1 + k (using the value of e1 before entering the above loops)
          scale > 0 iff e1 <= -256, and scale < 0 iff e1 >= 256

       From these we deduce:

          If scale > 0  then 1e-324 <= dv < 1e-241.
          If scale == 0 then 1e-255 <= dv < 1e+270.
          If scale < 0  then 1e+256 <= dv < 1e+309.

    */

    /* Ensure that rv / 2**scale is exactly representable.  This is an issue
     * only if scale > 0 and rv / 2**scale is subnormal: rv itself will be
     * normal in this case, so will have a full 53-bit significand, but rv /
     * 2**scale won't have that many bits available.
     *
     * Evil trickery ahead: We want to round rv to an exact multiple of
     * 2**scale * DBL_MIN * 2**(1 - P).  Rather than doing this by hand, we
     * persuade the FPU to do the rounding for us: assuming that rv < 2**scale
     * DBL_MIN, we add 2**scale * DBL_MIN to rv.  The result is in the binade
     * [2**scale * DBL_MIN, 2 * 2**scale * DBL_MIN], and the representable
     * floating-point numbers in this binade are exactly the multiples of
     * 2**scale * DBL_MIN * 2**(1 - P), hence the result of the addition is the
     * exact sum rounded to the nearest multiple of 2**scale * DBL_MIN *
     * 2**(1-P).  Subtracting 2**scale * DBL_MIN again gives the rounded rv
     * value.
     *
     * Note that at this point rv is still just an approximation, so we don't
     * care about discrepancies due to rounding mode.
     */
    if (scale > 0 && dval(&rv) < dval(&Dbl_min) * scalefac) {
        dval(&rv) += dval(&Dbl_min) * scalefac;
        dval(&rv) -= dval(&Dbl_min) * scalefac;
    }

    /* We now have our starting approximation rv / 2**scale.  Note that it's
     * not possible for rv to be *strictly* negative at this point, though it
     * might be -0.0; this is dealt with towards the end of the function.
     */
    assert(dval(&rv) >= 0.0);

    /* Step (2): Compute 'aadj', an (approximation to) ulps difference between
     * the true value (after truncation to STRTOD_DIGLIM significant digits)
     * and the current approximation rv / 2**scale.
     *
     * From this point onwards, all floating-point operations on rv that are
     * performed have an exactly representable result.  This ensures that the
     * FPU rounding mode can't affect the result.
     */

    /* Notation used in the comments: 'srv' is rv / 2**scale; 'tdv' is the
       value represented by the input (after truncation); 'ulp' is ulp(srv).
       We're aiming to compute (srv - tdv) / ulp.  We do this by using integer
       arithmetic to compute (M*srv - M*tdv) / (M*ulp) for a suitable scale
       factor M for which M*srv, M*tdv and M*ulp are all integral. */

    k = nd < STRTOD_DIGLIM ? nd : STRTOD_DIGLIM;
    bd = s2b(s0, nd0, k);          /* tdv == bd * 10**(e - k). */
    bb = sd2b(&rv, scale, &bbe);   /* srv == bb * 2**bbe. */
    bs = i2b(1);                   /* ulp == 2**bbe. */
    if (bd == NULL || bb == NULL || bs == NULL)
        goto failed_malloc;

    /* srv, tdv and ulp are proportional to bb, bd * 2**(e-k-bbe)*5**(e-k) and
       bs, respectively.  Scale bb, bd, bs by the appropriate powers of 5... */
    e5 = e - k;
    if (e5 > 0)
        bd = pow5mult(bd, e5);
    else if (e5 < 0) {
        bb = pow5mult(bb, -e5);
        bs = pow5mult(bs, -e5);
    }
    if (bd == NULL || bb == NULL || bs == NULL)
        goto failed_malloc;

    /* ... and powers of 2. */
    e2 = e - k - bbe;
    if (e2 > 0)
        bd = lshift(bd, e2);
    else if (e2 < 0) {
        bb = lshift(bb, -e2);
        bs = lshift(bs, -e2);
    }
    if (bd == NULL || bb == NULL || bs == NULL)
        goto failed_malloc;

    /* Now srv, tdv and ulp are proportional to bb, bd, and bs, respectively.
     * Compute the quotient aadj = (srv - tdv) / ulp = (bb - bd) / bs, as a
     * double. */
    delta = diff(bb, bd);
    dsign = delta->sign;
    if (delta == NULL)
        goto failed_malloc;
    aadj = ratio(delta, bs);
    Bfree(delta);
    if (dsign)
        aadj = -aadj;
    Bfree(bd);
    Bfree(bb);
    Bfree(bs);

    /* Step (3): Adjust rv using the computed value of aadj.  In most cases the
       correct result is unambiguous; in hard cases we move on to step (4). */

    ulp = sulp(&rv, scale);
    /* Invariant: here and after the adjustment, srv ~= tdv + ulp * aadj. */
    if (aadj <= 0.0) {
        /* next = ulps from next boundary up to rv (will be negative) */
        double next = (dval(&rv) - next_boundary(&rv, scale))/ulp;
        if (aadj <= next) {
            /* Adjustment takes us past a power of 2 boundary, going up. */
            aadj *= 0.5;
            next *= 0.5;
            ulp *= 2.0;
            /* Round to integer if next is even, else to half-odd-integer. */
            aadj_int = next + rnd(aadj - next);
        }
        else {
            /* Usual case. */
            aadj_int = rnd(aadj);
        }
    }
    else {
        double last = (dval(&rv) - last_boundary(&rv, scale))/ulp;
        if (aadj > last && last_boundary(&rv, scale) != 0.0) {
            /* Adjustment takes us past a power of 2 boundary, going down. */
            aadj *= 2.0;
            ulp *= 0.5;
        }
        aadj_int = rnd(aadj);
    }

    /* Adjust, and compute residual error. */
    dval(&rv) -= aadj_int * ulp;
    aadj -= aadj_int;

    /* If we're in a near halfway case, use bigcomp to figure out
       the correctly-rounded value */
    assert(aadj > -0.5000001 && aadj < 0.5000001);
    if (aadj < -0.4999999 || aadj > 0.4999999) {
        /* input to bigcomp should be the lower of the two possible results */
        if (aadj > 0.0) {
            dval(&rv) -= ulp;
            /* aadj += 1.0; */  /* Necessary only to maintain invariant; not
                                 * used beyond this point. */
        }
        bc.nd0 = nd0;
        bc.nd = nd;
        bc.e0 = e;
        bc.scale = scale;
        error = bigcomp(&rv, s0, &bc);
        if (error)
            goto failed_malloc;
    }

    /* Under some rounding modes (particularly FE_DOWNWARD), it's possible for
     * dval(&rv) to be -0.0 at this point; we want to get rid of the negative
     * sign. */
    if (word0(&rv) & Sign_bit)
        word0(&rv) &= ~Sign_bit;

    /* Do underflow and overflow checks, and undo the effects of scaling (if
     * applicable). */
    if (word0(&rv) == 0 && word1(&rv) == 0)
        goto undfl;
    if ((int)((word0(&rv) & Exp_mask) >> Exp_shift) - scale > Emax + Bias)
        goto ovfl;

    /* Undo the effects of the scaling, if applicable. */
    if (scale)
        dval(&rv) /= scalefac;

  ret:
    return sign ? -dval(&rv) : dval(&rv);

  parse_error:
    return 0.0;

  failed_malloc:
    if (bd != NULL) Bfree(bd);
    if (bb != NULL) Bfree(bb);
    if (bs != NULL) Bfree(bs);
    errno = ENOMEM;
    return -1.0;

  undfl:
    return sign ? -0.0 : 0.0;

  ovfl:
    errno = ERANGE;
    dval(&rv) = dval(&Inf);
    return sign ? -dval(&rv) : dval(&rv);

}

static char *
rv_alloc(int i)
{
    int j, k, *r;

    j = sizeof(ULong);
    for(k = 0;
        sizeof(Bigint) - sizeof(ULong) - sizeof(int) + j <= (unsigned)i;
        j <<= 1)
        k++;
    r = (int*)Balloc(k);
    if (r == NULL)
        return NULL;
    *r = k;
    return (char *)(r+1);
}

static char *
nrv_alloc(char *s, char **rve, int n)
{
    char *rv, *t;

    rv = rv_alloc(n);
    if (rv == NULL)
        return NULL;
    t = rv;
    while((*t = *s++)) t++;
    if (rve)
        *rve = t;
    return rv;
}

/* freedtoa(s) must be used to free values s returned by dtoa
 * when MULTIPLE_THREADS is #defined.  It should be used in all cases,
 * but for consistency with earlier versions of dtoa, it is optional
 * when MULTIPLE_THREADS is not defined.
 */

void
_Py_dg_freedtoa(char *s)
{
    Bigint *b = (Bigint *)((int *)s - 1);
    b->maxwds = 1 << (b->k = *(int*)b);
    Bfree(b);
}

/* dtoa for IEEE arithmetic (dmg): convert double to ASCII string.
 *
 * Inspired by "How to Print Floating-Point Numbers Accurately" by
 * Guy L. Steele, Jr. and Jon L. White [Proc. ACM SIGPLAN '90, pp. 112-126].
 *
 * Modifications:
 *      1. Rather than iterating, we use a simple numeric overestimate
 *         to determine k = floor(log10(d)).  We scale relevant
 *         quantities using O(log2(k)) rather than O(k) multiplications.
 *      2. For some modes > 2 (corresponding to ecvt and fcvt), we don't
 *         try to generate digits strictly left to right.  Instead, we
 *         compute with fewer bits and propagate the carry if necessary
 *         when rounding the final digit up.  This is often faster.
 *      3. Under the assumption that input will be rounded nearest,
 *         mode 0 renders 1e23 as 1e23 rather than 9.999999999999999e22.
 *         That is, we allow equality in stopping tests when the
 *         round-nearest rule will give the same floating-point value
 *         as would satisfaction of the stopping test with strict
 *         inequality.
 *      4. We remove common factors of powers of 2 from relevant
 *         quantities.
 *      5. When converting floating-point integers less than 1e16,
 *         we use floating-point arithmetic rather than resorting
 *         to multiple-precision integers.
 *      6. When asked to produce fewer than 15 digits, we first try
 *         to get by with floating-point arithmetic; we resort to
 *         multiple-precision integer arithmetic only if we cannot
 *         guarantee that the floating-point calculation has given
 *         the correctly rounded result.  For k requested digits and
 *         "uniformly" distributed input, the probability is
 *         something like 10^(k-15) that we must resort to the Long
 *         calculation.
 */

/* Additional notes (METD): (1) returns NULL on failure.  (2) to avoid memory
   leakage, a successful call to _Py_dg_dtoa should always be matched by a
   call to _Py_dg_freedtoa. */

char *
_Py_dg_dtoa(double dd, int mode, int ndigits,
            int *decpt, int *sign, char **rve)
{
    /*  Arguments ndigits, decpt, sign are similar to those
        of ecvt and fcvt; trailing zeros are suppressed from
        the returned string.  If not null, *rve is set to point
        to the end of the return value.  If d is +-Infinity or NaN,
        then *decpt is set to 9999.

        mode:
        0 ==> shortest string that yields d when read in
        and rounded to nearest.
        1 ==> like 0, but with Steele & White stopping rule;
        e.g. with IEEE P754 arithmetic , mode 0 gives
        1e23 whereas mode 1 gives 9.999999999999999e22.
        2 ==> max(1,ndigits) significant digits.  This gives a
        return value similar to that of ecvt, except
        that trailing zeros are suppressed.
        3 ==> through ndigits past the decimal point.  This
        gives a return value similar to that from fcvt,
        except that trailing zeros are suppressed, and
        ndigits can be negative.
        4,5 ==> similar to 2 and 3, respectively, but (in
        round-nearest mode) with the tests of mode 0 to
        possibly return a shorter string that rounds to d.
        With IEEE arithmetic and compilation with
        -DHonor_FLT_ROUNDS, modes 4 and 5 behave the same
        as modes 2 and 3 when FLT_ROUNDS != 1.
        6-9 ==> Debugging modes similar to mode - 4:  don't try
        fast floating-point estimate (if applicable).

        Values of mode other than 0-9 are treated as mode 0.

        Sufficient space is allocated to the return value
        to hold the suppressed trailing zeros.
    */

    int bbits, b2, b5, be, dig, i, ieps, ilim, ilim0, ilim1,
        j, j1, k, k0, k_check, leftright, m2, m5, s2, s5,
        spec_case, try_quick;
    Long L;
    int denorm;
    ULong x;
    Bigint *b, *b1, *delta, *mlo, *mhi, *S;
    U d2, eps, u;
    double ds;
    char *s, *s0;

    /* set pointers to NULL, to silence gcc compiler warnings and make
       cleanup easier on error */
    mlo = mhi = S = 0;
    s0 = 0;

    u.d = dd;
    if (word0(&u) & Sign_bit) {
        /* set sign for everything, including 0's and NaNs */
        *sign = 1;
        word0(&u) &= ~Sign_bit; /* clear sign bit */
    }
    else
        *sign = 0;

    /* quick return for Infinities, NaNs and zeros */
    if ((word0(&u) & Exp_mask) == Exp_mask)
    {
        /* Infinity or NaN */
        *decpt = 9999;
        if (!word1(&u) && !(word0(&u) & 0xfffff))
            return nrv_alloc("Infinity", rve, 8);
        return nrv_alloc("NaN", rve, 3);
    }
    if (!dval(&u)) {
        *decpt = 1;
        return nrv_alloc("0", rve, 1);
    }

    /* compute k = floor(log10(d)).  The computation may leave k
       one too large, but should never leave k too small. */
    b = d2b(&u, &be, &bbits);
    if (b == NULL)
        goto failed_malloc;
    if ((i = (int)(word0(&u) >> Exp_shift1 & (Exp_mask>>Exp_shift1)))) {
        dval(&d2) = dval(&u);
        word0(&d2) &= Frac_mask1;
        word0(&d2) |= Exp_11;

        /* log(x)       ~=~ log(1.5) + (x-1.5)/1.5
         * log10(x)      =  log(x) / log(10)
         *              ~=~ log(1.5)/log(10) + (x-1.5)/(1.5*log(10))
         * log10(d) = (i-Bias)*log(2)/log(10) + log10(d2)
         *
         * This suggests computing an approximation k to log10(d) by
         *
         * k = (i - Bias)*0.301029995663981
         *      + ( (d2-1.5)*0.289529654602168 + 0.176091259055681 );
         *
         * We want k to be too large rather than too small.
         * The error in the first-order Taylor series approximation
         * is in our favor, so we just round up the constant enough
         * to compensate for any error in the multiplication of
         * (i - Bias) by 0.301029995663981; since |i - Bias| <= 1077,
         * and 1077 * 0.30103 * 2^-52 ~=~ 7.2e-14,
         * adding 1e-13 to the constant term more than suffices.
         * Hence we adjust the constant term to 0.1760912590558.
         * (We could get a more accurate k by invoking log10,
         *  but this is probably not worthwhile.)
         */

        i -= Bias;
        denorm = 0;
    }
    else {
        /* d is denormalized */

        i = bbits + be + (Bias + (P-1) - 1);
        x = i > 32  ? word0(&u) << (64 - i) | word1(&u) >> (i - 32)
            : word1(&u) << (32 - i);
        dval(&d2) = x;
        word0(&d2) -= 31*Exp_msk1; /* adjust exponent */
        i -= (Bias + (P-1) - 1) + 1;
        denorm = 1;
    }
    ds = (dval(&d2)-1.5)*0.289529654602168 + 0.1760912590558 +
        i*0.301029995663981;
    k = (int)ds;
    if (ds < 0. && ds != k)
        k--;    /* want k = floor(ds) */
    k_check = 1;
    if (k >= 0 && k <= Ten_pmax) {
        if (dval(&u) < tens[k])
            k--;
        k_check = 0;
    }
    j = bbits - i - 1;
    if (j >= 0) {
        b2 = 0;
        s2 = j;
    }
    else {
        b2 = -j;
        s2 = 0;
    }
    if (k >= 0) {
        b5 = 0;
        s5 = k;
        s2 += k;
    }
    else {
        b2 -= k;
        b5 = -k;
        s5 = 0;
    }
    if (mode < 0 || mode > 9)
        mode = 0;

    try_quick = 1;

    if (mode > 5) {
        mode -= 4;
        try_quick = 0;
    }
    leftright = 1;
    ilim = ilim1 = -1;  /* Values for cases 0 and 1; done here to */
    /* silence erroneous "gcc -Wall" warning. */
    switch(mode) {
    case 0:
    case 1:
        i = 18;
        ndigits = 0;
        break;
    case 2:
        leftright = 0;
        /* no break */
    case 4:
        if (ndigits <= 0)
            ndigits = 1;
        ilim = ilim1 = i = ndigits;
        break;
    case 3:
        leftright = 0;
        /* no break */
    case 5:
        i = ndigits + k + 1;
        ilim = i;
        ilim1 = i - 1;
        if (i <= 0)
            i = 1;
    }
    s0 = rv_alloc(i);
    if (s0 == NULL)
        goto failed_malloc;
    s = s0;


    if (ilim >= 0 && ilim <= Quick_max && try_quick) {

        /* Try to get by with floating-point arithmetic. */

        i = 0;
        dval(&d2) = dval(&u);
        k0 = k;
        ilim0 = ilim;
        ieps = 2; /* conservative */
        if (k > 0) {
            ds = tens[k&0xf];
            j = k >> 4;
            if (j & Bletch) {
                /* prevent overflows */
                j &= Bletch - 1;
                dval(&u) /= bigtens[n_bigtens-1];
                ieps++;
            }
            for(; j; j >>= 1, i++)
                if (j & 1) {
                    ieps++;
                    ds *= bigtens[i];
                }
            dval(&u) /= ds;
        }
        else if ((j1 = -k)) {
            dval(&u) *= tens[j1 & 0xf];
            for(j = j1 >> 4; j; j >>= 1, i++)
                if (j & 1) {
                    ieps++;
                    dval(&u) *= bigtens[i];
                }
        }
        if (k_check && dval(&u) < 1. && ilim > 0) {
            if (ilim1 <= 0)
                goto fast_failed;
            ilim = ilim1;
            k--;
            dval(&u) *= 10.;
            ieps++;
        }
        dval(&eps) = ieps*dval(&u) + 7.;
        word0(&eps) -= (P-1)*Exp_msk1;
        if (ilim == 0) {
            S = mhi = 0;
            dval(&u) -= 5.;
            if (dval(&u) > dval(&eps))
                goto one_digit;
            if (dval(&u) < -dval(&eps))
                goto no_digits;
            goto fast_failed;
        }
        if (leftright) {
            /* Use Steele & White method of only
             * generating digits needed.
             */
            dval(&eps) = 0.5/tens[ilim-1] - dval(&eps);
            for(i = 0;;) {
                L = (Long)dval(&u);
                dval(&u) -= L;
                *s++ = '0' + (int)L;
                if (dval(&u) < dval(&eps))
                    goto ret1;
                if (1. - dval(&u) < dval(&eps))
                    goto bump_up;
                if (++i >= ilim)
                    break;
                dval(&eps) *= 10.;
                dval(&u) *= 10.;
            }
        }
        else {
            /* Generate ilim digits, then fix them up. */
            dval(&eps) *= tens[ilim-1];
            for(i = 1;; i++, dval(&u) *= 10.) {
                L = (Long)(dval(&u));
                if (!(dval(&u) -= L))
                    ilim = i;
                *s++ = '0' + (int)L;
                if (i == ilim) {
                    if (dval(&u) > 0.5 + dval(&eps))
                        goto bump_up;
                    else if (dval(&u) < 0.5 - dval(&eps)) {
                        while(*--s == '0');
                        s++;
                        goto ret1;
                    }
                    break;
                }
            }
        }
      fast_failed:
        s = s0;
        dval(&u) = dval(&d2);
        k = k0;
        ilim = ilim0;
    }

    /* Do we have a "small" integer? */

    if (be >= 0 && k <= Int_max) {
        /* Yes. */
        ds = tens[k];
        if (ndigits < 0 && ilim <= 0) {
            S = mhi = 0;
            if (ilim < 0 || dval(&u) <= 5*ds)
                goto no_digits;
            goto one_digit;
        }
        for(i = 1;; i++, dval(&u) *= 10.) {
            L = (Long)(dval(&u) / ds);
            dval(&u) -= L*ds;
            *s++ = '0' + (int)L;
            if (!dval(&u)) {
                break;
            }
            if (i == ilim) {
                dval(&u) += dval(&u);
                if (dval(&u) > ds || (dval(&u) == ds && L & 1)) {
                  bump_up:
                    while(*--s == '9')
                        if (s == s0) {
                            k++;
                            *s = '0';
                            break;
                        }
                    ++*s++;
                }
                break;
            }
        }
        goto ret1;
    }

    m2 = b2;
    m5 = b5;
    if (leftright) {
        i =
            denorm ? be + (Bias + (P-1) - 1 + 1) :
            1 + P - bbits;
        b2 += i;
        s2 += i;
        mhi = i2b(1);
        if (mhi == NULL)
            goto failed_malloc;
    }
    if (m2 > 0 && s2 > 0) {
        i = m2 < s2 ? m2 : s2;
        b2 -= i;
        m2 -= i;
        s2 -= i;
    }
    if (b5 > 0) {
        if (leftright) {
            if (m5 > 0) {
                mhi = pow5mult(mhi, m5);
                if (mhi == NULL)
                    goto failed_malloc;
                b1 = mult(mhi, b);
                Bfree(b);
                b = b1;
                if (b == NULL)
                    goto failed_malloc;
            }
            if ((j = b5 - m5)) {
                b = pow5mult(b, j);
                if (b == NULL)
                    goto failed_malloc;
            }
        }
        else {
            b = pow5mult(b, b5);
            if (b == NULL)
                goto failed_malloc;
        }
    }
    S = i2b(1);
    if (S == NULL)
        goto failed_malloc;
    if (s5 > 0) {
        S = pow5mult(S, s5);
        if (S == NULL)
            goto failed_malloc;
    }

    /* Check for special case that d is a normalized power of 2. */

    spec_case = 0;
    if ((mode < 2 || leftright)
        ) {
        if (!word1(&u) && !(word0(&u) & Bndry_mask)
            && word0(&u) & (Exp_mask & ~Exp_msk1)
            ) {
            /* The special case */
            b2 += Log2P;
            s2 += Log2P;
            spec_case = 1;
        }
    }

    /* Arrange for convenient computation of quotients:
     * shift left if necessary so divisor has 4 leading 0 bits.
     *
     * Perhaps we should just compute leading 28 bits of S once
     * and for all and pass them and a shift to quorem, so it
     * can do shifts and ors to compute the numerator for q.
     */
#define iInc 28
    i = dshift(S, s2);
    b2 += i;
    m2 += i;
    s2 += i;
    if (b2 > 0) {
        b = lshift(b, b2);
        if (b == NULL)
            goto failed_malloc;
    }
    if (s2 > 0) {
        S = lshift(S, s2);
        if (S == NULL)
            goto failed_malloc;
    }
    if (k_check) {
        if (cmp(b,S) < 0) {
            k--;
            b = multadd(b, 10, 0);      /* we botched the k estimate */
            if (b == NULL)
                goto failed_malloc;
            if (leftright) {
                mhi = multadd(mhi, 10, 0);
                if (mhi == NULL)
                    goto failed_malloc;
            }
            ilim = ilim1;
        }
    }
    if (ilim <= 0 && (mode == 3 || mode == 5)) {
        if (ilim < 0) {
            /* no digits, fcvt style */
          no_digits:
            k = -1 - ndigits;
            goto ret;
        }
        else {
            S = multadd(S, 5, 0);
            if (S == NULL)
                goto failed_malloc;
            if (cmp(b, S) <= 0)
                goto no_digits;
        }
      one_digit:
        *s++ = '1';
        k++;
        goto ret;
    }
    if (leftright) {
        if (m2 > 0) {
            mhi = lshift(mhi, m2);
            if (mhi == NULL)
                goto failed_malloc;
        }

        /* Compute mlo -- check for special case
         * that d is a normalized power of 2.
         */

        mlo = mhi;
        if (spec_case) {
            mhi = Balloc(mhi->k);
            if (mhi == NULL)
                goto failed_malloc;
            Bcopy(mhi, mlo);
            mhi = lshift(mhi, Log2P);
            if (mhi == NULL)
                goto failed_malloc;
        }

        for(i = 1;;i++) {
            dig = quorem(b,S) + '0';
            /* Do we yet have the shortest decimal string
             * that will round to d?
             */
            j = cmp(b, mlo);
            delta = diff(S, mhi);
            if (delta == NULL)
                goto failed_malloc;
            j1 = delta->sign ? 1 : cmp(b, delta);
            Bfree(delta);
            if (j1 == 0 && mode != 1 && !(word1(&u) & 1)
                ) {
                if (dig == '9')
                    goto round_9_up;
                if (j > 0)
                    dig++;
                *s++ = dig;
                goto ret;
            }
            if (j < 0 || (j == 0 && mode != 1
                          && !(word1(&u) & 1)
                    )) {
                if (!b->x[0] && b->wds <= 1) {
                    goto accept_dig;
                }
                if (j1 > 0) {
                    b = lshift(b, 1);
                    if (b == NULL)
                        goto failed_malloc;
                    j1 = cmp(b, S);
                    if ((j1 > 0 || (j1 == 0 && dig & 1))
                        && dig++ == '9')
                        goto round_9_up;
                }
              accept_dig:
                *s++ = dig;
                goto ret;
            }
            if (j1 > 0) {
                if (dig == '9') { /* possible if i == 1 */
                  round_9_up:
                    *s++ = '9';
                    goto roundoff;
                }
                *s++ = dig + 1;
                goto ret;
            }
            *s++ = dig;
            if (i == ilim)
                break;
            b = multadd(b, 10, 0);
            if (b == NULL)
                goto failed_malloc;
            if (mlo == mhi) {
                mlo = mhi = multadd(mhi, 10, 0);
                if (mlo == NULL)
                    goto failed_malloc;
            }
            else {
                mlo = multadd(mlo, 10, 0);
                if (mlo == NULL)
                    goto failed_malloc;
                mhi = multadd(mhi, 10, 0);
                if (mhi == NULL)
                    goto failed_malloc;
            }
        }
    }
    else
        for(i = 1;; i++) {
            *s++ = dig = quorem(b,S) + '0';
            if (!b->x[0] && b->wds <= 1) {
                goto ret;
            }
            if (i >= ilim)
                break;
            b = multadd(b, 10, 0);
            if (b == NULL)
                goto failed_malloc;
        }

    /* Round off last digit */

    b = lshift(b, 1);
    if (b == NULL)
        goto failed_malloc;
    j = cmp(b, S);
    if (j > 0 || (j == 0 && dig & 1)) {
      roundoff:
        while(*--s == '9')
            if (s == s0) {
                k++;
                *s++ = '1';
                goto ret;
            }
        ++*s++;
    }
    else {
        while(*--s == '0');
        s++;
    }
  ret:
    Bfree(S);
    if (mhi) {
        if (mlo && mlo != mhi)
            Bfree(mlo);
        Bfree(mhi);
    }
  ret1:
    Bfree(b);
    *s = 0;
    *decpt = k + 1;
    if (rve)
        *rve = s;
    return s0;
  failed_malloc:
    if (S)
        Bfree(S);
    if (mlo && mlo != mhi)
        Bfree(mlo);
    if (mhi)
        Bfree(mhi);
    if (b)
        Bfree(b);
    if (s0)
        _Py_dg_freedtoa(s0);
    return NULL;
}
#ifdef __cplusplus
}
#endif

#endif  /* PY_NO_SHORT_FLOAT_REPR */
