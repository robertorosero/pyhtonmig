# This script will generate the Resources interface for Python.
# It uses the "bgen" package to generate C code.
# It execs the file resgen.py which contain the function definitions
# (resgen.py was generated by resscan.py, scanning the <Resources.h> header file).

from macsupport import *


class ResMixIn:

	def checkit(self):
		OutLbrace()
		Output("OSErr _err = ResError();")
		Output("if (_err != noErr) return PyMac_Error(_err);")
		OutRbrace()
		FunctionGenerator.checkit(self) # XXX

class ResFunction(ResMixIn, FunctionGenerator): pass
class ResMethod(ResMixIn, MethodGenerator): pass

# includestuff etc. are imported from macsupport

includestuff = includestuff + """
#include <Resources.h>
#include <string.h>

#define resNotFound -192 /* Can't include <Errors.h> because of Python's "errors.h" */

/* Function to dispose a resource, with a "normal" calling sequence */
static void
PyMac_AutoDisposeHandle(Handle h)
{
	DisposeHandle(h);
}
"""

finalstuff = finalstuff + """

/* Alternative version of ResObj_New, which returns None for null argument */
PyObject *OptResObj_New(itself)
	Handle itself;
{
	if (itself == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	return ResObj_New(itself);
}

OptResObj_Convert(v, p_itself)
	PyObject *v;
	Handle *p_itself;
{
	PyObject *tmp;
	
	if ( v == Py_None ) {
		*p_itself = NULL;
		return 1;
	}
	if (ResObj_Check(v))
	{
		*p_itself = ((ResourceObject *)v)->ob_itself;
		return 1;
	}
	/* If it isn't a resource yet see whether it is convertible */
	if ( (tmp=PyObject_CallMethod(v, "as_Resource", "")) ) {
		*p_itself = ((ResourceObject *)tmp)->ob_itself;
		Py_DECREF(tmp);
		return 1;
	}
	PyErr_Clear();
	PyErr_SetString(PyExc_TypeError, "Resource required");
	return 0;
}
"""

initstuff = initstuff + """
"""

module = MacModule('Res', 'Res', includestuff, finalstuff, initstuff)

getattrHookCode = """
if (strcmp(name, "size") == 0)
	return PyInt_FromLong(GetHandleSize(self->ob_itself));
if (strcmp(name, "data") == 0) {
	PyObject *res;
	char state;
	state = HGetState(self->ob_itself);
	HLock(self->ob_itself);
	res = PyString_FromStringAndSize(
		*self->ob_itself,
		GetHandleSize(self->ob_itself));
	HUnlock(self->ob_itself);
	HSetState(self->ob_itself, state);
	return res;
}
if (strcmp(name, "__members__") == 0)
	return Py_BuildValue("[ss]", "data", "size");
"""

setattrCode = """
static int
ResObj_setattr(self, name, value)
	ResourceObject *self;
	char *name;
	PyObject *value;
{
	char *data;
	long size;
	
	if (strcmp(name, "data") != 0 || value == NULL )
		return -1;
	if ( !PyString_Check(value) )
		return -1;
	size = PyString_Size(value);
	data = PyString_AsString(value);
	/* XXXX Do I need the GetState/SetState calls? */
	SetHandleSize(self->ob_itself, size);
	if ( MemError())
		return -1;
	HLock(self->ob_itself);
	memcpy((char *)*self->ob_itself, data, size);
	HUnlock(self->ob_itself);
	/* XXXX Should I do the Changed call immedeately? */
	return 0;
}
"""

class ResDefinition(GlobalObjectDefinition):

	def outputCheckNewArg(self):
		Output("if (itself == NULL) return PyMac_Error(resNotFound);")
		
	def outputCheckConvertArg(self):
		# if it isn't a resource we may be able to coerce it
		Output("if (!%s_Check(v))", self.prefix)
		OutLbrace()
		Output("PyObject *tmp;")
		Output('if ( (tmp=PyObject_CallMethod(v, "as_Resource", "")) )')
		OutLbrace()
		Output("*p_itself = ((ResourceObject *)tmp)->ob_itself;")
		Output("Py_DECREF(tmp);")
		Output("return 1;")
		OutRbrace()
		Output("PyErr_Clear();")
		OutRbrace()

	def outputGetattrHook(self):
		Output(getattrHookCode)
		
	def outputSetattr(self):
		Output(setattrCode)

	def outputStructMembers(self):
		GlobalObjectDefinition.outputStructMembers(self)
		Output("void (*ob_freeit)(%s ptr);", self.itselftype)
		
	def outputInitStructMembers(self):
		GlobalObjectDefinition.outputInitStructMembers(self)
		Output("it->ob_freeit = NULL;")
		
	def outputCleanupStructMembers(self):
		Output("if (self->ob_freeit && self->ob_itself)")
		OutLbrace()
		Output("self->ob_freeit(self->ob_itself);")
		OutRbrace()
		Output("self->ob_itself = NULL;")


resobject = ResDefinition('Resource', 'ResObj', 'Handle')
module.addobject(resobject)

functions = []
resmethods = []

execfile('resgen.py')
execfile('resedit.py')

for f in functions: module.add(f)
for f in resmethods: resobject.add(f)

SetOutputFileName('Resmodule.c')
module.generate()
