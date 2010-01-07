/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef TYPEARITH_H
#define TYPEARITH_H


#include "mpdecimal.h"


/*****************************************************************************/
/*                      Native arithmetic on basic types                     */
/*****************************************************************************/


/** ------------------------------------------------------------
 **           Double width multiplication and division
 ** ------------------------------------------------------------
 */

#if defined (CONFIG_64)
#if defined (__GNUC__)
static inline void
_mpd_mul_words(mpd_uint_t *hi, mpd_uint_t *lo, mpd_uint_t a, mpd_uint_t b)
{
	mpd_uint_t h, l;

	asm(	"mulq %3\n\t"\
		: "=d" (h), "=a" (l)\
		: "%a" (a), "rm" (b)\
		: "cc"
	);

	*hi = h;
	*lo = l;
}

static inline void
_mpd_div_words(mpd_uint_t *q, mpd_uint_t *r, mpd_uint_t hi, mpd_uint_t lo, mpd_uint_t d)
{
	mpd_uint_t qq, rr;

	asm (	"divq %4\n\t"\
		: "=a" (qq), "=d" (rr)\
		: "a" (lo), "d" (hi), "rm" (d)\
		: "cc"
	);

	*q = qq;
	*r = rr;
}
/* END CONFIG_64: __GNUC__ */

#elif defined (_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_umul128)

static inline void
_mpd_mul_words(mpd_uint_t *hi, mpd_uint_t *lo, mpd_uint_t a, mpd_uint_t b)
{
	*lo = _umul128(a, b, hi);
}

void _mpd_div_words(mpd_uint_t *q, mpd_uint_t *r, mpd_uint_t hi, mpd_uint_t lo, mpd_uint_t d);
/* END CONFIG_64: _MSC_VER */

#else
  #error "need platform specific 128 bit multiplication and division"
#endif

static inline void
_mpd_divmod_pow10(mpd_uint_t *q, mpd_uint_t *r, mpd_uint_t v, mpd_uint_t exp)
{
	assert(exp <= 19);

	if (exp <= 9) {
		if (exp <= 4) {
			switch (exp) {
			case 0: *q = v; *r = 0; break;
			case 1: *q = v / 10ULL; *r = v - *q * 10ULL; break;
			case 2: *q = v / 100ULL; *r = v - *q * 100ULL; break;
			case 3: *q = v / 1000ULL; *r = v - *q * 1000ULL; break;
			case 4: *q = v / 10000ULL; *r = v - *q * 10000ULL; break;
			}
		}
		else {
			switch (exp) {
			case 5: *q = v / 100000ULL; *r = v - *q * 100000ULL; break;
			case 6: *q = v / 1000000ULL; *r = v - *q * 1000000ULL; break;
			case 7: *q = v / 10000000ULL; *r = v - *q * 10000000ULL; break;
			case 8: *q = v / 100000000ULL; *r = v - *q * 100000000ULL; break;
			case 9: *q = v / 1000000000ULL; *r = v - *q * 1000000000ULL; break;
			}
		}
	}
	else {
		if (exp <= 14) {
			switch (exp) {
			case 10: *q = v / 10000000000ULL; *r = v - *q * 10000000000ULL; break;
			case 11: *q = v / 100000000000ULL; *r = v - *q * 100000000000ULL; break;
			case 12: *q = v / 1000000000000ULL; *r = v - *q * 1000000000000ULL; break;
			case 13: *q = v / 10000000000000ULL; *r = v - *q * 10000000000000ULL; break;
			case 14: *q = v / 100000000000000ULL; *r = v - *q * 100000000000000ULL; break;
			}
		}
		else {
			switch (exp) {
			case 15: *q = v / 1000000000000000ULL; *r = v - *q * 1000000000000000ULL; break;
			case 16: *q = v / 10000000000000000ULL; *r = v - *q * 10000000000000000ULL; break;
			case 17: *q = v / 100000000000000000ULL; *r = v - *q * 100000000000000000ULL; break;
			case 18: *q = v / 1000000000000000000ULL; *r = v - *q * 1000000000000000000ULL; break;
			case 19: *q = v / 10000000000000000000ULL; *r = v - *q * 10000000000000000000ULL; break;
			}
		}
	}
}
/* END CONFIG_64 */

#elif defined (CONFIG_32)
#if defined (ANSI)
static inline void
_mpd_mul_words(mpd_uint_t *hi, mpd_uint_t *lo, mpd_uint_t a, mpd_uint_t b)
{
	mpd_uuint_t hl;

	hl = (mpd_uuint_t)a * b;

	*hi = hl >> 32;
	*lo = hl;
}

static inline void
_mpd_div_words(mpd_uint_t *q, mpd_uint_t *r, mpd_uint_t hi, mpd_uint_t lo, mpd_uint_t d)
{
	mpd_uuint_t hl;

	hl = ((mpd_uuint_t)hi<<32) + lo;
	*q = hl / d; /* quotient is known to fit */
	*r = hl - (mpd_uuint_t)(*q) * d;
}
/* END CONFIG_32: ANSI */

