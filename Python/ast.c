/*
 * This file includes functions to transform a concrete syntax tree (CST) to
 * an abstract syntax tree (AST).  The main function is PyAST_FromNode().
 *
 */
#include "Python.h"
#include "Python-ast.h"
#include "grammar.h"
#include "node.h"
#include "ast.h"
#include "token.h"
#include "parsetok.h"
#include "graminit.h"

#include <assert.h>

/* XXX TO DO
   - re-indent this file (should be done)
   - internal error checking (freeing memory, etc.)
   - syntax errors
*/

/*
  Note:
  
  You should rarely need to use the asdl_seq_free() in this file.
  If you use asdl_seq_free(), you will leak any objects held in the seq.
  If there is an appropriate asdl_*_seq_free() function, use it.
  If there isn't an asdl_*_seq_free() function for you, you will
  need to loop over the data in the sequence and free it.

  asdl_seq* seq;
  int i;

  for (i = 0; i < asdl_seq_LEN(seq); i++)
      free_***(asdl_seq_GET(seq, i));
  asdl_seq_free(seq); / * ok * /

  Almost all of the ast functions return a seq of expr, so you should
  use asdl_expr_seq_free().  The exception is ast_for_suite() which
  returns a seq of stmt's, so use asdl_stmt_seq_free() to free it.

  If asdl_seq_free is appropriate, you should mark it with an ok comment.

  There are still many memory problems in this file even though
  it runs clean in valgrind, save one problem that may have existed
  before the AST.

  Any code which does something like this:

      return ASTconstruct(local, LINENO(n));

  will leak memory.  The problem is if ASTconstruct (e.g., TryFinally)
  cannot allocate memory, local will be leaked.

  There was discussion on python-dev to replace the entire allocation
  scheme in this file with arenas.  Basically rather than allocate
  memory in little blocks with malloc(), we allocate one big honking
  hunk and deref everything into this block.  We would still need
  another block or technique to handle the PyObject*s.

  http://mail.python.org/pipermail/python-dev/2005-November/058138.html
*/

/* Data structure used internally */
struct compiling {
	char *c_encoding; /* source encoding */
};

static PyObject *seq_for_testlist(struct compiling *, const node *);
static PyObject *ast_for_expr(struct compiling *, const node *);
static PyObject *ast_for_stmt(struct compiling *, const node *);
static PyObject *ast_for_suite(struct compiling *, const node *);
static PyObject *ast_for_exprlist(struct compiling *, const node *, PyObject*);
static PyObject *ast_for_testlist(struct compiling *, const node *);
static PyObject *ast_for_testlist_gexp(struct compiling *, const node *);

/* Note different signature for ast_for_call */
static PyObject *ast_for_call(struct compiling *, const node *, PyObject*);

static PyObject *parsenumber(const char *);
static PyObject *parsestr(const char *s, const char *encoding);
static PyObject *parsestrplus(struct compiling *, const node *n);

#ifndef LINENO
#define LINENO(n)	((n)->n_lineno)
#endif

#define NEW_IDENTIFIER(n) PyString_InternFromString(STR(n))

#define Py_RELEASE(var)   do{Py_DECREF(var);var=NULL;}while(0);
#define STEAL_ITEM(l,i,o) do{PyList_SET_ITEM(l,i,o);o=NULL;}while(0);

/* This routine provides an invalid object for the syntax error.
   The outermost routine must unpack this error and create the
   proper object.  We do this so that we don't have to pass
   the filename to everything function.

   XXX Maybe we should just pass the filename...
*/

static int
ast_error(const node *n, const char *errstr)
{
    PyObject *u = Py_BuildValue("zi", errstr, LINENO(n));
    if (!u)
	return 0;
    PyErr_SetObject(PyExc_SyntaxError, u);
    Py_DECREF(u);
    return 0;
}

static void
ast_error_finish(const char *filename)
{
    PyObject *type, *value, *tback, *errstr, *loc, *tmp;
    int lineno;

    assert(PyErr_Occurred());
    if (!PyErr_ExceptionMatches(PyExc_SyntaxError))
	return;

    PyErr_Fetch(&type, &value, &tback);
    errstr = PyTuple_GetItem(value, 0);
    if (!errstr)
	return;
    Py_INCREF(errstr);
    lineno = PyInt_AsLong(PyTuple_GetItem(value, 1));
    if (lineno == -1)
	return;
    Py_DECREF(value);

    loc = PyErr_ProgramText(filename, lineno);
    if (!loc) {
	Py_INCREF(Py_None);
	loc = Py_None;
    }
    tmp = Py_BuildValue("(ziOO)", filename, lineno, Py_None, loc);
    Py_DECREF(loc);
    if (!tmp)
	return;
    value = Py_BuildValue("(OO)", errstr, tmp);
    Py_DECREF(errstr);
    Py_DECREF(tmp);
    if (!value)
	return;
    PyErr_Restore(type, value, tback);
}

/* num_stmts() returns number of contained statements.

   Use this routine to determine how big a sequence is needed for
   the statements in a parse tree.  Its raison d'etre is this bit of
   grammar:

   stmt: simple_stmt | compound_stmt
   simple_stmt: small_stmt (';' small_stmt)* [';'] NEWLINE

   A simple_stmt can contain multiple small_stmt elements joined
   by semicolons.  If the arg is a simple_stmt, the number of
   small_stmt elements is returned.
*/

static int
num_stmts(const node *n)
{
    int i, l;
    node *ch;

    switch (TYPE(n)) {
        case single_input:
            if (TYPE(CHILD(n, 0)) == NEWLINE)
                return 0;
            else
                return num_stmts(CHILD(n, 0));
        case file_input:
            l = 0;
            for (i = 0; i < NCH(n); i++) {
                ch = CHILD(n, i);
                if (TYPE(ch) == stmt)
                    l += num_stmts(ch);
            }
            return l;
        case stmt:
            return num_stmts(CHILD(n, 0));
        case compound_stmt:
            return 1;
        case simple_stmt:
            return NCH(n) / 2; /* Divide by 2 to remove count of semi-colons */
        case suite:
            if (NCH(n) == 1)
                return num_stmts(CHILD(n, 0));
            else {
                l = 0;
                for (i = 2; i < (NCH(n) - 1); i++)
                    l += num_stmts(CHILD(n, i));
                return l;
            }
        default: {
            char buf[128];

            sprintf(buf, "Non-statement found: %d %d\n",
                    TYPE(n), NCH(n));
            Py_FatalError(buf);
        }
    }
    assert(0);
    return 0;
}

/* Transform the CST rooted at node * to the appropriate AST
*/

PyObject*
PyAST_FromNode(const node *n, PyCompilerFlags *flags, const char *filename)
{
    PyObject *result = NULL;
    int i, j, num, pos;
    PyObject *stmts = NULL;
    PyObject *s = NULL;
    PyObject *testlist_ast = NULL;
    node *ch;
    struct compiling c;

    if (flags && flags->cf_flags & PyCF_SOURCE_IS_UTF8) {
            c.c_encoding = "utf-8";
    } else if (TYPE(n) == encoding_decl) {
        c.c_encoding = STR(n);
        n = CHILD(n, 0);
    } else {
        c.c_encoding = NULL;
    }

    switch (TYPE(n)) {
        case file_input:
            stmts = PyList_New(num_stmts(n));
	    pos = 0;
            if (!stmts)
		goto error;
            for (i = 0; i < NCH(n) - 1; i++) {
                ch = CHILD(n, i);
                if (TYPE(ch) == NEWLINE)
                    continue;
                REQ(ch, stmt);
                num = num_stmts(ch);
                if (num == 1) {
                    s = ast_for_stmt(&c, ch);
                    if (!s)
                        goto error;
                    PyList_SET_ITEM(stmts, pos++, s);
                }
                else {
                    ch = CHILD(ch, 0);
                    REQ(ch, simple_stmt);
                    for (j = 0; j < num; j++) {
                        s = ast_for_stmt(&c, CHILD(ch, j * 2));
                        if (!s)
                            goto error;
                        PyList_SET_ITEM(stmts, pos++, s);
                    }
                }
            }
	    assert(pos==PyList_GET_SIZE(stmts));
            result = Module(stmts);
	    goto success;
        case eval_input: {
            /* XXX Why not gen_for here? */
            testlist_ast = ast_for_testlist(&c, CHILD(n, 0));
            if (!testlist_ast)
                goto error;
            result = Expression(testlist_ast);
	    goto success;
        }
        case single_input:
            if (TYPE(CHILD(n, 0)) == NEWLINE) {
                stmts = PyList_New(1);
                if (!stmts)
		    goto error;
		s = Pass(n->n_lineno);
		if (!s)
		    goto error;
		STEAL_ITEM(stmts, 0, s);
		result = Interactive(stmts);
		goto success;
            }
            else {
                n = CHILD(n, 0);
                num = num_stmts(n);
                stmts = PyList_New(num);
                if (!stmts)
		    goto error;
                if (num == 1) {
		    s = ast_for_stmt(&c, n);
		    if (!s)
			goto error;
                    STEAL_ITEM(stmts, 0, s);
                }
                else {
                    /* Only a simple_stmt can contain multiple statements. */
                    REQ(n, simple_stmt);
                    for (i = 0; i < NCH(n); i += 2) {
                        if (TYPE(CHILD(n, i)) == NEWLINE)
                            break;
                        s = ast_for_stmt(&c, CHILD(n, i));
                        if (!s)
                            goto error;
                        STEAL_ITEM(stmts, i / 2, s);
                    }
                }

		result = Interactive(stmts);
		goto success;
            }
        default:
            goto error;
    }
  success:
    Py_XDECREF(stmts);
    Py_XDECREF(s);
    Py_XDECREF(testlist_ast);
    return result;
  error:
    Py_XDECREF(stmts);
    Py_XDECREF(s);
    Py_XDECREF(testlist_ast);
    ast_error_finish(filename);
    return NULL;
}

/* Return the AST repr. of the operator represented as syntax (|, ^, etc.)
*/

static PyObject*
get_operator(const node *n)
{
    switch (TYPE(n)) {
        case VBAR:
            return BitOr();
        case CIRCUMFLEX:
            return BitXor();
        case AMPER:
            return BitAnd();
        case LEFTSHIFT:
            return LShift();
        case RIGHTSHIFT:
            return RShift();
        case PLUS:
            return Add();
        case MINUS:
            return Sub();
        case STAR:
            return Mult();
        case SLASH:
            return Div();
        case DOUBLESLASH:
            return FloorDiv();
        case PERCENT:
            return Mod();
        default:
	    PyErr_BadInternalCall();
            return NULL;
    }
}

/* Set the context ctx for expr_ty e returning 0 on success, -1 on error.

   Only sets context for expr kinds that "can appear in assignment context"
   (according to ../Parser/Python.asdl).  For other expr kinds, it sets
   an appropriate syntax error and returns false.

   If e is a sequential type, items in sequence will also have their context
   set.

*/

static int
set_context(PyObject* _e, PyObject* ctx, const node *n)
{
    struct _expr *e = (struct _expr*)_e;
    PyObject *s = NULL;

#define SET_CTX(x) Py_DECREF(x); Py_INCREF(ctx); x = ctx
    switch (e->_kind) {
        case Attribute_kind:
	    if (Store_Check(ctx) &&
		!strcmp(PyString_AS_STRING(Attribute_attr(e)), "None")) {
		    return ast_error(n, "assignment to None");
	    }
	    SET_CTX(Attribute_ctx(e));
	    break;
        case Subscript_kind:
	    SET_CTX(Subscript_ctx(e));
	    break;
        case Name_kind:
	    if (Store_Check(ctx) &&
		!strcmp(PyString_AS_STRING(Name_id(e)), "None")) {
		    return ast_error(n, "assignment to None");
	    }
	    SET_CTX(Name_ctx(e));
	    break;
        case List_kind:
	    SET_CTX(List_ctx(e));
	    s = List_elts(e);
	    break;
        case Tuple_kind:
            if (PyList_GET_SIZE(Tuple_elts(e)) == 0) 
                return ast_error(n, "can't assign to ()");
	    SET_CTX(Tuple_ctx(e));
	    s = Tuple_elts(e);
	    break;
        case Call_kind:
	    if (Store_Check(ctx))
		return ast_error(n, "can't assign to function call");
	    else if (Del_Check(ctx))
		return ast_error(n, "can't delete function call");
	    else
		return ast_error(n, "unexpected operation on function call");
	    break;
        case BinOp_kind:
            return ast_error(n, "can't assign to operator");
        case GeneratorExp_kind:
            return ast_error(n, "assignment to generator expression "
                             "not possible");
        case Num_kind:
        case Str_kind:
	    return ast_error(n, "can't assign to literal");
        default: {
	   char buf[300];
	   PyOS_snprintf(buf, sizeof(buf), 
			 "unexpected expression in assignment %d (line %d)", 
			 e->_kind, e->lineno);
	   return ast_error(n, buf);
       }
    }
    /* If the LHS is a list or tuple, we need to set the assignment
       context for all the tuple elements.  
    */
    if (s) {
	int i;

	for (i = 0; i < PyList_GET_SIZE(s); i++) {
	    if (!set_context(PyList_GET_ITEM(s, i), ctx, n))
		return 0;
	}
    }
    return 1;
#undef SET_CTX
}

