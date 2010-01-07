/*
 * Copyright (c) 2008-2010 Stefan Krah. All Rights Reserved.
 * Licensed to PSF under a Contributor Agreement.
 */


#ifndef MPDECIMAL_H
#define MPDECIMAL_H

#if defined(CONFIG_64)
  #include "mpdecimal64.h"
#elif defined(CONFIG_32)
  #include "mpdecimal32.h"
#else
  #error "define CONFIG_64 or CONFIG_32"
#endif


#endif
