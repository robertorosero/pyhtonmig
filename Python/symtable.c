#include "Python.h"
#include "Python-ast.h"
#include "code.h"
#include "compile.h"
#include "symtable.h"
#include "structmember.h"

PySTEntryObject *
PySTEntry_New(struct symtable *st, identifier name, block_ty block,
	      void *key, int lineno)
{
	PySTEntryObject *ste = NULL;
	PyObject *k, *v;

	k = PyLong_FromVoidPtr(key);
	if (k == NULL)
		goto fail;
	ste = (PySTEntryObject *)PyObject_New(PySTEntryObject,
					      &PySTEntry_Type);
	ste->ste_table = st;
	ste->ste_id = k;
	ste->ste_tmpname = 0;

	ste->ste_name = name;
	Py_INCREF(name);
	
	v = PyDict_New();
	if (v == NULL)
	    goto fail;
	ste->ste_symbols = v;

	v = PyList_New(0);
	if (v == NULL)
	    goto fail;
	ste->ste_varnames = v;

	v = PyList_New(0);
	if (v == NULL)
	    goto fail;
	ste->ste_children = v;

	ste->ste_type = block;
	ste->ste_optimized = block == FunctionBlock;
	ste->ste_varargs = 0;
	ste->ste_varkeywords = 0;
	ste->ste_opt_lineno = 0;
	ste->ste_lineno = lineno;

	ste->ste_nested = 0;
	if (st->st_cur != NULL &&
	    (st->st_cur->ste_nested ||
	     st->st_cur->ste_type == FunctionBlock))
		ste->ste_nested = 1;
	ste->ste_child_free = 0;
	ste->ste_generator = 0;

	if (PyDict_SetItem(st->st_symbols, ste->ste_id, (PyObject *)ste) < 0)
	    goto fail;
	
	return ste;
 fail:
	Py_XDECREF(ste);
	return NULL;
}

static PyObject *
ste_repr(PySTEntryObject *ste)
{
	char buf[256];

	PyOS_snprintf(buf, sizeof(buf),
		      "<symtable entry %.100s(%ld), line %d>",
		      PyString_AS_STRING(ste->ste_name),
		      PyInt_AS_LONG(ste->ste_id), ste->ste_lineno);
	return PyString_FromString(buf);
}

static void
ste_dealloc(PySTEntryObject *ste)
{
	ste->ste_table = NULL;
	Py_XDECREF(ste->ste_id);
	Py_XDECREF(ste->ste_name);
	Py_XDECREF(ste->ste_symbols);
	Py_XDECREF(ste->ste_varnames);
	Py_XDECREF(ste->ste_children);
	PyObject_Del(ste);
}

#define OFF(x) offsetof(PySTEntryObject, x)

static PyMemberDef ste_memberlist[] = {
	{"id",       T_OBJECT, OFF(ste_id), READONLY},
	{"name",     T_OBJECT, OFF(ste_name), READONLY},
	{"symbols",  T_OBJECT, OFF(ste_symbols), READONLY},
	{"varnames", T_OBJECT, OFF(ste_varnames), READONLY},
	{"children", T_OBJECT, OFF(ste_children), READONLY},
	{"type",     T_INT,    OFF(ste_type), READONLY},
	{"lineno",   T_INT,    OFF(ste_lineno), READONLY},
	{NULL}
};

PyTypeObject PySTEntry_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"symtable entry",
	sizeof(PySTEntryObject),
	0,
	(destructor)ste_dealloc,                /* tp_dealloc */
	0,                                      /* tp_print */
	0,			               /* tp_getattr */
	0,					/* tp_setattr */
	0,			                /* tp_compare */
	(reprfunc)ste_repr,			/* tp_repr */
	0,					/* tp_as_number */
	0,			                /* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	0,					/* tp_call */
	0,					/* tp_str */
	PyObject_GenericGetAttr,		/* tp_getattro */
	0,					/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,	                /* tp_flags */
 	0,					/* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	0,					/* tp_methods */
	ste_memberlist,				/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	0,					/* tp_init */
	0,					/* tp_alloc */
	0,					/* tp_new */
};