static PyObject*
ast_for_augassign(const node *n)
{
    REQ(n, augassign);
    n = CHILD(n, 0);
    switch (STR(n)[0]) {
        case '+':
            return Add();
        case '-':
            return Sub();
        case '/':
            if (STR(n)[1] == '/')
                return FloorDiv();
            else
                return Div();
        case '%':
            return Mod();
        case '<':
            return LShift();
        case '>':
            return RShift();
        case '&':
            return BitAnd();
        case '^':
            return BitXor();
        case '|':
            return BitOr();
        case '*':
            if (STR(n)[1] == '*')
                return Pow();
            else
                return Mult();
        default:
            PyErr_Format(PyExc_SystemError, "invalid augassign: %s", STR(n));
            return 0;
    }
}

static PyObject*
ast_for_comp_op(const node *n)
{
    /* comp_op: '<'|'>'|'=='|'>='|'<='|'<>'|'!='|'in'|'not' 'in'|'is'
               |'is' 'not'
    */
    REQ(n, comp_op);
    if (NCH(n) == 1) {
	n = CHILD(n, 0);
	switch (TYPE(n)) {
            case LESS:
                return Lt();
            case GREATER:
                return Gt();
            case EQEQUAL:			/* == */
                return Eq();
            case LESSEQUAL:
                return LtE();
            case GREATEREQUAL:
                return GtE();
            case NOTEQUAL:
                return NotEq();
            case NAME:
                if (strcmp(STR(n), "in") == 0)
                    return In();
                if (strcmp(STR(n), "is") == 0)
                    return Is();
            default:
                PyErr_Format(PyExc_SystemError, "invalid comp_op: %s",
                             STR(n));
                return 0;
	}
    }
    else if (NCH(n) == 2) {
	/* handle "not in" and "is not" */
	switch (TYPE(CHILD(n, 0))) {
            case NAME:
                if (strcmp(STR(CHILD(n, 1)), "in") == 0)
                    return NotIn();
                if (strcmp(STR(CHILD(n, 0)), "is") == 0)
                    return IsNot();
            default:
                PyErr_Format(PyExc_SystemError, "invalid comp_op: %s %s",
                             STR(CHILD(n, 0)), STR(CHILD(n, 1)));
                return 0;
	}
    }
    PyErr_Format(PyExc_SystemError, "invalid comp_op: has %d children",
                 NCH(n));
    return 0;
}

static PyObject *
seq_for_testlist(struct compiling *c, const node *n)
{
    /* testlist: test (',' test)* [','] */
    PyObject *result = NULL;
    PyObject *seq = NULL;
    PyObject *expression;
    int i;
    assert(TYPE(n) == testlist
	   || TYPE(n) == listmaker
	   || TYPE(n) == testlist_gexp
	   || TYPE(n) == testlist_safe
	   );

    seq = PyList_New((NCH(n) + 1) / 2);
    if (!seq)
        return NULL;

    for (i = 0; i < NCH(n); i += 2) {
        REQ(CHILD(n, i), test);

        expression = ast_for_expr(c, CHILD(n, i));
        if (!expression) {
	    goto error;
        }

        assert(i / 2 < seq->size);
        PyList_SET_ITEM(seq, i / 2, expression);
    }
    result = seq;
    Py_INCREF(result);
    /* fall through */
 error:
    Py_XDECREF(seq);
    return result;
}

static PyObject*
compiler_complex_args(const node *n)
{
    int i, len = (NCH(n) + 1) / 2;
    PyObject *result = NULL;
    PyObject *args = PyList_New(len);
    PyObject *store = NULL;
    PyObject *arg = NULL;
    if (!args)
	goto error;
    store = Store();
    if (!store)
	goto error;

    REQ(n, fplist);

    for (i = 0; i < len; i++) {
        const node *child = CHILD(CHILD(n, 2*i), 0);
        if (TYPE(child) == NAME) {
	    if (!strcmp(STR(child), "None")) {
		ast_error(child, "assignment to None");
		goto error;
	    }
            arg = Name(NEW_IDENTIFIER(child), store, LINENO(child));
	}
        else
            arg = compiler_complex_args(CHILD(CHILD(n, 2*i), 1));
	if (!set_context(arg, store, n))
	    goto error;
        PyList_SET_ITEM(args, i, arg);
    }

    result = Tuple(args, store, LINENO(n));
    set_context(result, store, n);
 error:
    Py_XDECREF(args);
    Py_XDECREF(arg);
    Py_XDECREF(store);
    return result;
}

/* Create AST for argument list.

   XXX TO DO:
       - check for invalid argument lists like normal after default
*/

static PyObject*
ast_for_arguments(struct compiling *c, const node *n)
{
    /* parameters: '(' [varargslist] ')'
       varargslist: (fpdef ['=' test] ',')* ('*' NAME [',' '**' NAME]
            | '**' NAME) | fpdef ['=' test] (',' fpdef ['=' test])* [',']
    */
    int i, n_args = 0, n_defaults = 0, found_default = 0;
    int defno = 0, argno = 0;
    PyObject *result = NULL;
    PyObject *args = NULL;
    PyObject *defaults = NULL;
    PyObject *vararg = NULL;
    PyObject *kwarg = NULL;
    PyObject *e = NULL;
    PyObject *id = NULL;
    PyObject *param = NULL;
    node *ch;

    if (TYPE(n) == parameters) {
	if (NCH(n) == 2) /* () as argument list */
	    return arguments(NULL, NULL, NULL, NULL);
	n = CHILD(n, 1);
    }
    REQ(n, varargslist);

    /* first count the number of normal args & defaults */
    for (i = 0; i < NCH(n); i++) {
	ch = CHILD(n, i);
	if (TYPE(ch) == fpdef) {
	    n_args++;
	}
	if (TYPE(ch) == EQUAL)
	    n_defaults++;
    }
    args = (n_args ? PyList_New(n_args) : NULL);
    if (!args && n_args)
    	goto error;
    defaults = (n_defaults ? PyList_New(n_defaults) : NULL);
    if (!defaults && n_defaults)
        goto error;

    /* fpdef: NAME | '(' fplist ')'
       fplist: fpdef (',' fpdef)* [',']
    */
    i = 0;
    while (i < NCH(n)) {
	ch = CHILD(n, i);
	switch (TYPE(ch)) {
            case fpdef:
                /* XXX Need to worry about checking if TYPE(CHILD(n, i+1)) is
                   anything other than EQUAL or a comma? */
                /* XXX Should NCH(n) check be made a separate check? */
                if (i + 1 < NCH(n) && TYPE(CHILD(n, i + 1)) == EQUAL) {
		    e = ast_for_expr(c, CHILD(n, i + 2));
		    if (!e)
			goto error;
                    STEAL_ITEM(defaults, defno++, e);
                    i += 2;
		    found_default = 1;
                }
		else if (found_default) {
		    ast_error(n, 
			     "non-default argument follows default argument");
		    goto error;
		}

                if (NCH(ch) == 3) {
		    e = compiler_complex_args(CHILD(ch, 1));
		    if (!e)
			goto error;
                    STEAL_ITEM(args, argno++, e);
		}
                else if (TYPE(CHILD(ch, 0)) == NAME) {
		    if (!strcmp(STR(CHILD(ch, 0)), "None")) {
			    ast_error(CHILD(ch, 0), "assignment to None");
			    goto error;
		    }
		    id = NEW_IDENTIFIER(CHILD(ch, 0));
		    if (!id) goto error;
		    if (!param) param = Param();
		    if (!param) goto error;
                    e = Name(id, param, LINENO(ch));
                    if (!e)
                        goto error;
                    STEAL_ITEM(args, argno++, e);
					 
		}
                i += 2; /* the name and the comma */
                break;
            case STAR:
		if (!strcmp(STR(CHILD(n, i+1)), "None")) {
			ast_error(CHILD(n, i+1), "assignment to None");
			goto error;
		}
                vararg = NEW_IDENTIFIER(CHILD(n, i+1));
		if (!vararg)
		    goto error;
                i += 3;
                break;
            case DOUBLESTAR:
		if (!strcmp(STR(CHILD(n, i+1)), "None")) {
			ast_error(CHILD(n, i+1), "assignment to None");
			goto error;
		}
                kwarg = NEW_IDENTIFIER(CHILD(n, i+1));
		if (!kwarg)
		    goto error;
                i += 3;
                break;
            default:
                PyErr_Format(PyExc_SystemError,
                             "unexpected node in varargslist: %d @ %d",
                             TYPE(ch), i);
                goto error;
	}
    }

    result = arguments(args, vararg, kwarg, defaults);
 error:
    Py_XDECREF(vararg);
    Py_XDECREF(kwarg);
    Py_XDECREF(args);
    Py_XDECREF(defaults);
    Py_XDECREF(e);
    Py_XDECREF(id);
    Py_XDECREF(param);
    return result;
}

static PyObject*
ast_for_dotted_name(struct compiling *c, const node *n)
{
    PyObject *result = NULL;
    PyObject *e = NULL;
    PyObject *attrib = NULL;
    PyObject *id = NULL;
    PyObject *load = NULL;
    int i;

    REQ(n, dotted_name);
    
    id = NEW_IDENTIFIER(CHILD(n, 0));
    if (!id)
        goto error;
    load = Load();
    if (!load)
	goto error;
    e = Name(id, load, LINENO(n));
    if (!result)
	goto error;
    id = NULL;

    for (i = 2; i < NCH(n); i+=2) {
        id = NEW_IDENTIFIER(CHILD(n, i));
	if (!id)
	    goto error;
	attrib = Attribute(e, id, load, LINENO(CHILD(n, i)));
	if (!attrib)
	    goto error;
	e = attrib;
	attrib = NULL;
    }
    result = e;
    e = NULL;
    
  error:
    Py_XDECREF(id);
    Py_XDECREF(e);
    Py_XDECREF(attrib);
    Py_XDECREF(load);
    return NULL;
}

static PyObject*
ast_for_decorator(struct compiling *c, const node *n)
{
    /* decorator: '@' dotted_name [ '(' [arglist] ')' ] NEWLINE */
    PyObject *result = NULL;
    PyObject *d = NULL;
    PyObject *name_expr = NULL;
    
    REQ(n, decorator);
    
    if ((NCH(n) < 3 && NCH(n) != 5 && NCH(n) != 6)
	|| TYPE(CHILD(n, 0)) != AT || TYPE(RCHILD(n, -1)) != NEWLINE) {
	ast_error(n, "Invalid decorator node");
	goto error;
    }
    
    name_expr = ast_for_dotted_name(c, CHILD(n, 1));
    if (!name_expr)
	goto error;
	
    if (NCH(n) == 3) { /* No arguments */
	d = name_expr;
	name_expr = NULL;
    }
    else if (NCH(n) == 5) { /* Call with no arguments */
	d = Call(name_expr, NULL, NULL, NULL, NULL, LINENO(n));
	if (!d)
	    goto error;
    }
    else {
	d = ast_for_call(c, CHILD(n, 3), name_expr);
	if (!d)
	    goto error;
    }

    result = d;
    d = NULL;
    
  error:
    Py_XDECREF(name_expr);
    Py_XDECREF(d);
    return result;
}

static PyObject*
ast_for_decorators(struct compiling *c, const node *n)
{
    PyObject *result = NULL;
    PyObject *decorator_seq = NULL;
    PyObject *d = NULL;
    int i;
    
    REQ(n, decorators);

    decorator_seq = PyList_New(NCH(n));
    if (!decorator_seq)
        goto error;
	
    for (i = 0; i < NCH(n); i++) {
	d = ast_for_decorator(c, CHILD(n, i));
	if (!d)
	    goto error;
	STEAL_ITEM(decorator_seq, i, d);
    }
    result = decorator_seq;
    decorator_seq = NULL;
  error:
    Py_XDECREF(decorator_seq);
    Py_XDECREF(d);
    return result;
}

