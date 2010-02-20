/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef MPDECIMAL64_H
#define MPDECIMAL64_H


#ifdef __cplusplus
extern "C" {
#define __STDC_LIMIT_MACROS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>


/******************************************************************************/
/*                      Types for 64 bit architectures                        */
/******************************************************************************/

/* types for modular and base arithmetic */
#define MPD_UINT_MAX UINT64_MAX
#define MPD_BITS_PER_UINT 64
typedef uint64_t mpd_uint_t;  /* unsigned mod type */

/* enable CONFIG_32+ANSI on 64-bit platforms without resorting to -m32 */
#define MPD_SIZE_MAX SIZE_MAX
typedef size_t mpd_size_t; /* unsigned size type */

/* type for exp, digits, len, prec */
#define MPD_SSIZE_MAX INT64_MAX
#define MPD_SSIZE_MIN INT64_MIN
typedef int64_t mpd_ssize_t;
#define _mpd_strtossize strtoll

/* decimal arithmetic */
#define MPD_RADIX 10000000000000000000ULL  /* 10**19 */
#define MPD_RDIGITS 19
#define MPD_MAX_POW10 19
#define MPD_EXPDIGITS 19  /* MPD_EXPDIGITS <= MPD_RDIGITS+1 */

#define MPD_MAXTRANSFORM_2N 4294967296ULL      /* 2**32 */
#define MPD_MAX_PREC 999999999999999999LL
#define MPD_MAX_PREC_LOG2 64
#define MPD_ELIMIT  1000000000000000000LL
#define MPD_MAX_EMAX   999999999999999999LL    /* ELIMIT-1 */
#define MPD_MIN_EMIN  (-999999999999999999LL)  /* -EMAX */
#define MPD_MIN_ETINY (MPD_MIN_EMIN-(MPD_MAX_PREC-1))
#define MPD_EXP_INF (MPD_ELIMIT+1)
#define MPD_EXP_CLAMP (2*MPD_MIN_ETINY)
#define MPD_MAXIMPORT 105263157894736842L /* (2*MPD_MAX_PREC)/MPD_RDIGITS */


#if MPD_SIZE_MAX != MPD_UINT_MAX
  #error "unsupported platform: need mpd_size_t == mpd_uint_t"
#endif


/******************************************************************************/
/*                                Context                                     */
/******************************************************************************/

enum {
	MPD_ROUND_UP,          /* round away from 0               */
	MPD_ROUND_DOWN,        /* round toward 0 (truncate)       */
	MPD_ROUND_CEILING,     /* round toward +infinity          */
	MPD_ROUND_FLOOR,       /* round toward -infinity          */
	MPD_ROUND_HALF_UP,     /* 0.5 is rounded up               */
	MPD_ROUND_HALF_DOWN,   /* 0.5 is rounded down             */
	MPD_ROUND_HALF_EVEN,   /* 0.5 is rounded to even          */
	MPD_ROUND_05UP,        /* round zero or five away from 0  */
	MPD_ROUND_TRUNC,       /* truncate, but set infinity      */
	MPD_ROUND_GUARD
};

enum { MPD_CLAMP_DEFAULT, MPD_CLAMP_IEEE_754, MPD_CLAMP_GUARD };

extern const char *mpd_round_string[MPD_ROUND_GUARD];
extern const char *mpd_clamp_string[MPD_CLAMP_GUARD];


typedef struct {
	mpd_ssize_t prec;   /* precision */
	mpd_ssize_t emax;   /* max positive exp */
	mpd_ssize_t emin;   /* min negative exp */
	uint32_t traps;     /* status events that should be trapped */
	uint32_t status;    /* status flags */
	uint32_t newtrap;   /* set by mpd_addstatus_raise() */
	int      round;     /* rounding mode */
	int      clamp;     /* clamp mode */
	int      allcr;     /* all functions correctly rounded */
} mpd_context_t;


/* Status flags */
#define MPD_Clamped             0x00000001U
#define MPD_Conversion_syntax   0x00000002U
#define MPD_Division_by_zero    0x00000004U
#define MPD_Division_impossible 0x00000008U
#define MPD_Division_undefined  0x00000010U
#define MPD_Fpu_error           0x00000020U
#define MPD_Inexact             0x00000040U
#define MPD_Invalid_context     0x00000080U
#define MPD_Invalid_operation   0x00000100U
#define MPD_Malloc_error        0x00000200U
#define MPD_Not_implemented     0x00000400U
#define MPD_Overflow            0x00000800U
#define MPD_Rounded             0x00001000U
#define MPD_Subnormal           0x00002000U
#define MPD_Underflow           0x00004000U
#define MPD_Max_status         (0x00008000U-1U)

/* Conditions that result in an IEEE 754 exception */
#define MPD_IEEE_Invalid_operation (MPD_Conversion_syntax |   \
                                    MPD_Division_impossible | \
                                    MPD_Division_undefined |  \
                                    MPD_Fpu_error |           \
                                    MPD_Invalid_context |     \
                                    MPD_Invalid_operation |   \
                                    MPD_Malloc_error)         \

