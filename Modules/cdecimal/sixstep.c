/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "bits.h"
#include "difradix2.h"
#include "mpdecimal.h"
#include "mptypes.h"
#include "numbertheory.h"
#include "transpose.h"
#include "umodarith.h"
#include "sixstep.h"


/*
 * A variant of the six-step algorithm from:
 *
 * David H. Bailey: FFTs in External or Hierarchical Memory, Journal of
 * Supercomputing, vol. 4, no. 1 (March 1990), p. 23-35.
 *
 * URL: http://crd.lbl.gov/~dhbailey/dhbpapers/
 */


/* forward transform with sign = -1 */
int
six_step_fnt(mpd_uint_t *a, size_t n, int modnum, int ordered)
{
	struct fnt_params *tparams;
	size_t log2n, C, R;
	mpd_uint_t kernel;
	mpd_uint_t umod;
#ifdef PPRO
	double dmod;
	uint32_t dinvmod[3];
#endif
	mpd_uint_t *x, w0, w1, wstep;
	size_t i, k;


	assert(ispower2(n));
	assert(n >= 16);
	assert(n <= MPD_MAXTRANSFORM_2N);

	log2n = BSR(n);
	C = (ONE_UM << (log2n / 2));  /* number of columns */
	R = (ONE_UM << (log2n - (log2n / 2))); /* number of rows */


	transpose_pow2(a, R, C); /* view a as a RxC matrix, tranpose */

	if ((tparams = _mpd_init_fnt_params(R, -1, modnum)) == NULL) {
		return 0;
	}
	for (x = a; x < a+n; x += R) {
		fnt_dif2(x, R, tparams);
	}

	transpose_pow2(a, C, R);


	SETMODULUS(modnum);
	kernel = _mpd_getkernel(n, -1, modnum);
	for (i = 1; i < R; i++) {
		w0 = 1;
		w1 = POWMOD(kernel, i);
		wstep = MULMOD(w1, w1);
		for (k = 0; k < C; k += 2) {
			mpd_uint_t x0 = a[i*C+k];
			mpd_uint_t x1 = a[i*C+k+1];
			MULMOD2(&x0, w0, &x1, w1);
			MULMOD2C(&w0, &w1, wstep);
			a[i*C+k] = x0;
			a[i*C+k+1] = x1;
		}
	}


	if (C != R) {
		mpd_free(tparams);
		if ((tparams = _mpd_init_fnt_params(C, -1, modnum)) == NULL) {
			return 0;
		}
	}
	for (x = a; x < a+n; x += C) {
		fnt_dif2(x, C, tparams);
	}
	mpd_free(tparams);


	if (ordered) {
		transpose_pow2(a, R, C);
	}

	return 1;
}


/* reverse transform, sign = 1 */
int
inv_six_step_fnt(mpd_uint_t *a, size_t n, int modnum, int ordered)
{
	struct fnt_params *tparams;
	size_t log2n, C, R;
	mpd_uint_t kernel;
	mpd_uint_t umod;
#ifdef PPRO
	double dmod;
	uint32_t dinvmod[3];
#endif
	mpd_uint_t *x, w0, w1, wstep;
	size_t i, k;


	assert(ispower2(n));
	assert(n >= 16);
	assert(n <= MPD_MAXTRANSFORM_2N);

	log2n = BSR(n);
	C = (ONE_UM << (log2n / 2)); /* number of columns */
	R = (ONE_UM << (log2n - (log2n / 2))); /* number of rows */


	if (ordered) {
		transpose_pow2(a, C, R);
	}

	if ((tparams = _mpd_init_fnt_params(C, 1, modnum)) == NULL) {
		return 0;
	}
	for (x = a; x < a+n; x += C) {
		fnt_dif2(x, C, tparams);
	}

	transpose_pow2(a, R, C);


	SETMODULUS(modnum);
	kernel = _mpd_getkernel(n, 1, modnum);
	for (i = 1; i < C; i++) {
		w0 = 1;
		w1 = POWMOD(kernel, i);
		wstep = MULMOD(w1, w1);
		for (k = 0; k < R; k += 2) {
			mpd_uint_t x0 = a[i*R+k];
			mpd_uint_t x1 = a[i*R+k+1];
			MULMOD2(&x0, w0, &x1, w1);
			MULMOD2C(&w0, &w1, wstep);
			a[i*R+k] = x0;
			a[i*R+k+1] = x1;
		}
	}


	if (R != C) {
		mpd_free(tparams);
		if ((tparams = _mpd_init_fnt_params(R, 1, modnum)) == NULL) {
			return 0;
		}
	}
	for (x = a; x < a+n; x += R) {
		fnt_dif2(x, R, tparams);
	}
	mpd_free(tparams);

	transpose_pow2(a, C, R);

	return 1;
}


