/***********************************************************
Copyright 1991, 1992, 1993, 1994 by Stichting Mathematisch Centrum,
Amsterdam, The Netherlands.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Stichting Mathematisch
Centrum or CWI not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior permission.

STICHTING MATHEMATISCH CENTRUM DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL STICHTING MATHEMATISCH CENTRUM BE LIABLE
FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

******************************************************************/

/* Imgformat objects */

#include "allobjects.h"
#include "modsupport.h"		/* For getargs() etc. */

typedef struct {
	OB_HEAD
	char    *name;
} imgformatobject;

staticforward typeobject Imgformattype;

#define is_imgformatobject(v)		((v)->ob_type == &Imgformattype)

static object *dict;			/* Dictionary of known types */

static imgformatobject *
newimgformatobject(name)
	char *name;
{
	imgformatobject *xp;
	xp = NEWOBJ(imgformatobject, &Imgformattype);
	if (xp == NULL)
		return NULL;
	if( (xp->name = malloc(strlen(name)+1)) == NULL ) {
	    DEL(xp);
	    return (imgformatobject *)err_nomem();
	}
	strcpy(xp->name, name);
	return xp;
}

/* Imgformat methods */

static void
imgformat_dealloc(xp)
	imgformatobject *xp;
{
	free(xp->name);
	DEL(xp);
}

static object *
imgformat_repr(self)
	imgformatobject *self;
{
	char buf[100];

	sprintf(buf, "<imgformat '%.70s' at %x>", self->name, self);
	return newstringobject(buf);
}

static struct methodlist imgformat_methods[] = {
	{NULL,		NULL}		/* sentinel */
};

static typeobject Imgformattype = {
	OB_HEAD_INIT(&Typetype)
	0,			/*ob_size*/
	"imgformat",		/*tp_name*/
	sizeof(imgformatobject), /*tp_basicsize*/
	0,			/*tp_itemsize*/
	/* methods */
	(destructor)imgformat_dealloc, /*tp_dealloc*/
	0,			/*tp_print*/
	0,			 /*tp_getattr*/
	0,			 /*tp_setattr*/
	0,			/*tp_compare*/
	(reprfunc)imgformat_repr, /*tp_repr*/
	0,			/*tp_as_number*/
	0,			/*tp_as_sequence*/
	0,			/*tp_as_mapping*/
	0,			/*tp_hash*/
};


static object *
imgformat_new(self, args)
	object *self; /* Not used */
	object *args;
{
	char *name, *descr;
	object *obj;
	
	if (!getargs(args, "(ss)", &name, &descr))
		return NULL;
	obj = (object *)newimgformatobject(descr);
	if (obj == NULL)
		return NULL;
	if (dictinsert(dict, name, obj) != 0) {
		DECREF(obj);
		return NULL;
	}
	return obj;
}

/* Helper function for other modules, to obtain imgformat-by-name */
object *
getimgformat(name)
	char *name;
{
	return dictlookup(dict, name);
}

/* List of functions defined in the module */

static struct methodlist imgformat_module_methods[] = {
	{"new",		imgformat_new},
	{NULL,		NULL}		/* sentinel */
};


/* Initialization function for the module (*must* be called initimgformat) */

void
initimgformat()
{
	object *m, *d, *x;

	/* Create the module and add the functions */
	m = initmodule("imgformat", imgformat_module_methods);

	dict = getmoduledict(m);

	x = (object *)newimgformatobject("SGI 32bit RGB(A) top-to-bottom");
	dictinsert(dict, "rgb", x);

	x = (object *)newimgformatobject("SGI 32bit RGB(A) bottom-to-top");
	dictinsert(dict, "rgb_b2t", x);

	x = (object *)newimgformatobject("SGI 3:3:2 RGB top-to-bottom");
	dictinsert(dict, "rgb8", x);

	x = (object *)newimgformatobject("SGI 3:3:2 RGB bottom-to-top");
	dictinsert(dict, "rgb8_b2t", x);

	x = (object *)newimgformatobject("SGI 8bit grey top-to-bottom");
	dictinsert(dict, "grey", x);

	x = (object *)newimgformatobject("SGI 8bit grey bottom-to-top");
	dictinsert(dict, "grey_b2t", x);

	x = (object *)newimgformatobject("SGI 8bit colormap top-to-bottom");
	dictinsert(dict, "colormap", x);

	x = (object *)newimgformatobject("SGI 8bit colormap bottom-to-top");
	dictinsert(dict, "colormap_b2t", x);


	/* Check for errors */
	if (err_occurred())
		fatal("can't initialize module imgformat");
}
