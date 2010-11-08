/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef MPTYPES_H
#define MPTYPES_H

#if defined(_MSC_VER)
  #define PRIi64 "lld"
  #define PRIu64 "llu"
  #define PRIi32 "d"
  #define PRIu32 "u"
#endif

#if defined(CONFIG_64)
  #if defined(_MSC_VER) || defined(__OpenBSD__)
    #define PRI_mpd_size_t "llu"
    #define PRI_mpd_ssize_t "lld"
  #else
    #define PRI_mpd_size_t "lu"
    #define PRI_mpd_ssize_t "ld"
  #endif
  #define PRI_time_t "ld"
  #define CONV_mpd_ssize_t "L"
  #define ONE_UM 1ULL
#elif defined(CONFIG_32)
  #define PRI_mpd_size_t "u"
  #define PRI_mpd_ssize_t "d"
  #if defined (__OpenBSD__)
    #define PRI_time_t "d"
  #elif defined(__FreeBSD__)
    #if defined(__x86_64__)
      #define PRI_time_t "ld"
    #else
      #define PRI_time_t "d"
    #endif
  #else
    #define PRI_time_t "ld"
  #endif
  #if MPD_SSIZE_MAX != INT_MAX
    #error "define CONV_mpd_ssize_t"
  #endif
  #define CONV_mpd_ssize_t "i"
  #define ONE_UM 1UL
#else
  #error "define CONFIG_64 or CONFIG_32"
#endif

#endif


