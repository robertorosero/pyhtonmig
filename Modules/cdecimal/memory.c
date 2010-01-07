/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#include <stdio.h>
#include <stdlib.h>
#include "mpdecimal.h"
#include "typearith.h"
#include "memory.h"


mpd_ssize_t MPD_MINALLOC = MPD_MINALLOC_MIN;

void *(* mpd_mallocfunc)(size_t size) = malloc;
void *(* mpd_reallocfunc)(void *ptr, size_t size) = realloc;
void *(* mpd_callocfunc)(size_t nmemb, size_t size) = calloc;
void (* mpd_free)(void *ptr) = free;


/* emulate calloc if it is not available */
void *
mpd_callocfunc_em(size_t nmemb, size_t size)
{
	void *ptr;
	size_t req;

	req = mul_size_t(nmemb, size);
	if ((ptr = mpd_mallocfunc(req)) == NULL) {
		return NULL;
	}
	/* used on uint32_t or uint64_t */
	memset(ptr, 0, req);

	return ptr;
}


/* malloc with overflow checking */
void *
mpd_alloc(size_t nmemb, size_t size)
{
	void *ptr;
	size_t req;

	req = mul_size_t(nmemb, size);
	if ((ptr = mpd_mallocfunc(req)) == NULL) {
		return NULL;
	}

	return ptr;
}

/* calloc with overflow checking */
void *
mpd_calloc(size_t nmemb, size_t size)
{
	void *ptr;

	if ((ptr = mpd_callocfunc(nmemb, size)) == NULL) {
		return NULL;
	}

	return ptr;
}

/* realloc with overflow checking */
void *
mpd_realloc(void *ptr, size_t nmemb, size_t size, uint8_t *err)
{
	void *new;
	size_t req;

	req = mul_size_t(nmemb, size);
	if ((new = mpd_reallocfunc(ptr, req)) == NULL) {
		*err = 1;
		return ptr;
	}

	return new;
}

/* struct hack malloc with overflow checking */
void *
mpd_sh_alloc(size_t struct_size, size_t nmemb, size_t size)
{
	void *ptr;
	size_t req;

	req = mul_size_t(nmemb, size);
	req = add_size_t(req, struct_size);
	if ((ptr = mpd_mallocfunc(req)) == NULL) {
		return NULL;
	}

	return ptr;
}

/* mpd_callocfunc must have overflow checking */
void *
mpd_sh_calloc(size_t struct_size, size_t nmemb, size_t size)
{
	void *ptr;
	size_t req;

	req = mul_size_t(nmemb, size);
	req = add_size_t(req, struct_size);
	if ((ptr = mpd_callocfunc(req, 1)) == NULL) {
		return NULL;
	}

	return ptr;
}

/* struct hack realloc with overflow checking */
void *
mpd_sh_realloc(void *ptr, size_t struct_size, size_t nmemb, size_t size, uint8_t *err)
{
	void *new;
	size_t req;

	req = mul_size_t(nmemb, size);
	req = add_size_t(req, struct_size);
	if ((new = mpd_reallocfunc(ptr, req)) == NULL) {
		*err = 1;
		return ptr;
	}

	return new;
}


/* Allocate a new decimal with data-size 'size'.
 * In case of an error the return value is NULL.
 */
mpd_t *
mpd_qnew_size(mpd_ssize_t size)
{
	mpd_t *result;

	size = (size < MPD_MINALLOC) ? MPD_MINALLOC : size;

	if ((result = mpd_alloc(1, sizeof *result)) == NULL) {
		return NULL;
	}
	if ((result->data = mpd_alloc(size, sizeof *result->data)) == NULL) {
		mpd_free(result);
		return NULL;
	}

	result->flags = result->exp = result->digits = result->len = 0;
	result->alloc = size;

	return result;
}

/* Allocate a new decimal with data-size MPD_MINALLOC.
 * In case of an error the return value is NULL.
 */
mpd_t *
mpd_qnew(void)
{
	return mpd_qnew_size(MPD_MINALLOC);
}

/* Allocate new decimal. Caller can check for NULL or MPD_Malloc_error.
 * Raises on error.
 */
mpd_t *
mpd_new(mpd_context_t *ctx)
{
	mpd_t *result;

	if ((result = mpd_qnew()) == NULL) {
		mpd_addstatus_raise(ctx, MPD_Malloc_error);
	}
	return result;
}

int
mpd_switch_to_dyn(mpd_t *result, mpd_ssize_t size, uint32_t *status)
{
	mpd_uint_t *p = result->data;

	if ((result->data = mpd_alloc(size, sizeof *result->data)) == NULL) {
		result->data = p;
		mpd_set_qnan(result);
		mpd_set_positive(result);
		result->exp = result->digits = result->len = 0;
		*status |= MPD_Malloc_error;
		return 0;
	}

	memcpy(result->data, p, result->len * (sizeof *result->data));
	result->alloc = size;
	mpd_set_dynamic_data(result);
	return 1;
}

int
mpd_switch_to_dyn_zero(mpd_t *result, mpd_ssize_t size, uint32_t *status)
{
	mpd_uint_t *p = result->data;

	if ((result->data = mpd_calloc(size, sizeof *result->data)) == NULL) {
		result->data = p;
		mpd_set_qnan(result);
		mpd_set_positive(result);
		result->exp = result->digits = result->len = 0;
		*status |= MPD_Malloc_error;
		return 0;
	}

	result->alloc = size;
	mpd_set_dynamic_data(result);

	return 1;
}

int
mpd_realloc_dyn(mpd_t *result, mpd_ssize_t size, uint32_t *status)
{
	uint8_t err = 0;

	result->data = mpd_realloc(result->data, size, sizeof *result->data, &err);
	if (!err) {
		result->alloc = size;
	}
	else if (size > result->alloc) {
		mpd_set_qnan(result);
		mpd_set_positive(result);
		result->exp = result->digits = result->len = 0;
		*status |= MPD_Malloc_error;
		return 0;
	}

	return 1;
}


