/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef TRANSPOSE_H
#define TRANSPOSE_H


#include <stdio.h>
#include "mpdecimal.h"


enum {FORWARD_CYCLE, BACKWARD_CYCLE};


void std_trans(mpd_uint_t dest[], mpd_uint_t src[], size_t rows, size_t cols);
int transpose_pow2(mpd_uint_t *matrix, size_t rows, size_t cols);
void transpose_3xpow2(mpd_uint_t *matrix, size_t rows, size_t cols);


static inline void pointerswap(mpd_uint_t **a, mpd_uint_t **b)
{
	mpd_uint_t *tmp;

	tmp = *b;
	*b = *a;
	*a = tmp;
}


#endif
