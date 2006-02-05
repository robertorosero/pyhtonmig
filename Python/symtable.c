#include "Python.h"
#include "Python-ast.h"
#include "code.h"
#include "symtable.h"
#include "structmember.h"

/* two error strings used for warnings */
#define GLOBAL_AFTER_ASSIGN \
"name '%.400s' is assigned to before global declaration"

#define GLOBAL_AFTER_USE \
"name '%.400s' is used prior to global declaration"

PySTEntryObject *
PySTEntry_New(struct symtable *st, PyObject *name, _Py_block_ty block,
	      void *key, int lineno)
{
	PySTEntryObject *ste = NULL;
	PyObject *k;

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

	ste->ste_symbols = NULL;
	ste->ste_varnames = NULL;
	ste->ste_children = NULL;

	ste->ste_symbols = PyDict_New();
	if (ste->ste_symbols == NULL)
	    goto fail;

	ste->ste_varnames = PyList_New(0);
	if (ste->ste_varnames == NULL)
	    goto fail;

	ste->ste_children = PyList_New(0);
	if (ste->ste_children == NULL)
	    goto fail;

	ste->ste_type = block;
	ste->ste_unoptimized = 0;
	ste->ste_nested = 0;
	ste->ste_free = 0;
	ste->ste_varargs = 0;
	ste->ste_varkeywords = 0;
	ste->ste_opt_lineno = 0;
	ste->ste_tmpname = 0;
	ste->ste_lineno = lineno;

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
static int symtable_warn(struct symtable *st, char *msg);
static int symtable_enter_block(struct symtable *st, PyObject *name, 
				_Py_block_ty block, void *ast, int lineno);
static int symtable_exit_block(struct symtable *st, void *ast);
static int symtable_visit_stmt(struct symtable *st, PyObject *s);
static int symtable_visit_expr(struct symtable *st, PyObject *s);
static int symtable_visit_genexp(struct symtable *st, PyObject *s);
static int symtable_visit_arguments(struct symtable *st, PyObject *);
static int symtable_visit_excepthandler(struct symtable *st, PyObject *);
static int symtable_visit_alias(struct symtable *st, PyObject *);
static int symtable_visit_comprehension(struct symtable *st, PyObject *);
static int symtable_visit_keyword(struct symtable *st, PyObject *);
static int symtable_visit_slice(struct symtable *st, PyObject *);
static int symtable_visit_params(struct symtable *st, PyObject *args, int top);
static int symtable_visit_params_nested(struct symtable *st, PyObject *args);
static int symtable_implicit_arg(struct symtable *st, int pos);


static PyObject *top = NULL, *lambda = NULL, *genexpr = NULL;

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
	st->st_symbols = NULL;

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
PySymtable_Build(PyObject *mod, const char *filename, PyFutureFeatures *future)
{
	struct symtable *st = symtable_new();
	PyObject *seq;
	int i;

	if (st == NULL)
		return st;
	st->st_filename = filename;
	st->st_future = future;
	symtable_enter_block(st, GET_IDENTIFIER(top), ModuleBlock, 
			     (void *)mod, 0);
	st->st_top = st->st_cur;
	st->st_cur->ste_unoptimized = OPT_TOPLEVEL;
	/* Any other top-level initialization? */
	switch (mod_kind(mod)) {
	case Module_kind:
		seq = Module_body(mod);
		for (i = 0; i < PyList_GET_SIZE(seq); i++)
			if (!symtable_visit_stmt(st, PyList_GET_ITEM(seq, i)))
				goto error;
		break;
	case Expression_kind:
		if (!symtable_visit_expr(st, Expression_body(mod)))
			goto error;
		break;
	case Interactive_kind:
		seq = Interactive_body(mod);
		for (i = 0; i < PyList_GET_SIZE(seq); i++)
			if (!symtable_visit_stmt(st, PyList_GET_ITEM(seq, i)))
				goto error;
		break;
	case Suite_kind:
		PyErr_SetString(PyExc_RuntimeError,
				"this compiler does not handle Suites");
		goto error;
	}
	if (!symtable_exit_block(st, (void *)mod)) {
		PySymtable_Free(st);
		return NULL;
	}
	if (symtable_analyze(st))
		return st;
	PySymtable_Free(st);
	return NULL;
 error:
	(void) symtable_exit_block(st, (void *)mod);
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
		Py_INCREF(v);
	}
	else {
		PyErr_SetString(PyExc_KeyError,
				"unknown symbol table entry");
	}

	Py_DECREF(k);
	return (PySTEntryObject *)v;
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
	if (PyDict_SetItem((DICT), (NAME), o) < 0) { \
		Py_DECREF(o); \
		return 0; \
	} \
	Py_DECREF(o); \
}