static PyObject*
ast_for_funcdef(struct compiling *c, const node *n)
{
    /* funcdef: 'def' [decorators] NAME parameters ':' suite */
    PyObject *result = NULL;
    PyObject *name = NULL;
    PyObject *args = NULL;
    PyObject *body = NULL;
    PyObject *decorator_seq = NULL;
    int name_i;

    REQ(n, funcdef);

    if (NCH(n) == 6) { /* decorators are present */
	decorator_seq = ast_for_decorators(c, CHILD(n, 0));
	if (!decorator_seq)
	    goto error;
	name_i = 2;
    }
    else {
	name_i = 1;
    }

    name = NEW_IDENTIFIER(CHILD(n, name_i));
    if (!name)
	goto error;
    else if (!strcmp(STR(CHILD(n, name_i)), "None")) {
	ast_error(CHILD(n, name_i), "assignment to None");
	goto error;
    }
    args = ast_for_arguments(c, CHILD(n, name_i + 1));
    if (!args)
	goto error;
    body = ast_for_suite(c, CHILD(n, name_i + 3));
    if (!body)
	goto error;

    result = FunctionDef(name, args, body, decorator_seq, LINENO(n));

error:
    Py_XDECREF(body);
    Py_XDECREF(decorator_seq);
    Py_XDECREF(args);
    Py_XDECREF(name);
    return result;
}

static PyObject*
ast_for_lambdef(struct compiling *c, const node *n)
{
    /* lambdef: 'lambda' [varargslist] ':' test */
    PyObject *result = NULL;
    PyObject *args = NULL;
    PyObject *expression = NULL;

    if (NCH(n) == 3) {
        args = arguments(NULL, NULL, NULL, NULL);
        if (!args)
            goto error;
        expression = ast_for_expr(c, CHILD(n, 2));
        if (!expression)
            goto error;
    }
    else {
        args = ast_for_arguments(c, CHILD(n, 1));
        if (!args)
            goto error;
        expression = ast_for_expr(c, CHILD(n, 3));
        if (!expression)
            goto error;
    }

    result = Lambda(args, expression, LINENO(n));
 error:
    Py_XDECREF(args);
    Py_XDECREF(expression);
    return result;
}

/* Count the number of 'for' loop in a list comprehension.

   Helper for ast_for_listcomp().
*/

static int
count_list_fors(const node *n)
{
    int n_fors = 0;
    node *ch = CHILD(n, 1);

 count_list_for:
    n_fors++;
    REQ(ch, list_for);
    if (NCH(ch) == 5)
	ch = CHILD(ch, 4);
    else
	return n_fors;
 count_list_iter:
    REQ(ch, list_iter);
    ch = CHILD(ch, 0);
    if (TYPE(ch) == list_for)
	goto count_list_for;
    else if (TYPE(ch) == list_if) {
        if (NCH(ch) == 3) {
            ch = CHILD(ch, 2);
            goto count_list_iter;
        }
        else
            return n_fors;
    }
    else {
        /* Should never be reached */
        PyErr_SetString(PyExc_SystemError, "logic error in count_list_fors");
        return -1;
    }
}

/* Count the number of 'if' statements in a list comprehension.

   Helper for ast_for_listcomp().
*/

static int
count_list_ifs(const node *n)
{
    int n_ifs = 0;

 count_list_iter:
    REQ(n, list_iter);
    if (TYPE(CHILD(n, 0)) == list_for)
	return n_ifs;
    n = CHILD(n, 0);
    REQ(n, list_if);
    n_ifs++;
    if (NCH(n) == 2)
	return n_ifs;
    n = CHILD(n, 2);
    goto count_list_iter;
}

static PyObject*
ast_for_listcomp(struct compiling *c, const node *n)
{
    /* listmaker: test ( list_for | (',' test)* [','] )
       list_for: 'for' exprlist 'in' testlist_safe [list_iter]
       list_iter: list_for | list_if
       list_if: 'if' test [list_iter]
       testlist_safe: test [(',' test)+ [',']]
    */
    PyObject *result;
    PyObject *elt = NULL;
    PyObject *listcomps = NULL;
    PyObject *t = NULL;
    PyObject *expression = NULL;
    PyObject *lc = NULL;
    PyObject *store = NULL;
    PyObject *ifs = NULL;
    PyObject *tmp = NULL;
    int i, n_fors;
    node *ch;

    REQ(n, listmaker);
    assert(NCH(n) > 1);

    elt = ast_for_expr(c, CHILD(n, 0));
    if (!elt)
        goto error;

    n_fors = count_list_fors(n);
    if (n_fors == -1)
        goto error;

    listcomps = PyList_New(n_fors);
    if (!listcomps)
    	goto error;
    
    ch = CHILD(n, 1);
    for (i = 0; i < n_fors; i++) {
	/* each variable should be NULL each round */
	assert(lc == NULL);
	assert(t == NULL);
	assert(expression == NULL);
	assert(ifs == NULL);

	REQ(ch, list_for);

	if (!store) store = Store();
	if (!store) goto error;
	t = ast_for_exprlist(c, CHILD(ch, 1), store);
        if (!t)
	    goto error;
        expression = ast_for_testlist(c, CHILD(ch, 3));
        if (!expression)
	    goto error;

	if (PyList_GET_SIZE(t) == 1) {
	    lc = comprehension(PyList_GET_ITEM(t, 0), expression, NULL);
	    if (!lc)
		goto error;
	}
	else {
	    tmp = Tuple(t, store, LINENO(ch));
	    if (!t)
		goto error;
	    lc = comprehension(tmp, expression, NULL);
	    if (!lc)
		goto error;
	    Py_RELEASE(tmp);
	}
	Py_RELEASE(t);
	Py_RELEASE(expression);

	if (NCH(ch) == 5) {
	    int j, n_ifs;

	    ch = CHILD(ch, 4);
	    n_ifs = count_list_ifs(ch);
            if (n_ifs == -1)
		goto error;

	    ifs = PyList_New(n_ifs);
	    if (!ifs)
		goto error;

	    for (j = 0; j < n_ifs; j++) {
		REQ(ch, list_iter);

		ch = CHILD(ch, 0);
		REQ(ch, list_if);

		t = ast_for_expr(c, CHILD(ch, 1));
		if (!t)
		    goto error;
		STEAL_ITEM(ifs, j, t);
		if (NCH(ch) == 3)
		    ch = CHILD(ch, 2);
	    }
	    /* on exit, must guarantee that ch is a list_for */
	    if (TYPE(ch) == list_iter)
		ch = CHILD(ch, 0);
	    Py_DECREF(comprehension_ifs(lc));
	    comprehension_ifs(lc) = ifs;
	    ifs = NULL;
	}
	STEAL_ITEM(listcomps, i, lc);
    }

    result = ListComp(elt, listcomps, LINENO(n));
 error:
    Py_XDECREF(elt);
    Py_XDECREF(listcomps);
    Py_XDECREF(t);
    Py_XDECREF(expression);
    Py_XDECREF(lc);
    Py_XDECREF(store);
    Py_XDECREF(ifs);
    Py_XDECREF(tmp);
    return result;
}

/*
   Count the number of 'for' loops in a generator expression.

   Helper for ast_for_genexp().
*/

static int
count_gen_fors(const node *n)
{
	int n_fors = 0;
	node *ch = CHILD(n, 1);

 count_gen_for:
	n_fors++;
	REQ(ch, gen_for);
	if (NCH(ch) == 5)
		ch = CHILD(ch, 4);
	else
		return n_fors;
 count_gen_iter:
	REQ(ch, gen_iter);
	ch = CHILD(ch, 0);
	if (TYPE(ch) == gen_for)
		goto count_gen_for;
	else if (TYPE(ch) == gen_if) {
		if (NCH(ch) == 3) {
			ch = CHILD(ch, 2);
			goto count_gen_iter;
		}
		else
		    return n_fors;
	}
	else {
		/* Should never be reached */
		PyErr_SetString(PyExc_SystemError,
				"logic error in count_gen_fors");
		return -1;
	}
}

/* Count the number of 'if' statements in a generator expression.

   Helper for ast_for_genexp().
*/

static int
count_gen_ifs(const node *n)
{
	int n_ifs = 0;

	while (1) {
		REQ(n, gen_iter);
		if (TYPE(CHILD(n, 0)) == gen_for)
			return n_ifs;
		n = CHILD(n, 0);
		REQ(n, gen_if);
		n_ifs++;
		if (NCH(n) == 2)
			return n_ifs;
		n = CHILD(n, 2);
	}
}

static PyObject*
ast_for_genexp(struct compiling *c, const node *n)
{
    /* testlist_gexp: test ( gen_for | (',' test)* [','] )
       argument: [test '='] test [gen_for]	 # Really [keyword '='] test */
    PyObject *result = NULL;
    PyObject *elt = NULL;
    PyObject *genexps = NULL;
    PyObject *ge = NULL;
    PyObject *t = NULL;
    PyObject *expression = NULL;
    PyObject *store = NULL;
    PyObject *tmp = NULL;
    int i, n_fors;
    node *ch;
    
    assert(TYPE(n) == (testlist_gexp) || TYPE(n) == (argument));
    assert(NCH(n) > 1);
    
    elt = ast_for_expr(c, CHILD(n, 0));
    if (!elt)
	goto error;
    
    n_fors = count_gen_fors(n);
    if (n_fors == -1)
        goto error;
    
    genexps = PyList_New(n_fors);
    if (!genexps)
	goto error;

    store = Store();
    if (!store)
	goto error;
    
    ch = CHILD(n, 1);
    for (i = 0; i < n_fors; i++) {
        assert(ge == NULL);
	assert(t == NULL);
	assert(expression == NULL);
        
        REQ(ch, gen_for);
        
        t = ast_for_exprlist(c, CHILD(ch, 1), store);
        if (!t)
	    goto error;
        expression = ast_for_expr(c, CHILD(ch, 3));
        if (!expression) 
	    goto error;
        
        if (PyList_GET_SIZE(t) == 1) {
            ge = comprehension(PyList_GET_ITEM(t, 0), expression,
                               NULL);
	}
        else {
	    tmp = Tuple(t, store, LINENO(ch));
	    if (!tmp)
		goto error;
            ge = comprehension(tmp, expression, NULL);
	    if (!ge)
		goto error;
	    Py_RELEASE(tmp);
	}

	if (!ge)
	    goto error;
	Py_RELEASE(t);
	Py_RELEASE(expression);
        
        if (NCH(ch) == 5) {
            int j, n_ifs;
            PyObject *ifs = NULL;
            
            ch = CHILD(ch, 4);
            n_ifs = count_gen_ifs(ch);
            if (n_ifs == -1)
		goto error;
            
            ifs = PyList_New(n_ifs);
            if (!ifs)
		goto error;
            
            for (j = 0; j < n_ifs; j++) {
                REQ(ch, gen_iter);
                ch = CHILD(ch, 0);
                REQ(ch, gen_if);
                
                expression = ast_for_expr(c, CHILD(ch, 1));
                if (!expression)
		    goto error;
                STEAL_ITEM(ifs, j, expression);
                if (NCH(ch) == 3)
                    ch = CHILD(ch, 2);
            }
            /* on exit, must guarantee that ch is a gen_for */
            if (TYPE(ch) == gen_iter)
                ch = CHILD(ch, 0);
	    Py_DECREF(comprehension_ifs(ge));
	    Py_INCREF(ifs);
	    comprehension_ifs(ge) = ifs;
        }
        STEAL_ITEM(genexps, i, ge);
    }
    
    result = GeneratorExp(elt, genexps, LINENO(n));
 error:
    Py_XDECREF(elt);
    Py_XDECREF(genexps);
    Py_XDECREF(ge);
    Py_XDECREF(t);
    Py_XDECREF(expression);
    return result;
}

