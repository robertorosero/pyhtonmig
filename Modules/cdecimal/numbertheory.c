/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#include <stdlib.h>
#include <assert.h>
#include "bits.h"
#include "mpdecimal.h"
#include "umodarith.h"
#include "numbertheory.h"


/* transform kernel */
mpd_uint_t
_mpd_getkernel(mpd_uint_t n, int sign, int modnum)
{
	mpd_uint_t umod, p, r, xi;
#ifdef PPRO
	double dmod;
	uint32_t dinvmod[3];
#endif

	SETMODULUS(modnum);
	r = mpd_roots[modnum];
	p = umod;
	xi = (p-1) / n;

	if (sign == -1)
		return POWMOD(r, (p-1-xi));
	else
		return POWMOD(r, xi);
}

/* initialize transform parameters */
struct fnt_params *
_mpd_init_fnt_params(size_t n, int sign, int modnum)
{
	struct fnt_params *tparams;
	mpd_uint_t umod;
#ifdef PPRO
	double dmod;
	uint32_t dinvmod[3];
#endif
	mpd_uint_t kernel, imag, w;
	mpd_uint_t i;
	size_t nhalf;

	assert(ispower2(n));
	assert(sign == -1 || sign == 1);
	assert(P1 <= modnum && modnum <= P3);

	nhalf = n/2;
	tparams = mpd_sh_alloc(sizeof *tparams, nhalf, sizeof (mpd_uint_t));
	if (tparams == NULL) {
		return NULL;
	}

	SETMODULUS(modnum);
	kernel = _mpd_getkernel(n, sign, modnum);
	imag = _mpd_getkernel(4, -sign, modnum);

	tparams->modnum = modnum;
	tparams->modulus = umod;
	tparams->imag = imag;
	tparams->kernel = kernel;

	w  = 1;
	for (i = 0; i < nhalf; i++) {
		tparams->wtable[i] = w;
		w = MULMOD(w, kernel);
	}

	return tparams;
}

/* initialize wtable of size three */
void
_mpd_init_w3table(mpd_uint_t w3table[3], int sign, int modnum)
{
	mpd_uint_t umod;
#ifdef PPRO
	double dmod;
	uint32_t dinvmod[3];
#endif
	mpd_uint_t kernel;

	SETMODULUS(modnum);
	kernel = _mpd_getkernel(3, sign, modnum);

	w3table[0] = 1;
	w3table[1] = kernel;
	w3table[2] = POWMOD(kernel, 2);
}


