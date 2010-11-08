/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#include "mpdecimal.h"
#include <stdio.h>
#include <stdarg.h>


void mpd_err_doit(int action, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);

	if (action == MPD_ERR_EXIT) {
		exit(EXIT_FAILURE); /* GCOV_NOT_REACHED */
	}
}