static PyObject*
ast_for_atom(struct compiling *c, const node *n)
{
    /* atom: '(' [yield_expr|testlist_gexp] ')' | '[' [listmaker] ']'
       | '{' [dictmaker] '}' | '`' testlist '`' | NAME | NUMBER | STRING+
    */
    PyObject *tmp = NULL;
    PyObject *elts = NULL; 
    PyObject *result = NULL;
    PyObject *keys = NULL; 
    PyObject *values = NULL;
    node *ch = CHILD(n, 0);
    
    switch (TYPE(ch)) {
    case NAME: {
	/* All names start in Load context, but may later be
	   changed. */
	PyObject *tmp = Load();
	if (!tmp)
	    goto error;
	result = Name(NEW_IDENTIFIER(ch), tmp, LINENO(n));
	break;
    }
    case STRING: {
	PyObject *str = parsestrplus(c, n);
	
	if (!str)
	    goto error;
	
	result = Str(str, LINENO(n));
	break;
    }
    case NUMBER: {
	tmp = parsenumber(STR(ch));
	
	if (!tmp)
	    goto error;
	
	result = Num(tmp, LINENO(n));
	break;
    }
    case LPAR: {/* some parenthesized expressions */
	ch = CHILD(n, 1);
	
	if (TYPE(ch) == RPAR) {
	    tmp = Load();
	    if (!tmp)
		goto error;
	    result = Tuple(NULL, tmp, LINENO(n));
	} 
	else if (TYPE(ch) == yield_expr)
	    result = ast_for_expr(c, ch);
	else if ((NCH(ch) > 1) && (TYPE(CHILD(ch, 1)) == gen_for))
	    result = ast_for_genexp(c, ch);
	else
	    result = ast_for_testlist_gexp(c, ch);
	break;
    }
    case LSQB: /* list (or list comprehension) */
	tmp = Load();
	if (!tmp)
	    goto error;
	ch = CHILD(n, 1);
	
	if (TYPE(ch) == RSQB)
	    result = List(NULL, tmp, LINENO(n));
	else {
	    REQ(ch, listmaker);
	    if (NCH(ch) == 1 || TYPE(CHILD(ch, 1)) == COMMA) {
		elts = seq_for_testlist(c, ch);
		
		if (!elts)
		    return NULL;
	    
		result = List(elts, tmp, LINENO(n));
	    }
	    else
		result = ast_for_listcomp(c, ch);
	}
	break;
    case LBRACE: {
	/* dictmaker: test ':' test (',' test ':' test)* [','] */
	int i, size;
	
	ch = CHILD(n, 1);
	size = (NCH(ch) + 1) / 4; /* +1 in case no trailing comma */
	keys = PyList_New(size);
	if (!keys)
	    goto error;
	
	values = PyList_New(size);
	if (!values)
	    goto error;
	
	for (i = 0; i < NCH(ch); i += 4) {
	    
	    tmp = ast_for_expr(c, CHILD(ch, i));
	    if (!tmp)
		goto error;
	    
	    STEAL_ITEM(keys, i / 4, tmp);
	    
	    tmp = ast_for_expr(c, CHILD(ch, i + 2));
	    if (!tmp)
		goto error;

	    STEAL_ITEM(values, i / 4, tmp);
	}
	result = Dict(keys, values, LINENO(n));
	break;
    }
    case BACKQUOTE: { /* repr */
	tmp = ast_for_testlist(c, CHILD(n, 1));
	
	if (!tmp)
	    goto error;
	
	result = Repr(tmp, LINENO(n));
	break;
    }
    default:
	PyErr_Format(PyExc_SystemError, "unhandled atom %d", TYPE(ch));
    }
 error:
    Py_XDECREF(tmp);
    Py_XDECREF(elts);
    Py_XDECREF(keys);
    Py_XDECREF(values);
    return result;
}

static PyObject*
ast_for_slice(struct compiling *c, const node *n)
{
    PyObject *result = NULL;
    node *ch;
    PyObject *lower = NULL;
    PyObject *upper = NULL;
    PyObject *step = NULL;

    REQ(n, subscript);

    /*
       subscript: '.' '.' '.' | test | [test] ':' [test] [sliceop]
       sliceop: ':' [test]
    */
    ch = CHILD(n, 0);
    if (TYPE(ch) == DOT)
	return Ellipsis();

    if (NCH(n) == 1 && TYPE(ch) == test) {
        /* 'step' variable hold no significance in terms of being used over
           other vars */
        step = ast_for_expr(c, ch); 
        if (!step)
            goto error;
            
	result = Index(step);
	goto success;
    }

    if (TYPE(ch) == test) {
	lower = ast_for_expr(c, ch);
        if (!lower)
            goto error;
    }

    /* If there's an upper bound it's in the second or third position. */
    if (TYPE(ch) == COLON) {
	if (NCH(n) > 1) {
	    node *n2 = CHILD(n, 1);

	    if (TYPE(n2) == test) {
		upper = ast_for_expr(c, n2);
                if (!upper)
                    goto error;
            }
	}
    } else if (NCH(n) > 2) {
	node *n2 = CHILD(n, 2);

	if (TYPE(n2) == test) {
	    upper = ast_for_expr(c, n2);
            if (!upper)
                goto error;
        }
    }

    ch = CHILD(n, NCH(n) - 1);
    if (TYPE(ch) == sliceop) {
	if (NCH(ch) == 1)
            /* XXX: If only 1 child, then should just be a colon.  Should we
               just skip assigning and just get to the return? */
	    ch = CHILD(ch, 0);
	else
	    ch = CHILD(ch, 1);
	if (TYPE(ch) == test) {
	    step = ast_for_expr(c, ch);
            if (!step)
                goto error;
        }
    }

    result = Slice(lower, upper, step);
 success:
 error:
    Py_XDECREF(lower);
    Py_XDECREF(upper);
    Py_XDECREF(step);
    return result;
}

static PyObject*
ast_for_binop(struct compiling *c, const node *n)
{
	/* Must account for a sequence of expressions.
	   How should A op B op C by represented?  
	   BinOp(BinOp(A, op, B), op, C).
	*/

	PyObject *result = NULL;
	int i, nops;
	PyObject *expr1 = NULL; 
	PyObject *expr2 = NULL; 
	PyObject *tmp_result = NULL;
	PyObject *tmp1 = NULL;
	PyObject *tmp2 = NULL;
        PyObject *operator = NULL;

        expr1 = ast_for_expr(c, CHILD(n, 0));
        if (!expr1)
            return NULL;

        expr2 = ast_for_expr(c, CHILD(n, 2));
        if (!expr2)
            return NULL;

        operator = get_operator(CHILD(n, 1));
        if (!operator)
            return NULL;

	tmp_result = BinOp(expr1, operator, expr2, LINENO(n));
	if (!tmp_result)
            return NULL;

	nops = (NCH(n) - 1) / 2;
	for (i = 1; i < nops; i++) {
		const node* next_oper = CHILD(n, i * 2 + 1);

		operator = get_operator(next_oper);
                if (!operator)
                    goto error;

                tmp1 = ast_for_expr(c, CHILD(n, i * 2 + 2));
                if (!tmp1)
                    goto error;

                tmp2 = BinOp(tmp_result, operator, tmp1, 
			     LINENO(next_oper));
		if (!tmp_result) 
		    goto error;
		tmp_result = tmp2;
		tmp2 = NULL;
		Py_RELEASE(tmp1);
	}
	result = tmp_result;
	tmp_result = NULL;
 error:
	Py_XDECREF(expr1);
	Py_XDECREF(expr2);
	Py_XDECREF(operator);
	Py_XDECREF(tmp_result);
	Py_XDECREF(tmp1);
	Py_XDECREF(tmp2);
	return NULL;
}

static PyObject*
ast_for_trailer(struct compiling *c, const node *n, expr_ty left_expr)
{
    /* trailer: '(' [arglist] ')' | '[' subscriptlist ']' | '.' NAME */
    PyObject *result = NULL;
    PyObject *e = NULL;
    REQ(n, trailer);
    if (TYPE(CHILD(n, 0)) == LPAR) {
        if (NCH(n) == 2)
            e = Call(left_expr, NULL, NULL, NULL, NULL, LINENO(n));
        else
            e = ast_for_call(c, CHILD(n, 1), left_expr);
    }
    else if (TYPE(CHILD(n, 0)) == LSQB) {
        REQ(CHILD(n, 2), RSQB);
        n = CHILD(n, 1);
        if (NCH(n) <= 2) {
            slice_ty slc = ast_for_slice(c, CHILD(n, 0));
            if (!slc)
                return NULL;
            e = Subscript(left_expr, slc, Load, LINENO(n));
            if (!e) {
                free_slice(slc);
                return NULL;
            }
        }
        else {
            int j;
            slice_ty slc;
            asdl_seq *slices = asdl_seq_new((NCH(n) + 1) / 2);
            if (!slices)
                return NULL;
            for (j = 0; j < NCH(n); j += 2) {
                slc = ast_for_slice(c, CHILD(n, j));
                if (!slc) {
		    for (j = j / 2; j >= 0; j--)
                        free_slice(asdl_seq_GET(slices, j));
                    asdl_seq_free(slices); /* ok */
                    return NULL;
                }
                asdl_seq_SET(slices, j / 2, slc);
            }
            e = Subscript(left_expr, ExtSlice(slices), Load, LINENO(n));
            if (!e) {
		for (j = 0; j < asdl_seq_LEN(slices); j++)
                    free_slice(asdl_seq_GET(slices, j));
                asdl_seq_free(slices); /* ok */
                return NULL;
            }
        }
    }
    else {
        assert(TYPE(CHILD(n, 0)) == DOT);
        e = Attribute(left_expr, NEW_IDENTIFIER(CHILD(n, 1)), Load, LINENO(n));
    }
    return e;
}

static PyObject*
ast_for_power(struct compiling *c, const node *n)
{
    /* power: atom trailer* ('**' factor)*
     */
    PyObject *result = NULL;
    int i;
    PyObject *e = NULL; PyObject *tmp = NULL;
    REQ(n, power);
    e = ast_for_atom(c, CHILD(n, 0));
    if (!e)
        return NULL;
    if (NCH(n) == 1)
        return e;
    for (i = 1; i < NCH(n); i++) {
        node *ch = CHILD(n, i);
        if (TYPE(ch) != trailer)
            break;
        tmp = ast_for_trailer(c, ch, e);
        if (!tmp) {
            free_expr(e);
            return NULL;
        }
        e = tmp;
    }
    if (TYPE(CHILD(n, NCH(n) - 1)) == factor) {
        expr_ty f = ast_for_expr(c, CHILD(n, NCH(n) - 1));
        if (!f) {
            free_expr(e);
            return NULL;
        }
        tmp = BinOp(e, Pow, f, LINENO(n));
        if (!tmp) {
            free_expr(f);
            free_expr(e);
            return NULL;
        }
        e = tmp;
    }
    return e;
}

/* Do not name a variable 'expr'!  Will cause a compile error.
*/

