#include "Python.h"
#include "Python-ast.h"
#include "node.h"
#include "token.h"
#include "graminit.h"
#include "code.h"
#include "compile.h"
#include "symtable.h"

#define UNDEFINED_FUTURE_FEATURE "future feature %.100s is not defined"

static int
future_check_features(PyFutureFeatures *ff, PyObject *s, const char *filename)
{
	int i;
	PyObject *names;

	assert(stmt_kind(s) == ImportFrom_kind);

	names = ImportFrom_names(s);
	for (i = 0; i < PyList_GET_SIZE(names); i++) {
                PyObject *name = PyList_GET_ITEM(names, i);
		const char *feature = PyString_AsString(alias_name(name));
		if (!feature)
			return 0;
		if (strcmp(feature, FUTURE_NESTED_SCOPES) == 0) {
			continue;
		} else if (strcmp(feature, FUTURE_GENERATORS) == 0) {
			continue;
		} else if (strcmp(feature, FUTURE_DIVISION) == 0) {
			ff->ff_features |= CO_FUTURE_DIVISION;
		} else if (strcmp(feature, "braces") == 0) {
			PyErr_SetString(PyExc_SyntaxError,
					"not a chance");
			PyErr_SyntaxLocation(filename, ((struct _stmt*)s)->lineno);
			return 0;
		} else {
			PyErr_Format(PyExc_SyntaxError,
				     UNDEFINED_FUTURE_FEATURE, feature);
			PyErr_SyntaxLocation(filename, ((struct _stmt*)s)->lineno);
			return 0;
		}
	}
	return 1;
}

static int
future_parse(PyFutureFeatures *ff, PyObject *mod, const char *filename)
{
	int i, found_docstring = 0, done = 0, prev_line = 0;

	static PyObject *future;
	if (!future) {
		future = PyString_InternFromString("__future__");
		if (!future)
			return 0;
	}

	if (!(mod_kind(mod) == Module_kind || mod_kind(mod) == Interactive_kind))
		return 1;

	/* A subsequent pass will detect future imports that don't
	   appear at the beginning of the file.  There's one case,
	   however, that is easier to handl here: A series of imports
	   joined by semi-colons, where the first import is a future
	   statement but some subsequent import has the future form
	   but is preceded by a regular import.
	*/
	   

	for (i = 0; i < PyList_GET_SIZE(Module_body(mod)); i++) {
		PyObject *s = PyList_GET_ITEM(Module_body(mod), i);

		if (done && ((struct _stmt*)s)->lineno > prev_line)
			return 1;
		prev_line = ((struct _stmt*)s)->lineno;

		/* The tests below will return from this function unless it is
		   still possible to find a future statement.  The only things
		   that can precede a future statement are another future
		   statement and a doc string.
		*/

		if (stmt_kind(s) == ImportFrom_kind) {
			if (ImportFrom_module (s)== future) {
				if (done) {
					PyErr_SetString(PyExc_SyntaxError,
							ERR_LATE_FUTURE);
					PyErr_SyntaxLocation(filename, 
							     ((struct _stmt*)s)->lineno);
					return 0;
				}
				if (!future_check_features(ff, s, filename))
					return 0;
				ff->ff_lineno = ((struct _stmt*)s)->lineno;
			}
			else
				done = 1;
		}
		else if (stmt_kind(s) == Expr_kind && !found_docstring) {
			PyObject *e = Expr_value(s);
			if (stmt_kind(e) != Str_kind)
				done = 1;
			else
				found_docstring = 1;
		}
		else
			done = 1;
	}
	return 1;
}


PyFutureFeatures *
PyFuture_FromAST(PyObject *mod, const char *filename)
{
	PyFutureFeatures *ff;

	ff = (PyFutureFeatures *)PyMem_Malloc(sizeof(PyFutureFeatures));
	if (ff == NULL)
		return NULL;
	ff->ff_features = 0;
	ff->ff_lineno = -1;

	if (!future_parse(ff, mod, filename)) {
		PyMem_Free((void *)ff);
		return NULL;
	}
	return ff;
}
