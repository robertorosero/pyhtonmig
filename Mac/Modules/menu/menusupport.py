# This script generates a Python interface for an Apple Macintosh Manager.
# It uses the "bgen" package to generate C code.
# The function specifications are generated by scanning the mamager's header file,
# using the "scantools" package (customized for this particular manager).

import string

# Declarations that change for each manager
MACHEADERFILE = 'Menus.h'		# The Apple header file
MODNAME = 'Menu'			# The name of the module
OBJECTNAME = 'Menu'			# The basic name of the objects used here

# The following is *usually* unchanged but may still require tuning
MODPREFIX = MODNAME			# The prefix for module-wide routines
OBJECTTYPE = OBJECTNAME + 'Handle'	# The C type used to represent them
OBJECTPREFIX = MODPREFIX + 'Obj'	# The prefix for object methods
INPUTFILE = string.lower(MODPREFIX) + 'gen.py' # The file generated by the scanner
EXTRAFILE = string.lower(MODPREFIX) + 'edit.py' # A similar file but hand-made
OUTPUTFILE = MODNAME + "module.c"	# The file generated by this program

from macsupport import *

# Create the type objects

MenuHandle = OpaqueByValueType(OBJECTTYPE, OBJECTPREFIX)
MenuRef = MenuHandle
Handle = OpaqueByValueType("Handle", "ResObj")
MenuBarHandle = OpaqueByValueType("MenuBarHandle", "ResObj")
MenuID = Type("MenuID", "h")
MenuItemIndex = Type("MenuItemIndex", "h")
MenuCommand = Type("MenuCommand", "l")
MenuAttributes = Type("MenuAttributes", "l")
MenuItemAttributes = Type("MenuItemAttributes", "l")
unsigned_char = Type('unsigned char', 'b')
FMFontFamily = Type("FMFontFamily", "h")
FMFontStyle = Type("FMFontStyle", "h")

includestuff = includestuff + """
#include <Devices.h> /* Defines OpenDeskAcc in universal headers */
#include <%s>""" % MACHEADERFILE + """

#if !ACCESSOR_CALLS_ARE_FUNCTIONS
#define GetMenuID(menu) ((*(menu))->menuID)
#define GetMenuWidth(menu) ((*(menu))->menuWidth)
#define GetMenuHeight(menu) ((*(menu))->menuHeight)

#define SetMenuID(menu, id) ((*(menu))->menuID = (id))
#define SetMenuWidth(menu, width) ((*(menu))->menuWidth = (width))
#define SetMenuHeight(menu, height) ((*(menu))->menuHeight = (height))
#endif

#define as_Menu(h) ((MenuHandle)h)
#define as_Resource(h) ((Handle)h)
"""

class MyObjectDefinition(GlobalObjectDefinition):
	pass

# Create the generator groups and link them
module = MacModule(MODNAME, MODPREFIX, includestuff, finalstuff, initstuff)
object = MyObjectDefinition(OBJECTNAME, OBJECTPREFIX, OBJECTTYPE)
module.addobject(object)

# Create the generator classes used to populate the lists
Function = OSErrFunctionGenerator
Method = OSErrMethodGenerator

# Create and populate the lists
functions = []
methods = []
execfile(INPUTFILE)
execfile(EXTRAFILE)

# add the populated lists to the generator groups
for f in functions: module.add(f)
for f in methods: object.add(f)

# generate output (open the output file as late as possible)
SetOutputFileName(OUTPUTFILE)
module.generate()
