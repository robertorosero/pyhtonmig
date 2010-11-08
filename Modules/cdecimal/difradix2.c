/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#include "mpdecimal.h"
#include <stdio.h>
#include <assert.h>
#include "bits.h"
#include "numbertheory.h"
#include "umodarith.h"
#include "difradix2.h"


/*
 * Generate bit reversed words and carry out the permutation.
 * Algorithm due to Brent Lehmann, see Joerg Arndt, "Matters Computational",
 * Chapter 1.13.4. [http://www.jjj.de/fxt/]
 */
static inline void
bitreverse_permute(mpd_uint_t a[], mpd_size_t n)
{
	mpd_size_t x = 0;
	mpd_size_t r = 0;
	mpd_uint_t t;

	do {
		if (r > x) {
			t = a[x];
			a[x] = a[r];
			a[r] = t;
		}
		x += 1;
		r ^= (n - (n >> (mpd_bsf(x)+1)));
	} while (x < n);
}


/* Fast Number Theoretic Transform, decimation in frequency. */
void
fnt_dif2(mpd_uint_t a[], mpd_size_t n, struct fnt_params *tparams)
{
	mpd_uint_t *wtable = tparams->wtable;
	mpd_uint_t umod;
#ifdef PPRO
	double dmod;
	uint32_t dinvmod[3];
#endif
	mpd_uint_t u0, u1, v0, v1;
	mpd_uint_t w, w0, w1, wstep;
	mpd_size_t m ,mhalf;
	mpd_size_t j, r;


	assert(ispower2(n));
	assert(n >= 4);

	SETMODULUS(tparams->modnum);

	mhalf = n / 2;
	for (j = 0; j < mhalf; j += 2) {

		w0 = wtable[j];
		w1 = wtable[j+1];

		u0 = a[j];
		v0 = a[j+mhalf];

		u1 = a[j+1];
		v1 = a[j+1+mhalf];

		a[j] = addmod(u0, v0, umod);
		v0 = submod(u0, v0, umod);

		a[j+1] = addmod(u1, v1, umod);
		v1 = submod(u1, v1, umod);

		MULMOD2(&v0, w0, &v1, w1);

		a[j+mhalf] = v0;
		a[j+1+mhalf] = v1;

	}

	wstep = 2;
	for (m = n/2; m >= 2; m>>=1, wstep<<=1) {

		mhalf = m / 2;

		/* j = 0 */
		for (r = 0; r < n; r += 2*m) {

			u0 = a[r];
			v0 = a[r+mhalf];

			u1 = a[m+r];
			v1 = a[m+r+mhalf];

			a[r] = addmod(u0, v0, umod);
			v0 = submod(u0, v0, umod);

			a[m+r] = addmod(u1, v1, umod);
			v1 = submod(u1, v1, umod);

			a[r+mhalf] = v0;
			a[m+r+mhalf] = v1;
		}

		for (j = 1; j < mhalf; j++) {

			w = wtable[j*wstep];

			for (r = 0; r < n; r += 2*m) {

				u0 = a[r+j];
				v0 = a[r+j+mhalf];

				u1 = a[m+r+j];
				v1 = a[m+r+j+mhalf];

				a[r+j] = addmod(u0, v0, umod);
				v0 = submod(u0, v0, umod);

				a[m+r+j] = addmod(u1, v1, umod);
				v1 = submod(u1, v1, umod);

				MULMOD2C(&v0, &v1, w);

				a[r+j+mhalf] = v0;
				a[m+r+j+mhalf] = v1;
			}

		}

	}

	bitreverse_permute(a, n);
}


