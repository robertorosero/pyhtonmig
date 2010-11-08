/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef VCCOMPAT_H
#define VCCOMPAT_H


/* Visual C fixes: no stdint.h, no snprintf ...*/
#ifdef _MSC_VER
  #include "vcstdint.h"
  #undef inline
  #define inline __inline
  #undef random
  #define random rand
  #undef srandom
  #define srandom srand
  #undef snprintf
  #define snprintf sprintf_s
  #define HAVE_SNPRINTF
  #undef strncasecmp
  #define strncasecmp _strnicmp
  #undef strcasecmp
  #define strcasecmp _stricmp
  #undef strtoll
  #define strtoll _strtoi64
  #define strdup _strdup
#endif


#endif /* VCCOMPAT_H */