static PyObject*
ast_for_expr(struct compiling *c, const node *n)
{
    /* handle the full range of simple expressions
       test: and_test ('or' and_test)* | lambdef
       and_test: not_test ('and' not_test)*
       not_test: 'not' not_test | comparison
       comparison: expr (comp_op expr)*
       expr: xor_expr ('|' xor_expr)*
       xor_expr: and_expr ('^' and_expr)*
       and_expr: shift_expr ('&' shift_expr)*
       shift_expr: arith_expr (('<<'|'>>') arith_expr)*
       arith_expr: term (('+'|'-') term)*
       term: factor (('*'|'/'|'%'|'//') factor)*
       factor: ('+'|'-'|'~') factor | power
       power: atom trailer* ('**' factor)*
    */

    PyObject *result = NULL;
    PyObject *seq = NULL;
    int i;

 loop:
    switch (TYPE(n)) {
        case test:
            if (TYPE(CHILD(n, 0)) == lambdef)
                return ast_for_lambdef(c, CHILD(n, 0));
            /* Fall through to and_test */
        case and_test:
            if (NCH(n) == 1) {
                n = CHILD(n, 0);
                goto loop;
            }
            seq = asdl_seq_new((NCH(n) + 1) / 2);
            if (!seq)
                return NULL;
            for (i = 0; i < NCH(n); i += 2) {
                expr_ty e = ast_for_expr(c, CHILD(n, i));
                if (!e)
                    return NULL;
                asdl_seq_SET(seq, i / 2, e);
            }
            if (!strcmp(STR(CHILD(n, 1)), "and"))
                return BoolOp(And, seq, LINENO(n));
            else {
                assert(!strcmp(STR(CHILD(n, 1)), "or"));
                return BoolOp(Or, seq, LINENO(n));
            }
            break;
        case not_test:
            if (NCH(n) == 1) {
                n = CHILD(n, 0);
                goto loop;
            }
            else {
                expr_ty expression = ast_for_expr(c, CHILD(n, 1));
                if (!expression)
                    return NULL;

                return UnaryOp(Not, expression, LINENO(n));
            }
        case comparison:
            if (NCH(n) == 1) {
                n = CHILD(n, 0);
                goto loop;
            }
            else {
                PyObject *expression = NULL;
                PyObject *ops = NULL; PyObject *cmps = NULL;
                ops = asdl_seq_new(NCH(n) / 2);
                if (!ops)
                    return NULL;
                cmps = asdl_seq_new(NCH(n) / 2);
                if (!cmps) {
                    asdl_seq_free(ops); /* ok */
                    return NULL;
                }
                for (i = 1; i < NCH(n); i += 2) {
                    /* XXX cmpop_ty is just an enum */
                    cmpop_ty operator;

                    operator = ast_for_comp_op(CHILD(n, i));
                    if (!operator) {
		        asdl_expr_seq_free(ops);
		        asdl_expr_seq_free(cmps);
                        return NULL;
		    }

                    expression = ast_for_expr(c, CHILD(n, i + 1));
                    if (!expression) {
		        asdl_expr_seq_free(ops);
		        asdl_expr_seq_free(cmps);
                        return NULL;
		    }
                        
                    asdl_seq_SET(ops, i / 2, (void *)operator);
                    asdl_seq_SET(cmps, i / 2, expression);
                }
                expression = ast_for_expr(c, CHILD(n, 0));
                if (!expression) {
		    asdl_expr_seq_free(ops);
		    asdl_expr_seq_free(cmps);
                    return NULL;
		}
                    
                return Compare(expression, ops, cmps, LINENO(n));
            }
            break;

        /* The next five cases all handle BinOps.  The main body of code
           is the same in each case, but the switch turned inside out to
           reuse the code for each type of operator.
         */
        case expr:
        case xor_expr:
        case and_expr:
        case shift_expr:
        case arith_expr:
        case term:
            if (NCH(n) == 1) {
                n = CHILD(n, 0);
                goto loop;
            }
            return ast_for_binop(c, n);
        case yield_expr: {
	    expr_ty exp = NULL;
	    if (NCH(n) == 2) {
		exp = ast_for_testlist(c, CHILD(n, 1));
		if (!exp)
		    return NULL;
	    }
	    return Yield(exp, LINENO(n));
	}
        case factor: {
            PyObject *expression = NULL;
            
            if (NCH(n) == 1) {
                n = CHILD(n, 0);
                goto loop;
            }

            expression = ast_for_expr(c, CHILD(n, 1));
            if (!expression)
                return NULL;

            switch (TYPE(CHILD(n, 0))) {
                case PLUS:
                    return UnaryOp(UAdd, expression, LINENO(n));
                case MINUS:
                    return UnaryOp(USub, expression, LINENO(n));
                case TILDE:
                    return UnaryOp(Invert, expression, LINENO(n));
            }
            PyErr_Format(PyExc_SystemError, "unhandled factor: %d",
	    		 TYPE(CHILD(n, 0)));
            break;
        }
        case power:
            return ast_for_power(c, n);
        default:
            PyErr_Format(PyExc_SystemError, "unhandled expr: %d", TYPE(n));
            return NULL;
    }
    /* should never get here */
    return NULL;
}

static PyObject*
ast_for_call(struct compiling *c, const node *n, expr_ty func)
{
    /*
      arglist: (argument ',')* (argument [',']| '*' test [',' '**' test]
               | '**' test)
      argument: [test '='] test [gen_for]	 # Really [keyword '='] test
    */

    PyObject *result = NULL;
    int i, nargs, nkeywords, ngens;
    asdl_seq *args = NULL;
    asdl_seq *keywords = NULL;
    expr_ty vararg = NULL, kwarg = NULL;

    REQ(n, arglist);

    nargs = 0;
    nkeywords = 0;
    ngens = 0;
    for (i = 0; i < NCH(n); i++) {
	node *ch = CHILD(n, i);
	if (TYPE(ch) == argument) {
	    if (NCH(ch) == 1)
		nargs++;
	    else if (TYPE(CHILD(ch, 1)) == gen_for)
		ngens++;
            else
		nkeywords++;
	}
    }
    if (ngens > 1 || (ngens && (nargs || nkeywords))) {
        ast_error(n, "Generator expression must be parenthesised "
		  "if not sole argument");
	return NULL;
    }

    if (nargs + nkeywords + ngens > 255) {
      ast_error(n, "more than 255 arguments");
      return NULL;
    }

    args = asdl_seq_new(nargs + ngens);
    if (!args)
        goto error;
    keywords = asdl_seq_new(nkeywords);
    if (!keywords)
        goto error;
    nargs = 0;
    nkeywords = 0;
    for (i = 0; i < NCH(n); i++) {
	node *ch = CHILD(n, i);
	if (TYPE(ch) == argument) {
	    PyObject *e = NULL;
	    if (NCH(ch) == 1) {
		e = ast_for_expr(c, CHILD(ch, 0));
                if (!e)
                    goto error;
		asdl_seq_SET(args, nargs++, e);
	    }  
	    else if (TYPE(CHILD(ch, 1)) == gen_for) {
        	e = ast_for_genexp(c, ch);
                if (!e)
                    goto error;
		asdl_seq_SET(args, nargs++, e);
            }
	    else {
		keyword_ty kw;
		identifier key;

		/* CHILD(ch, 0) is test, but must be an identifier? */ 
		e = ast_for_expr(c, CHILD(ch, 0));
                if (!e)
                    goto error;
                /* f(lambda x: x[0] = 3) ends up getting parsed with
                 * LHS test = lambda x: x[0], and RHS test = 3.
                 * SF bug 132313 points out that complaining about a keyword
                 * then is very confusing.
                 */
                if (e->kind == Lambda_kind) {
                  ast_error(CHILD(ch, 0), "lambda cannot contain assignment");
                  goto error;
                } else if (e->kind != Name_kind) {
                  ast_error(CHILD(ch, 0), "keyword can't be an expression");
                  goto error;
                }
		key = Name_id(e);
		free(e); /* XXX: is free correct here? */
		e = ast_for_expr(c, CHILD(ch, 2));
                if (!e)
                    goto error;
		kw = keyword(key, e);
                if (!kw)
                    goto error;
		asdl_seq_SET(keywords, nkeywords++, kw);
	    }
	}
	else if (TYPE(ch) == STAR) {
	    vararg = ast_for_expr(c, CHILD(n, i+1));
	    i++;
	}
	else if (TYPE(ch) == DOUBLESTAR) {
	    kwarg = ast_for_expr(c, CHILD(n, i+1));
	    i++;
	}
    }

    return Call(func, args, keywords, vararg, kwarg, LINENO(n));

 error:
    free_expr(vararg);
    free_expr(kwarg);
    if (args)
        asdl_expr_seq_free(args);
    if (keywords) {
	for (i = 0; i < asdl_seq_LEN(keywords); i++)
            free_keyword(asdl_seq_GET(keywords, i));
        asdl_seq_free(keywords); /* ok */
    }
    return NULL;
}

static PyObject*
ast_for_testlist(struct compiling *c, const node* n)
{
    /* testlist_gexp: test (',' test)* [','] */
    /* testlist: test (',' test)* [','] */
    /* testlist_safe: test (',' test)+ [','] */
    /* testlist1: test (',' test)* */
    PyObject *result = NULL;
    assert(NCH(n) > 0);
    if (TYPE(n) == testlist_gexp) {
        if (NCH(n) > 1)
            assert(TYPE(CHILD(n, 1)) != gen_for);
    }
    else {
        assert(TYPE(n) == testlist ||
               TYPE(n) == testlist_safe ||
               TYPE(n) == testlist1);
    }
    if (NCH(n) == 1)
	return ast_for_expr(c, CHILD(n, 0));
    else {
        asdl_seq *tmp = seq_for_testlist(c, n);
        if (!tmp)
            return NULL;
	return Tuple(tmp, Load, LINENO(n));
    }
}

static PyObject*
ast_for_testlist_gexp(struct compiling *c, const node* n)
{
    /* testlist_gexp: test ( gen_for | (',' test)* [','] ) */
    /* argument: test [ gen_for ] */
    PyObject *result = NULL;
    assert(TYPE(n) == testlist_gexp || TYPE(n) == argument);
    if (NCH(n) > 1 && TYPE(CHILD(n, 1)) == gen_for) {
	return ast_for_genexp(c, n);
    }
    else
        return ast_for_testlist(c, n);
}

/* like ast_for_testlist() but returns a sequence */
static asdl_seq*
ast_for_class_bases(struct compiling *c, const node* n)
{
    /* testlist: test (',' test)* [','] */
    PyObject *result = NULL;
    assert(NCH(n) > 0);
    REQ(n, testlist);
    if (NCH(n) == 1) {
        PyObject *base = NULL;
        asdl_seq *bases = asdl_seq_new(1);
        if (!bases)
            return NULL;
        base = ast_for_expr(c, CHILD(n, 0));
        if (!base) {
            asdl_seq_free(bases); /* ok */
            return NULL;
        }
        asdl_seq_SET(bases, 0, base);
        return bases;
    }
    else {
        return seq_for_testlist(c, n);
    }
}

static PyObject*
ast_for_expr_stmt(struct compiling *c, const node *n)
{
    PyObject *result = NULL;
    REQ(n, expr_stmt);
    /* expr_stmt: testlist (augassign (yield_expr|testlist) 
                | ('=' (yield_expr|testlist))*)
       testlist: test (',' test)* [',']
       augassign: '+=' | '-=' | '*=' | '/=' | '%=' | '&=' | '|=' | '^='
	        | '<<=' | '>>=' | '**=' | '//='
       test: ... here starts the operator precendence dance
     */

    if (NCH(n) == 1) {
	expr_ty e = ast_for_testlist(c, CHILD(n, 0));
        if (!e)
            return NULL;

	return Expr(e, LINENO(n));
    }
    else if (TYPE(CHILD(n, 1)) == augassign) {
        PyObject *expr1 = NULL; PyObject *expr2 = NULL;
        operator_ty operator;
	node *ch = CHILD(n, 0);

	if (TYPE(ch) == testlist)
	    expr1 = ast_for_testlist(c, ch);
	else
	    expr1 = Yield(ast_for_expr(c, CHILD(ch, 0)), LINENO(ch));

        if (!expr1)
            return NULL;
        if (expr1->kind == GeneratorExp_kind) {
	    free_expr(expr1);
	    ast_error(ch, "augmented assignment to generator "
		      "expression not possible");
	    return NULL;
        }
	if (expr1->kind == Name_kind) {
		char *var_name = PyString_AS_STRING(expr1->v.Name.id);
		if (var_name[0] == 'N' && !strcmp(var_name, "None")) {
			free_expr(expr1);
			ast_error(ch, "assignment to None");
			return NULL;
		}
	}

	ch = CHILD(n, 2);
	if (TYPE(ch) == testlist)
	    expr2 = ast_for_testlist(c, ch);
	else
	    expr2 = Yield(ast_for_expr(c, ch), LINENO(ch));
        if (!expr2) {
            free_expr(expr1);
            return NULL;
        }

        operator = ast_for_augassign(CHILD(n, 1));
        if (!operator) {
            free_expr(expr1);
            free_expr(expr2);
            return NULL;
        }

	return AugAssign(expr1, operator, expr2, LINENO(n));
    }
    else {
	int i;
	PyObject *targets = NULL;
	node *value;
        PyObject *expression = NULL;

	/* a normal assignment */
	REQ(CHILD(n, 1), EQUAL);
	targets = asdl_seq_new(NCH(n) / 2);
	if (!targets)
	    return NULL;
	for (i = 0; i < NCH(n) - 2; i += 2) {
	    PyObject *e = NULL;
	    node *ch = CHILD(n, i);
	    if (TYPE(ch) == yield_expr) {
		ast_error(ch, "assignment to yield expression not possible");
		goto error;
	    }
	    e = ast_for_testlist(c, ch);

	    /* set context to assign */
	    if (!e) 
	      goto error;

	    if (!set_context(e, Store, CHILD(n, i))) {
              free_expr(e);
	      goto error;
            }

	    asdl_seq_SET(targets, i / 2, e);
	}
	value = CHILD(n, NCH(n) - 1);
	if (TYPE(value) == testlist)
	    expression = ast_for_testlist(c, value);
	else
	    expression = ast_for_expr(c, value);
	if (!expression)
	    goto error;
	return Assign(targets, expression, LINENO(n));
    error:
        asdl_expr_seq_free(targets);
    }
    return NULL;
}

