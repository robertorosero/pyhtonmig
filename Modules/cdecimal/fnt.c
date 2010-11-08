/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#include "mpdecimal.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "bits.h"
#include "difradix2.h"
#include "numbertheory.h"
#include "fnt.h"


/* forward transform, sign = -1 */
int
std_fnt(mpd_uint_t *a, mpd_size_t n, int modnum)
{
	struct fnt_params *tparams;

	assert(ispower2(n));
	assert(n >= 4);
	assert(n <= 3*MPD_MAXTRANSFORM_2N);

	if ((tparams = _mpd_init_fnt_params(n, -1, modnum)) == NULL) {
		return 0;
	}
	fnt_dif2(a, n, tparams);

	mpd_free(tparams);
	return 1;
}

/* reverse transform, sign = 1 */
int
std_inv_fnt(mpd_uint_t *a, mpd_size_t n, int modnum)
{
	struct fnt_params *tparams;

	assert(ispower2(n));
	assert(n >= 4);
	assert(n <= 3*MPD_MAXTRANSFORM_2N);

	if ((tparams = _mpd_init_fnt_params(n, 1, modnum)) == NULL) {
		return 0;
	}
	fnt_dif2(a, n, tparams);

	mpd_free(tparams);
	return 1;
}



