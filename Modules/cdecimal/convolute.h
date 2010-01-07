/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef CONVOLUTE_H
#define CONVOLUTE_H


#include <stdio.h>
#include "mpdecimal.h"

#define SIX_STEP_THRESHOLD 4096

int fnt_convolute(mpd_uint_t *c1, mpd_uint_t *c2, size_t n, int modnum);
int fnt_autoconvolute(mpd_uint_t *c1, size_t n, int modnum);


#endif
