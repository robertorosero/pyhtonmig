/* Return the copyright string.  This is updated manually. */

#include "Python.h"

static char cprt[] = 
"Copyright (c) Corporation for National Research Initiatives.\n\
Copyright 1991-1995 Stichting Mathematisch Centrum, Amsterdam.";

const char *
Py_GetCopyright()
{
	return cprt;
}
