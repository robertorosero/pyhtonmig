/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef MPDECIMAL_H
#define MPDECIMAL_H


#ifdef _MSC_VER
  #include "vccompat.h"
  #ifndef UNUSED
    #define UNUSED
  #endif
  #define EXTINLINE extern inline
#else
  #include "pyconfig.h"
  #ifdef HAVE_STDINT_H
    #include <stdint.h>
  #endif
  #ifdef HAVE_INTTYPES_H
    #include <inttypes.h>
  #endif
  #define __GNUC_STDC_INLINE__
  #define UNUSED __attribute__((unused))
  #define EXTINLINE
#endif


#if !defined(LEGACY_COMPILER)
  #if !defined(UINT64_MAX)
    /* XXX: This warning is temporary and can be removed at some point. */
    #error "Warning: Compiler without uint64_t. Comment out this line."
    #define LEGACY_COMPILER
  #endif
#endif


#if defined(CONFIG_64)
  #include "mpdecimal64.h"
#elif defined(CONFIG_32)
  #include "mpdecimal32.h"
#else
  #error "define CONFIG_64 or CONFIG_32"
#endif


#endif