static int symtable_analyze(struct symtable *st);
static int symtable_enter_block(struct symtable *st, identifier name, 
				block_ty block, void *ast, int lineno);
static int symtable_exit_block(struct symtable *st, void *ast);
static int symtable_visit_stmt(struct symtable *st, stmt_ty s);
static int symtable_visit_expr(struct symtable *st, expr_ty s);
static int symtable_visit_arguments(struct symtable *st, arguments_ty);
static int symtable_visit_excepthandler(struct symtable *st, excepthandler_ty);
static int symtable_visit_alias(struct symtable *st, alias_ty);
static int symtable_visit_listcomp(struct symtable *st, listcomp_ty);
static int symtable_visit_keyword(struct symtable *st, keyword_ty);
static int symtable_visit_slice(struct symtable *st, slice_ty);

static identifier top = NULL, lambda = NULL;

#define GET_IDENTIFIER(VAR) \
	((VAR) ? (VAR) : ((VAR) = PyString_InternFromString(# VAR)))

#define DUPLICATE_ARGUMENT \
"duplicate argument '%s' in function definition"

static struct symtable *
symtable_new(void)
{
	struct symtable *st;

	st = (struct symtable *)PyMem_Malloc(sizeof(struct symtable));
	if (st == NULL)
		return NULL;

	st->st_filename = NULL;
	if ((st->st_stack = PyList_New(0)) == NULL)
		goto fail;
	if ((st->st_symbols = PyDict_New()) == NULL)
		goto fail; 
	st->st_cur = NULL;
	st->st_tmpname = 0;
	st->st_private = NULL;
	return st;
 fail:
	PySymtable_Free(st);
	return NULL;
}

struct symtable *
PySymtable_Build(mod_ty mod, const char *filename, PyFutureFeatures *future)
{
	struct symtable *st = symtable_new();
	asdl_seq *seq;
	int i;

	if (st == NULL)
		return st;
	st->st_filename = filename;
	st->st_future = future;
	symtable_enter_block(st, GET_IDENTIFIER(top), ModuleBlock, 
			     (void *)mod, 0);
	st->st_top = st->st_cur;
	/* Any other top-level initialization? */
	switch (mod->kind) {
	case Module_kind:
		seq = mod->v.Module.body;
		for (i = 0; i < asdl_seq_LEN(seq); i++)
			if (!symtable_visit_stmt(st, asdl_seq_GET(seq, i)))
				goto error;
		break;
	case Expression_kind: 
		if (!symtable_visit_expr(st, mod->v.Expression.body))
			goto error;
		break;
	case Interactive_kind:
		seq = mod->v.Interactive.body;
		for (i = 0; i < asdl_seq_LEN(seq); i++)
			if (!symtable_visit_stmt(st, asdl_seq_GET(seq, i)))
				goto error;
		break;
	case Suite_kind:
		PyErr_SetString(PyExc_RuntimeError,
				"this compiler does not handle Suites");
		return NULL;
	}
	symtable_exit_block(st, (void *)mod);
	if (symtable_analyze(st))
		return st;
 error:
	PySymtable_Free(st);
	return NULL;
}

void
PySymtable_Free(struct symtable *st)
{
	Py_XDECREF(st->st_symbols);
	Py_XDECREF(st->st_stack);
	PyMem_Free((void *)st);
}

PySTEntryObject *
PySymtable_Lookup(struct symtable *st, void *key)
{
	PyObject *k, *v;

	k = PyLong_FromVoidPtr(key);
	if (k == NULL)
		return NULL;
	v = PyDict_GetItem(st->st_symbols, k);
	if (v) {
		assert(PySTEntry_Check(v));
		Py_DECREF(k);
		Py_INCREF(v);
		return (PySTEntryObject *)v;
	}
	else {
		PyErr_SetString(PyExc_KeyError,
				"unknown symbol table entry");
		return NULL;
	}
}

int 
PyST_GetScope(PySTEntryObject *ste, PyObject *name)
{
	PyObject *v = PyDict_GetItem(ste->ste_symbols, name);
	if (!v)
		return 0;
	assert(PyInt_Check(v));
	return (PyInt_AS_LONG(v) >> SCOPE_OFF) & SCOPE_MASK;
}


/* Analyze raw symbol information to determine scope of each name.

   The next several functions are helpers for PySymtable_Analyze(),
   which determines whether a name is local, global, or free.  In addition, 
   it determines which local variables are cell variables; they provide
   bindings that are used for free variables in enclosed blocks.  

   There are also two kinds of free variables, implicit and explicit.  An 
   explicit global is declared with the global statement.  An implicit
   global is a free variable for which the compiler has found no binding
   in an enclosing function scope.  The implicit global is either a global
   or a builtin.  Python's module and class blocks use the xxx_NAME opcodes
   to handle these names to implement slightly odd semantics.  In such a
   block, the name is treated as global until it is assigned to; then it
   is treated as a local.

   The symbol table requires two passes to determine the scope of each name.
   The first pass collects raw facts from the AST: the name is a parameter 
   here, the name is used by not defined here, etc.  The second pass analyzes
   these facts during a pass over the PySTEntryObjects created during pass 1.

   When a function is entered during the second pass, the parent passes
   the set of all name bindings visible to its children.  These bindings 
   are used to determine if the variable is free or an implicit global.
   After doing the local analysis, it analyzes each of its child blocks
   using an updated set of name bindings.  

   The children update the free variable set.  If a local variable is free 
   in a child, the variable is marked as a cell.  The current function must 
   provide runtime storage for the variable that may outlive the function's 
   frame.  Cell variables are removed from the free set before the analyze
   function returns to its parent.
   
   The sets of bound and free variables are implemented as dictionaries
   mapping strings to None.
*/

#define SET_SCOPE(DICT, NAME, I) { \
	PyObject *o = PyInt_FromLong(I); \
	if (!o) \
		return 0; \
	if (PyDict_SetItem((DICT), (NAME), o) < 0) \
		return 0; \
}

/* XXX handle this:
   def f(x):
       global y
       def g():
           return x + y
   so that y is a global
*/

static int 
analyze_name(PyObject *dict, PyObject *name, int flags, PyObject *bound,
	     PyObject *local, PyObject *free, int nested)
{
	if (flags & DEF_GLOBAL) {
		if (flags & DEF_PARAM)
			return 0; /* can't declare a parameter as global */
		SET_SCOPE(dict, name, GLOBAL_EXPLICIT);
		return 1;
	}
	if (flags & DEF_BOUND) {
		SET_SCOPE(dict, name, LOCAL);
		if (PyDict_SetItem(local, name, Py_None) < 0)
			return 0;
		return 1;
	}
	/* If the function is nested, then it can have free vars. */
	if (nested && bound && PyDict_GetItem(bound, name)) {
		SET_SCOPE(dict, name, FREE);
		if (PyDict_SetItem(free, name, Py_None) < 0)
			return 0;
		return 1;
	}
	else {
		SET_SCOPE(dict, name, GLOBAL_IMPLICIT);
		return 1;
	}
	return 0; /* Can't get here */
}

#undef SET_SCOPE

/* If a name is defined in free and also in locals, then this block
   provides the binding for the free variable.  The name should be
   marked CELL in this block and removed from the free list.

   Note that the current block's free variables are included in free.
   That's safe because no name can be free and local in the same scope.
*/

static int
analyze_cells(PyObject *scope, PyObject *free)
{
	PyObject *name, *v, *w;
	int flags, pos = 0, success = 0;

	w = PyInt_FromLong(CELL);
	if (!w)
		return 0;
	while (PyDict_Next(scope, &pos, &name, &v)) {
		assert(PyInt_Check(v));
		flags = PyInt_AS_LONG(v);
		if (flags != LOCAL)
			continue;
		if (!PyDict_GetItem(free, name))
			continue;
		/* Replace LOCAL with CELL for this name, and remove
		   from free. It is safe to replace the value of name 
		   in the dict, because it will not cause a resize.
		 */
		if (PyDict_SetItem(scope, name, w) < 0)
			goto error;
		if (!PyDict_DelItem(free, name) < 0)
			goto error;
	}
	success = 1;
 error:
	Py_DECREF(w);
	return success;
}

/* Enter the final scope information into the st_symbols dict. */
static int
update_symbols(PyObject *symbols, PyObject *scope)
{
	PyObject *name, *v, *u, *w;
	int i, flags, pos = 0;

	while (PyDict_Next(symbols, &pos, &name, &v)) {
		assert(PyInt_Check(v));
		flags = PyInt_AS_LONG(v);
		w = PyDict_GetItem(scope, name);
		assert(w && PyInt_Check(w));
		i = PyInt_AS_LONG(w);
		flags |= (i << SCOPE_OFF);
		u = PyInt_FromLong(flags);
		if (PyDict_SetItem(symbols, name, u) < 0) {
			Py_DECREF(u);
			return 0;
		}
		Py_DECREF(u);
	}
	return 1;
}   

static int
analyze_block(PySTEntryObject *ste, PyObject *bound, PyObject *free)
{
	PyObject *name, *v, *local = NULL, *scope = NULL, *newbound = NULL;
	int i, flags, pos = 0, success = 0;

	local = PyDict_New();
	if (!local)
		goto error;
	scope = PyDict_New();
	if (!scope)
		goto error;

	assert(PySTEntry_Check(ste));
	assert(PyDict_Check(ste->ste_symbols));
	while (PyDict_Next(ste->ste_symbols, &pos, &name, &v)) {
		flags = PyInt_AS_LONG(v);
		if (!analyze_name(scope, name, flags, bound, local, free,
				  ste->ste_nested))
			goto error;
	}

	/* create a new bound dictionary to pass to children */
	newbound = PyDict_New();
	if (!newbound)
		goto error;
	if (ste->ste_type != ClassBlock) {
		if (PyDict_Update(newbound, local) < 0)
			goto error;
	}
	if (bound) {
		if (PyDict_Update(newbound, bound) < 0)
			goto error;
	}

	for (i = 0; i < PyList_GET_SIZE(ste->ste_children); ++i) {
		PyObject *c = PyList_GET_ITEM(ste->ste_children, i);
		assert(c && PySTEntry_Check(c));
		if (!analyze_block((PySTEntryObject *)c, local, free))
			goto error;
	}

	if (!analyze_cells(scope, free))
		goto error;
	if (!update_symbols(ste->ste_symbols, scope))
		goto error;
	success = 1;
 error:
	Py_XDECREF(local);
	Py_XDECREF(scope);
	Py_XDECREF(newbound);
	if (!success)
		assert(PyErr_Occurred());
	return success;
}

static int
symtable_analyze(struct symtable *st)
{
	PyObject *free;
	int r;

	free = PyDict_New();
	if (!free)
		return 0;
	r = analyze_block(st->st_top, NULL, free);
	Py_DECREF(free);
	return r;
}


/* symtable_enter_block() gets a reference via PySTEntry_New().
   This reference is released when the block is exited, via the DECREF
   in symtable_exit_block().
*/

static int
symtable_exit_block(struct symtable *st, void *ast)
{
	int end;

	Py_DECREF(st->st_cur);
	end = PyList_GET_SIZE(st->st_stack) - 1;
	if (end >= 0) {
		st->st_cur = (PySTEntryObject *)PyList_GET_ITEM(st->st_stack, 
								end);
		if (PySequence_DelItem(st->st_stack, end) < 0)
			return 0;
	}
	return 1;
}

static int
symtable_enter_block(struct symtable *st, identifier name, block_ty block, 
		     void *ast, int lineno)
{
	PySTEntryObject *prev = NULL;

	if (st->st_cur) {
		prev = st->st_cur;
		if (PyList_Append(st->st_stack, (PyObject *)st->st_cur) < 0) {
			Py_DECREF(st->st_cur);
			return 0;
		}
	}
	st->st_cur = PySTEntry_New(st, name, block, ast, lineno);
	if (name == GET_IDENTIFIER(top))
		st->st_global = st->st_cur->ste_symbols;
	if (prev) {
		if (PyList_Append(prev->ste_children, 
				  (PyObject *)st->st_cur) < 0) {
			return 0;
		}
	}
	return 1;
}

static int
symtable_add_def(struct symtable *st, PyObject *name, int flag) 
{
	PyObject *o;
	PyObject *dict;
	int val;

	/* XXX must always be called with mangled names. */

	dict = st->st_cur->ste_symbols;
	if ((o = PyDict_GetItem(dict, name))) {
	    val = PyInt_AS_LONG(o);
	    if ((flag & DEF_PARAM) && (val & DEF_PARAM)) {
		    PyErr_Format(PyExc_SyntaxError, DUPLICATE_ARGUMENT,
				 PyString_AsString(name));
		    PyErr_SyntaxLocation(st->st_filename,
				       st->st_cur->ste_lineno);
		    return 0;
	    }
	    val |= flag;
	} else
	    val = flag;
	o = PyInt_FromLong(val);
        if (o == NULL)
            return 0;
	if (PyDict_SetItem(dict, name, o) < 0) {
		Py_DECREF(o);
		return 0;
	}
	Py_DECREF(o);

	if (flag & DEF_PARAM) {
		if (PyList_Append(st->st_cur->ste_varnames, name) < 0) 
			return 0;
	} else	if (flag & DEF_GLOBAL) {
		/* XXX need to update DEF_GLOBAL for other flags too;
		   perhaps only DEF_FREE_GLOBAL */
		val = flag;
		if ((o = PyDict_GetItem(st->st_global, name))) {
			val |= PyInt_AS_LONG(o);
		}
		o = PyInt_FromLong(val);
		if (o == NULL)
			return 0;
		if (PyDict_SetItem(st->st_global, name, o) < 0) {
			Py_DECREF(o);
			return 0;
		}
		Py_DECREF(o);
	}
	return 1;
}

/* VISIT and VISIT_SEQ takes an ASDL type as their second argument.  They use
   the ASDL name to synthesize the name of the C type and the visit function. 
*/

#define VISIT(ST, TYPE, V) \
	if (!symtable_visit_ ## TYPE((ST), (V))) \
		return 0; 
						    
#define VISIT_SEQ(ST, TYPE, SEQ) { \
	int i; \
	asdl_seq *seq = (SEQ); /* avoid variable capture */ \
	for (i = 0; i < asdl_seq_LEN(seq); i++) { \
		TYPE ## _ty elt = asdl_seq_GET(seq, i); \
		if (!symtable_visit_ ## TYPE((ST), elt)) \
			return 0; \
	} \
}
						    
static int
symtable_visit_stmt(struct symtable *st, stmt_ty s)
{
	switch (s->kind) {
        case FunctionDef_kind:
		if (!symtable_add_def(st, s->v.FunctionDef.name, DEF_LOCAL))
			return 0;
		if (s->v.FunctionDef.args->defaults)
			VISIT_SEQ(st, expr, s->v.FunctionDef.args->defaults);
		if (!symtable_enter_block(st, s->v.FunctionDef.name, 
					  FunctionBlock, (void *)s, s->lineno))
			return 0;
		VISIT(st, arguments, s->v.FunctionDef.args);
		VISIT_SEQ(st, stmt, s->v.FunctionDef.body);
		if (!symtable_exit_block(st, s))
			return 0;
		break;
        case ClassDef_kind:
		if (!symtable_add_def(st, s->v.ClassDef.name, DEF_LOCAL))
			return 0;
		VISIT_SEQ(st, expr, s->v.ClassDef.bases);
		if (!symtable_enter_block(st, s->v.ClassDef.name, ClassBlock, 
					  (void *)s, s->lineno))
			return 0;
		VISIT_SEQ(st, stmt, s->v.ClassDef.body);
		if (!symtable_exit_block(st, s))
			return 0;
		break;
        case Return_kind:
		if (s->v.Return.value)
			VISIT(st, expr, s->v.Return.value);
		break;
        case Yield_kind:
		VISIT(st, expr, s->v.Yield.value);
                st->st_cur->ste_generator = 1;
		break;
        case Delete_kind:
		VISIT_SEQ(st, expr, s->v.Delete.targets);
		break;
        case Assign_kind:
		VISIT_SEQ(st, expr, s->v.Assign.targets);
		VISIT(st, expr, s->v.Assign.value);
		break;
        case AugAssign_kind:
		VISIT(st, expr, s->v.AugAssign.target);
		VISIT(st, expr, s->v.AugAssign.value);
		break;
        case Print_kind:
		if (s->v.Print.dest)
			VISIT(st, expr, s->v.Print.dest);
		VISIT_SEQ(st, expr, s->v.Print.values);
		break;
        case For_kind:
		VISIT(st, expr, s->v.For.target);
		VISIT(st, expr, s->v.For.iter);
		VISIT_SEQ(st, stmt, s->v.For.body);
		if (s->v.For.orelse)
			VISIT_SEQ(st, stmt, s->v.For.orelse);
		break;
        case While_kind:
		VISIT(st, expr, s->v.While.test);
		VISIT_SEQ(st, stmt, s->v.While.body);
		if (s->v.While.orelse)
			VISIT_SEQ(st, stmt, s->v.While.orelse);
		break;
        case If_kind:
		/* XXX if 0: and lookup_yield() hacks */
		VISIT(st, expr, s->v.If.test);
		VISIT_SEQ(st, stmt, s->v.If.body);
		if (s->v.If.orelse)
			VISIT_SEQ(st, stmt, s->v.If.orelse);
		break;
        case Raise_kind:
		if (s->v.Raise.type) {
			VISIT(st, expr, s->v.Raise.type);
			if (s->v.Raise.inst) {
				VISIT(st, expr, s->v.Raise.inst);
				if (s->v.Raise.tback)
					VISIT(st, expr, s->v.Raise.tback);
			}
		}
		break;
        case TryExcept_kind:
		VISIT_SEQ(st, stmt, s->v.TryExcept.body);
		VISIT_SEQ(st, stmt, s->v.TryExcept.orelse);
		VISIT_SEQ(st, excepthandler, s->v.TryExcept.handlers);
		break;
        case TryFinally_kind:
		VISIT_SEQ(st, stmt, s->v.TryFinally.body);
		VISIT_SEQ(st, stmt, s->v.TryFinally.finalbody);
		break;
        case Assert_kind:
		VISIT(st, expr, s->v.Assert.test);
		if (s->v.Assert.msg)
			VISIT(st, expr, s->v.Assert.msg);
		break;
        case Import_kind:
		VISIT_SEQ(st, alias, s->v.Import.names);
		break;
        case ImportFrom_kind:
		VISIT_SEQ(st, alias, s->v.ImportFrom.names);
		break;
        case Exec_kind:
		VISIT(st, expr, s->v.Exec.body);
		st->st_cur->ste_optimized = 0;
		if (s->v.Exec.globals) {
			VISIT(st, expr, s->v.Exec.globals);
			if (s->v.Exec.locals) 
				VISIT(st, expr, s->v.Exec.locals);
		}
		break;
        case Global_kind: {
		int i;
		asdl_seq *seq = s->v.Global.names;
		for (i = 0; i < asdl_seq_LEN(seq); i++)
			if (!symtable_add_def(st, asdl_seq_GET(seq, i), 
					      DEF_GLOBAL))
				return 0;
		
		break;
	}
        case Expr_kind:
		VISIT(st, expr, s->v.Expr.value);
		break;
        case Pass_kind:
        case Break_kind:
        case Continue_kind:
		/* nothing to do here */
		break;
	}
	return 1;
}

static int 
symtable_visit_expr(struct symtable *st, expr_ty e)
{
	switch (e->kind) {
        case BoolOp_kind:
		VISIT_SEQ(st, expr, e->v.BoolOp.values);
		break;
        case BinOp_kind:
		VISIT(st, expr, e->v.BinOp.left);
		VISIT(st, expr, e->v.BinOp.right);
		break;
        case UnaryOp_kind:
		VISIT(st, expr, e->v.UnaryOp.operand);
		break;
        case Lambda_kind: {
		if (!symtable_add_def(st, GET_IDENTIFIER(lambda), DEF_LOCAL))
			return 0;
		if (e->v.Lambda.args->defaults)
			VISIT_SEQ(st, expr, e->v.Lambda.args->defaults);
		/* XXX how to get line numbers for expressions */
		symtable_enter_block(st, GET_IDENTIFIER(lambda),
				     FunctionBlock, (void *)e, 0);
		VISIT(st, arguments, e->v.Lambda.args);
		VISIT(st, expr, e->v.Lambda.body);
		symtable_exit_block(st, (void *)e);
		break;
	}
        case Dict_kind:
		VISIT_SEQ(st, expr, e->v.Dict.keys);
		VISIT_SEQ(st, expr, e->v.Dict.values);
		break;
        case ListComp_kind: {
		char tmpname[256];
		identifier tmp;

		PyOS_snprintf(tmpname, sizeof(tmpname), "_[%d]",
			      ++st->st_cur->ste_tmpname);
		tmp = PyString_FromString(tmpname);
		if (!symtable_add_def(st, tmp, DEF_LOCAL))
			return 0;
		VISIT(st, expr, e->v.ListComp.elt);
		VISIT_SEQ(st, listcomp, e->v.ListComp.generators);
		break;
	}
        case Compare_kind:
		VISIT(st, expr, e->v.Compare.left);
		VISIT_SEQ(st, expr, e->v.Compare.comparators);
		break;
        case Call_kind:
		VISIT(st, expr, e->v.Call.func);
		VISIT_SEQ(st, expr, e->v.Call.args);
		VISIT_SEQ(st, keyword, e->v.Call.keywords);
		if (e->v.Call.starargs)
			VISIT(st, expr, e->v.Call.starargs);
		if (e->v.Call.kwargs)
			VISIT(st, expr, e->v.Call.kwargs);
		break;
        case Repr_kind:
		VISIT(st, expr, e->v.Repr.value);
		break;
        case Num_kind:
        case Str_kind:
		/* Nothing to do here. */
		break;
	/* The following exprs can be assignment targets. */
        case Attribute_kind:
		VISIT(st, expr, e->v.Attribute.value);
		break;
        case Subscript_kind:
		VISIT(st, expr, e->v.Subscript.value);
		VISIT(st, slice, e->v.Subscript.slice);
		break;
        case Name_kind:
		if (!symtable_add_def(st, e->v.Name.id, 
				      e->v.Name.ctx == Load ? USE : DEF_LOCAL))
			return 0;
		break;
	/* child nodes of List and Tuple will have expr_context set */
        case List_kind:
		VISIT_SEQ(st, expr, e->v.List.elts);
		break;
        case Tuple_kind:
		VISIT_SEQ(st, expr, e->v.Tuple.elts);
		break;
	}
	return 1;
}

static int
symtable_implicit_arg(struct symtable *st, int pos)
{
	PyObject *id = PyString_FromFormat(".%d", pos);
	if (id == NULL)
		return 0;
	/* XXX intern id? */
	if (!symtable_add_def(st, id, DEF_PARAM))
		return 0;
	Py_DECREF(id);
	return 1;
}

static int 
symtable_visit_params(struct symtable *st, asdl_seq *args, int toplevel)
{
	int i, complex = 0;
	
        /* go through all the toplevel arguments first */
	for (i = 0; i < asdl_seq_LEN(args); i++) {
		expr_ty arg = asdl_seq_GET(args, i);
		if (arg->kind == Name_kind) {
			assert(arg->v.Name.ctx == Param ||
                               (arg->v.Name.ctx == Store && !toplevel));
			if (!symtable_add_def(st, arg->v.Name.id, DEF_PARAM))
				return 0;
		}
		else if (arg->kind == Tuple_kind) {
			assert(arg->v.Tuple.ctx == Store);
                        complex = 1;
			if (toplevel) {
				if (!symtable_implicit_arg(st, i))
					return 0;
			}
		}
		else {
			/* syntax error */
			fprintf(stderr, "unexpected expr in parameter list\n");
			return 0;
		}
	}

        /* visit all the nested arguments */
        if (complex) {
                for (i = 0; i < asdl_seq_LEN(args); i++) {
                        expr_ty arg = asdl_seq_GET(args, i);
                        if (arg->kind == Tuple_kind &&
                            !symtable_visit_params(st, arg->v.Tuple.elts, 0))
                            return 0;
                }
	}
	
	return 1;
}

static int 
symtable_visit_arguments(struct symtable *st, arguments_ty a)
{
	/* skip default arguments inside function block
	   XXX should ast be different?
	*/
	if (a->args && !symtable_visit_params(st, a->args, 1))
		return 0;
	if (a->vararg) {
		if (!symtable_add_def(st, a->vararg, DEF_PARAM))
			return 0;
		st->st_cur->ste_varargs = 1;
	}
	if (a->kwarg) {
		if (!symtable_add_def(st, a->kwarg, DEF_PARAM))
			return 0;
		st->st_cur->ste_varkeywords = 1;
	}
	return 1;
}


static int 
symtable_visit_excepthandler(struct symtable *st, excepthandler_ty eh)
{
	if (eh->type)
		VISIT(st, expr, eh->type);
	if (eh->name)
		VISIT(st, expr, eh->name);
	VISIT_SEQ(st, stmt, eh->body);
	return 1;
}


static int 
symtable_visit_alias(struct symtable *st, alias_ty a)
{
	PyObject *name = (a->asname == NULL) ? a->name : a->asname;
	if (strcmp(PyString_AS_STRING(name), "*"))
	    return symtable_add_def(st, name, DEF_IMPORT);
	else {
	    st->st_cur->ste_optimized = 0;
	    return 1;
	}
}


static int 
symtable_visit_listcomp(struct symtable *st, listcomp_ty lc)
{
	VISIT(st, expr, lc->target);
	VISIT(st, expr, lc->iter);
	VISIT_SEQ(st, expr, lc->ifs);
	return 1;
}


static int 
symtable_visit_keyword(struct symtable *st, keyword_ty k)
{
	VISIT(st, expr, k->value);
	return 1;
}


static int 
symtable_visit_slice(struct symtable *st, slice_ty s)
{
	switch (s->kind) {
	case Slice_kind:
		if (s->v.Slice.lower)
			VISIT(st, expr, s->v.Slice.lower)
		if (s->v.Slice.upper)
			VISIT(st, expr, s->v.Slice.upper)
		if (s->v.Slice.step)
			VISIT(st, expr, s->v.Slice.step)
		break;
	case ExtSlice_kind:
		VISIT_SEQ(st, slice, s->v.ExtSlice.dims)
		break;
	case Index_kind:
		VISIT(st, expr, s->v.Index.value)
		break;
	case Ellipsis_kind:
		break;
	}
	return 1;
}

