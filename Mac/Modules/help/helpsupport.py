# This script generates a Python interface for an Apple Macintosh Manager.
# It uses the "bgen" package to generate C code.
# The function specifications are generated by scanning the mamager's header file,
# using the "scantools" package (customized for this particular manager).

import string

# Declarations that change for each manager
MODNAME = '_Help'				# The name of the module
OBJECTNAME = 'UNUSED'			# The basic name of the objects used here
KIND = 'Record'				# Usually 'Ptr' or 'Handle'

# The following is *usually* unchanged but may still require tuning
MODPREFIX = 'Help'			# The prefix for module-wide routines
OBJECTTYPE = OBJECTNAME + KIND		# The C type used to represent them
OBJECTPREFIX = MODPREFIX + 'Obj'	# The prefix for object methods
INPUTFILE = string.lower(MODPREFIX) + 'gen.py' # The file generated by the scanner
OUTPUTFILE = MODNAME + "module.c"	# The file generated by this program

from macsupport import *

# Create the type objects
MenuRef = OpaqueByValueType("MenuRef", "MenuObj")
MenuItemIndex = Type("MenuItemIndex", "H")

#WindowPeek = OpaqueByValueType("WindowPeek", OBJECTPREFIX)

#RgnHandle = FakeType("(RgnHandle)0")
# XXXX Should be next, but this will break a lot of code...
# RgnHandle = OpaqueByValueType("RgnHandle", "OptResObj")

#KeyMap = ArrayOutputBufferType("KeyMap")
##MacOSEventKind = Type("MacOSEventKind", "h") # Old-style
##MacOSEventMask = Type("MacOSEventMask", "h") # Old-style
#EventMask = Type("EventMask", "H")
#EventKind = Type("EventKind", "H")

includestuff = includestuff + """
#ifndef PyDoc_STR
#define PyDoc_STR(x) (x)
#endif
#ifdef WITHOUT_FRAMEWORKS
#include <MacHelp.h>
#else
#include <Carbon/Carbon.h>
#endif
"""

class MyObjectDefinition(GlobalObjectDefinition):
	def outputCheckNewArg(self):
		Output("if (itself == NULL) return PyMac_Error(resNotFound);")
	def outputCheckConvertArg(self):
		OutLbrace("if (DlgObj_Check(v))")
		Output("*p_itself = ((WindowObject *)v)->ob_itself;")
		Output("return 1;")
		OutRbrace()
		Out("""
		if (v == Py_None) { *p_itself = NULL; return 1; }
		if (PyInt_Check(v)) { *p_itself = (WindowPtr)PyInt_AsLong(v); return 1; }
		""")

# From here on it's basically all boiler plate...

# Create the generator groups and link them
module = MacModule(MODNAME, MODPREFIX, includestuff, finalstuff, initstuff)
##object = MyObjectDefinition(OBJECTNAME, OBJECTPREFIX, OBJECTTYPE)
##module.addobject(object)

# Create the generator classes used to populate the lists
Function = OSErrFunctionGenerator
##Method = OSErrMethodGenerator

# Create and populate the lists
functions = []
##methods = []
execfile(INPUTFILE)

# add the populated lists to the generator groups
# (in a different wordl the scan program would generate this)
for f in functions: module.add(f)
##for f in methods: object.add(f)

# generate output (open the output file as late as possible)
SetOutputFileName(OUTPUTFILE)
module.generate()

