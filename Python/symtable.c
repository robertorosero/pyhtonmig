#include "Python.h"
#include "Python-ast.h"
#include "compile.h"
#include "symtable.h"
#include "structmember.h"

PySymtableEntryObject *
PySymtableEntry_New(struct symtable *st, identifier name, scope_ty scope,
		    void *key, int lineno)
{
	PySymtableEntryObject *ste = NULL;
	PyObject *k, *v;

	k = PyLong_FromVoidPtr(key);
	if (k == NULL)
		goto fail;
	v = PyDict_GetItem(st->st_symbols, k);
	if (v) {
		assert(PySymtableEntry_Check(v));
		Py_DECREF(k);
		Py_INCREF(v);
		return (PySymtableEntryObject *)v;
	}
	
	ste = (PySymtableEntryObject *)PyObject_New(PySymtableEntryObject,
						    &PySymtableEntry_Type);
	ste->ste_table = st;
	ste->ste_id = k;

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

	ste->ste_type = scope;
	ste->ste_optimized = 0;
	ste->ste_opt_lineno = 0;
	ste->ste_lineno = lineno;

	if (st->st_cur == NULL)
		ste->ste_nested = 0;
	else if (st->st_cur->ste_nested 
		 || st->st_cur->ste_type == FunctionScope)
		ste->ste_nested = 1;
	else
		ste->ste_nested = 0;
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
ste_repr(PySymtableEntryObject *ste)
{
	char buf[256];

	PyOS_snprintf(buf, sizeof(buf),
		      "<symtable entry %.100s(%ld), line %d>",
		      PyString_AS_STRING(ste->ste_name),
		      PyInt_AS_LONG(ste->ste_id),
		      ste->ste_lineno);
	return PyString_FromString(buf);
}

static void
ste_dealloc(PySymtableEntryObject *ste)
{
	ste->ste_table = NULL;
	Py_XDECREF(ste->ste_id);
	Py_XDECREF(ste->ste_name);
	Py_XDECREF(ste->ste_symbols);
	Py_XDECREF(ste->ste_varnames);
	Py_XDECREF(ste->ste_children);
	PyObject_Del(ste);
}

#define OFF(x) offsetof(PySymtableEntryObject, x)

static PyMemberDef ste_memberlist[] = {
	{"id",       T_OBJECT, OFF(ste_id), READONLY},
	{"name",     T_OBJECT, OFF(ste_name), READONLY},
	{"symbols",  T_OBJECT, OFF(ste_symbols), READONLY},
	{"varnames", T_OBJECT, OFF(ste_varnames), READONLY},
	{"children", T_OBJECT, OFF(ste_children), READONLY},
	{"type",     T_INT,    OFF(ste_type), READONLY},
	{"lineno",   T_INT,    OFF(ste_lineno), READONLY},
	{"optimized",T_INT,    OFF(ste_optimized), READONLY},
	{"nested",   T_INT,    OFF(ste_nested), READONLY},
	{NULL}
};

PyTypeObject PySymtableEntry_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"symtable entry",
	sizeof(PySymtableEntryObject),
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

static int symtable_enter_scope(struct symtable *st, identifier name, 
				scope_ty scope, void *ast, int lineno);
static int symtable_exit_scope(struct symtable *st, void *ast);
static int symtable_visit_stmt(struct symtable *st, stmt_ty s);
static int symtable_visit_stmts(struct symtable *st, asdl_seq *seq);
static int symtable_visit_expr(struct symtable *st, expr_ty s);
static int symtable_visit_exprs(struct symtable *st, asdl_seq *seq); 

static identifier top = NULL, lambda = NULL;

#define GET_IDENTIFIER(VAR) \
	((VAR) ? (VAR) : ((VAR) = PyString_InternFromString(# VAR)))

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
	st->st_errors = 0;
	st->st_tmpname = 0;
	st->st_private = NULL;
	return st;
 fail:
	PySymtable_Free(st);
	return NULL;
}

void
PySymtable_Free(struct symtable *st)
{
	Py_XDECREF(st->st_symbols);
	Py_XDECREF(st->st_stack);
	Py_XDECREF(st->st_cur);
	PyMem_Free((void *)st);
}

struct symtable *
PySymtable_Build(mod_ty mod, const char *filename, PyFutureFeatures *future)
{
	struct symtable *st = symtable_new();

	if (st == NULL)
		return st;
	st->st_filename = filename;
	st->st_future = future;
	/* XXX not a stmt_ty */
	symtable_enter_scope(st, GET_IDENTIFIER(top),
			     ModuleScope, (void *)mod, 0);
	/* Any other top-level initialization? */
	if (mod->kind == Module_kind)
		symtable_visit_stmts(st, mod->v.Module.body);
	/* XXX not a stmt_ty */
	symtable_exit_scope(st, (void *)mod);
	return st;
}


/* symtable_enter_scope() gets a reference via PySymtableEntry_New().
   This reference is released when the scope is exited, via the DECREF
   in symtable_exit_scope().
*/

static int
symtable_exit_scope(struct symtable *st, void *ast)
{
	int end;

	if (st->st_pass == 1)
		symtable_update_free_vars(st);
	Py_DECREF(st->st_cur);
	end = PyList_GET_SIZE(st->st_stack) - 1;
	st->st_cur = (PySymtableEntryObject *)PyList_GET_ITEM(st->st_stack, 
							      end);
	if (PySequence_DelItem(st->st_stack, end) < 0)
		return -1;
	return 0;
}

static int
symtable_enter_scope(struct symtable *st, identifier name, scope_ty scope, 
		     void *ast, int lineno)
{
	PySymtableEntryObject *prev = NULL;

	if (st->st_cur) {
		prev = st->st_cur;
		if (PyList_Append(st->st_stack, (PyObject *)st->st_cur) < 0) {
			Py_DECREF(st->st_cur);
			st->st_errors++;
			return 0;
		}
	}
	st->st_cur = PySymtableEntry_New(st, name, scope, ast, lineno);
	if (name == GET_IDENTIFIER(top))
		st->st_global = st->st_cur->ste_symbols;
	if (prev && st->st_pass == 1) {
		if (PyList_Append(prev->ste_children, 
				  (PyObject *)st->st_cur) < 0) {
			st->st_errors++;
			return 0;
		}
	}
	return 1;
}

/* macros to help visit expressions that result in def or use 
   will return 0 from current function on error.
*/
#define V_EXPR(ST, E) { \
	if (!symtable_visit_expr((ST), (E))) \
		return 0; \
}
						    
#define V_EXPRS(ST, LE) { \
	if (!symtable_visit_exprs((ST), (LE))) \
		return 0; \
}

#define V_STMTS(ST, LS) { \
	if (!symtable_visit_stmts((ST), (LS))) \
		return 0; \
}
						    
