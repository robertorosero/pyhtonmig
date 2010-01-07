/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef FNT_H
#define FNT_H


#include <stdio.h>
#include "mpdecimal.h"

int std_fnt(mpd_uint_t a[], size_t n, int modnum, int ordered UNUSED);
int std_inv_fnt(mpd_uint_t a[], size_t n, int modnum, int ordered UNUSED);


#endif

