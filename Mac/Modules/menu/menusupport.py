# This script generates a Python interface for an Apple Macintosh Manager.
# It uses the "bgen" package to generate C code.
# The function specifications are generated by scanning the mamager's header file,
# using the "scantools" package (customized for this particular manager).

import string

# Declarations that change for each manager
MACHEADERFILE = 'Menus.h'		# The Apple header file
MODNAME = '_Menu'			# The name of the module
OBJECTNAME = 'Menu'			# The basic name of the objects used here

# The following is *usually* unchanged but may still require tuning
MODPREFIX = 'Menu'			# The prefix for module-wide routines
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
MenuItemID = Type("MenuItemID", "l")
MenuCommand = Type("MenuCommand", "l")
MenuAttributes = Type("MenuAttributes", "l")
MenuItemAttributes = Type("MenuItemAttributes", "l")
unsigned_char = Type('unsigned char', 'b')
FMFontFamily = Type("FMFontFamily", "h")
FMFontStyle = Type("FMFontStyle", "h")
CFStringRef = OpaqueByValueType("CFStringRef", "CFStringRefObj")
UniChar = Type("UniChar", "h")

includestuff = includestuff + """
#ifdef WITHOUT_FRAMEWORKS
#include <Devices.h> /* Defines OpenDeskAcc in universal headers */
#include <Menus.h>
#else
#include <Carbon/Carbon.h>
#endif


#ifdef USE_TOOLBOX_OBJECT_GLUE

extern PyObject *_MenuObj_New(MenuHandle);
extern int _MenuObj_Convert(PyObject *, MenuHandle *);

#define MenuObj_New _MenuObj_New
#define MenuObj_Convert _MenuObj_Convert 
#endif

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

initstuff = initstuff + """
	PyMac_INIT_TOOLBOX_OBJECT_NEW(MenuHandle, MenuObj_New);
	PyMac_INIT_TOOLBOX_OBJECT_CONVERT(MenuHandle, MenuObj_Convert);
"""

class MyObjectDefinition(GlobalObjectDefinition):
	pass

# Create the generator groups and link them
module = MacModule(MODNAME, MODPREFIX, includestuff, finalstuff, initstuff)
object = MyObjectDefinition(OBJECTNAME, OBJECTPREFIX, OBJECTTYPE)
module.addobject(object)

# Create the generator classes used to populate the lists
Function = OSErrWeakLinkFunctionGenerator
Method = OSErrWeakLinkMethodGenerator

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