static int
symtable_visit_stmts(struct symtable *st, asdl_seq *seq)
{
	int i;
	for (i = 0; i < asdl_seq_LEN(seq); i++) {
		stmt_ty s = asdl_seq_get(seq, i);
		if (!symtable_visit_stmt(st, s))
			return 0;
	}
	return 1;
}

static int
symtable_visit_stmt(struct symtable *st, stmt_ty s)
{
	switch (s->kind) {
        case FunctionDef_kind:
		symtable_add_def_o(st, s->v.FunctionDef.name, DEF_LOCAL);
		if (!symtable_visit_arguments(st, s->v.FunctionDef.args))
			return 0;
		symtable_enter_scope(st, s->v.FunctionDef.name, FunctionScope,
				     (void *)s, s->lineno);
		V_STMTS(st, s->v.FunctionDef.body);
		symtable_exit_scope(st, s);
		break;
        case ClassDef_kind:
		symtable_add_def_o(st, s->v.ClassDef.name, DEF_LOCAL);
		V_EXPRS(st, s->v.ClassDef.bases);
		symtable_enter_scope(st, s->v.ClassDef.name, ClassScope, 
				     (void *)s, s->lineno);
		V_STMTS(st, s->v.ClassDef.body);
		symtable_exit_scope(st, s);
		break;
        case Return_kind:
		if (s->v.Return.value)
			V_EXPR(st, s->v.Return.value);
		break;
        case Yield_kind:
		V_EXPR(st, s->v.Yield.value);
		break;
        case Delete_kind:
		V_EXPRS(st, s->v.Delete.targets);
		break;
        case Assign_kind:
		V_EXPRS(st, s->v.Assign.targets);
		V_EXPR(st, s->v.Assign.value);
		break;
        case AugAssign_kind:
		V_EXPR(st, s->v.AugAssign.target);
		V_EXPR(st, s->v.AugAssign.value);
		break;
        case Print_kind:
		if (s->v.Print.dest)
			V_EXPR(st, s->v.Print.dest);
		V_EXPRS(st, s->v.Print.value);
		break;
        case For_kind:
		V_EXPR(st, s->v.For.target);
		V_EXPR(st, s->v.For.iter);
		V_STMTS(st, s->v.For.body);
		if (s->v.For.orelse)
			V_STMTS(st, s->v.For.orelse);
		break;
        case While_kind:
		V_EXPR(st, s->v.While.test);
		V_STMTS(st, s->v.While.body);
		if (s->v.While.orelse)
			V_STMTS(st, s->v.While.orelse);
		break;
        case If_kind:
		V_EXPR(st, s->v.If.test);
		V_STMTS(st, s->v.If.body);
		if (s->v.If.orelse)
			V_STMTS(st, s->v.If.orelse);
		break;
        case Raise_kind:
		if (s->v.Raise.type) {
			V_EXPR(st, s->v.Raise.type);
			if (s->v.Raise.inst) {
				V_EXPR(st, s->v.Raise.inst);
				if (s->v.Raise.tback)
					V_EXPR(st, s->v.Raise.tback);
			}
		}
		break;
        case TryExcept_kind:
		V_STMTS(st, s->v.TryExcept.body);
		V_STMTS(st, s->v.TryExcept.orelse);
		if (!symtable_visit_excepthandles(st, s->v.TryExcept.handlers))
			return 0;
		break;
        case TryFinally_kind:
		V_STMTS(st, s->v.TryFinally.body);
		V_STMTS(st, s->v.TryFinally.finalbody);
		break;
        case Assert_kind:
		V_EXPR(st, s->v.Assert.test);
		if (s->v.Assert.msg)
			V_EXPR(st, s->v.Assert.msg);
		break;
        case Import_kind:
		if (!symtable_visit_aliases(st, s->v.Import.names))
			return 0;
		break;
        case ImportFrom_kind:
		if (!symtable_visit_aliases(st, s->v.ImportFrom.names))
			return 0;
		break;
        case Exec_kind:
		V_EXPR(st, s->v.Exec.body);
		if (s->v.Exec.globals) {
			V_EXPR(st, s->v.Exec.globals);
			if (s->v.Exec.locals) 
				V_EXPR(st, s->v.Exec.locals);
		}
		break;
        case Global_kind: {
		int i;
		asdl_seq *seq = s->v.Global.names;
		for (i = 0; i < asdl_seq_SIZE(seq); i++)
			symtable_add_def_o(st, asdl_seq_get(seq, i),
					   DEF_GLOBAL);
		
		break;
	}
        case Expr_kind:
		V_EXPR(st, s->v.Expr.value);
		break;
        case Pass_kind:
        case Break_kind:
        case Continue_kind:
		/* nothing to do here */
		break;
	default:
		PyErr_Format(PyExc_AssertionError,
			     "invalid statement kind: %d\n", s->kind);
		return 0;
	}
	return 1;
}

