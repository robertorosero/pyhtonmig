# This script generates the Dialogs interface for Python.
# It uses the "bgen" package to generate C code.
# It execs the file dlggen.py which contain the function definitions
# (dlggen.py was generated by dlgscan.py, scanning the <Dialogs.h> header file).

from macsupport import *

# Create the type objects

DialogPtr = OpaqueByValueType("DialogPtr", "DlgObj")
DialogRef = DialogPtr

# An OptHandle is either a handle or None (in case NULL is passed in).
# This is needed for GetDialogItem().
OptHandle = OpaqueByValueType("Handle", "OptResObj")

ModalFilterProcPtr = InputOnlyType("PyObject*", "O")
ModalFilterProcPtr.passInput = lambda name: "Dlg_PassFilterProc(%s)" % name
ModalFilterUPP = ModalFilterProcPtr

RgnHandle = OpaqueByValueType("RgnHandle", "ResObj")
TEHandle = OpaqueByValueType("TEHandle", "ResObj")
CGrafPtr = OpaqueByValueType("CGrafPtr", "GrafObj")

DITLMethod = Type("DITLMethod", "h")
DialogItemIndex = Type("DialogItemIndex", "h")
DialogItemType = Type("DialogItemType", "h")
DialogItemIndexZeroBased = Type("DialogItemIndexZeroBased", "h")
AlertType = Type("AlertType", "h")
StringPtr = Str255
EventMask = Type("EventMask", "H")

includestuff = includestuff + """
#ifdef WITHOUT_FRAMEWORKS
#include <Dialogs.h>
#else
#include <Carbon/Carbon.h>
#endif

#ifdef USE_TOOLBOX_OBJECT_GLUE
extern PyObject *_DlgObj_New(DialogRef);
extern PyObject *_DlgObj_WhichDialog(DialogRef);
extern int _DlgObj_Convert(PyObject *, DialogRef *);

#define DlgObj_New _DlgObj_New
#define DlgObj_WhichDialog _DlgObj_WhichDialog
#define DlgObj_Convert _DlgObj_Convert
#endif

#if !ACCESSOR_CALLS_ARE_FUNCTIONS
#define GetDialogTextEditHandle(dlg) (((DialogPeek)(dlg))->textH)
#define SetPortDialogPort(dlg) SetPort(dlg)
#define GetDialogPort(dlg) ((CGrafPtr)(dlg))
#define GetDialogFromWindow(win) ((DialogRef)(win))
#endif

/* XXX Shouldn't this be a stack? */
static PyObject *Dlg_FilterProc_callback = NULL;

static pascal Boolean Dlg_UnivFilterProc(DialogPtr dialog,
                                         EventRecord *event,
                                         short *itemHit)
{
	Boolean rv;
	PyObject *args, *res;
	PyObject *callback = Dlg_FilterProc_callback;
	if (callback == NULL)
		return 0; /* Default behavior */
	Dlg_FilterProc_callback = NULL; /* We'll restore it when call successful */
	args = Py_BuildValue("O&O&", DlgObj_WhichDialog, dialog, PyMac_BuildEventRecord, event);
	if (args == NULL)
		res = NULL;
	else {
		res = PyEval_CallObject(callback, args);
		Py_DECREF(args);
	}
	if (res == NULL) {
		PySys_WriteStderr("Exception in Dialog Filter\\n");
		PyErr_Print();
		*itemHit = -1; /* Fake return item */
		return 1; /* We handled it */
	}
	else {
		Dlg_FilterProc_callback = callback;
		if (PyInt_Check(res)) {
			*itemHit = PyInt_AsLong(res);
			rv = 1;
		}
		else
			rv = PyObject_IsTrue(res);
	}
	Py_DECREF(res);
	return rv;
}

static ModalFilterUPP
Dlg_PassFilterProc(PyObject *callback)
{
	PyObject *tmp = Dlg_FilterProc_callback;
	static ModalFilterUPP UnivFilterUpp = NULL;
	
	Dlg_FilterProc_callback = NULL;
	if (callback == Py_None) {
		Py_XDECREF(tmp);
		return NULL;
	}
	Py_INCREF(callback);
	Dlg_FilterProc_callback = callback;
	Py_XDECREF(tmp);
	if ( UnivFilterUpp == NULL )
		UnivFilterUpp = NewModalFilterUPP(&Dlg_UnivFilterProc);
	return UnivFilterUpp;
}

static PyObject *Dlg_UserItemProc_callback = NULL;

static pascal void Dlg_UnivUserItemProc(DialogPtr dialog,
                                         short item)
{
	PyObject *args, *res;

	if (Dlg_UserItemProc_callback == NULL)
		return; /* Default behavior */
	Dlg_FilterProc_callback = NULL; /* We'll restore it when call successful */
	args = Py_BuildValue("O&h", DlgObj_WhichDialog, dialog, item);
	if (args == NULL)
		res = NULL;
	else {
		res = PyEval_CallObject(Dlg_UserItemProc_callback, args);
		Py_DECREF(args);
	}
	if (res == NULL) {
		PySys_WriteStderr("Exception in Dialog UserItem proc\\n");
		PyErr_Print();
	}
	Py_XDECREF(res);
	return;
}

#if 0
/*
** Treating DialogObjects as WindowObjects is (I think) illegal under Carbon.
** However, as they are still identical under MacOS9 Carbon this is a problem, even
** if we neatly call GetDialogWindow() at the right places: there's one refcon field
** and it points to the DialogObject, so WinObj_WhichWindow will smartly return the
** dialog object, and therefore we still don't have a WindowObject.
** I'll leave the chaining code in place for now, with this comment to warn the
** unsuspecting victims (i.e. me, probably, in a few weeks:-)
*/
extern PyMethodChain WinObj_chain;
#endif
"""

