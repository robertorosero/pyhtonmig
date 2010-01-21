/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "mpdecimal.h"
#include "mptypes.h"
#include "mptest.h"


/*
 * Test Karatsuba multiplication against FNT multiplication.
 */


#define WORDS 1200


int
main(int argc UNUSED, char **argv)
{
	mpd_uint_t *a, *b;
	mpd_uint_t *fntresult, *kresult;
	mpd_size_t alen, blen, k;
	double total = (WORDS-1)*(WORDS)-12;
	double counter = 0;
	mpd_size_t rsize;
	time_t seed;

	a = malloc(WORDS * (sizeof *a));
	b = malloc(WORDS * (sizeof *b));

	for (k = 0; k < WORDS; k++) {
		a[k] = MPD_RADIX-1;
	}
	for (k = 0; k < WORDS; k++) {
		b[k] = MPD_RADIX-1;
	}


	/* test with all digits 9 */
	for (alen = 4; alen < WORDS; alen++) {

		fprintf(stderr, "\r%s: progress: %2.4f%%", argv[0], (counter/total*100));

		for (blen = 1; blen <= alen; blen++) {

			counter++;

			fntresult = _mpd_fntmul(a, b, alen, blen, &rsize);
			kresult = _mpd_kmul(a, b, alen, blen, &rsize);

			for (k = 0; k < alen+blen; k++) {
				if (kresult[k] != fntresult[k]) {
					fprintf(stderr, " FAIL\n");
					exit(1);
				}
			}
	
			mpd_free(fntresult);
			mpd_free(kresult);
		}
	}

	/* random test */
	seed = time(NULL);
	srandom((unsigned int)seed);

	for (alen = 4; alen < WORDS; alen++) {

		fprintf(stderr, "\r%s: progress: %2.4f%%", argv[0], (counter/total*100));

		for (k = 0; k < alen; k++) {
			a[k] = random()%MPD_RADIX;
		}

		for (blen = 1; blen <= alen; blen++) {

			counter++;

			for (k = 0; k < blen; k++) {
				b[k] = random()%MPD_RADIX;
			}

			fntresult = _mpd_fntmul(a, b, alen, blen, &rsize);
			kresult = _mpd_kmul(a, b, alen, blen, &rsize);

			for (k = 0; k < alen+blen; k++) {
				if (kresult[k] != fntresult[k]) {
					fprintf(stderr, " FAIL: seed = %"PRI_time_t"\n", seed);
					exit(1);
				}
			}
	
			mpd_free(fntresult);
			mpd_free(kresult);
		}
	}

	fprintf(stderr, "\r%s: progress: %2.4f%%", argv[0], 100.0);
	fprintf(stderr, " PASS\n");

	mpd_free(a);
	mpd_free(b);

	return 0;
}