/* Errors that require the result of an operation to be set to NaN */
#define MPD_Errors (MPD_IEEE_Invalid_operation | \
                    MPD_Division_by_zero)

/* Default traps */
#define MPD_Traps (MPD_IEEE_Invalid_operation | \
                   MPD_Division_by_zero |       \
                   MPD_Overflow |               \
                   MPD_Underflow)

/* Official name */
#define MPD_Insufficient_storage MPD_Malloc_error


#define MPD_MINALLOC_MIN 2
#define MPD_MINALLOC_MAX 64
extern mpd_ssize_t MPD_MINALLOC;
extern void (* mpd_traphandler)(mpd_context_t *);

void mpd_init(mpd_context_t *ctx, mpd_ssize_t prec);

void mpd_maxcontext(mpd_context_t *ctx);
void mpd_defaultcontext(mpd_context_t *ctx);
void mpd_basiccontext(mpd_context_t *ctx);
void mpd_extcontext(mpd_context_t *ctx);

mpd_ssize_t mpd_getprec(const mpd_context_t *ctx);
mpd_ssize_t mpd_getemax(const mpd_context_t *ctx);
mpd_ssize_t mpd_getemin(const mpd_context_t *ctx);
int mpd_getround(const mpd_context_t *ctx);
uint32_t mpd_gettraps(const mpd_context_t *ctx);
uint32_t mpd_getstatus(const mpd_context_t *ctx);
int mpd_getclamp(const mpd_context_t *ctx);
int mpd_getcr(const mpd_context_t *ctx);

int mpd_qsetprec(mpd_context_t *ctx, mpd_ssize_t prec);
int mpd_qsetemax(mpd_context_t *ctx, mpd_ssize_t emax);
int mpd_qsetemin(mpd_context_t *ctx, mpd_ssize_t emin);
int mpd_qsetround(mpd_context_t *ctx, int newround);
int mpd_qsettraps(mpd_context_t *ctx, uint32_t flags);
int mpd_qsetstatus(mpd_context_t *ctx, uint32_t flags);
int mpd_qsetclamp(mpd_context_t *ctx, int c);
int mpd_qsetcr(mpd_context_t *ctx, int c);
void mpd_addstatus_raise(mpd_context_t *ctx, uint32_t flags);


/******************************************************************************/
/*                           Decimal Arithmetic                               */
/******************************************************************************/

/* mpd_t flags */
#define MPD_POS                 ((uint8_t)0)
#define MPD_NEG                 ((uint8_t)1)
#define MPD_INF                 ((uint8_t)2)
#define MPD_NAN                 ((uint8_t)4)
#define MPD_SNAN                ((uint8_t)8)
#define MPD_SPECIAL (MPD_INF|MPD_NAN|MPD_SNAN)
#define MPD_STATIC              ((uint8_t)16)
#define MPD_STATIC_DATA         ((uint8_t)32)
#define MPD_SHARED_DATA         ((uint8_t)64)
#define MPD_CONST_DATA          ((uint8_t)128)
#define MPD_DATAFLAGS (MPD_STATIC_DATA|MPD_SHARED_DATA|MPD_CONST_DATA)

/* mpd_t */
typedef struct {
	uint8_t flags;
	mpd_ssize_t exp;
	mpd_ssize_t digits;
	mpd_ssize_t len;
	mpd_ssize_t alloc;
	mpd_uint_t *data;
} mpd_t;


typedef unsigned char uchar;
extern mpd_t mpd_ln10;


/******************************************************************************/
/*                       Quiet, thread-safe functions                         */
/******************************************************************************/