static int
symtable_visit_exprs(struct symtable *st, asdl_seq *seq)
{
	int i;
	for (i = 0; i < asdl_seq_LEN(seq); i++) {
		stmt_ty s = asdl_seq_get(seq, i);
		if (!symtable_visit_expr(st, s))
			return 0;
	}
	return 1;
}

static int 
symtable_visit_expr(struct symtable *st, expr_ty e)
{
	switch (e->kind) {
        case BoolOp_kind:
		V_EXPRS(st, e->v.BoolOp.values);
		break;
        case BinOp_kind:
		V_EXPR(st, e->v.BinOp.left);
		V_EXPR(st, e->v.BinOp.right);
		break;
        case UnaryOp_kind:
		V_EXPR(st, e->v.UnaryOp.operand);
		break;
        case Lambda_kind:
		symtable_add_def_o(st, GET_IDENTIFIER(lambda), DEF_LOCAL);
		if (!symtable_visit_arguments(st, e->v.Lambda.args))
			return 0;
		/* XXX need to make name an identifier
		   XXX how to get line numbers for expressions
		*/
		symtable_enter_scope(st, GET_IDENTIFIER(lambda),
				     FunctionScope, (void *)e, 0);
		V_STMTS(st, e->v.Lambda.body);
		symtable_exit_scope(st, (void *)e);
		break;
        case Dict_kind:
		V_EXPRS(st, e->v.Dict.keys);
		V_EXPRS(st, e->v.Dict.values);
		break;
        case ListComp_kind:
		V_EXPR(st, e->v.ListComp.target);
		if (!symtable_visit_listcomp(e->v.ListComp.generators))
			return 0;
		break;
        case Compare_kind:
		V_EXPR(st, e->v.Compare.left);
		V_EXPRS(st, e->v.Compare.comparators);
		break;
        case Call_kind:
		V_EXPR(st, e->v.Call.func);
		V_EXPRS(st, e->v.Call.args);
		if (!symtable_visit_keyword(st, e->v.Call.keywords))
			return 0;
		if (e->v.Call.starargs)
			V_EXPR(st, e->v.Call.starargs);
		if (e->v.Call.kwargs)
			V_EXPR(st, e->v.Call.kwargs);
		break;
        case Repr_kind:
		V_EXPR(st, e->v.Repr.value);
		break;
        case Num_kind:
        case Str_kind:
		/* Nothing to do here. */
		break;
	/* The following exprs can be assignment targets. */
        case Attribute_kind:
		V_EXPR(st, e->v.Attribute.value);
		break;
        case Subscript_kind:
		V_EXPR(st, e->v.Subscript.value);
		if (!symtable_visit_slice(st, e->v.Subscript.slice))
			return 0;
		break;
        case Name_kind:
		symtable_add_def_o(st, e->v.Name.id, 
				   e->v.Name.ctx == Load ? USE : DEF_LOCAL);
		break;
	/* child nodes of List and Tuple will have expr_context set */
        case List_kind:
		V_EXPRS(st, e->v.List.elts);
		break;
        case Tuple_kind:
		V_EXPRS(st, e->v.Tuple.elts);
		break;
	default:
		PyErr_Format(PyExc_AssertionError,
			     "invalid expression kind: %d\n", e->kind);
		return 0;
	}
	return 1;
}

