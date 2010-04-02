/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#include "mpdecimal.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>


static void
mpd_dflt_traphandler(mpd_context_t *ctx UNUSED)
{
	raise(SIGFPE);
}

void (* mpd_traphandler)(mpd_context_t *) = mpd_dflt_traphandler;


static void
mpd_setminalloc(mpd_ssize_t n)
{
	static int minalloc_is_set = 0;

	if (minalloc_is_set) {
		mpd_err_warn("mpd_setminalloc: ignoring request to set "
		             "MPD_MINALLOC a second time\n");
		return;
	}
	if (n < MPD_MINALLOC_MIN || n > MPD_MINALLOC_MAX) {
		mpd_err_fatal("illegal value for MPD_MINALLOC");
	}
	MPD_MINALLOC = n;
	minalloc_is_set = 1;
}

void
mpd_init(mpd_context_t *ctx, mpd_ssize_t prec)
{
	mpd_ssize_t ideal_minalloc;

	mpd_defaultcontext(ctx);

	if (!mpd_qsetprec(ctx, prec)) {
		mpd_addstatus_raise(ctx, MPD_Invalid_context);
		return;
	}

	ideal_minalloc = 2 * ((prec+MPD_RDIGITS-1) / MPD_RDIGITS);
	if (ideal_minalloc < MPD_MINALLOC_MIN) ideal_minalloc = MPD_MINALLOC_MIN;
	if (ideal_minalloc > MPD_MINALLOC_MAX) ideal_minalloc = MPD_MINALLOC_MAX;

	mpd_setminalloc(ideal_minalloc);
}

void
mpd_maxcontext(mpd_context_t *ctx)
{
	ctx->prec=MPD_MAX_PREC;
	ctx->emax=MPD_MAX_EMAX;
	ctx->emin=MPD_MIN_EMIN;
	ctx->round=MPD_ROUND_HALF_EVEN;
	ctx->traps=MPD_Traps;
	ctx->status=0;
	ctx->newtrap=0;
	ctx->clamp=0;
	ctx->allcr=1;
}

void
mpd_defaultcontext(mpd_context_t *ctx)
{
	ctx->prec=2*MPD_RDIGITS;
	ctx->emax=MPD_MAX_EMAX;
	ctx->emin=MPD_MIN_EMIN;
	ctx->round=MPD_ROUND_HALF_UP;
	ctx->traps=MPD_Traps;
	ctx->status=0;
	ctx->newtrap=0;
	ctx->clamp=0;
	ctx->allcr=1;
}

void
mpd_basiccontext(mpd_context_t *ctx)
{
	ctx->prec=9;
	ctx->emax=MPD_MAX_EMAX;
	ctx->emin=MPD_MIN_EMIN;
	ctx->round=MPD_ROUND_HALF_UP;
	ctx->traps=MPD_Traps|MPD_Clamped;
	ctx->status=0;
	ctx->newtrap=0;
	ctx->clamp=0;
	ctx->allcr=1;
}

void
mpd_extcontext(mpd_context_t *ctx)
{
	ctx->prec=9;
	ctx->emax=MPD_MAX_EMAX;
	ctx->emin=MPD_MIN_EMIN;
	ctx->round=MPD_ROUND_HALF_EVEN;
	ctx->traps=0;
	ctx->status=0;
	ctx->newtrap=0;
	ctx->clamp=0;
	ctx->allcr=1;
}


mpd_ssize_t
mpd_getprec(const mpd_context_t *ctx)
{
	return ctx->prec;
}

mpd_ssize_t
mpd_getemax(const mpd_context_t *ctx)
{
	return ctx->emax;
}

mpd_ssize_t
mpd_getemin(const mpd_context_t *ctx)
{
	return ctx->emin;
}

int
mpd_getround(const mpd_context_t *ctx)
{
	return ctx->round;
}

uint32_t
mpd_gettraps(const mpd_context_t *ctx)
{
	return ctx->traps;
}

uint32_t
mpd_getstatus(const mpd_context_t *ctx)
{
	return ctx->status;
}

int
mpd_getclamp(const mpd_context_t *ctx)
{
	return ctx->clamp;
}

int
mpd_getcr(const mpd_context_t *ctx)
{
	return ctx->allcr;
}


int
mpd_qsetprec(mpd_context_t *ctx, mpd_ssize_t prec)
{
	if (prec <= 0 || prec > MPD_MAX_PREC) {
		return 0;
	}
	ctx->prec = prec;
	return 1;
}

int
mpd_qsetemax(mpd_context_t *ctx, mpd_ssize_t emax)
{
	if (emax < 0 || emax > MPD_MAX_EMAX) {
		return 0;
	}
	ctx->emax = emax;
	return 1;
}

int
mpd_qsetemin(mpd_context_t *ctx, mpd_ssize_t emin)
{
	if (emin > 0 || emin < MPD_MIN_EMIN) {
		return 0;
	}
	ctx->emin = emin;
	return 1;
}

int
mpd_qsetround(mpd_context_t *ctx, int round)
{
	int i;

	for (i = 0; i < MPD_ROUND_GUARD; i++) {
		if (i == round) {
			ctx->round = round;
			return 1;
		}
	}
	return 0;
}

int
mpd_qsettraps(mpd_context_t *ctx, uint32_t traps)
{
	if (traps > MPD_Max_status) {
		return 0;
	}
	ctx->traps = traps;
	return 1;
}

int
mpd_qsetstatus(mpd_context_t *ctx, uint32_t flags)
{
	if (flags > MPD_Max_status) {
		return 0;
	}
	ctx->status = flags;
	return 1;
}

int
mpd_qsetclamp(mpd_context_t *ctx, int c)
{
	if (c != 0 && c != 1) {
		return 0;
	}
	ctx->clamp = c;
	return 1;
}

int
mpd_qsetcr(mpd_context_t *ctx, int c)
{
	if (c != 0 && c != 1) {
		return 0;
	}
	ctx->allcr = c;
	return 1;
}


void
mpd_addstatus_raise(mpd_context_t *ctx, uint32_t flags)
{
	ctx->status |= flags;
	if (flags&ctx->traps) {
		ctx->newtrap = (flags&ctx->traps);
		mpd_traphandler(ctx);
	}
}


