/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef SIX_STEP_H
#define SIX_STEP_H


#include "mpdecimal.h"
#include <stdio.h>


int six_step_fnt(mpd_uint_t *a, mpd_size_t n, int modnum);
int inv_six_step_fnt(mpd_uint_t *a, mpd_size_t n, int modnum);


#endif
