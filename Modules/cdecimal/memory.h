/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef MEMORY_H
#define MEMORY_H


#include "mpdecimal.h"


int mpd_switch_to_dyn(mpd_t *result, mpd_ssize_t size, uint32_t *status);
int mpd_switch_to_dyn_zero(mpd_t *result, mpd_ssize_t size, uint32_t *status);
int mpd_realloc_dyn(mpd_t *result, mpd_ssize_t size, uint32_t *status);


#endif



