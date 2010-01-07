/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef BITS_H
#define BITS_H


#include <stdio.h>
#include "vccompat.h"


/* Check if n is a power of 2 */
static inline int
ispower2(size_t n)
{
	return n != 0 && (n & (n-1)) == 0;
}

/*
 * Returns the most significant bit position of n from 0 to 32 (64).
 * Caller has to make sure that n is not 0.
 */
static inline int
std_bsr(size_t n)
{
	int pos = 0;
	size_t tmp;

#ifdef CONFIG_64
	tmp = n >> 32;
	if (tmp != 0) { n = tmp; pos += 32; }
#endif
	tmp = n >> 16;
	if (tmp != 0) { n = tmp; pos += 16; }
	tmp = n >> 8;
	if (tmp != 0) { n = tmp; pos += 8; }
	tmp = n >> 4;
	if (tmp != 0) { n = tmp; pos += 4; }
	tmp = n >> 2;
	if (tmp != 0) { n = tmp; pos += 2; }
	tmp = n >> 1;
	if (tmp != 0) { n = tmp; pos += 1; }

	return pos + (int)n - 1;
}


/*
 * Returns the least significant bit position of n from 0 to 32 (64).
 * Caller has to make sure that n is not 0.
 */
static inline int
std_bsf(size_t n)
{
	int pos;

#ifdef CONFIG_64
	pos = 63;
	if (n & 0x00000000FFFFFFFFULL) { pos -= 32; } else { n >>= 32; }
	if (n & 0x000000000000FFFFULL) { pos -= 16; } else { n >>= 16; }
	if (n & 0x00000000000000FFULL) { pos -=  8; } else { n >>=  8; }
	if (n & 0x000000000000000FULL) { pos -=  4; } else { n >>=  4; }
	if (n & 0x0000000000000003ULL) { pos -=  2; } else { n >>=  2; }
	if (n & 0x0000000000000001ULL) { pos -=  1; }
#else
	pos = 31;
	if (n & 0x000000000000FFFFUL) { pos -= 16; } else { n >>= 16; }
	if (n & 0x00000000000000FFUL) { pos -=  8; } else { n >>=  8; }
	if (n & 0x000000000000000FUL) { pos -=  4; } else { n >>=  4; }
	if (n & 0x0000000000000003UL) { pos -=  2; } else { n >>=  2; }
	if (n & 0x0000000000000001UL) { pos -=  1; }
#endif
	return pos;
}


#ifdef __GNUC__
/*
 * Bit scan reverse.
 * Caller has to make sure that a is not 0.
 */
static inline int
x86_bsr(size_t a) 
{
	size_t retval;

	__asm__ (
#ifdef CONFIG_64
		"bsrq %1, %0\n\t"
#else
		"bsr %1, %0\n\t"
#endif
		:"=r" (retval)
		:"r" (a)
		:"cc"
	);

	return (int)retval;
}

/*
 * Bit scan forward.
 * Caller has to make sure that a is not 0.
 */
static inline int
x86_bsf(size_t a) 
{
	size_t retval;

	__asm__ (
#ifdef CONFIG_64
		"bsfq %1, %0"
#else
		"bsf %1, %0"
#endif
		:"=r" (retval)
		:"r" (a)
		:"cc"
	); 

	return (int)retval;
}
#endif /* __GNUC__ */


#ifdef _MSC_VER
#include <intrin.h>
/*
 * Bit scan reverse.
 * Caller has to make sure that a is not 0.
 */
static inline int __cdecl
x86_bsr(size_t a) 
{
	unsigned long retval;

#ifdef CONFIG_64
	_BitScanReverse64(&retval, a);
#else
	_BitScanReverse(&retval, a);
#endif

	return (int)retval;
}

/*
 * Bit scan forward.
 * Caller has to make sure that a is not 0.
 */
static inline int __cdecl
x86_bsf(size_t a) 
{
	unsigned long retval;

#ifdef CONFIG_64
	_BitScanForward64(&retval, a);
#else
	_BitScanForward(&retval, a);
#endif

	return (int)retval;
}
#endif /* _MSC_VER */


#endif /* BITS_H */