/* Decide on scope of name, given flags.

   The dicts passed in as arguments are modified as necessary.
   ste is passed so that flags can be updated.
*/

static int 
analyze_name(PySTEntryObject *ste, PyObject *dict, PyObject *name, int flags,
	     PyObject *bound, PyObject *local, PyObject *free, 
	     PyObject *global)
{
	if (flags & DEF_GLOBAL) {
		if (flags & DEF_PARAM) {
			PyErr_Format(PyExc_SyntaxError,
				     "name '%s' is local and global",
				     PyString_AS_STRING(name));
			return 0;
		}
		SET_SCOPE(dict, name, GLOBAL_EXPLICIT);
		if (PyDict_SetItem(global, name, Py_None) < 0)
			return 0;
		if (bound && PyDict_GetItem(bound, name)) {
			if (PyDict_DelItem(bound, name) < 0)
				return 0;
		}
		return 1;
	}
	if (flags & DEF_BOUND) {
		SET_SCOPE(dict, name, LOCAL);
		if (PyDict_SetItem(local, name, Py_None) < 0)
			return 0;
		if (PyDict_GetItem(global, name)) {
			if (PyDict_DelItem(global, name) < 0)
				return 0;
		}
		return 1;
	}
	/* If an enclosing block has a binding for this name, it
	   is a free variable rather than a global variable.
	   Note that having a non-NULL bound implies that the block
	   is nested.
	*/
	if (bound && PyDict_GetItem(bound, name)) {
		SET_SCOPE(dict, name, FREE);
		ste->ste_free = 1;
		if (PyDict_SetItem(free, name, Py_None) < 0)
			return 0;
		return 1;
	}
	/* If a parent has a global statement, then call it global
	   explicit?  It could also be global implicit.
	 */
	else if (global && PyDict_GetItem(global, name)) {
		SET_SCOPE(dict, name, GLOBAL_EXPLICIT);
		return 1;
	}
	else {
		if (ste->ste_nested)
			ste->ste_free = 1;
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

/* Check for illegal statements in unoptimized namespaces */
static int
check_unoptimized(const PySTEntryObject* ste) {
	char buf[300];
	const char* trailer;

	if (ste->ste_type != FunctionBlock || !ste->ste_unoptimized
	    || !(ste->ste_free || ste->ste_child_free))
		return 1;

	trailer = (ste->ste_child_free ? 
		       "contains a nested function with free variables" :
			       "is a nested function");

	switch (ste->ste_unoptimized) {
	case OPT_TOPLEVEL: /* exec / import * at top-level is fine */
	case OPT_EXEC: /* qualified exec is fine */
		return 1;
	case OPT_IMPORT_STAR:
		PyOS_snprintf(buf, sizeof(buf), 
			      "import * is not allowed in function '%.100s' "
			      "because it is %s",
			      PyString_AS_STRING(ste->ste_name), trailer);
		break;
	case OPT_BARE_EXEC:
		PyOS_snprintf(buf, sizeof(buf),
			      "unqualified exec is not allowed in function "
			      "'%.100s' it %s",
			      PyString_AS_STRING(ste->ste_name), trailer);
		break;
	default:
		PyOS_snprintf(buf, sizeof(buf), 
			      "function '%.100s' uses import * and bare exec, "
			      "which are illegal because it %s",
			      PyString_AS_STRING(ste->ste_name), trailer);
		break;
	}

	PyErr_SetString(PyExc_SyntaxError, buf);
	PyErr_SyntaxLocation(ste->ste_table->st_filename, 
			     ste->ste_opt_lineno);
	return 0;
}

/* Enter the final scope information into the st_symbols dict. 
 * 
 * All arguments are dicts.  Modifies symbols, others are read-only.
*/
static int
update_symbols(PyObject *symbols, PyObject *scope, 
               PyObject *bound, PyObject *free, int class)
{
	PyObject *name, *v, *u, *w, *free_value = NULL;
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

        free_value = PyInt_FromLong(FREE << SCOPE_OFF);
        if (!free_value)
		return 0;

        /* add a free variable when it's only use is for creating a closure */
        pos = 0;
	while (PyDict_Next(free, &pos, &name, &v)) {
		PyObject *o = PyDict_GetItem(symbols, name);

		if (o) {
			/* It could be a free variable in a method of
			   the class that has the same name as a local
			   or global in the class scope.
			*/
			if  (class && 
			     PyInt_AS_LONG(o) & (DEF_BOUND | DEF_GLOBAL)) {
				int i = PyInt_AS_LONG(o) | DEF_FREE_CLASS;
				o = PyInt_FromLong(i);
				if (!o) {
					Py_DECREF(free_value);
					return 0;
				}
				if (PyDict_SetItem(symbols, name, o) < 0) {
					Py_DECREF(o);
					Py_DECREF(free_value);
					return 0;
				}
				Py_DECREF(o);
			}
			/* else it's not free, probably a cell */
			continue;
		}
		if (!PyDict_GetItem(bound, name))
			continue;       /* it's a global */

		if (PyDict_SetItem(symbols, name, free_value) < 0) {
			Py_DECREF(free_value);
			return 0;
		}
        }
        Py_DECREF(free_value);
	return 1;
}   

/* Make final symbol table decisions for block of ste.
   Arguments:
   ste -- current symtable entry (input/output)
   bound -- set of variables bound in enclosing scopes (input)
   free -- set of free variables in enclosed scopes (output)
   globals -- set of declared global variables in enclosing scopes (input)
*/

static int
analyze_block(PySTEntryObject *ste, PyObject *bound, PyObject *free, 
	      PyObject *global)
{
	PyObject *name, *v, *local = NULL, *scope = NULL, *newbound = NULL;
	PyObject *newglobal = NULL, *newfree = NULL;
	int i, flags, pos = 0, success = 0;

	local = PyDict_New();
	if (!local)
		goto error;
	scope = PyDict_New();
	if (!scope)
		goto error;
	newglobal = PyDict_New();
	if (!newglobal)
		goto error;
	newfree = PyDict_New();
	if (!newfree)
		goto error;
	newbound = PyDict_New();
	if (!newbound)
		goto error;

	if (ste->ste_type == ClassBlock) {
		/* make a copy of globals before calling analyze_name(),
		   because global statements in the class have no effect
		   on nested functions.
		*/
		if (PyDict_Update(newglobal, global) < 0)
			goto error;
		if (bound)
			if (PyDict_Update(newbound, bound) < 0)
				goto error;
	}

	assert(PySTEntry_Check(ste));
	assert(PyDict_Check(ste->ste_symbols));
	while (PyDict_Next(ste->ste_symbols, &pos, &name, &v)) {
		flags = PyInt_AS_LONG(v);
		if (!analyze_name(ste, scope, name, flags, bound, local, free,
				  global))
			goto error;
	}

	if (ste->ste_type != ClassBlock) {
		if (ste->ste_type == FunctionBlock) {
			if (PyDict_Update(newbound, local) < 0)
				goto error;
		}
		if (bound) {
			if (PyDict_Update(newbound, bound) < 0)
				goto error;
		}
		if (PyDict_Update(newglobal, global) < 0)
			goto error;
	}

	/* Recursively call analyze_block() on each child block */
	for (i = 0; i < PyList_GET_SIZE(ste->ste_children); ++i) {
		PyObject *c = PyList_GET_ITEM(ste->ste_children, i);
		PySTEntryObject* entry;
		assert(c && PySTEntry_Check(c));
		entry = (PySTEntryObject*)c;
		if (!analyze_block(entry, newbound, newfree, newglobal))
			goto error;
		if (entry->ste_free || entry->ste_child_free)
			ste->ste_child_free = 1;
	}

	if (ste->ste_type == FunctionBlock && !analyze_cells(scope, newfree))
		goto error;
	if (!update_symbols(ste->ste_symbols, scope, bound, newfree,
			    ste->ste_type == ClassBlock))
		goto error;
	if (!check_unoptimized(ste))
		goto error;

	if (PyDict_Update(free, newfree) < 0)
		goto error;
	success = 1;
 error:
	Py_XDECREF(local);
	Py_XDECREF(scope);
	Py_XDECREF(newbound);
	Py_XDECREF(newglobal);
	Py_XDECREF(newfree);
	if (!success)
		assert(PyErr_Occurred());
	return success;
}

static int
symtable_analyze(struct symtable *st)
{
	PyObject *free, *global;
	int r;

	free = PyDict_New();
	if (!free)
	    return 0;
	global = PyDict_New();
	if (!global) {
	    Py_DECREF(free);
	    return 0;
	}
	r = analyze_block(st->st_top, NULL, free, global);
	Py_DECREF(free);
	Py_DECREF(global);
	return r;
}


static int
symtable_warn(struct symtable *st, char *msg)
{
	if (PyErr_WarnExplicit(PyExc_SyntaxWarning, msg, st->st_filename,
			       st->st_cur->ste_lineno, NULL, NULL) < 0)	{
		if (PyErr_ExceptionMatches(PyExc_SyntaxWarning)) {
			PyErr_SetString(PyExc_SyntaxError, msg);
			PyErr_SyntaxLocation(st->st_filename, 
					     st->st_cur->ste_lineno);
		}
		return 0;
	}
	return 1;
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
		Py_INCREF(st->st_cur);
		if (PySequence_DelItem(st->st_stack, end) < 0)
			return 0;
	}
	return 1;
}

static int
symtable_enter_block(struct symtable *st, PyObject *name, _Py_block_ty block, 
		     void *ast, int lineno)
{
	PySTEntryObject *prev = NULL;

	if (st->st_cur) {
		prev = st->st_cur;
		if (PyList_Append(st->st_stack, (PyObject *)st->st_cur) < 0) {
			return 0;
		}
		Py_DECREF(st->st_cur);
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
symtable_lookup(struct symtable *st, PyObject *name)
{
	PyObject *o;
	PyObject *mangled = _Py_Mangle(st->st_private, name);
	if (!mangled)
		return 0;
	o = PyDict_GetItem(st->st_cur->ste_symbols, mangled);
	Py_DECREF(mangled);
	if (!o)
		return 0;
	return PyInt_AsLong(o);
}

static int
symtable_add_def(struct symtable *st, PyObject *name, int flag) 
{
	PyObject *o;
	PyObject *dict;
	int val;
	PyObject *mangled = _Py_Mangle(st->st_private, name);

	if (!mangled)
		return 0;
	dict = st->st_cur->ste_symbols;
	if ((o = PyDict_GetItem(dict, mangled))) {
	    val = PyInt_AS_LONG(o);
	    if ((flag & DEF_PARAM) && (val & DEF_PARAM)) {
		    /* Is it better to use 'mangled' or 'name' here? */
		    PyErr_Format(PyExc_SyntaxError, DUPLICATE_ARGUMENT,
				 PyString_AsString(name));
		    PyErr_SyntaxLocation(st->st_filename,
				       st->st_cur->ste_lineno);
		    goto error;
	    }
	    val |= flag;
	} else
	    val = flag;
	o = PyInt_FromLong(val);
        if (o == NULL)
	    goto error;
	if (PyDict_SetItem(dict, mangled, o) < 0) {
		Py_DECREF(o);
		goto error;
	}
	Py_DECREF(o);

	if (flag & DEF_PARAM) {
		if (PyList_Append(st->st_cur->ste_varnames, mangled) < 0)
			goto error;
	} else	if (flag & DEF_GLOBAL) {
		/* XXX need to update DEF_GLOBAL for other flags too;
		   perhaps only DEF_FREE_GLOBAL */
		val = flag;
		if ((o = PyDict_GetItem(st->st_global, mangled))) {
			val |= PyInt_AS_LONG(o);
		}
		o = PyInt_FromLong(val);
		if (o == NULL)
			goto error;
		if (PyDict_SetItem(st->st_global, mangled, o) < 0) {
			Py_DECREF(o);
			goto error;
		}
		Py_DECREF(o);
	}
	Py_DECREF(mangled);
	return 1;

error:
	Py_DECREF(mangled);
	return 0;
}

/* VISIT, VISIT_SEQ and VIST_SEQ_TAIL take an ASDL type as their second argument.
   They use the ASDL name to synthesize the name of the C type and the visit
   function. 
   
   VISIT_SEQ_TAIL permits the start of an ASDL sequence to be skipped, which is
   useful if the first node in the sequence requires special treatment.
*/

#define VISIT(ST, TYPE, V) {\
	if (!symtable_visit_ ## TYPE((ST), (V))) \
		return 0;\
}

#define VISIT_IN_BLOCK(ST, TYPE, V, S) {\
	if (!symtable_visit_ ## TYPE((ST), (V))) { \
		symtable_exit_block((ST), (S)); \
		return 0; \
	}\
}

#define VISIT_SEQ(ST, TYPE, SEQ) { \
	int i; \
	PyObject *seq = (SEQ); /* avoid variable capture */ \
	for (i = 0; i < PyList_GET_SIZE(seq); i++) { \
		PyObject *elt = PyList_GET_ITEM(seq, i); \
		if (!symtable_visit_ ## TYPE((ST), elt)) \
			return 0; \
	} \
}

#define VISIT_SEQ_IN_BLOCK(ST, TYPE, SEQ, S) { \
	int i; \
	PyObject *seq = (SEQ); /* avoid variable capture */ \
	for (i = 0; i < PyList_GET_SIZE(seq); i++) { \
		PyObject *elt = PyList_GET_ITEM(seq, i); \
		if (!symtable_visit_ ## TYPE((ST), elt)) { \
			symtable_exit_block((ST), (S)); \
			return 0; \
		} \
	} \
}

#define VISIT_SEQ_TAIL(ST, TYPE, SEQ, START) { \
	int i; \
	PyObject *seq = (SEQ); /* avoid variable capture */ \
	for (i = (START); i < PyList_GET_SIZE(seq); i++) { \
		PyObject *elt = PyList_GET_ITEM(seq, i); \
		if (!symtable_visit_ ## TYPE((ST), elt)) \
			return 0; \
	} \
}

#define VISIT_SEQ_TAIL_IN_BLOCK(ST, TYPE, SEQ, START, S) { \
	int i; \
	PyObject *seq = (SEQ); /* avoid variable capture */ \
	for (i = (START); i < PyList_GET_SIZE(seq); i++) { \
		PyObject *elt = PyList_GET_ITEM(seq, i); \
		if (!symtable_visit_ ## TYPE((ST), elt)) { \
			symtable_exit_block((ST), (S)); \
			return 0; \
		} \
	} \
}

