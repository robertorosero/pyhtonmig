#include "Python.h"
#include "Python-ast.h"
#include "node.h"
#include "token.h"
#include "graminit.h"
#include "code.h"
#include "compile.h"
#include "symtable.h"

#define UNDEFINED_FUTURE_FEATURE "future feature %.100s is not defined"
#define FUTURE_IMPORT_STAR "future statement does not support import *"

static int
future_check_features(PyFutureFeatures *ff, stmt_ty s, const char *filename)
{
	int i;
	const char *feature;
	asdl_seq *names;

	assert(s->kind == ImportFrom_kind);

	names = s->v.ImportFrom.names;
	for (i = 0; i < asdl_seq_LEN(names); i++) {
                alias_ty name = asdl_seq_GET(names, i);
		feature = PyString_AsString(name->name);
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
			PyErr_SyntaxLocation(filename, s->lineno);
			return 0;
		} else {
			PyErr_Format(PyExc_SyntaxError,
				     UNDEFINED_FUTURE_FEATURE, feature);
			PyErr_SyntaxLocation(filename, s->lineno);
			return 0;
		}
	}
	return 1;
}

int
future_parse(PyFutureFeatures *ff, mod_ty mod, const char *filename)
{
	int i;

	static PyObject *future;
	if (!future) {
		future = PyString_InternFromString("__future__");
		if (!future)
			return 0;
	}

	if (!(mod->kind == Module_kind || mod->kind == Interactive_kind))
		return 1;
	for (i = 0; i < asdl_seq_LEN(mod->v.Module.body); i++) {
		stmt_ty s = asdl_seq_GET(mod->v.Module.body, i);

		/* The tests below will return from this function unless it is
		   still possible to find a future statement.  The only things
		   that can precede a future statement are another future
		   statement and a doc string.
		*/

		if (s->kind == ImportFrom_kind) {
			if (s->v.ImportFrom.module == future) {
				if (!future_check_features(ff, s, filename))
					return 0;
			}
			else
				return 1;
		}
		else if (s->kind == Expr_kind) {
			expr_ty e = s->v.Expr.value;
			if (e->kind != Str_kind)
				return 1;
		}
		else
			return 1;
	}
	return 1;
}


PyFutureFeatures *
PyFuture_FromAST(mod_ty mod, const char *filename)
{
	PyFutureFeatures *ff;

	ff = (PyFutureFeatures *)PyMem_Malloc(sizeof(PyFutureFeatures));
	if (ff == NULL)
		return NULL;
	ff->ff_found_docstring = 0;
	ff->ff_last_lineno = -1;
	ff->ff_features = 0;

	if (!future_parse(ff, mod, filename)) {
		PyMem_Free((void *)ff);
		return NULL;
	}
	return ff;
}

