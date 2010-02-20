/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#include <stdio.h>
#include "mpdecimal.h"


#ifdef PPRO

#define ANSI

#include <stdlib.h>
#include <time.h>
#include "constants.h"
#include "numbertheory.h"
#include "mptypes.h"
#include "mptest.h"
#include "umodarith.h"

/*
 * These are random tests for the pentium pro modular multiplication functions.
 */

int
main(int argc UNUSED, char **argv)
{
	double dmod;
	uint32_t dinvmod[3];
	mpd_uint_t umod;
	mpd_uint_t a, b, c, d;
	mpd_uint_t a1, a2, b1, b2;
	long i;
	int cw;

	cw = mpd_set_fenv();


	fprintf(stderr, "%s:\n", argv[0]);
	fprintf(stderr, "testing MULMOD ... ");

	srandom((unsigned int)time(NULL));

	SETMODULUS(P1);
	for (i = 0; i < 100000000; i++) {
		a = random()%umod;
		b = random()%umod;
		c = std_mulmod(a, b, umod);
		d = MULMOD(a, b);
		if (c != d) {
			fprintf(stderr, "FAIL:  a: %u  b: %u  ansi: %u "
			                "ppro: %u\n", a, b, c, d);
			exit(1);
		}
	}

	SETMODULUS(P2);
	for (i = 0; i < 100000000; i++) {
		a = random()%umod;
		b = random()%umod;
		c = std_mulmod(a, b, umod);
		d = MULMOD(a, b);
		if (c != d) {
			fprintf(stderr, "FAIL:  a: %u  b: %u  ansi: %u "
			                "ppro: %u\n", a, b, c, d);
			exit(1);
		}
	}

	SETMODULUS(P3);
	for (i = 0; i < 100000000; i++) {
		a = random()%umod;
		b = random()%umod;
		c = std_mulmod(a, b, umod);
		d = MULMOD(a, b);
		if (c != d) {
			fprintf(stderr, "FAIL:  a: %u  b: %u  ansi: %u "
			                "ppro: %u\n", a, b, c, d);
			exit(1);
		}
	}

	fprintf(stderr, "PASS\n");
	fprintf(stderr, "testing MULMOD2C ... ");

	SETMODULUS(P1);
	for (i = 0; i < 100000000; i++) {
		a1 = a2 = a = random()%umod;
		b1 = b2 = b = random()%umod;
		c = random()%umod;
		std_mulmod2c(&a1, &b1, c, umod);
		MULMOD2C(&a2, &b2, c);
		if (a1 != a2 || b1 != b2) {
			fprintf(stderr, "FAIL:  a: %u  b: %u  c: %u  "
                                        "ansi1: %u  ppro1: %u  "
                                        "ansi2: %u  ppro2: %u  ",
			                a, b, c, a1, a2, b1, b2);
			exit(1);
		}
	}

	SETMODULUS(P2);
	for (i = 0; i < 100000000; i++) {
		a1 = a2 = a = random()%umod;
		b1 = b2 = b = random()%umod;
		c = random()%umod;
		std_mulmod2c(&a1, &b1, c, umod);
		MULMOD2C(&a2, &b2, c);
		if (a1 != a2 || b1 != b2) {
			fprintf(stderr, "FAIL:  a: %u  b: %u  c: %u  "
                                        "ansi1: %u  ppro1: %u  "
                                        "ansi2: %u  ppro2: %u  ",
			                a, b, c, a1, a2, b1, b2);
			exit(1);
		}
	}

	SETMODULUS(P3);
	for (i = 0; i < 100000000; i++) {
		a1 = a2 = a = random()%umod;
		b1 = b2 = b = random()%umod;
		c = random()%umod;
		std_mulmod2c(&a1, &b1, c, umod);
		MULMOD2C(&a2, &b2, c);
		if (a1 != a2 || b1 != b2) {
			fprintf(stderr, "FAIL:  a: %u  b: %u  c: %u  "
                                        "ansi1: %u  ppro1: %u  "
                                        "ansi2: %u  ppro2: %u  ",
			                a, b, c, a1, a2, b1, b2);
			exit(1);
		}
	}

	fprintf(stderr, "PASS\n");
	fprintf(stderr, "testing MULMOD2 ... ");

	SETMODULUS(P1);
	for (i = 0; i < 100000000; i++) {
		a1 = a2 = a = random()%umod;
		b1 = b2 = b = random()%umod;
		c = random()%umod;
		d = random()%umod;
		std_mulmod2(&a1, c, &b1, d, umod);
		MULMOD2(&a2, c, &b2, d);
		if (a1 != a2 || b1 != b2) {
			fprintf(stderr, "FAIL:  a: %u  b: %u  c: %u  d: %u  "
                                        "ansi1: %u  ppro1: %u  "
                                        "ansi2: %u  ppro2: %u  ",
			                a, b, c, d, a1, a2, b1, b2);
			exit(1);
		}
	}

	SETMODULUS(P2);
	for (i = 0; i < 100000000; i++) {
		a1 = a2 = a = random()%umod;
		b1 = b2 = b = random()%umod;
		c = random()%umod;
		d = random()%umod;
		std_mulmod2(&a1, c, &b1, d, umod);
		MULMOD2(&a2, c, &b2, d);
		if (a1 != a2 || b1 != b2) {
			fprintf(stderr, "FAIL:  a: %u  b: %u  c: %u  d: %u  "
                                        "ansi1: %u  ppro1: %u  "
                                        "ansi2: %u  ppro2: %u  ",
			                a, b, c, d, a1, a2, b1, b2);
			exit(1);
		}
	}

	SETMODULUS(P3);
	for (i = 0; i < 100000000; i++) {
		a1 = a2 = a = random()%umod;
		b1 = b2 = b = random()%umod;
		c = random()%umod;
		d = random()%umod;
		std_mulmod2(&a1, c, &b1, d, umod);
		MULMOD2(&a2, c, &b2, d);
		if (a1 != a2 || b1 != b2) {
			fprintf(stderr, "FAIL:  a: %u  b: %u  c: %u  d: %u  "
                                        "ansi1: %u  ppro1: %u  "
                                        "ansi2: %u  ppro2: %u  ",
			                a, b, c, d, a1, a2, b1, b2);
			exit(1);
		}
	}

	fprintf(stderr, "PASS\n");

	return 0;
}
/* END PPRO_GCC */
#else

int
main(int argc UNUSED, char **argv)
{
	fprintf(stderr, "%s: PASS\n", argv[0]);
	return 0;
}

#endif



