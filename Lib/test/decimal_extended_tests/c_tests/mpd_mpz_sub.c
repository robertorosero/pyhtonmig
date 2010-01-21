/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gmp.h>
#include "mpdecimal.h"
#include "mptypes.h"


/*
 * Test mpd_sub against mpz_sub, for all possible lengths
 * (in decimal digits) from 1x1 upto (WORDS*MPD_RDIGITS)x(WORDS*MPD_RDIGITS).
 */


#define WORDS 80


int
main(int argc UNUSED, char **argv)
{
    mpd_context_t ctx;
    mpd_t *a, *b, *c;
    mpz_t x, y, z;
    mpd_ssize_t ma;
    char *mpdresult, *mpzresult;
    char *s;
    size_t alen, blen, k;
    double total = (WORDS*MPD_RDIGITS)*(WORDS*MPD_RDIGITS)*63;
    double counter = 0;
    time_t seed;
    uint32_t status = 0;


    mpd_maxcontext(&ctx);
    s = malloc(WORDS*MPD_RDIGITS+1);
    seed = time(NULL);
    srandom((unsigned int)seed);

    for (ma = MPD_MINALLOC_MIN; ma <= MPD_MINALLOC_MAX; ma++) {

        /* DON'T do this in a real program. You have to be sure
         * that no previously allocated decimals will ever be used. */
        MPD_MINALLOC = ma;

        a = mpd_new(&ctx);
        b = mpd_new(&ctx);
        c = mpd_new(&ctx);
        mpz_init(x);
        mpz_init(y);
        mpz_init(z);

        for (alen = 1; alen <= WORDS*MPD_RDIGITS; alen++) {

            fprintf(stderr, "\r%s: progress: %2.4f%%", argv[0],
                    (counter/total*100));

            for (k = 0; k < alen; k++) {
                s[k] = '0' + random()%10;
            }
            s[k] = '\0';
            mpd_qset_string(a, s, &ctx, &status);
            mpz_set_str(x, s, 10);

            for (blen = 1; blen <= WORDS*MPD_RDIGITS; blen++) {

                counter++;

                for (k = 0; k < blen; k++) {
                    s[k] = '0' + random()%10;
                }
                s[k] = '\0';

                mpd_qset_string(b, s, &ctx, &status);
                mpz_set_str(y, s, 10);

                mpd_qsub(c, a, b, &ctx, &status);
                mpdresult = mpd_to_sci(c, 1);

                mpz_sub(z, x, y);
                mpzresult = mpz_get_str(NULL, 10, z);

                if (strcmp(mpzresult, mpdresult) != 0) {
                    fprintf(stderr, " FAIL: seed = %"PRI_time_t"\n", seed);
                    exit(1);
                }
    
                mpd_free(mpdresult);
                mpd_free(mpzresult);
            }
        }

        mpd_del(a);
        mpd_del(b);
        mpd_del(c);
        mpz_clear(x);
        mpz_clear(y);
        mpz_clear(z);
    }

    free(s);
    fprintf(stderr, "\r%s: progress: %2.4f%%", argv[0], 100.0);
    fprintf(stderr, " PASS\n");

    return 0;
}


