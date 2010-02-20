/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef MPDECIMAL_H
#define MPDECIMAL_H


#include "pyconfig.h"


#ifdef _MSC_VER
  #include "vccompat.h"
  #ifndef UNUSED
    #define UNUSED
  #endif
  #define EXTINLINE extern inline
#else
  #if HAVE_STDINT_H
    #include <stdint.h>
  #endif
  #define __GNUC_STDC_INLINE__
  #define UNUSED __attribute__((unused))
  #define EXTINLINE
#endif

#if !defined(LEGACY_COMPILER)
  #if !defined(UINT64_MAX)
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
