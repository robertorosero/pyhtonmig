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
	symtable_enter_scope(st, GET_IDENTIFIER(top), ModuleScope, 
			     (void *)mod, 0);
	/* Any other top-level initialization? */
	if (mod->kind == Module_kind) {
		int i;
		asdl_seq *seq = mod->v.Module.body;
		for (i = 0; i < asdl_seq_LEN(seq); i++)
			if (!symtable_visit_stmt(st, asdl_seq_get(seq, i))) {
				PySymtable_Free(st);
				return NULL;
			}
	}
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
		    return -1;
	    }
	    val |= flag;
	} else
	    val = flag;
	o = PyInt_FromLong(val);
	if (PyDict_SetItem(dict, name, o) < 0) {
		Py_DECREF(o);
		return -1;
	}
	Py_DECREF(o);

	if (flag & DEF_PARAM) {
		if (PyList_Append(st->st_cur->ste_varnames, name) < 0) 
			return -1;
	} else	if (flag & DEF_GLOBAL) {
		/* XXX need to update DEF_GLOBAL for other flags too;
		   perhaps only DEF_FREE_GLOBAL */
		if ((o = PyDict_GetItem(st->st_global, name))) {
			val = PyInt_AS_LONG(o);
			val |= flag;
		} else
			val = flag;
		o = PyInt_FromLong(val);
		if (PyDict_SetItem(st->st_global, name, o) < 0) {
			Py_DECREF(o);
			return -1;
		}
		Py_DECREF(o);
	}
	return 0;
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
		TYPE ## _ty elt = asdl_seq_get(seq, i); \
		if (!symtable_visit_ ## TYPE((ST), elt)) \
			return 0; \
	} \
}
						    
static int
symtable_visit_stmt(struct symtable *st, stmt_ty s)
{
	switch (s->kind) {
        case FunctionDef_kind:
		symtable_add_def(st, s->v.FunctionDef.name, DEF_LOCAL);
		if (s->v.FunctionDef.args->defaults)
			VISIT_SEQ(st, expr, s->v.FunctionDef.args->defaults);
		symtable_enter_scope(st, s->v.FunctionDef.name, FunctionScope,
				     (void *)s, s->lineno);
		VISIT(st, arguments, s->v.FunctionDef.args);
		VISIT_SEQ(st, stmt, s->v.FunctionDef.body);
		symtable_exit_scope(st, s);
		break;
        case ClassDef_kind:
		symtable_add_def(st, s->v.ClassDef.name, DEF_LOCAL);
		VISIT_SEQ(st, expr, s->v.ClassDef.bases);
		symtable_enter_scope(st, s->v.ClassDef.name, ClassScope, 
				     (void *)s, s->lineno);
		VISIT_SEQ(st, stmt, s->v.ClassDef.body);
		symtable_exit_scope(st, s);
		break;
        case Return_kind:
		if (s->v.Return.value)
			VISIT(st, expr, s->v.Return.value);
		break;
        case Yield_kind:
		VISIT(st, expr, s->v.Yield.value);
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
		VISIT_SEQ(st, expr, s->v.Print.value);
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
			symtable_add_def(st, asdl_seq_get(seq, i), DEF_GLOBAL);
		
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
	default:
		PyErr_Format(PyExc_AssertionError,
			     "invalid statement kind: %d\n", s->kind);
		return 0;
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
        case Lambda_kind:
		symtable_add_def(st, GET_IDENTIFIER(lambda), DEF_LOCAL);
		VISIT(st, arguments, e->v.Lambda.args);
		/* XXX how to get line numbers for expressions */
		symtable_enter_scope(st, GET_IDENTIFIER(lambda),
				     FunctionScope, (void *)e, 0);
		VISIT(st, expr, e->v.Lambda.body);
		symtable_exit_scope(st, (void *)e);
		break;
        case Dict_kind:
		VISIT_SEQ(st, expr, e->v.Dict.keys);
		VISIT_SEQ(st, expr, e->v.Dict.values);
		break;
        case ListComp_kind:
		VISIT(st, expr, e->v.ListComp.target);
		VISIT_SEQ(st, listcomp, e->v.ListComp.generators);
		break;
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
		symtable_add_def(st, e->v.Name.id, 
				 e->v.Name.ctx == Load ? USE : DEF_LOCAL);
		break;
	/* child nodes of List and Tuple will have expr_context set */
        case List_kind:
		VISIT_SEQ(st, expr, e->v.List.elts);
		break;
        case Tuple_kind:
		VISIT_SEQ(st, expr, e->v.Tuple.elts);
		break;
	default:
		PyErr_Format(PyExc_AssertionError,
			     "invalid expression kind: %d\n", e->kind);
		return 0;
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
	symtable_add_def(st, id, DEF_PARAM);
	Py_DECREF(id);
	return 1;
}

static int 
symtable_visit_params(struct symtable *st, asdl_seq *args, int toplevel)
{
	int i;
	
	for (i = 0; i < asdl_seq_LEN(args); i++) {
		expr_ty arg = asdl_seq_get(args, i);
		if (arg->kind == Name_kind) {
			assert(arg->v.Name.ctx == Load);
			if (!symtable_add_def(st, arg->v.Name.id, DEF_PARAM))
				return 0;
		}
		else if (arg->kind == Tuple_kind) {
			assert(arg->v.Tuple.ctx == Load);
			if (toplevel) {
				if (!symtable_implicit_arg(st, i))
					return 0;
			}
			if (!symtable_visit_params(st, arg->v.Tuple.elts, 0))
				return 0;
		}
		else {
			/* syntax error */
			return 0;
		}
	}	
	
	return 1;
}

static int 
symtable_visit_arguments(struct symtable *st, arguments_ty a)
{
	/* skip default arguments inside function scope
	   XXX should ast be different?
	*/
	if (!symtable_visit_params(st, a->args, 1))
		return 0;

	if (a->vararg)
		if (!symtable_add_def(st, a->vararg, DEF_PARAM))
			return 0;
	if (a->kwarg)
		if (!symtable_add_def(st, a->kwarg, DEF_PARAM))
			return 0;
	
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
	if (a->asname) {
		if (!symtable_add_def(st, a->asname, DEF_IMPORT))
			return 0;
	}
	else if (!symtable_add_def(st, a->name, DEF_IMPORT))
		return 0;

	return 1;
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


