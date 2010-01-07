/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef MPTYPES_H
#define MPTYPES_H


#if defined(CONFIG_64)
  #if defined(_MSC_VER)
    #define PRI_size_t "llu"
    #define PRI_mpd_ssize_t "lld"
  #else
    #define PRI_size_t "lu"
    #define PRI_mpd_ssize_t "ld"
  #endif
  #define PRI_time_t "ld"
  #define CONV_mpd_ssize_t "L"
  #define ONE_UM 1ULL
#elif defined(CONFIG_32)
  #if defined (__OpenBSD__)
    #define PRI_size_t "lu"
    #define PRI_mpd_ssize_t "ld"
    #define PRI_time_t "d"
  #else
    #define PRI_size_t "u"
    #define PRI_mpd_ssize_t "d"
    #define PRI_time_t "ld"
  #endif
  #define CONV_mpd_ssize_t "l"
  #define ONE_UM 1UL
#else
  #error "define CONFIG_64 or CONFIG_32"
#endif


#endif