#elif defined(__GNUC__)
static inline void
_mpd_mul_words(mpd_uint_t *hi, mpd_uint_t *lo, mpd_uint_t a, mpd_uint_t b)
{
	mpd_uint_t h, l;

	asm(	"mull %3\n\t"\
		: "=d" (h), "=a" (l)\
		: "%a" (a), "rm" (b)\
		: "cc"
	);

	*hi = h;
	*lo = l;
}

static inline void
_mpd_div_words(mpd_uint_t *q, mpd_uint_t *r, mpd_uint_t hi, mpd_uint_t lo, mpd_uint_t d)
{
	mpd_uint_t qq, rr;

	asm (	"divl %4\n\t"\
		: "=a" (qq), "=d" (rr)\
		: "a" (lo), "d" (hi), "rm" (d)\
		: "cc"
	);

	*q = qq;
	*r = rr;
}
/* END CONFIG_32: __GNUC__ */

#elif defined (_MSC_VER)
static inline void __cdecl
_mpd_mul_words(mpd_uint_t *hi, mpd_uint_t *lo, mpd_uint_t a, mpd_uint_t b)
{
	mpd_uint_t h, l;

	__asm {
		mov eax, a
		mul b
		mov h, edx
		mov l, eax
	}

	*hi = h;
	*lo = l;
}

static inline void __cdecl
_mpd_div_words(mpd_uint_t *q, mpd_uint_t *r, mpd_uint_t hi, mpd_uint_t lo, mpd_uint_t d)
{
	mpd_uint_t qq, rr;

	__asm {
		mov eax, lo
		mov edx, hi
		div d
		mov qq, eax
		mov rr, edx
	}

	*q = qq;
	*r = rr;
}
/* END CONFIG_32: _MSC_VER */

#else
  #error "need platform specific 64 bit multiplication and division"
#endif

static inline void
_mpd_divmod_pow10(mpd_uint_t *q, mpd_uint_t *r, mpd_uint_t v, mpd_uint_t exp)
{
	assert(exp <= 9);

	if (exp <= 4) {
		switch (exp) {
		case 0: *q = v; *r = 0; break;
		case 1: *q = v / 10ULL; *r = v - *q * 10ULL; break;
		case 2: *q = v / 100ULL; *r = v - *q * 100ULL; break;
		case 3: *q = v / 1000ULL; *r = v - *q * 1000ULL; break;
		case 4: *q = v / 10000ULL; *r = v - *q * 10000ULL; break;
		}
	}
	else {
		switch (exp) {
		case 5: *q = v / 100000ULL; *r = v - *q * 100000ULL; break;
		case 6: *q = v / 1000000ULL; *r = v - *q * 1000000ULL; break;
		case 7: *q = v / 10000000ULL; *r = v - *q * 10000000ULL; break;
		case 8: *q = v / 100000000ULL; *r = v - *q * 100000000ULL; break;
		case 9: *q = v / 1000000000ULL; *r = v - *q * 1000000000ULL; break;
		}
	}
}
/* END CONFIG_32 */

/* NO CONFIG */
#else
  #error "define CONFIG_64 or CONFIG_32"
#endif /* CONFIG */


static inline void
_mpd_div_word(mpd_uint_t *q, mpd_uint_t *r, mpd_uint_t v, mpd_uint_t d)
{
	*q = v / d;
	*r = v - *q * d;
}

static inline void
_mpd_idiv_word(mpd_ssize_t *q, mpd_ssize_t *r, mpd_ssize_t v, mpd_ssize_t d)
{
	*q = v / d;
	*r = v - *q * d;
}


/** ------------------------------------------------------------
 **              Arithmetic with overflow checking
 ** ------------------------------------------------------------
 */
static inline size_t
add_size_t(size_t a, size_t b)
{
	if (a > SIZE_MAX - b) {
		mpd_err_fatal("add_size_t(): overflow: check the context");
	}
	return a + b;
}

static inline size_t
sub_size_t(size_t a, size_t b)
{
	if (b > a) {
		mpd_err_fatal("sub_size_t(): overflow: check the context");
	}
	return a - b;
}

#if SIZE_MAX != MPD_UINT_MAX
  #error "adapt mul_size_t(), mod_mpd_ssize_t() and mulmod_size_t()"
#endif

static inline size_t
mul_size_t(size_t a, size_t b)
{
	mpd_uint_t hi, lo;

	_mpd_mul_words(&hi, &lo, (mpd_uint_t)a, (mpd_uint_t)b);
	if (hi) {
		mpd_err_fatal("mul_size_t(): overflow: check the context");
	}
	return lo;
}

static inline mpd_ssize_t
mod_mpd_ssize_t(mpd_ssize_t a, mpd_ssize_t m)
{
	mpd_ssize_t r = a % m;
	return (r < 0) ? r + m : r;
}

static inline size_t
mulmod_size_t(size_t a, size_t b, size_t m)
{
	mpd_uint_t hi, lo;
	mpd_uint_t q, r;

	_mpd_mul_words(&hi, &lo, (mpd_uint_t)a, (mpd_uint_t)b);
	_mpd_div_words(&q, &r, hi, lo, (mpd_uint_t)m);

	return r;
}


#endif /* TYPEARITH_H */