finalstuff = finalstuff + """
/* Return the WindowPtr corresponding to a DialogObject */
#if 0
WindowPtr
DlgObj_ConvertToWindow(PyObject *self)
{
	if ( DlgObj_Check(self) )
		return GetDialogWindow(((DialogObject *)self)->ob_itself);
	return NULL;
}
#endif
/* Return the object corresponding to the dialog, or None */

PyObject *
DlgObj_WhichDialog(DialogPtr d)
{
	PyObject *it;
	
	if (d == NULL) {
		it = Py_None;
		Py_INCREF(it);
	} else {
		WindowPtr w = GetDialogWindow(d);
		
		it = (PyObject *) GetWRefCon(w);
		if (it == NULL || ((DialogObject *)it)->ob_itself != d || !DlgObj_Check(it)) {
#if 0
			/* Should do this, but we don't have an ob_freeit for dialogs yet. */
			it = WinObj_New(w);
			((WindowObject *)it)->ob_freeit = NULL;
#else
			it = Py_None;
			Py_INCREF(it);
#endif
		} else {
			Py_INCREF(it);
		}
	}
	return it;
}
"""

initstuff = initstuff + """
	PyMac_INIT_TOOLBOX_OBJECT_NEW(DialogPtr, DlgObj_New);
	PyMac_INIT_TOOLBOX_OBJECT_NEW(DialogPtr, DlgObj_WhichDialog);
	PyMac_INIT_TOOLBOX_OBJECT_CONVERT(DialogPtr, DlgObj_Convert);
"""


# Define a class which specializes our object definition
class MyObjectDefinition(GlobalObjectDefinition):
	def __init__(self, name, prefix = None, itselftype = None):
		GlobalObjectDefinition.__init__(self, name, prefix, itselftype)
## This won't work in Carbon, so we disable it for all MacPythons:-(
## But see the comment above:-((
##		self.basechain = "&WinObj_chain"

	def outputInitStructMembers(self):
		GlobalObjectDefinition.outputInitStructMembers(self)
		Output("SetWRefCon(GetDialogWindow(itself), (long)it);")

	def outputCheckNewArg(self):
		Output("if (itself == NULL) return Py_None;")

	def outputCheckConvertArg(self):
		Output("if (v == Py_None) { *p_itself = NULL; return 1; }")
		Output("if (PyInt_Check(v)) { *p_itself = (DialogPtr)PyInt_AsLong(v);")
		Output("                      return 1; }")

	def outputCompare(self):
		Output()
		Output("static int %s_compare(%s *self, %s *other)", self.prefix, self.objecttype, self.objecttype)
		OutLbrace()
		Output("if ( self->ob_itself > other->ob_itself ) return 1;")
		Output("if ( self->ob_itself < other->ob_itself ) return -1;")
		Output("return 0;")
		OutRbrace()
		
	def outputHash(self):
		Output()
		Output("static int %s_hash(%s *self)", self.prefix, self.objecttype)
		OutLbrace()
		Output("return (int)self->ob_itself;")
		OutRbrace()
		
	def outputFreeIt(self, itselfname):
		Output("DisposeDialog(%s);", itselfname)

# Create the generator groups and link them
module = MacModule('Dlg', 'Dlg', includestuff, finalstuff, initstuff)
object = MyObjectDefinition('Dialog', 'DlgObj', 'DialogPtr')
module.addobject(object)

# Create the generator classes used to populate the lists
Function = OSErrFunctionGenerator
Method = OSErrMethodGenerator

# Create and populate the lists
functions = []
methods = []
execfile("dlggen.py")

# add the populated lists to the generator groups
for f in functions: module.add(f)
for f in methods: object.add(f)

# Some methods that are currently macro's in C, but will be real routines
# in MacOS 8.

##f = Method(ExistingWindowPtr, 'GetDialogWindow', (DialogRef, 'dialog', InMode))
##object.add(f)
##f = Method(SInt16, 'GetDialogDefaultItem', (DialogRef, 'dialog', InMode))
##object.add(f)
##f = Method(SInt16, 'GetDialogCancelItem', (DialogRef, 'dialog', InMode))
##object.add(f)
##f = Method(SInt16, 'GetDialogKeyboardFocusItem', (DialogRef, 'dialog', InMode))
##object.add(f)
f = Method(void, 'SetGrafPortOfDialog', (DialogRef, 'dialog', InMode), 
	condition='#if !TARGET_API_MAC_CARBON')
object.add(f)

setuseritembody = """
	PyObject *new = NULL;
	
	
	if (!PyArg_ParseTuple(_args, "|O", &new))
		return NULL;

	if (Dlg_UserItemProc_callback && new && new != Py_None) {
		PyErr_SetString(Dlg_Error, "Another UserItemProc is already installed");
		return NULL;
	}
	
	if (new == NULL || new == Py_None) {
		new = NULL;
		_res = Py_None;
		Py_INCREF(Py_None);
	} else {
		Py_INCREF(new);
		_res = Py_BuildValue("O&", ResObj_New, (Handle)NewUserItemUPP(Dlg_UnivUserItemProc));
	}
	
	Dlg_UserItemProc_callback = new;
	return _res;
"""
f = ManualGenerator("SetUserItemHandler", setuseritembody)
module.add(f)

# generate output
SetOutputFileName('Dlgmodule.c')
module.generate()