static PyObject*
ast_for_print_stmt(struct compiling *c, const node *n)
{
    /* print_stmt: 'print' ( [ test (',' test)* [','] ]
                             | '>>' test [ (',' test)+ [','] ] )
     */
    PyObject *result = NULL;
    expr_ty dest = NULL, expression;
    PyObject *seq = NULL;
    int nl;
    int i, start = 1;

    REQ(n, print_stmt);
    if (NCH(n) >= 2 && TYPE(CHILD(n, 1)) == RIGHTSHIFT) {
	dest = ast_for_expr(c, CHILD(n, 2));
        if (!dest)
            return NULL;
	start = 4;
    }
    seq = asdl_seq_new((NCH(n) + 1 - start) / 2);
    if (!seq)
	return NULL;
    for (i = start; i < NCH(n); i += 2) {
        expression = ast_for_expr(c, CHILD(n, i));
        if (!expression) {
	    free_expr(dest);
	    asdl_expr_seq_free(seq);
            return NULL;
	}

	asdl_seq_APPEND(seq, expression);
    }
    nl = (TYPE(CHILD(n, NCH(n) - 1)) == COMMA) ? false : true;
    return Print(dest, seq, nl, LINENO(n));
}

static asdl_seq *
ast_for_exprlist(struct compiling *c, const node *n, PyObject* context)
{
    PyObject *seq = NULL;
    int i;
    PyObject *e = NULL;

    REQ(n, exprlist);

    seq = asdl_seq_new((NCH(n) + 1) / 2);
    if (!seq)
	return NULL;
    for (i = 0; i < NCH(n); i += 2) {
	e = ast_for_expr(c, CHILD(n, i));
	if (!e)
	    goto error;
	asdl_seq_SET(seq, i / 2, e);
	if (context) {
	    if (!set_context(e, context, CHILD(n, i)))
	    	goto error;
        }
    }
    return seq;

error:
    asdl_expr_seq_free(seq);
    return NULL;
}

static PyObject*
ast_for_del_stmt(struct compiling *c, const node *n)
{
    PyObject *result = NULL;
    PyObject *expr_list = NULL;
    
    /* del_stmt: 'del' exprlist */
    REQ(n, del_stmt);

    expr_list = ast_for_exprlist(c, CHILD(n, 1), Del);
    if (!expr_list)
        return NULL;
    return Delete(expr_list, LINENO(n));
}

static PyObject*
ast_for_flow_stmt(struct compiling *c, const node *n)
{
    /*
      flow_stmt: break_stmt | continue_stmt | return_stmt | raise_stmt
                 | yield_stmt
      break_stmt: 'break'
      continue_stmt: 'continue'
      return_stmt: 'return' [testlist]
      yield_stmt: yield_expr
      yield_expr: 'yield' testlist
      raise_stmt: 'raise' [test [',' test [',' test]]]
    */
    PyObject *result = NULL;
    node *ch;

    REQ(n, flow_stmt);
    ch = CHILD(n, 0);
    switch (TYPE(ch)) {
        case break_stmt:
            return Break(LINENO(n));
        case continue_stmt:
            return Continue(LINENO(n));
        case yield_stmt: { /* will reduce to yield_expr */
	    expr_ty exp = ast_for_expr(c, CHILD(ch, 0));
	    if (!exp)
		return NULL;
            return Expr(exp, LINENO(n));
        }
        case return_stmt:
            if (NCH(ch) == 1)
                return Return(NULL, LINENO(n));
            else {
                expr_ty expression = ast_for_testlist(c, CHILD(ch, 1));
                if (!expression)
                    return NULL;
                return Return(expression, LINENO(n));
            }
        case raise_stmt:
            if (NCH(ch) == 1)
                return Raise(NULL, NULL, NULL, LINENO(n));
            else if (NCH(ch) == 2) {
                expr_ty expression = ast_for_expr(c, CHILD(ch, 1));
                if (!expression)
                    return NULL;
                return Raise(expression, NULL, NULL, LINENO(n));
            }
            else if (NCH(ch) == 4) {
                PyObject *expr1 = NULL; PyObject *expr2 = NULL;

                expr1 = ast_for_expr(c, CHILD(ch, 1));
                if (!expr1)
                    return NULL;
                expr2 = ast_for_expr(c, CHILD(ch, 3));
                if (!expr2)
                    return NULL;

                return Raise(expr1, expr2, NULL, LINENO(n));
            }
            else if (NCH(ch) == 6) {
                PyObject *expr1 = NULL; PyObject *expr2 = NULL; PyObject *expr3 = NULL;

                expr1 = ast_for_expr(c, CHILD(ch, 1));
                if (!expr1)
                    return NULL;
                expr2 = ast_for_expr(c, CHILD(ch, 3));
                if (!expr2)
                    return NULL;
                expr3 = ast_for_expr(c, CHILD(ch, 5));
                if (!expr3)
                    return NULL;
                    
                return Raise(expr1, expr2, expr3, LINENO(n));
            }
        default:
            PyErr_Format(PyExc_SystemError,
                         "unexpected flow_stmt: %d", TYPE(ch));
            return NULL;
    }
}

static PyObject*
alias_for_import_name(const node *n)
{
    /*
      import_as_name: NAME [NAME NAME]
      dotted_as_name: dotted_name [NAME NAME]
      dotted_name: NAME ('.' NAME)*
    */
    PyObject *result = NULL;
 loop:
    switch (TYPE(n)) {
        case import_as_name:
            if (NCH(n) == 3)
                return alias(NEW_IDENTIFIER(CHILD(n, 0)),
                             NEW_IDENTIFIER(CHILD(n, 2)));
            else
                return alias(NEW_IDENTIFIER(CHILD(n, 0)),
                             NULL);
            break;
        case dotted_as_name:
            if (NCH(n) == 1) {
                n = CHILD(n, 0);
                goto loop;
            }
            else {
                alias_ty a = alias_for_import_name(CHILD(n, 0));
                assert(!a->asname);
                a->asname = NEW_IDENTIFIER(CHILD(n, 2));
                return a;
            }
            break;
        case dotted_name:
            if (NCH(n) == 1)
                return alias(NEW_IDENTIFIER(CHILD(n, 0)), NULL);
            else {
                /* Create a string of the form "a.b.c" */
                int i, len;
                PyObject *str;
                char *s;

                len = 0;
                for (i = 0; i < NCH(n); i += 2)
                    /* length of string plus one for the dot */
                    len += strlen(STR(CHILD(n, i))) + 1;
                len--; /* the last name doesn't have a dot */
                str = PyString_FromStringAndSize(NULL, len);
                if (!str)
                    return NULL;
                s = PyString_AS_STRING(str);
                if (!s)
                    return NULL;
                for (i = 0; i < NCH(n); i += 2) {
                    char *sch = STR(CHILD(n, i));
                    strcpy(s, STR(CHILD(n, i)));
                    s += strlen(sch);
                    *s++ = '.';
                }
                --s;
                *s = '\0';
                PyString_InternInPlace(&str);
                return alias(str, NULL);
            }
            break;
        case STAR:
            return alias(PyString_InternFromString("*"), NULL);
        default:
            PyErr_Format(PyExc_SystemError,
                         "unexpected import name: %d", TYPE(n));
            return NULL;
    }
    return NULL;
}

static PyObject*
ast_for_import_stmt(struct compiling *c, const node *n)
{
    /*
      import_stmt: import_name | import_from
      import_name: 'import' dotted_as_names
      import_from: 'from' dotted_name 'import' ('*' | 
                                                '(' import_as_names ')' | 
                                                import_as_names)
    */
    PyObject *result = NULL;
    int i;
    PyObject *aliases = NULL;

    REQ(n, import_stmt);
    n = CHILD(n, 0);
    if (STR(CHILD(n, 0))[0] == 'i') { /* import */
        n = CHILD(n, 1);
	REQ(n, dotted_as_names);
	aliases = asdl_seq_new((NCH(n) + 1) / 2);
	if (!aliases)
		return NULL;
	for (i = 0; i < NCH(n); i += 2) {
            alias_ty import_alias = alias_for_import_name(CHILD(n, i));
            if (!import_alias) {
                asdl_alias_seq_free(aliases);
                return NULL;
            }
	    asdl_seq_SET(aliases, i / 2, import_alias);
        }
	return Import(aliases, LINENO(n));
    }
    else if (STR(CHILD(n, 0))[0] == 'f') { /* from */
	stmt_ty import;
        int n_children;
        const char *from_modules;
	int lineno = LINENO(n);
	alias_ty mod = alias_for_import_name(CHILD(n, 1));
	if (!mod)
            return NULL;

        /* XXX this needs to be cleaned up */

        from_modules = STR(CHILD(n, 3));
        if (!from_modules) {
            n = CHILD(n, 3);                  /* from ... import x, y, z */
            if (NCH(n) % 2 == 0) {
                /* it ends with a comma, not valid but the parser allows it */
		free_alias(mod);
                ast_error(n, "trailing comma not allowed without"
                             " surrounding parentheses");
                return NULL;
            }
        }
        else if (from_modules[0] == '*') {
            n = CHILD(n, 3); /* from ... import * */
        }
        else if (from_modules[0] == '(')
            n = CHILD(n, 4);                  /* from ... import (x, y, z) */
        else {
	    /* XXX: don't we need to call ast_error(n, "..."); */
	    free_alias(mod);
            return NULL;
	}

        n_children = NCH(n);
        if (from_modules && from_modules[0] == '*')
            n_children = 1;

	aliases = asdl_seq_new((n_children + 1) / 2);
	if (!aliases) {
            free_alias(mod);
            return NULL;
	}

        /* handle "from ... import *" special b/c there's no children */
        if (from_modules && from_modules[0] == '*') {
            alias_ty import_alias = alias_for_import_name(n);
            if (!import_alias) {
                asdl_alias_seq_free(aliases);
                free_alias(mod);
                return NULL;
            }
	    asdl_seq_APPEND(aliases, import_alias);
        }

	for (i = 0; i < NCH(n); i += 2) {
            alias_ty import_alias = alias_for_import_name(CHILD(n, i));
            if (!import_alias) {
                asdl_alias_seq_free(aliases);
                free_alias(mod);
                return NULL;
            }
	    asdl_seq_APPEND(aliases, import_alias);
        }
        Py_INCREF(mod->name);
	import = ImportFrom(mod->name, aliases, lineno);
	free_alias(mod);
	return import;
    }
    PyErr_Format(PyExc_SystemError,
                 "unknown import statement: starts with command '%s'",
                 STR(CHILD(n, 0)));
    return NULL;
}

static PyObject*
ast_for_global_stmt(struct compiling *c, const node *n)
{
    /* global_stmt: 'global' NAME (',' NAME)* */
    PyObject *result = NULL;
    identifier name;
    PyObject *s = NULL;
    int i;

    REQ(n, global_stmt);
    s = asdl_seq_new(NCH(n) / 2);
    if (!s)
    	return NULL;
    for (i = 1; i < NCH(n); i += 2) {
	name = NEW_IDENTIFIER(CHILD(n, i));
	if (!name) {
	    for (i = i / 2; i > 0; i--)
                Py_XDECREF((identifier) asdl_seq_GET(s, i));
	    asdl_seq_free(s); /* ok */
	    return NULL;
	}
	asdl_seq_SET(s, i / 2, name);
    }
    return Global(s, LINENO(n));
}

static PyObject*
ast_for_exec_stmt(struct compiling *c, const node *n)
{
    expr_ty expr1, globals = NULL, locals = NULL;
    int n_children = NCH(n);
    if (n_children != 2 && n_children != 4 && n_children != 6) {
        PyErr_Format(PyExc_SystemError,
                     "poorly formed 'exec' statement: %d parts to statement",
                     n_children);
        return NULL;
    }

    /* exec_stmt: 'exec' expr ['in' test [',' test]] */
    REQ(n, exec_stmt);
    expr1 = ast_for_expr(c, CHILD(n, 1));
    if (!expr1)
        return NULL;
    if (n_children >= 4) {
        globals = ast_for_expr(c, CHILD(n, 3));
        if (!globals)
            return NULL;
    }
    if (n_children == 6) {
        locals = ast_for_expr(c, CHILD(n, 5));
        if (!locals)
            return NULL;
    }

    return Exec(expr1, globals, locals, LINENO(n));
}

static PyObject*
ast_for_assert_stmt(struct compiling *c, const node *n)
{
    /* assert_stmt: 'assert' test [',' test] */
    PyObject *result = NULL;
    REQ(n, assert_stmt);
    if (NCH(n) == 2) {
        expr_ty expression = ast_for_expr(c, CHILD(n, 1));
        if (!expression)
            return NULL;
	return Assert(expression, NULL, LINENO(n));
    }
    else if (NCH(n) == 4) {
        PyObject *expr1 = NULL; PyObject *expr2 = NULL;

        expr1 = ast_for_expr(c, CHILD(n, 1));
        if (!expr1)
            return NULL;
        expr2 = ast_for_expr(c, CHILD(n, 3));
        if (!expr2)
            return NULL;
            
	return Assert(expr1, expr2, LINENO(n));
    }
    PyErr_Format(PyExc_SystemError,
                 "improper number of parts to 'assert' statement: %d",
                 NCH(n));
    return NULL;
}

