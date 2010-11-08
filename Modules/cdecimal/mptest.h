/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef MPTEST_H
#define MPTEST_H


#include "mpdecimal.h"


/* newton division undergoes the same rigorous tests as standard division */
void mpd_test_newtondiv(mpd_t *q, const mpd_t *a, const mpd_t *b, mpd_context_t *ctx);
void mpd_test_newtondivint(mpd_t *q, const mpd_t *a, const mpd_t *b, mpd_context_t *ctx);
void mpd_test_newtonrem(mpd_t *r, const mpd_t *a, const mpd_t *b, mpd_context_t *ctx);
void mpd_test_newtondivmod(mpd_t *q, mpd_t *r, const mpd_t *a, const mpd_t *b, mpd_context_t *ctx);

/* fenv */
unsigned int mpd_set_fenv(void);
void mpd_restore_fenv(unsigned int);

mpd_uint_t *_mpd_fntmul(const mpd_uint_t *u, const mpd_uint_t *v, mpd_size_t ulen, mpd_size_t vlen, mpd_size_t *rsize);
mpd_uint_t *_mpd_kmul(const mpd_uint_t *u, const mpd_uint_t *v, mpd_size_t ulen, mpd_size_t vlen, mpd_size_t *rsize);
mpd_uint_t *_mpd_kmul_fnt(const mpd_uint_t *u, const mpd_uint_t *v, mpd_size_t ulen, mpd_size_t vlen, mpd_size_t *rsize);


#endif