/* format specification */
typedef struct {
	mpd_ssize_t min_width; /* minimum field width */
	mpd_ssize_t prec;      /* fraction digits or significant digits */
	char type;             /* conversion specifier */
	char align;            /* alignment */
	char sign;             /* sign printing/alignment */
	char fill[5];          /* fill character */
	const char *dot;       /* decimal point */
	const char *sep;       /* thousands separator */
	const char *grouping;  /* grouping of digits */
} mpd_spec_t;

/* output to a string */
char *mpd_to_sci(const mpd_t *dec, int fmt);
char *mpd_to_eng(const mpd_t *dec, int fmt);
int mpd_parse_fmt_str(mpd_spec_t *spec, const char *fmt);
char * mpd_qformat_spec(const mpd_t *dec, mpd_spec_t *spec, const mpd_context_t *ctx, uint32_t *status);
char *mpd_qformat(const mpd_t *dec, const char *fmt, const mpd_context_t *ctx, uint32_t *status);

#define MPD_NUM_FLAGS 15
#define MPD_MAX_FLAG_STRING 208
#define MPD_MAX_FLAG_LIST (MPD_MAX_FLAG_STRING+18)
#define MPD_MAX_SIGNAL_LIST 121
int mpd_snprint_flags(char *dest, int nmemb, uint32_t flags);
int mpd_lsnprint_flags(char *dest, int nmemb, uint32_t flags, const char *flag_string[]);
int mpd_lsnprint_signals(char *dest, int nmemb, uint32_t flags, const char *signal_string[]);

/* output to a file */
void mpd_fprint(FILE *file, const mpd_t *dec);
void mpd_print(const mpd_t *dec);

/* assignment from a string */
void mpd_qset_string(mpd_t *dec, const char *s, const mpd_context_t *ctx, uint32_t *status);

/* set to NaN with error flags */
void mpd_seterror(mpd_t *result, uint32_t flags, uint32_t *status);
/* set a special with sign and type */
void mpd_setspecial(mpd_t *dec, uint8_t sign, uint8_t type);
/* set coefficient to zero or all nines */
void mpd_zerocoeff(mpd_t *result);
void mpd_qmaxcoeff(mpd_t *result, const mpd_context_t *ctx, uint32_t *status);