static asdl_seq *
ast_for_suite(struct compiling *c, const node *n)
{
    /* suite: simple_stmt | NEWLINE INDENT stmt+ DEDENT */
    asdl_seq *seq = NULL;
    stmt_ty s;
    int i, total, num, end, pos = 0;
    node *ch;

    REQ(n, suite);

    total = num_stmts(n);
    seq = asdl_seq_new(total);
    if (!seq)
    	return NULL;
    if (TYPE(CHILD(n, 0)) == simple_stmt) {
	n = CHILD(n, 0);
	/* simple_stmt always ends with a NEWLINE,
	   and may have a trailing SEMI 
	*/
	end = NCH(n) - 1;
	if (TYPE(CHILD(n, end - 1)) == SEMI)
	    end--;
        /* loop by 2 to skip semi-colons */
	for (i = 0; i < end; i += 2) {
	    ch = CHILD(n, i);
	    s = ast_for_stmt(c, ch);
	    if (!s)
		goto error;
	    asdl_seq_SET(seq, pos++, s);
	}
    }
    else {
	for (i = 2; i < (NCH(n) - 1); i++) {
	    ch = CHILD(n, i);
	    REQ(ch, stmt);
	    num = num_stmts(ch);
	    if (num == 1) {
		/* small_stmt or compound_stmt with only one child */
		s = ast_for_stmt(c, ch);
		if (!s)
		    goto error;
		asdl_seq_SET(seq, pos++, s);
	    }
	    else {
		int j;
		ch = CHILD(ch, 0);
		REQ(ch, simple_stmt);
		for (j = 0; j < NCH(ch); j += 2) {
		    s = ast_for_stmt(c, CHILD(ch, j));
		    if (!s)
			goto error;
		    asdl_seq_SET(seq, pos++, s);
		}
	    }
	}
    }
    assert(pos == seq->size);
    return seq;
 error:
    if (seq)
	asdl_stmt_seq_free(seq);
    return NULL;
}

static PyObject*
ast_for_if_stmt(struct compiling *c, const node *n)
{
    /* if_stmt: 'if' test ':' suite ('elif' test ':' suite)*
       ['else' ':' suite]
    */
    PyObject *result = NULL;
    char *s;

    REQ(n, if_stmt);

    if (NCH(n) == 4) {
        PyObject *expression = NULL;
        PyObject *suite_seq = NULL;

        expression = ast_for_expr(c, CHILD(n, 1));
        if (!expression)
            return NULL;
        suite_seq = ast_for_suite(c, CHILD(n, 3)); 
        if (!suite_seq) {
	    free_expr(expression);
            return NULL;
	}
            
	return If(expression, suite_seq, NULL, LINENO(n));
    }
    s = STR(CHILD(n, 4));
    /* s[2], the third character in the string, will be
       's' for el_s_e, or
       'i' for el_i_f
    */
    if (s[2] == 's') {
        PyObject *expression = NULL;
        PyObject *seq1 = NULL; PyObject *seq2 = NULL;

        expression = ast_for_expr(c, CHILD(n, 1));
        if (!expression)
            return NULL;
        seq1 = ast_for_suite(c, CHILD(n, 3));
        if (!seq1) {
	    free_expr(expression);
            return NULL;
	}
        seq2 = ast_for_suite(c, CHILD(n, 6));
        if (!seq2) {
	    asdl_stmt_seq_free(seq1);
	    free_expr(expression);
            return NULL;
	}

	return If(expression, seq1, seq2, LINENO(n));
    }
    else if (s[2] == 'i') {
	int i, n_elif, has_else = 0;
	asdl_seq *orelse = NULL;
	n_elif = NCH(n) - 4;
        /* must reference the child n_elif+1 since 'else' token is third,
           not fourth, child from the end. */
	if (TYPE(CHILD(n, (n_elif + 1))) == NAME
	    && STR(CHILD(n, (n_elif + 1)))[2] == 's') {
	    has_else = 1;
	    n_elif -= 3;
	}
	n_elif /= 4;

	if (has_else) {
            PyObject *expression = NULL;
            PyObject *seq1 = NULL; PyObject *seq2 = NULL;

	    orelse = asdl_seq_new(1);
	    if (!orelse)
		return NULL;
            expression = ast_for_expr(c, CHILD(n, NCH(n) - 6));
            if (!expression) {
                asdl_seq_free(orelse); /* ok */
                return NULL;
            }
            seq1 = ast_for_suite(c, CHILD(n, NCH(n) - 4));
            if (!seq1) {
                free_expr(expression);
                asdl_seq_free(orelse); /* ok */
                return NULL;
            }
            seq2 = ast_for_suite(c, CHILD(n, NCH(n) - 1));
            if (!seq2) {
                free_expr(expression);
                asdl_stmt_seq_free(seq1);
                asdl_seq_free(orelse); /* ok */
                return NULL;
            }

	    asdl_seq_SET(orelse, 0, If(expression, seq1, seq2, 
				       LINENO(CHILD(n, NCH(n) - 6))));
	    /* the just-created orelse handled the last elif */
	    n_elif--;
	}
        else
            orelse  = NULL;

	for (i = 0; i < n_elif; i++) {
	    int off = 5 + (n_elif - i - 1) * 4;
            PyObject *expression = NULL;
            PyObject *suite_seq = NULL;
	    asdl_seq *new = asdl_seq_new(1);
	    if (!new) {
		asdl_stmt_seq_free(orelse);
		return NULL;
	    }
            expression = ast_for_expr(c, CHILD(n, off));
            if (!expression) {
		asdl_stmt_seq_free(orelse);
                asdl_seq_free(new); /* ok */
                return NULL;
            }
            suite_seq = ast_for_suite(c, CHILD(n, off + 2));
            if (!suite_seq) {
		asdl_stmt_seq_free(orelse);
	        free_expr(expression);
                asdl_seq_free(new); /* ok */
                return NULL;
            }

	    asdl_seq_SET(new, 0,
			 If(expression, suite_seq, orelse, 
			    LINENO(CHILD(n, off))));
	    orelse = new;
	}
	return If(ast_for_expr(c, CHILD(n, 1)),
		  ast_for_suite(c, CHILD(n, 3)),
		  orelse, LINENO(n));
    }
    else {
        PyErr_Format(PyExc_SystemError,
                     "unexpected token in 'if' statement: %s", s);
        return NULL;
    }
}

static PyObject*
ast_for_while_stmt(struct compiling *c, const node *n)
{
    /* while_stmt: 'while' test ':' suite ['else' ':' suite] */
    PyObject *result = NULL;
    REQ(n, while_stmt);

    if (NCH(n) == 4) {
        PyObject *expression = NULL;
        PyObject *suite_seq = NULL;

        expression = ast_for_expr(c, CHILD(n, 1));
        if (!expression)
            return NULL;
        suite_seq = ast_for_suite(c, CHILD(n, 3));
        if (!suite_seq) {
	    free_expr(expression);
            return NULL;
	}
	return While(expression, suite_seq, NULL, LINENO(n));
    }
    else if (NCH(n) == 7) {
        PyObject *expression = NULL;
        PyObject *seq1 = NULL; PyObject *seq2 = NULL;

        expression = ast_for_expr(c, CHILD(n, 1));
        if (!expression)
            return NULL;
        seq1 = ast_for_suite(c, CHILD(n, 3));
        if (!seq1) {
	    free_expr(expression);
            return NULL;
	}
        seq2 = ast_for_suite(c, CHILD(n, 6));
        if (!seq2) {
	    asdl_stmt_seq_free(seq1);
	    free_expr(expression);
            return NULL;
	}

	return While(expression, seq1, seq2, LINENO(n));
    }
    else {
        PyErr_Format(PyExc_SystemError,
                     "wrong number of tokens for 'while' statement: %d",
                     NCH(n));
        return NULL;
    }
}

static PyObject*
ast_for_for_stmt(struct compiling *c, const node *n)
{
    PyObject *result = NULL;
    asdl_seq *_target = NULL, *seq = NULL, *suite_seq = NULL;
    PyObject *expression = NULL;
    PyObject *target = NULL;
    /* for_stmt: 'for' exprlist 'in' testlist ':' suite ['else' ':' suite] */
    REQ(n, for_stmt);

    if (NCH(n) == 9) {
	seq = ast_for_suite(c, CHILD(n, 8));
        if (!seq)
            return NULL;
    }

    _target = ast_for_exprlist(c, CHILD(n, 1), Store);
    if (!_target) {
	asdl_stmt_seq_free(seq);
        return NULL;
    }
    if (asdl_seq_LEN(_target) == 1) {
	target = asdl_seq_GET(_target, 0);
	asdl_seq_free(_target); /* ok */
    }
    else
	target = Tuple(_target, Store, LINENO(n));

    expression = ast_for_testlist(c, CHILD(n, 3));
    if (!expression) {
	free_expr(target);
	asdl_stmt_seq_free(seq);
        return NULL;
    }
    suite_seq = ast_for_suite(c, CHILD(n, 5));
    if (!suite_seq) {
	free_expr(target);
	free_expr(expression);
	asdl_stmt_seq_free(seq);
        return NULL;
    }

    return For(target, expression, suite_seq, seq, LINENO(n));
}

static PyObject*
ast_for_except_clause(struct compiling *c, const node *exc, node *body)
{
    /* except_clause: 'except' [test [',' test]] */
    PyObject *result = NULL;
    REQ(exc, except_clause);
    REQ(body, suite);

    if (NCH(exc) == 1) {
        asdl_seq *suite_seq = ast_for_suite(c, body);
        if (!suite_seq)
            return NULL;

	return excepthandler(NULL, NULL, suite_seq);
    }
    else if (NCH(exc) == 2) {
        PyObject *expression = NULL;
        PyObject *suite_seq = NULL;

        expression = ast_for_expr(c, CHILD(exc, 1));
        if (!expression)
            return NULL;
        suite_seq = ast_for_suite(c, body);
        if (!suite_seq) {
	    free_expr(expression);
            return NULL;
	}

	return excepthandler(expression, NULL, suite_seq);
    }
    else if (NCH(exc) == 4) {
        PyObject *suite_seq = NULL;
        PyObject *expression = NULL;
	expr_ty e = ast_for_expr(c, CHILD(exc, 3));
	if (!e)
            return NULL;
	if (!set_context(e, Store, CHILD(exc, 3))) {
	    free_expr(e);
            return NULL;
	}
        expression = ast_for_expr(c, CHILD(exc, 1));
        if (!expression) {
	    free_expr(e);
            return NULL;
	}
        suite_seq = ast_for_suite(c, body);
        if (!suite_seq) {
	    free_expr(expression);
	    free_expr(e);
            return NULL;
	}

	return excepthandler(expression, e, suite_seq);
    }
    else {
        PyErr_Format(PyExc_SystemError,
                     "wrong number of children for 'except' clause: %d",
                     NCH(exc));
        return NULL;
    }
}

static PyObject*
ast_for_try_stmt(struct compiling *c, const node *n)
{
    PyObject *result = NULL;
    REQ(n, try_stmt);

    if (TYPE(CHILD(n, 3)) == NAME) {/* must be 'finally' */
	/* try_stmt: 'try' ':' suite 'finally' ':' suite) */
        PyObject *s1 = NULL; PyObject *s2 = NULL;
        s1 = ast_for_suite(c, CHILD(n, 2));
        if (!s1)
            return NULL;
        s2 = ast_for_suite(c, CHILD(n, 5));
        if (!s2) {
	    asdl_stmt_seq_free(s1);
            return NULL;
	}
            
	return TryFinally(s1, s2, LINENO(n));
    }
    else if (TYPE(CHILD(n, 3)) == except_clause) {
	/* try_stmt: ('try' ':' suite (except_clause ':' suite)+
           ['else' ':' suite]
	*/
        PyObject *suite_seq1 = NULL; PyObject *suite_seq2 = NULL;
	PyObject *handlers = NULL;
	int i, has_else = 0, n_except = NCH(n) - 3;
	if (TYPE(CHILD(n, NCH(n) - 3)) == NAME) {
	    has_else = 1;
	    n_except -= 3;
	}
	n_except /= 3;
	handlers = asdl_seq_new(n_except);
	if (!handlers)
		return NULL;
	for (i = 0; i < n_except; i++) {
            excepthandler_ty e = ast_for_except_clause(c,
                                                       CHILD(n, 3 + i * 3),
                                                       CHILD(n, 5 + i * 3));
            if (!e) {
		for ( ; i >= 0; i--)
		    free_excepthandler(asdl_seq_GET(handlers, i));
	        asdl_seq_free(handlers); /* ok */
                return NULL;
	    }
	    asdl_seq_SET(handlers, i, e);
        }

        suite_seq1 = ast_for_suite(c, CHILD(n, 2));
        if (!suite_seq1) {
	    for (i = 0; i < asdl_seq_LEN(handlers); i++)
		free_excepthandler(asdl_seq_GET(handlers, i));
	    asdl_seq_free(handlers); /* ok */
            return NULL;
	}
        if (has_else) {
            suite_seq2 = ast_for_suite(c, CHILD(n, NCH(n) - 1));
            if (!suite_seq2) {
	        for (i = 0; i < asdl_seq_LEN(handlers); i++)
		    free_excepthandler(asdl_seq_GET(handlers, i));
	        asdl_seq_free(handlers); /* ok */
		asdl_stmt_seq_free(suite_seq1);
                return NULL;
	    }
        }
        else
            suite_seq2 = NULL;

	return TryExcept(suite_seq1, handlers, suite_seq2, LINENO(n));
    }
    else {
        ast_error(n, "malformed 'try' statement");
        return NULL;
    }
}