static int
symtable_visit_stmt(struct symtable *st, PyObject *s)
{
	switch (stmt_kind(s)) {
        case FunctionDef_kind:
		if (!symtable_add_def(st, FunctionDef_name(s), DEF_LOCAL))
			return 0;
		if (arguments_defaults(FunctionDef_args(s)))
			VISIT_SEQ(st, expr, arguments_defaults(FunctionDef_args(s)));
		if (FunctionDef_decorators(s))
			VISIT_SEQ(st, expr, FunctionDef_decorators(s));
		if (!symtable_enter_block(st, FunctionDef_name(s), 
					  FunctionBlock, (void *)s, ((struct _stmt*)s)->lineno))
			return 0;
		VISIT_IN_BLOCK(st, arguments, FunctionDef_args(s), s);
		VISIT_SEQ_IN_BLOCK(st, stmt, FunctionDef_body(s), s);
		if (!symtable_exit_block(st, s))
			return 0;
		break;
        case ClassDef_kind: {
		PyObject *tmp;
		if (!symtable_add_def(st, ClassDef_name(s), DEF_LOCAL))
			return 0;
		VISIT_SEQ(st, expr, ClassDef_bases(s));
		if (!symtable_enter_block(st, ClassDef_name(s), ClassBlock, 
					  (void *)s, ((struct _stmt*)s)->lineno))
			return 0;
		tmp = st->st_private;
		st->st_private = ClassDef_name(s);
		VISIT_SEQ_IN_BLOCK(st, stmt, ClassDef_body(s), s);
		st->st_private = tmp;
		if (!symtable_exit_block(st, s))
			return 0;
		break;
	}
        case Return_kind:
		if (Return_value(s) != Py_None)
			VISIT(st, expr, Return_value(s));
		break;
        case Delete_kind:
		VISIT_SEQ(st, expr, Delete_targets(s));
		break;
        case Assign_kind:
		VISIT_SEQ(st, expr, Assign_targets(s));
		VISIT(st, expr, Assign_value(s));
		break;
        case AugAssign_kind:
		VISIT(st, expr, AugAssign_target(s));
		VISIT(st, expr, AugAssign_value(s));
		break;
        case Print_kind:
		if (Print_dest(s) != Py_None)
			VISIT(st, expr, Print_dest(s));
		VISIT_SEQ(st, expr, Print_values(s));
		break;
        case For_kind:
		VISIT(st, expr, For_target(s));
		VISIT(st, expr, For_iter(s));
		VISIT_SEQ(st, stmt, For_body(s));
		/* if (For_orelse(s)) */
		VISIT_SEQ(st, stmt, For_orelse(s));
		break;
        case While_kind:
		VISIT(st, expr, While_test(s));
		VISIT_SEQ(st, stmt, While_body(s));
		/* if (While_orelse(s)) */
		VISIT_SEQ(st, stmt, While_orelse(s));
		break;
        case If_kind:
		/* XXX if 0: and lookup_yield() hacks */
		VISIT(st, expr, If_test(s));
		VISIT_SEQ(st, stmt, If_body(s));
		/* if (If_orelse(s)) */
		VISIT_SEQ(st, stmt, If_orelse(s));
		break;
        case Raise_kind:
		if (Raise_type(s) != Py_None) {
			VISIT(st, expr, Raise_type(s));
			if (Raise_inst(s) != Py_None) {
				VISIT(st, expr, Raise_inst(s));
				if (Raise_tback(s) != Py_None)
					VISIT(st, expr, Raise_tback(s));
			}
		}
		break;
        case TryExcept_kind:
		VISIT_SEQ(st, stmt, TryExcept_body(s));
		VISIT_SEQ(st, stmt, TryExcept_orelse(s));
		VISIT_SEQ(st, excepthandler, TryExcept_handlers(s));
		break;
        case TryFinally_kind:
		VISIT_SEQ(st, stmt, TryFinally_body(s));
		VISIT_SEQ(st, stmt, TryFinally_finalbody(s));
		break;
        case Assert_kind:
		VISIT(st, expr, Assert_test(s));
		if (Assert_msg(s) != Py_None)
			VISIT(st, expr, Assert_msg(s));
		break;
        case Import_kind:
		VISIT_SEQ(st, alias, Import_names(s));
		/* XXX Don't have the lineno available inside
		   visit_alias */
		if (st->st_cur->ste_unoptimized && !st->st_cur->ste_opt_lineno)
			st->st_cur->ste_opt_lineno = ((struct _stmt*)s)->lineno;
		break;
        case ImportFrom_kind:
		VISIT_SEQ(st, alias, ImportFrom_names(s));
		/* XXX Don't have the lineno available inside
		   visit_alias */
		if (st->st_cur->ste_unoptimized && !st->st_cur->ste_opt_lineno)
			st->st_cur->ste_opt_lineno = ((struct _stmt*)s)->lineno;
		break;
        case Exec_kind:
		VISIT(st, expr, Exec_body(s));
		if (!st->st_cur->ste_opt_lineno)
			st->st_cur->ste_opt_lineno = ((struct _stmt*)s)->lineno;
		if (Exec_globals(s) != Py_None) {
			st->st_cur->ste_unoptimized |= OPT_EXEC;
			VISIT(st, expr, Exec_globals(s));
			if (Exec_locals(s) != Py_None) 
				VISIT(st, expr, Exec_locals(s));
		} else {
			st->st_cur->ste_unoptimized |= OPT_BARE_EXEC;
		}
		break;
        case Global_kind: {
		int i;
		PyObject *seq = Global_names(s);
		for (i = 0; i < PyList_GET_SIZE(seq); i++) {
			PyObject *name = PyList_GET_ITEM(seq, i);
			char *c_name = PyString_AS_STRING(name);
			int cur = symtable_lookup(st, name);
			if (cur < 0)
				return 0;
			if (cur & (DEF_LOCAL | USE)) {
				char buf[256];
				if (cur & DEF_LOCAL) 
					PyOS_snprintf(buf, sizeof(buf),
						      GLOBAL_AFTER_ASSIGN,
						      c_name);
				else
					PyOS_snprintf(buf, sizeof(buf),
						      GLOBAL_AFTER_USE,
						      c_name);
				if (!symtable_warn(st, buf))
                                    return 0;
			}
			if (!symtable_add_def(st, name, DEF_GLOBAL))
				return 0;
		}
		break;
	}
        case Expr_kind:
		VISIT(st, expr, Expr_value(s));
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
symtable_visit_expr(struct symtable *st, PyObject *e)
{
	switch (expr_kind(e)) {
        case BoolOp_kind:
		VISIT_SEQ(st, expr, BoolOp_values(e));
		break;
        case BinOp_kind:
		VISIT(st, expr, BinOp_left(e));
		VISIT(st, expr, BinOp_right(e));
		break;
        case UnaryOp_kind:
		VISIT(st, expr, UnaryOp_operand(e));
		break;
        case Lambda_kind: {
		if (!symtable_add_def(st, GET_IDENTIFIER(lambda), DEF_LOCAL))
			return 0;
		if (arguments_defaults(Lambda_args(e)))
			VISIT_SEQ(st, expr, arguments_defaults(Lambda_args(e)));
		/* XXX how to get line numbers for expressions */
		if (!symtable_enter_block(st, GET_IDENTIFIER(lambda),
                                          FunctionBlock, (void *)e, 0))
			return 0;
		VISIT_IN_BLOCK(st, arguments, Lambda_args(e), (void*)e);
		VISIT_IN_BLOCK(st, expr, Lambda_body(e), (void*)e);
		if (!symtable_exit_block(st, (void *)e))
			return 0;
		break;
	}
        case Dict_kind:
		VISIT_SEQ(st, expr, Dict_keys(e));
		VISIT_SEQ(st, expr, Dict_values(e));
		break;
        case ListComp_kind: {
		char tmpname[256];
		PyObject *tmp;

		PyOS_snprintf(tmpname, sizeof(tmpname), "_[%d]",
			      ++st->st_cur->ste_tmpname);
		tmp = PyString_InternFromString(tmpname);
		if (!symtable_add_def(st, tmp, DEF_LOCAL))
			return 0;
		Py_DECREF(tmp);
		VISIT(st, expr, ListComp_elt(e));
		VISIT_SEQ(st, comprehension, ListComp_generators(e));
		break;
	}
        case GeneratorExp_kind: {
		if (!symtable_visit_genexp(st, e)) {
			return 0;
		}
		break;
	}
        case Yield_kind:
		if (Yield_value(e) != Py_None)
			VISIT(st, expr, Yield_value(e));
                st->st_cur->ste_generator = 1;
		break;
        case Compare_kind:
		VISIT(st, expr, Compare_left(e));
		VISIT_SEQ(st, expr, Compare_comparators(e));
		break;
        case Call_kind:
		VISIT(st, expr, Call_func(e));
		VISIT_SEQ(st, expr, Call_args(e));
		VISIT_SEQ(st, keyword, Call_keywords(e));
		if (Call_starargs(e) != Py_None)
			VISIT(st, expr, Call_starargs(e));
		if (Call_kwargs(e) != Py_None)
			VISIT(st, expr, Call_kwargs(e));
		break;
        case Repr_kind:
		VISIT(st, expr, Repr_value(e));
		break;
        case Num_kind:
        case Str_kind:
		/* Nothing to do here. */
		break;
	/* The following exprs can be assignment targets. */
        case Attribute_kind:
		VISIT(st, expr, Attribute_value(e));
		break;
        case Subscript_kind:
		VISIT(st, expr, Subscript_value(e));
		VISIT(st, slice, Subscript_slice(e));
		break;
        case Name_kind:
		if (!symtable_add_def(st, Name_id(e), 
				      expr_context_kind(Name_ctx(e)) == Load_kind ? USE : DEF_LOCAL))
			return 0;
		break;
	/* child nodes of List and Tuple will have expr_context set */
        case List_kind:
		VISIT_SEQ(st, expr, List_elts(e));
		break;
        case Tuple_kind:
		VISIT_SEQ(st, expr, Tuple_elts(e));
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
	if (!symtable_add_def(st, id, DEF_PARAM)) {
		Py_DECREF(id);
		return 0;
	}
	Py_DECREF(id);
	return 1;
}

static int 
symtable_visit_params(struct symtable *st, PyObject *args, int toplevel)
{
	int i, complex = 0;
	
        /* go through all the toplevel arguments first */
	for (i = 0; i < PyList_GET_SIZE(args); i++) {
		PyObject *arg = PyList_GET_ITEM(args, i);
		if (expr_kind(arg) == Name_kind) {
			assert(Name_ctx(arg) == Param ||
                               (Name_ctx(arg) == Store && !toplevel));
			if (!symtable_add_def(st, Name_id(arg), DEF_PARAM))
				return 0;
		}
		else if (expr_kind(arg) == Tuple_kind) {
			assert(Tuple_ctx(arg) == Store);
                        complex = 1;
			if (toplevel) {
				if (!symtable_implicit_arg(st, i))
					return 0;
			}
		}
		else {
		        PyErr_SetString(PyExc_SyntaxError,
					"invalid expression in parameter list");
		        PyErr_SyntaxLocation(st->st_filename,
				             st->st_cur->ste_lineno);
			return 0;
		}
	}

	if (!toplevel) {
		if (!symtable_visit_params_nested(st, args))
			return 0;
	}

	return 1;
}

static int
symtable_visit_params_nested(struct symtable *st, PyObject *args)
{
	int i;
	for (i = 0; i < PyList_GET_SIZE(args); i++) {
		PyObject *arg = PyList_GET_ITEM(args, i);
		if (expr_kind(arg) == Tuple_kind &&
		    !symtable_visit_params(st, Tuple_elts(arg), 0))
			return 0;
	}
	
	return 1;
}

static int 
symtable_visit_arguments(struct symtable *st, PyObject *a)
{
	/* skip default arguments inside function block
	   XXX should ast be different?
	*/
	/* if (arguments_args(a) && !symtable_visit_params(st, arguments_args(a), 1)) */
	if (!symtable_visit_params(st, arguments_args(a), 1))
		return 0;
	if (arguments_vararg(a) != Py_None) {
		if (!symtable_add_def(st, arguments_vararg(a), DEF_PARAM))
			return 0;
		st->st_cur->ste_varargs = 1;
	}
	if (arguments_kwarg(a) != Py_None) {
		if (!symtable_add_def(st, arguments_kwarg(a), DEF_PARAM))
			return 0;
		st->st_cur->ste_varkeywords = 1;
	}
	/* if (arguments_args(a) && !symtable_visit_params_nested(st, arguments_args(a))) */
	if (!symtable_visit_params_nested(st, arguments_args(a)))
		return 0;
	return 1;
}


static int 
symtable_visit_excepthandler(struct symtable *st, PyObject *eh)
{
	if (excepthandler_type(eh) != Py_None)
		VISIT(st, expr, excepthandler_type(eh));
	if (excepthandler_name(eh) != Py_None)
		VISIT(st, expr, excepthandler_name(eh));
	VISIT_SEQ(st, stmt, excepthandler_body(eh));
	return 1;
}


static int 
symtable_visit_alias(struct symtable *st, PyObject *a)
{
	/* Compute store_name, the name actually bound by the import
	   operation.  It is diferent than alias_name(a) when alias_name(a) is a
	   dotted package name (e.g. spam.eggs) 
	*/
	PyObject *store_name;
	PyObject *name = (alias_asname(a) == NULL) ? alias_name(a) : alias_asname(a);
	const char *base = PyString_AS_STRING(name);
	char *dot = strchr(base, '.');
	if (dot)
		store_name = PyString_FromStringAndSize(base, dot - base);
	else {
		store_name = name;
		Py_INCREF(store_name);
	}
	if (strcmp(PyString_AS_STRING(name), "*")) {
		int r = symtable_add_def(st, store_name, DEF_IMPORT); 
		Py_DECREF(store_name);
		return r;
	}
	else {
            if (st->st_cur->ste_type != ModuleBlock) {
                if (!symtable_warn(st,
                                   "import * only allowed at module level")) {
                    Py_DECREF(store_name);
                    return 0;
		}
            }
	    st->st_cur->ste_unoptimized |= OPT_IMPORT_STAR;
	    Py_DECREF(store_name);
	    return 1;
	}
}


static int 
symtable_visit_comprehension(struct symtable *st, PyObject *lc)
{
	VISIT(st, expr, comprehension_target(lc));
	VISIT(st, expr, comprehension_iter(lc));
	VISIT_SEQ(st, expr, comprehension_ifs(lc));
	return 1;
}


static int 
symtable_visit_keyword(struct symtable *st, PyObject *k)
{
	VISIT(st, expr, keyword_value(k));
	return 1;
}


static int 
symtable_visit_slice(struct symtable *st, PyObject *s)
{
	switch (slice_kind(s)) {
	case Slice_kind:
		if (Slice_lower(s) != Py_None)
			VISIT(st, expr, Slice_lower(s))
		if (Slice_upper(s) != Py_None)
			VISIT(st, expr, Slice_upper(s))
		if (Slice_step(s) != Py_None)
			VISIT(st, expr, Slice_step(s))
		break;
	case ExtSlice_kind:
		VISIT_SEQ(st, slice, ExtSlice_dims(s))
		break;
	case Index_kind:
		VISIT(st, expr, Index_value(s))
		break;
	case Ellipsis_kind:
		break;
	}
	return 1;
}

static int 
symtable_visit_genexp(struct symtable *st, PyObject *e)
{
	PyObject *outermost = ((PyObject *)
			 (PyList_GET_ITEM(GeneratorExp_generators(e), 0)));
	/* Outermost iterator is evaluated in current scope */
	VISIT(st, expr, comprehension_iter(outermost));
	/* Create generator scope for the rest */
	if (!symtable_enter_block(st, GET_IDENTIFIER(genexpr),
				  FunctionBlock, (void *)e, 0)) {
		return 0;
	}
	st->st_cur->ste_generator = 1;
	/* Outermost iter is received as an argument */
	if (!symtable_implicit_arg(st, 0)) {
		symtable_exit_block(st, (void *)e);
		return 0;
	}
	VISIT_IN_BLOCK(st, expr, comprehension_target(outermost), (void*)e);
	VISIT_SEQ_IN_BLOCK(st, expr, comprehension_ifs(outermost), (void*)e);
	VISIT_SEQ_TAIL_IN_BLOCK(st, comprehension,
				GeneratorExp_generators(e), 1, (void*)e);
	VISIT_IN_BLOCK(st, expr, GeneratorExp_elt(e), (void*)e);
	if (!symtable_exit_block(st, (void *)e))
		return 0;
	return 1;
}