/* quietly assign a C integer type to an mpd_t */
void mpd_qset_ssize(mpd_t *result, mpd_ssize_t a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qset_i32(mpd_t *result, int32_t a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qset_i64(mpd_t *result, int64_t a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qset_uint(mpd_t *result, mpd_uint_t a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qset_u32(mpd_t *result, uint32_t a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qset_u64(mpd_t *result, uint64_t a, const mpd_context_t *ctx, uint32_t *status);

/* quietly assign a C integer type to an mpd_t with a static coefficient */
void mpd_qsset_ssize(mpd_t *result, mpd_ssize_t a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsset_i32(mpd_t *result, int32_t a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsset_i64(mpd_t *result, int64_t a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsset_uint(mpd_t *result, mpd_uint_t a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsset_u32(mpd_t *result, uint32_t a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsset_u64(mpd_t *result, uint64_t a, const mpd_context_t *ctx, uint32_t *status);

/* quietly get a C integer type from an mpd_t */
mpd_ssize_t mpd_qget_ssize(const mpd_t *dec, uint32_t *status);
int64_t mpd_qget_i64(const mpd_t *dec, uint32_t *status);
mpd_uint_t mpd_qget_uint(const mpd_t *dec, uint32_t *status);
uint64_t mpd_qget_u64(const mpd_t *dec, uint32_t *status);


/* quiet functions */
int mpd_qcheck_nan(mpd_t *nanresult, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
int mpd_qcheck_nans(mpd_t *nanresult, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qfinalize(mpd_t *result, const mpd_context_t *ctx, uint32_t *status);

const char * mpd_class(const mpd_t *a, const mpd_context_t *ctx);

int mpd_qcopy(mpd_t *result, const mpd_t *a,  uint32_t *status);
mpd_t *mpd_qncopy(const mpd_t *a);
int mpd_qcopy_abs(mpd_t *result, const mpd_t *a, uint32_t *status);
int mpd_qcopy_negate(mpd_t *result, const mpd_t *a, uint32_t *status);
int mpd_qcopy_sign(mpd_t *result, const mpd_t *a, const mpd_t *b, uint32_t *status);

void mpd_qand(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qinvert(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qlogb(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qor(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qscaleb(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qxor(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
int mpd_same_quantum(const mpd_t *a, const mpd_t *b);

void mpd_qrotate(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
int mpd_qshiftl(mpd_t *result, const mpd_t *a, mpd_ssize_t n, uint32_t *status);
mpd_uint_t mpd_qshiftr(mpd_t *result, const mpd_t *a, mpd_ssize_t n, uint32_t *status);
mpd_uint_t mpd_qshiftr_inplace(mpd_t *result, mpd_ssize_t n);
void mpd_qshift(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qshiftn(mpd_t *result, const mpd_t *a, mpd_ssize_t n, const mpd_context_t *ctx, uint32_t *status);

int mpd_qcmp(const mpd_t *a, const mpd_t *b, uint32_t *status);
int mpd_qcompare(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
int mpd_qcompare_signal(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
int mpd_cmp_total(const mpd_t *a, const mpd_t *b);
int mpd_cmp_total_mag(const mpd_t *a, const mpd_t *b);
int mpd_compare_total(mpd_t *result, const mpd_t *a, const mpd_t *b);
int mpd_compare_total_mag(mpd_t *result, const mpd_t *a, const mpd_t *b);

void mpd_qround_to_intx(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qround_to_int(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qfloor(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qceil(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);

void mpd_qabs(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qmax(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qmax_mag(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qmin(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qmin_mag(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qminus(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qplus(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qnext_minus(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qnext_plus(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qnext_toward(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qquantize(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qrescale(mpd_t *result, const mpd_t *a, mpd_ssize_t exp, const mpd_context_t *ctx, uint32_t *status);
void mpd_qreduce(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qadd(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qadd_ssize(mpd_t *result, const mpd_t *a, mpd_ssize_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qadd_i32(mpd_t *result, const mpd_t *a, int32_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qadd_i64(mpd_t *result, const mpd_t *a, int64_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qadd_uint(mpd_t *result, const mpd_t *a, mpd_uint_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qadd_u32(mpd_t *result, const mpd_t *a, uint32_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qadd_u64(mpd_t *result, const mpd_t *a, uint64_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsub(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsub_ssize(mpd_t *result, const mpd_t *a, mpd_ssize_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsub_i32(mpd_t *result, const mpd_t *a, int32_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsub_i64(mpd_t *result, const mpd_t *a, int64_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsub_uint(mpd_t *result, const mpd_t *a, mpd_uint_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsub_u32(mpd_t *result, const mpd_t *a, uint32_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsub_u64(mpd_t *result, const mpd_t *a, uint64_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qmul(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qmul_ssize(mpd_t *result, const mpd_t *a, mpd_ssize_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qmul_i32(mpd_t *result, const mpd_t *a, int32_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qmul_i64(mpd_t *result, const mpd_t *a, int64_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qmul_uint(mpd_t *result, const mpd_t *a, mpd_uint_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qmul_u32(mpd_t *result, const mpd_t *a, uint32_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qmul_u64(mpd_t *result, const mpd_t *a, uint64_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qfma(mpd_t *result, const mpd_t *a, const mpd_t *b, const mpd_t *c, const mpd_context_t *ctx, uint32_t *status);
void mpd_qdiv(mpd_t *q, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qdiv_ssize(mpd_t *result, const mpd_t *a, mpd_ssize_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qdiv_i32(mpd_t *result, const mpd_t *a, int32_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qdiv_i64(mpd_t *result, const mpd_t *a, int64_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qdiv_uint(mpd_t *result, const mpd_t *a, mpd_uint_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qdiv_u32(mpd_t *result, const mpd_t *a, uint32_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qdiv_u64(mpd_t *result, const mpd_t *a, uint64_t b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qdivint(mpd_t *q, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qrem(mpd_t *r, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qrem_near(mpd_t *r, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qdivmod(mpd_t *q, mpd_t *r, const mpd_t *a, const mpd_t *b, const mpd_context_t *ctx, uint32_t *status);
void mpd_qpow(mpd_t *result, const mpd_t *base, const mpd_t *exp, const mpd_context_t *ctx, uint32_t *status);
void mpd_qpowmod(mpd_t *result, const mpd_t *base, const mpd_t *exp, const mpd_t *mod, const mpd_context_t *ctx, uint32_t *status);
void mpd_qexp(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_update_ln10(mpd_ssize_t prec, uint32_t *status);
void mpd_qln(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qlog10(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qsqrt(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);
void mpd_qinvroot(mpd_t *result, const mpd_t *a, const mpd_context_t *ctx, uint32_t *status);


size_t mpd_sizeinbase(mpd_t *a, uint32_t base);
void mpd_qimport_u16(mpd_t *result, const uint16_t *srcdata, size_t srclen,
                     uint8_t srcsign, uint32_t srcbase,
                     const mpd_context_t *ctx, uint32_t *status);
void mpd_qimport_u32(mpd_t *result, const uint32_t *srcdata, size_t srclen,
                     uint8_t srcsign, uint32_t srcbase,
                     const mpd_context_t *ctx, uint32_t *status);
size_t mpd_qexport_u16(uint16_t *rdata, size_t rlen, uint32_t base,
                       const mpd_t *src, uint32_t *status);
size_t mpd_qexport_u32(uint32_t *rdata, size_t rlen, uint32_t base,
                       const mpd_t *src, uint32_t *status);


/******************************************************************************/
/*                       Get attributes of a decimal                          */
/******************************************************************************/

EXTINLINE mpd_ssize_t mpd_adjexp(const mpd_t *dec);
EXTINLINE mpd_ssize_t mpd_etiny(const mpd_context_t *ctx);
EXTINLINE mpd_ssize_t mpd_etop(const mpd_context_t *ctx);
EXTINLINE mpd_uint_t mpd_msword(const mpd_t *dec);
EXTINLINE int mpd_word_digits(mpd_uint_t word);
/* most significant digit of a word */
EXTINLINE mpd_uint_t mpd_msd(mpd_uint_t word);
/* least significant digit of a word */
EXTINLINE mpd_uint_t mpd_lsd(mpd_uint_t word);
/* coefficient size needed to store 'digits' */
EXTINLINE mpd_ssize_t mpd_digits_to_size(mpd_ssize_t digits);
/* number of digits in the exponent, undefined for MPD_SSIZE_MIN */
EXTINLINE int mpd_exp_digits(mpd_ssize_t exp);
EXTINLINE int mpd_iscanonical(const mpd_t *dec UNUSED);
EXTINLINE int mpd_isfinite(const mpd_t *dec);
EXTINLINE int mpd_isinfinite(const mpd_t *dec);
EXTINLINE int mpd_isinteger(const mpd_t *dec);
EXTINLINE int mpd_isnan(const mpd_t *dec);
EXTINLINE int mpd_isnegative(const mpd_t *dec);
EXTINLINE int mpd_ispositive(const mpd_t *dec);
EXTINLINE int mpd_isqnan(const mpd_t *dec);
EXTINLINE int mpd_issigned(const mpd_t *dec);
EXTINLINE int mpd_issnan(const mpd_t *dec);
EXTINLINE int mpd_isspecial(const mpd_t *dec);
EXTINLINE int mpd_iszero(const mpd_t *dec);
/* undefined for special numbers */
EXTINLINE int mpd_iszerocoeff(const mpd_t *dec);
EXTINLINE int mpd_isnormal(const mpd_t *dec, const mpd_context_t *ctx);
EXTINLINE int mpd_issubnormal(const mpd_t *dec, const mpd_context_t *ctx);
/* odd word */
EXTINLINE int mpd_isoddword(mpd_uint_t word);
/* odd coefficient */
EXTINLINE int mpd_isoddcoeff(const mpd_t *dec);
/* odd decimal, only defined for integers */
int mpd_isodd(const mpd_t *dec);
/* even decimal, only defined for integers */
int mpd_iseven(const mpd_t *dec);
/* 0 if dec is positive, 1 if dec is negative */
EXTINLINE uint8_t mpd_sign(const mpd_t *dec);
/* 1 if dec is positive, -1 if dec is negative */
EXTINLINE int mpd_arith_sign(const mpd_t *dec);
EXTINLINE long mpd_radix(void);
EXTINLINE int mpd_isdynamic(mpd_t *dec);
EXTINLINE int mpd_isstatic(mpd_t *dec);
EXTINLINE int mpd_isdynamic_data(mpd_t *dec);
EXTINLINE int mpd_isstatic_data(mpd_t *dec);
EXTINLINE int mpd_isshared_data(mpd_t *dec);
EXTINLINE int mpd_isconst_data(mpd_t *dec);
EXTINLINE mpd_ssize_t mpd_trail_zeros(const mpd_t *dec);


/******************************************************************************/
/*                       Set attributes of a decimal                          */
/******************************************************************************/

/* set number of decimal digits in the coefficient */
EXTINLINE void mpd_setdigits(mpd_t *result);
EXTINLINE void mpd_set_sign(mpd_t *result, uint8_t sign);
/* copy sign from another decimal */
EXTINLINE void mpd_signcpy(mpd_t *result, mpd_t *a);
EXTINLINE void mpd_set_infinity(mpd_t *result);
EXTINLINE void mpd_set_qnan(mpd_t *result);
EXTINLINE void mpd_set_snan(mpd_t *result);
EXTINLINE void mpd_set_negative(mpd_t *result);
EXTINLINE void mpd_set_positive(mpd_t *result);
EXTINLINE void mpd_set_dynamic(mpd_t *result);
EXTINLINE void mpd_set_static(mpd_t *result);
EXTINLINE void mpd_set_dynamic_data(mpd_t *result);
EXTINLINE void mpd_set_static_data(mpd_t *result);
EXTINLINE void mpd_set_shared_data(mpd_t *result);
EXTINLINE void mpd_set_const_data(mpd_t *result);
EXTINLINE void mpd_clear_flags(mpd_t *result);
EXTINLINE void mpd_set_flags(mpd_t *result, uint8_t flags);
EXTINLINE void mpd_copy_flags(mpd_t *result, const mpd_t *a);


/******************************************************************************/
/*                              Error Macros                                  */
/******************************************************************************/

enum {MPD_ERR_EXIT, MPD_ERR_WARN};
#define mpd_err_fatal(format, ...) \
	mpd_err_doit(MPD_ERR_EXIT, "%s:%d: error: " format, __FILE__, __LINE__, ##__VA_ARGS__)
#define mpd_err_warn(format, ...) \
	mpd_err_doit(MPD_ERR_WARN, "%s:%d: error: " format, __FILE__, __LINE__, ##__VA_ARGS__)

void mpd_err_doit(int action, const char *fmt, ...);


/******************************************************************************/
/*                            Memory handling                                 */
/******************************************************************************/

extern void *(* mpd_mallocfunc)(size_t size);
extern void *(* mpd_callocfunc)(size_t nmemb, size_t size);
extern void *(* mpd_reallocfunc)(void *ptr, size_t size);
extern void (* mpd_free)(void *ptr);

void *mpd_callocfunc_em(size_t nmemb, size_t size);

void *mpd_alloc(mpd_size_t nmemb, mpd_size_t size);
void *mpd_calloc(mpd_size_t nmemb, mpd_size_t size);
void *mpd_realloc(void *ptr, mpd_size_t nmemb, mpd_size_t size, uint8_t *err);
void *mpd_sh_alloc(mpd_size_t struct_size, mpd_size_t nmemb, mpd_size_t size);
void *mpd_sh_calloc(mpd_size_t struct_size, mpd_size_t nmemb, mpd_size_t size);
void *mpd_sh_realloc(void *ptr, mpd_size_t struct_size, mpd_size_t nmemb, mpd_size_t size, uint8_t *err);

mpd_t *mpd_qnew(void);
mpd_t *mpd_new(mpd_context_t *ctx);
mpd_t *mpd_qnew_size(mpd_ssize_t size);
void mpd_del(mpd_t *dec);

void mpd_uint_zero(mpd_uint_t *dest, mpd_size_t len);
int mpd_qresize(mpd_t *result, mpd_ssize_t size, uint32_t *status);
int mpd_qresize_zero(mpd_t *result, mpd_ssize_t size, uint32_t *status);
void mpd_minalloc(mpd_t *result);

int mpd_resize(mpd_t *result, mpd_ssize_t size, mpd_context_t *ctx);
int mpd_resize_zero(mpd_t *result, mpd_ssize_t size, mpd_context_t *ctx);


#ifdef __cplusplus
} /* END extern "C" */
#endif


#endif /* MPDECIMAL64_H */