static PyObject*
ast_for_classdef(struct compiling *c, const node *n)
{
    /* classdef: 'class' NAME ['(' testlist ')'] ':' suite */
    PyObject *result = NULL;
    PyObject *bases = NULL; PyObject *s = NULL;
    
    REQ(n, classdef);

    if (!strcmp(STR(CHILD(n, 1)), "None")) {
	    ast_error(n, "assignment to None");
	    return NULL;
    }

    if (NCH(n) == 4) {
        s = ast_for_suite(c, CHILD(n, 3));
        if (!s)
            return NULL;
	return ClassDef(NEW_IDENTIFIER(CHILD(n, 1)), NULL, s, LINENO(n));
    }
    /* check for empty base list */
    if (TYPE(CHILD(n,3)) == RPAR) {
	s = ast_for_suite(c, CHILD(n,5));
	if (!s)
		return NULL;
	return ClassDef(NEW_IDENTIFIER(CHILD(n, 1)), NULL, s, LINENO(n));
    }

    /* else handle the base class list */
    bases = ast_for_class_bases(c, CHILD(n, 3));
    if (!bases)
        return NULL;

    s = ast_for_suite(c, CHILD(n, 6));
    if (!s) {
        asdl_expr_seq_free(bases);
        return NULL;
    }
    return ClassDef(NEW_IDENTIFIER(CHILD(n, 1)), bases, s, LINENO(n));
}

static PyObject*
ast_for_stmt(struct compiling *c, const node *n)
{
    PyObject *result = NULL;
    if (TYPE(n) == stmt) {
	assert(NCH(n) == 1);
	n = CHILD(n, 0);
    }
    if (TYPE(n) == simple_stmt) {
	assert(num_stmts(n) == 1);
	n = CHILD(n, 0);
    }
    if (TYPE(n) == small_stmt) {
	REQ(n, small_stmt);
	n = CHILD(n, 0);
	/* small_stmt: expr_stmt | print_stmt  | del_stmt | pass_stmt
	             | flow_stmt | import_stmt | global_stmt | exec_stmt
                     | assert_stmt
	*/
	switch (TYPE(n)) {
            case expr_stmt:
                return ast_for_expr_stmt(c, n);
            case print_stmt:
                return ast_for_print_stmt(c, n);
            case del_stmt:
                return ast_for_del_stmt(c, n);
            case pass_stmt:
                return Pass(LINENO(n));
            case flow_stmt:
                return ast_for_flow_stmt(c, n);
            case import_stmt:
                return ast_for_import_stmt(c, n);
            case global_stmt:
                return ast_for_global_stmt(c, n);
            case exec_stmt:
                return ast_for_exec_stmt(c, n);
            case assert_stmt:
                return ast_for_assert_stmt(c, n);
            default:
                PyErr_Format(PyExc_SystemError,
                             "unhandled small_stmt: TYPE=%d NCH=%d\n",
                             TYPE(n), NCH(n));
                return NULL;
        }
    }
    else {
        /* compound_stmt: if_stmt | while_stmt | for_stmt | try_stmt
	                | funcdef | classdef
	*/
	node *ch = CHILD(n, 0);
	REQ(n, compound_stmt);
	switch (TYPE(ch)) {
            case if_stmt:
                return ast_for_if_stmt(c, ch);
            case while_stmt:
                return ast_for_while_stmt(c, ch);
            case for_stmt:
                return ast_for_for_stmt(c, ch);
            case try_stmt:
                return ast_for_try_stmt(c, ch);
            case funcdef:
                return ast_for_funcdef(c, ch);
            case classdef:
                return ast_for_classdef(c, ch);
            default:
                PyErr_Format(PyExc_SystemError,
                             "unhandled small_stmt: TYPE=%d NCH=%d\n",
                             TYPE(n), NCH(n));
                return NULL;
	}
    }
}

static PyObject *
parsenumber(const char *s)
{
	PyObject *result = NULL;
	const char *end;
	long x;
	double dx;
#ifndef WITHOUT_COMPLEX
	Py_complex c;
	int imflag;
#endif

	errno = 0;
	end = s + strlen(s) - 1;
#ifndef WITHOUT_COMPLEX
	imflag = *end == 'j' || *end == 'J';
#endif
	if (*end == 'l' || *end == 'L')
		return PyLong_FromString((char *)s, (char **)0, 0);
	if (s[0] == '0') {
		x = (long) PyOS_strtoul((char *)s, (char **)&end, 0);
 		if (x < 0 && errno == 0) {
	 			return PyLong_FromString((char *)s,
							 (char **)0,
							 0);
		}
	}
	else
		x = PyOS_strtol((char *)s, (char **)&end, 0);
	if (*end == '\0') {
		if (errno != 0)
			return PyLong_FromString((char *)s, (char **)0, 0);
		return PyInt_FromLong(x);
	}
	/* XXX Huge floats may silently fail */
#ifndef WITHOUT_COMPLEX
	if (imflag) {
		c.real = 0.;
		PyFPE_START_PROTECT("atof", return 0)
		c.imag = atof(s);
		PyFPE_END_PROTECT(c)
		return PyComplex_FromCComplex(c);
	}
	else
#endif
	{
		PyFPE_START_PROTECT("atof", return 0)
		dx = atof(s);
		PyFPE_END_PROTECT(dx)
		return PyFloat_FromDouble(dx);
	}
}

static PyObject *
decode_utf8(const char **sPtr, const char *end, char* encoding)
{
	PyObject *result = NULL;
#ifndef Py_USING_UNICODE
	Py_FatalError("decode_utf8 should not be called in this build.");
        return NULL;
#else
	PyObject *u, *v;
	char *s, *t;
	t = s = (char *)*sPtr;
	/* while (s < end && *s != '\\') s++; */ /* inefficient for u".." */
	while (s < end && (*s & 0x80)) s++;
	*sPtr = s;
	u = PyUnicode_DecodeUTF8(t, s - t, NULL);
	if (u == NULL)
		return NULL;
	v = PyUnicode_AsEncodedString(u, encoding, NULL);
	Py_DECREF(u);
	return v;
#endif
}

static PyObject *
decode_unicode(const char *s, size_t len, int rawmode, const char *encoding)
{
	PyObject *result = NULL;
	PyObject *v, *u;
	char *buf;
	char *p;
	const char *end;
	if (encoding == NULL) {
	     	buf = (char *)s;
		u = NULL;
	} else if (strcmp(encoding, "iso-8859-1") == 0) {
	     	buf = (char *)s;
		u = NULL;
	} else {
		/* "\XX" may become "\u005c\uHHLL" (12 bytes) */
		u = PyString_FromStringAndSize((char *)NULL, len * 4);
		if (u == NULL)
			return NULL;
		p = buf = PyString_AsString(u);
		end = s + len;
		while (s < end) {
			if (*s == '\\') {
				*p++ = *s++;
				if (*s & 0x80) {
					strcpy(p, "u005c");
					p += 5;
				}
			}
			if (*s & 0x80) { /* XXX inefficient */
				PyObject *w;
				char *r;
				int rn, i;
				w = decode_utf8(&s, end, "utf-16-be");
				if (w == NULL) {
					Py_DECREF(u);
					return NULL;
				}
				r = PyString_AsString(w);
				rn = PyString_Size(w);
				assert(rn % 2 == 0);
				for (i = 0; i < rn; i += 2) {
					sprintf(p, "\\u%02x%02x",
						r[i + 0] & 0xFF,
						r[i + 1] & 0xFF);
					p += 6;
				}
				Py_DECREF(w);
			} else {
				*p++ = *s++;
			}
		}
		len = p - buf;
		s = buf;
	}
	if (rawmode)
		v = PyUnicode_DecodeRawUnicodeEscape(s, len, NULL);
	else
		v = PyUnicode_DecodeUnicodeEscape(s, len, NULL);
	Py_XDECREF(u);
	return v;
}

/* s is a Python string literal, including the bracketing quote characters,
 * and r &/or u prefixes (if any), and embedded escape sequences (if any).
 * parsestr parses it, and returns the decoded Python string object.
 */
static PyObject *
parsestr(const char *s, const char *encoding)
{
	PyObject *result = NULL;
	PyObject *v;
	size_t len;
	int quote = *s;
	int rawmode = 0;
	int need_encoding;
	int unicode = 0;

	if (isalpha(quote) || quote == '_') {
		if (quote == 'u' || quote == 'U') {
			quote = *++s;
			unicode = 1;
		}
		if (quote == 'r' || quote == 'R') {
			quote = *++s;
			rawmode = 1;
		}
	}
	if (quote != '\'' && quote != '\"') {
		PyErr_BadInternalCall();
		return NULL;
	}
	s++;
	len = strlen(s);
	if (len > INT_MAX) {
		PyErr_SetString(PyExc_OverflowError, 
				"string to parse is too long");
		return NULL;
	}
	if (s[--len] != quote) {
		PyErr_BadInternalCall();
		return NULL;
	}
	if (len >= 4 && s[0] == quote && s[1] == quote) {
		s += 2;
		len -= 2;
		if (s[--len] != quote || s[--len] != quote) {
			PyErr_BadInternalCall();
			return NULL;
		}
	}
#ifdef Py_USING_UNICODE
	if (unicode || Py_UnicodeFlag) {
		return decode_unicode(s, len, rawmode, encoding);
	}
#endif
	need_encoding = (encoding != NULL &&
			 strcmp(encoding, "utf-8") != 0 &&
			 strcmp(encoding, "iso-8859-1") != 0);
	if (rawmode || strchr(s, '\\') == NULL) {
		if (need_encoding) {
#ifndef Py_USING_UNICODE
			/* This should not happen - we never see any other
			   encoding. */
			Py_FatalError("cannot deal with encodings in this build.");
#else
			PyObject* u = PyUnicode_DecodeUTF8(s, len, NULL);
			if (u == NULL)
				return NULL;
			v = PyUnicode_AsEncodedString(u, encoding, NULL);
			Py_DECREF(u);
			return v;
#endif
		} else {
			return PyString_FromStringAndSize(s, len);
		}
	}

	v = PyString_DecodeEscape(s, len, NULL, unicode,
				  need_encoding ? encoding : NULL);
	return v;
}

/* Build a Python string object out of a STRING atom.  This takes care of
 * compile-time literal catenation, calling parsestr() on each piece, and
 * pasting the intermediate results together.
 */
static PyObject *
parsestrplus(struct compiling *c, const node *n)
{
	PyObject *result = NULL;
	PyObject *v;
	int i;
	REQ(CHILD(n, 0), STRING);
	if ((v = parsestr(STR(CHILD(n, 0)), c->c_encoding)) != NULL) {
		/* String literal concatenation */
		for (i = 1; i < NCH(n); i++) {
			PyObject *s;
			s = parsestr(STR(CHILD(n, i)), c->c_encoding);
			if (s == NULL)
				goto onError;
			if (PyString_Check(v) && PyString_Check(s)) {
				PyString_ConcatAndDel(&v, s);
				if (v == NULL)
				    goto onError;
			}
#ifdef Py_USING_UNICODE
			else {
				PyObject *temp;
				temp = PyUnicode_Concat(v, s);
				Py_DECREF(s);
				if (temp == NULL)
					goto onError;
				Py_DECREF(v);
				v = temp;
			}
#endif
		}
	}
	return v;

 onError:
	Py_XDECREF(v);
	return NULL;
}
