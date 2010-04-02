/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef BASEARITH_H
#define BASEARITH_H


#include "mpdecimal.h"
#include <stdio.h>
#include "typearith.h"


mpd_uint_t _mpd_baseadd(mpd_uint_t *w, const mpd_uint_t *u, const mpd_uint_t *v,
                        mpd_size_t m, mpd_size_t n);
void _mpd_baseaddto(mpd_uint_t *w, const mpd_uint_t *u, mpd_size_t n);
mpd_uint_t _mpd_shortadd(mpd_uint_t *w, mpd_size_t m, mpd_uint_t v);
mpd_uint_t _mpd_shortadd_b(mpd_uint_t *w, mpd_size_t m, mpd_uint_t v,
                           mpd_uint_t b);
mpd_uint_t _mpd_baseincr(mpd_uint_t *u, mpd_size_t n);
void _mpd_basesub(mpd_uint_t *w, const mpd_uint_t *u, const mpd_uint_t *v,
                  mpd_size_t m, mpd_size_t n);
void _mpd_basesubfrom(mpd_uint_t *w, const mpd_uint_t *u, mpd_size_t n);
void _mpd_basemul(mpd_uint_t *w, const mpd_uint_t *u, const mpd_uint_t *v,
                  mpd_size_t m, mpd_size_t n);
void _mpd_shortmul(mpd_uint_t *w, const mpd_uint_t *u, mpd_size_t n,
                   mpd_uint_t v);
void _mpd_shortmul_b(mpd_uint_t *w, const mpd_uint_t *u, mpd_size_t n,
                     mpd_uint_t v, mpd_uint_t b);
mpd_uint_t _mpd_shortdiv(mpd_uint_t *w, const mpd_uint_t *u, mpd_size_t n,
                         mpd_uint_t v);
mpd_uint_t _mpd_shortdiv_b(mpd_uint_t *w, const mpd_uint_t *u, mpd_size_t n,
                           mpd_uint_t v, mpd_uint_t b);
int _mpd_basedivmod(mpd_uint_t *q, mpd_uint_t *r, const mpd_uint_t *uconst,
                    const mpd_uint_t *vconst, mpd_size_t nplusm, mpd_size_t n);
void _mpd_baseshiftl(mpd_uint_t *dest, mpd_uint_t *src, mpd_size_t n,
                     mpd_size_t m, mpd_size_t shift);
mpd_uint_t _mpd_baseshiftr(mpd_uint_t *dest, mpd_uint_t *src, mpd_size_t slen,
                           mpd_size_t shift);



#ifdef CONFIG_64
extern const mpd_uint_t mprime_rdx;

/*
 * Algorithm from: Division by Invariant Integers using Multiplication,
 * T. Granlund and P. L. Montgomery, Proceedings of the SIGPLAN '94
 * Conference on Programming Language Design and Implementation.
 *
 * http://gmplib.org/~tege/divcnst-pldi94.pdf
 */
static inline void
_mpd_div_words_r(mpd_uint_t *q, mpd_uint_t *r, mpd_uint_t hi, mpd_uint_t lo)
{
	mpd_uint_t n_adj, h, l, t;
	mpd_uint_t n1_neg;

	n1_neg = (lo & (1ULL<<63)) ? MPD_UINT_MAX : 0;
	n_adj = lo + (n1_neg & MPD_RADIX);

	_mpd_mul_words(&h, &l, mprime_rdx, hi-n1_neg);
	l = l + n_adj;
	if (l < n_adj) h++;
	t = h + hi; /* q1 */

	t = MPD_UINT_MAX - t;

	_mpd_mul_words(&h, &l, t, MPD_RADIX);
	l = l + lo;
	if (l < lo) h++;
	h += hi;
	h -= MPD_RADIX;

	*q = (h - t); 
	*r = l + (MPD_RADIX & h);
}
#else
static inline void
_mpd_div_words_r(mpd_uint_t *q, mpd_uint_t *r, mpd_uint_t hi, mpd_uint_t lo)
{
	_mpd_div_words(q, r, hi, lo, MPD_RADIX);
}
#endif


/* Multiply two single base b words, store result in array w[2]. */
static inline void
_mpd_singlemul(mpd_uint_t w[2], mpd_uint_t u, mpd_uint_t v)
{
	mpd_uint_t hi, lo;

	_mpd_mul_words(&hi, &lo, u, v);
	_mpd_div_words_r(&w[1], &w[0], hi, lo);
}

/* Multiply u (len 2) and v (len 1 or 2). */
static inline void
_mpd_mul_2_le2(mpd_uint_t w[4], mpd_uint_t u[2], mpd_uint_t v[2], mpd_ssize_t m)
{
	mpd_uint_t hi, lo;

	_mpd_mul_words(&hi, &lo, u[0], v[0]);
	_mpd_div_words_r(&w[1], &w[0], hi, lo);

	_mpd_mul_words(&hi, &lo, u[1], v[0]);
	lo = w[1] + lo;
	if (lo < w[1]) hi++;
	_mpd_div_words_r(&w[2], &w[1], hi, lo);
	if (m == 1) return;

	_mpd_mul_words(&hi, &lo, u[0], v[1]);
	lo = w[1] + lo;
	if (lo < w[1]) hi++;
	_mpd_div_words_r(&w[3], &w[1], hi, lo);

	_mpd_mul_words(&hi, &lo, u[1], v[1]);
	lo = w[2] + lo;
	if (lo < w[2]) hi++;
	lo = w[3] + lo;
	if (lo < w[3]) hi++;
	_mpd_div_words_r(&w[3], &w[2], hi, lo);
}


/*
 * Test if all words from data[len-1] to data[0] are zero. If len is 0, nothing
 * is tested and the coefficient is regarded as "all zero".
 */
static inline int
_mpd_isallzero(const mpd_uint_t *data, mpd_ssize_t len)
{
	while (--len >= 0) {
		if (data[len] != 0) return 0;
	}
	return 1;
}

/*
 * Test if all words from data[len-1] to data[0] are MPD_RADIX-1 (all nines).
 * Assumes that len > 0.
 */
static inline int
_mpd_isallnine(const mpd_uint_t *data, mpd_ssize_t len)
{
	while (--len >= 0) {
		if (data[len] != MPD_RADIX-1) return 0;
	}
	return 1;
}


#endif /* BASEARITH_H */



