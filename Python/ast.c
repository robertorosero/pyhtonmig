#include "Python.h"
#include "Python-ast.h"
#include "grammar.h"
#include "node.h"
#include "ast.h"
#include "token.h"
#include "parsetok.h"
#include "graminit.h"

#include <assert.h>

#if 1
#define fprintf if (0) fprintf
#endif

/* XXX TO DO
   - re-indent this file (should be done)
   - internal error checking (freeing memory, etc.)
   - syntax errors
*/

static asdl_seq *seq_for_testlist(const node *);
static expr_ty ast_for_expr(const node *);
static stmt_ty ast_for_stmt(const node *);
static asdl_seq *ast_for_suite(const node *);
static asdl_seq *ast_for_exprlist(const node *, int);
static expr_ty ast_for_testlist(const node *);

/* Note different signature for ast_for_call */
static expr_ty ast_for_call(const node *, expr_ty);

static PyObject *parsenumber(const char *);
static PyObject *parsestr(const char *s);
static PyObject *parsestrplus(const node *n);

extern grammar _PyParser_Grammar; /* From graminit.c */

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

mod_ty
PyAST_FromNode(const node *n)
{
    int i, j, num, total;
    asdl_seq *stmts = NULL;
    stmt_ty s;
    node *ch;

    switch (TYPE(n)) {
        case file_input:
            total = num_stmts(n);
            fprintf(stderr, "file_input containing %d statements\n", total);
            stmts = asdl_seq_new(total);
            if (!stmts)
                    return NULL;
            for (i = 0; i < NCH(n) - 1; i++) {
                ch = CHILD(n, i);
                if (TYPE(ch) == NEWLINE)
                    continue;
                REQ(ch, stmt);
                num = num_stmts(ch);
                if (num == 1) {
                    s = ast_for_stmt(ch);
                    if (!s)
                        goto error;
                    asdl_seq_APPEND(stmts, s);
                }
                else {
                    ch = CHILD(ch, 0);
                    REQ(ch, simple_stmt);
                    for (j = 0; j < num; j++) {
                        s = ast_for_stmt(CHILD(ch, j * 2));
                        if (!s)
                            goto error;
                        asdl_seq_APPEND(stmts, s);
                    }
                }
            }
            return Module(stmts);
        case eval_input: {
            expr_ty testlist_ast;
            
            testlist_ast = ast_for_testlist(CHILD(n, 0));
            if (!testlist_ast)
                goto error;
            return Expression(testlist_ast);
        }
        case single_input:
            if (TYPE(CHILD(n, 0)) == NEWLINE) {
                stmts = asdl_seq_new(1);
                if (!stmts)
                    return NULL;
                asdl_seq_SET(stmts, 0, Pass(n->n_lineno));
                return Interactive(stmts);
            }
            else {
                n = CHILD(n, 0);
                num = num_stmts(n);
                stmts = asdl_seq_new(num);
                if (!stmts)
                    return NULL;
                if (num == 1) {
                    asdl_seq_SET(stmts, 0, ast_for_stmt(n));
                }
                else {
                    /* Only a simple_stmt can contain multiple statements. */
                    REQ(n, simple_stmt);
                    for (i = 0; i < NCH(n); i += 2) {
                        stmt_ty s;
                        if (TYPE(CHILD(n, i)) == NEWLINE)
                            break;
                        s = ast_for_stmt(CHILD(n, i));
                        if (!s)
                            goto error;
                        asdl_seq_SET(stmts, i / 2, s);
                    }
                }

                return Interactive(stmts);
            }
        case encoding_decl:
	    /* XXX need to handle properlyi, ignore for now */
            stmts = asdl_seq_new(1);
            if (!stmts)
                return NULL;
            asdl_seq_SET(stmts, 0, Pass(n->n_lineno));
            return Interactive(stmts);
        default:
            goto error;
    }
 error:
    if (stmts)
	asdl_seq_free(stmts);
    fprintf(stderr, "error in PyAST_FromNode() exc? %c\n",
            PyErr_Occurred() ? 'Y': 'N');
    return NULL;
}

#ifndef LINENO
#define LINENO(n)	((n)->n_lineno)
#endif

#define NEW_IDENTIFIER(n) PyString_InternFromString(STR(n))

/* Return the AST repr. of the operator represented as syntax (|, ^, etc.)
*/

static operator_ty
get_operator(const node *n)
{
    switch (TYPE(n)) {
        case VBAR:
            return BitOr;
        case CIRCUMFLEX:
            return BitXor;
        case AMPER:
            return BitAnd;
        case LEFTSHIFT:
            return LShift;
        case RIGHTSHIFT:
            return RShift;
        case PLUS:
            return Add;
        case MINUS:
            return Sub;
        case STAR:
            return Mult;
        case SLASH:
            return Div;
        case DOUBLESLASH:
            return FloorDiv;
        case PERCENT:
            return Mod;
        default:
            return 0;
    }
}

/* Set the context ctx for expr_ty e returning 0 on success, -1 on error.

   Only sets context for expr kinds that "can appear in assignment context"
   (according to ../Parser/Python.asdl)

   If e is a sequential type, items in sequence will also have their context
   set.

   XXX: Exception got thrown when called with context 8 (Call_kind) while
   running ``make``:

       Traceback (most recent call last):
         File "./setup.py", line 4, in <module>
             __version__ = "$Revision$"
             Exception: can't set context for 8

    Another exception from running regrtest.py:

        code Lib/test/regrtest.py
        XXX undetected error
        Traceback (most recent call last):
          File "Lib/test/regrtest.py", line 71, in <module>
              import sys
              SyntaxError: can't set context for 8
   
*/

static int
set_context(expr_ty e, expr_context_ty ctx)
{
    asdl_seq *s = NULL;

    switch (e->kind) {
        case Attribute_kind:
            e->v.Attribute.ctx = ctx;
            break;
        case Subscript_kind:
            e->v.Subscript.ctx = ctx;
            break;
        case Name_kind:
            e->v.Name.ctx = ctx;
            break;
        case List_kind:
            e->v.List.ctx = ctx;
            s = e->v.List.elts;
            break;
        case Tuple_kind:
            e->v.Tuple.ctx = ctx;
            s = e->v.Tuple.elts;
            break;
        default:
	    /* XXX It's not clear why were't getting into this code,
	       although list comps seem like one possibility.

               This occurs in at least 2 cases:
                 [x(i) for i in range(3)] # Call_kind (8)
                 [i*2 for i in range(3)]  # BinOp_kind (2)

               The byte code generated seems to work fine.
               Maybe there's a problem with nested list comps?
	    */
	    fprintf(stderr, "can't set context for %d\n", e->kind);
            return 0;
    }
    if (s) {
	int i;

	for (i = 0; i < asdl_seq_LEN(s); i++) {
	    if (set_context(asdl_seq_GET(s, i), ctx) < 0)
		return -1;
	}
    }
    return 0;
}

static operator_ty
ast_for_augassign(const node *n)
{
    REQ(n, augassign);
    n = CHILD(n, 0);
    switch (STR(n)[0]) {
        case '+':
            return Add;
        case '-':
            return Sub;
        case '/':
            if (STR(n)[1] == '/')
                return FloorDiv;
            else
                return Div;
        case '%':
            return Mod;
        case '<':
            return LShift;
        case '>':
            return RShift;
        case '&':
            return BitAnd;
        case '^':
            return BitXor;
        case '|':
            return BitOr;
        case '*':
            if (STR(n)[1] == '*')
                return Pow;
            else
                return Mult;
        default:
            PyErr_Format(PyExc_Exception, "invalid augassign: %s", STR(n));
            return 0;
    }
}

static cmpop_ty
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
                return Lt;
            case GREATER:
                return Gt;
            case EQEQUAL:			/* == */
            case EQUAL:
                return Eq;
            case LESSEQUAL:
                return LtE;
            case GREATEREQUAL:
                return GtE;
            case NOTEQUAL:
                return NotEq;
            case NAME:
                if (strcmp(STR(n), "in") == 0)
                    return In;
                if (strcmp(STR(n), "is") == 0)
                    return Is;
            default:
                PyErr_Format(PyExc_Exception, "invalid comp_op: %s",
                             STR(n));
                return 0;
	}
    }
    else if (NCH(n) == 2) {
	/* handle "not in" and "is not" */
	switch (TYPE(CHILD(n, 0))) {
            case NAME:
                if (strcmp(STR(CHILD(n, 1)), "in") == 0)
                    return NotIn;
                if (strcmp(STR(CHILD(n, 0)), "is") == 0)
                    return IsNot;
            default:
                PyErr_Format(PyExc_Exception, "invalid comp_op: %s %s",
                             STR(CHILD(n, 0)), STR(CHILD(n, 1)));
                return 0;
	}
    }
    PyErr_Format(PyExc_Exception, "invalid comp_op: has %d children",
                 NCH(n));
    return 0;
}

static asdl_seq *
seq_for_testlist(const node *n)
{
    /* testlist: test (',' test)* [','] */
    asdl_seq *seq;
    expr_ty expression;
    int i;

    seq = asdl_seq_new((NCH(n) + 1) / 2);
    if (!seq)
        return NULL;

    for (i = 0; i < NCH(n); i += 2) {
        expression = ast_for_expr(CHILD(n, i));
        if (!expression) {
            asdl_seq_free(seq);
            return NULL;
        }
        
        asdl_seq_SET(seq, i / 2, expression);
    }
    return seq;
}

/* Create AST for argument list.

   XXX TO DO:
       - check for invalid argument lists like normal after default
       - handle nested tuple arguments
       - handle default arguments properly (might be problem somewhere else)
*/

static arguments_ty
ast_for_arguments(const node *n)
{
    /* parameters: '(' [varargslist] ')'
       varargslist: (fpdef ['=' test] ',')* ('*' NAME [',' '**' NAME]
            | '**' NAME) | fpdef ['=' test] (',' fpdef ['=' test])* [',']
    */
    int i, n_args = 0, n_defaults = 0;
    asdl_seq *args, *defaults;
    identifier vararg = NULL, kwarg = NULL;
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
	if (TYPE(ch) == fpdef)
	    n_args++;
	if (TYPE(ch) == EQUAL)
	    n_defaults++;
    }
    args = (n_args ? asdl_seq_new(n_args) : NULL);
    if (!args && n_args)
    	return NULL; /* Don't need to go to NULL; nothing allocated */
    defaults = (n_defaults ? asdl_seq_new(n_defaults) : NULL);
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
                if (NCH(ch) == 3)
                    /* XXX don't handle fplist yet */
                    goto error;
                if (TYPE(CHILD(ch, 0)) == NAME)
                    /* XXX check return value of Name call */
                    asdl_seq_APPEND(args, Name(NEW_IDENTIFIER(CHILD(ch, 0)),
                                               Param));
                /* XXX Need to worry about checking if TYPE(CHILD(n, i+1)) is
                   anything other than EQUAL or a comma? */
                /* XXX Should NCH(n) check be made a separate check? */
                if (i + 1 < NCH(n) && TYPE(CHILD(n, i + 1)) == EQUAL) {
                    asdl_seq_APPEND(defaults, ast_for_expr(CHILD(n, i + 2)));
                    i += 2;
                }
                i += 2; /* the name and the comma */
                break;
            case STAR:
                vararg = NEW_IDENTIFIER(CHILD(n, i+1));
                i += 3;
                break;
            case DOUBLESTAR:
                kwarg = NEW_IDENTIFIER(CHILD(n, i+1));
                i += 3;
                break;
            default:
                PyErr_Format(PyExc_Exception,
                             "unexpected node in varargslist: %d @ %d",
                             TYPE(ch), i);
                goto error;
	}
    }

    return arguments(args, vararg, kwarg, defaults);

 error:
    if (args)
        asdl_seq_free(args);
    if (defaults)
        asdl_seq_free(defaults);
    return NULL;
}

static stmt_ty
ast_for_funcdef(const node *n)
{
    /* funcdef: 'def' NAME parameters ':' suite */
    identifier name = NEW_IDENTIFIER(CHILD(n, 1));
    arguments_ty args;
    asdl_seq *body;
    
    REQ(n, funcdef);
    args = ast_for_arguments(CHILD(n, 2));
    if (!args)
        return NULL;
    body = ast_for_suite(CHILD(n, 4));
    if (!body)
        return NULL;

    return FunctionDef(name, args, body, LINENO(n));
}

static expr_ty
ast_for_lambdef(const node *n)
{
    /* lambdef: 'lambda' [varargslist] ':' test */
    arguments_ty args;
    expr_ty expression;

    if (NCH(n) == 3) {
        args = arguments(NULL, NULL, NULL, NULL);
        if (!args)
            return NULL;
        expression = ast_for_expr(CHILD(n, 2));
        if (!expression)
            return NULL;

	return Lambda(args, expression);
    }
    else {
        args = ast_for_arguments(CHILD(n, 1));
        if (!args)
            return NULL;
        expression = ast_for_expr(CHILD(n, 3));
        if (!expression)
            return NULL;
            
	return Lambda(args, expression);
    }
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
        PyErr_SetString(PyExc_Exception, "logic error in count_list_fors");
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

static expr_ty
ast_for_listcomp(const node *n)
{
    /* listmaker: test ( list_for | (',' test)* [','] )
       list_for: 'for' exprlist 'in' testlist_safe [list_iter]
       list_iter: list_for | list_if
       list_if: 'if' test [list_iter]
       testlist_safe: test [(',' test)+ [',']]
    */
    expr_ty elt;
    asdl_seq *listcomps;
    int i, n_fors;
    node *ch;

    fprintf(stderr, "listcomp at %d\n", n->n_lineno);

    REQ(n, listmaker);
    assert(NCH(n) > 1);

    elt = ast_for_expr(CHILD(n, 0));
    if (!elt)
        return NULL;

    if (set_context(elt, Load) == -1) {
	fprintf(stderr, "XXX 2\n");
        return NULL;
    }

    n_fors = count_list_fors(n);
    if (n_fors == -1)
        return NULL;

    listcomps = asdl_seq_new(n_fors);
    if (!listcomps) {
	/* XXX free(elt); */
    	return NULL;
    }
    
    ch = CHILD(n, 1);
    for (i = 0; i < n_fors; i++) {
	listcomp_ty c;
	asdl_seq *t;
        expr_ty expression;

	REQ(ch, list_for);

	t = ast_for_exprlist(CHILD(ch, 1), Store);
        if (!t) {
            asdl_seq_free(listcomps);
	    /* XXX free(elt); */
            return NULL;
        }
        expression = ast_for_testlist(CHILD(ch, 3));
        if (!expression) {
            asdl_seq_free(listcomps);
	    /* XXX free(elt); */
            return NULL;
        }

	if (asdl_seq_LEN(t) == 1)
	    c = listcomp(asdl_seq_GET(t, 0), expression, NULL);
	else
	    c = listcomp(Tuple(t, Store), expression, NULL);

        if (!c) {
            asdl_seq_free(listcomps);
	    /* XXX free(elt); */
            return NULL;
        }

	if (NCH(ch) == 5) {
	    int j, n_ifs;
	    asdl_seq *ifs;

	    ch = CHILD(ch, 4);
	    n_ifs = count_list_ifs(ch);
            if (n_ifs == -1) {
                asdl_seq_free(listcomps);
		/* XXX free(elt); */
                return NULL;
            }

	    ifs = asdl_seq_new(n_ifs);
	    if (!ifs) {
		asdl_seq_free(listcomps);
		/* XXX free(elt); */
		return NULL;
	    }

	    for (j = 0; j < n_ifs; j++) {
		REQ(ch, list_iter);

		ch = CHILD(ch, 0);
		REQ(ch, list_if);

		asdl_seq_APPEND(ifs, ast_for_expr(CHILD(ch, 1)));
		if (NCH(ch) == 3)
		    ch = CHILD(ch, 2);
	    }
	    /* on exit, must guarantee that ch is a list_for */
	    if (TYPE(ch) == list_iter)
		ch = CHILD(ch, 0);
            c->ifs = ifs;
	}
	asdl_seq_APPEND(listcomps, c);
    }

    return ListComp(elt, listcomps);
}

static expr_ty
ast_for_atom(const node *n)
{
    /* atom: '(' [testlist] ')' | '[' [listmaker] ']'
           | '{' [dictmaker] '}' | '`' testlist '`' | NAME | NUMBER | STRING+
    */
    node *ch = CHILD(n, 0);

    /* fprintf(stderr, "ast_for_atom((%d, %d))\n", TYPE(ch), NCH(ch)); */
    switch (TYPE(ch)) {
        case NAME:
            /* All names start in Load context, but may later be changed. */
            return Name(NEW_IDENTIFIER(ch), Load);
        case STRING: {
            PyObject *str = parsestrplus(n);

            if (!str)
                return NULL;

            return Str(str);
        }
        case NUMBER: {
            PyObject *pynum = parsenumber(STR(ch));

            if (!pynum)
                return NULL;
                
            return Num(pynum);
        }
        case LPAR: /* some parenthesized expressions */
            return ast_for_testlist(CHILD(n, 1));
        case LSQB: /* list (or list comprehension) */
            ch = CHILD(n, 1);
            if (TYPE(ch) == RSQB)
                    return List(NULL, Load);
            REQ(ch, listmaker);
            if (NCH(ch) == 1 || TYPE(CHILD(ch, 1)) == COMMA) {
                asdl_seq *elts = seq_for_testlist(ch);

                if (!elts)
                    return NULL;

                return List(elts, Load);
            }
            else
                return ast_for_listcomp(ch);
        case LBRACE: {
            /* dictmaker: test ':' test (',' test ':' test)* [','] */
            int i, size;
            asdl_seq *keys, *values;

            ch = CHILD(n, 1);
            size = (NCH(ch) + 1) / 4; /* plus one in case no trailing comma */
            keys = asdl_seq_new(size);
            if (!keys)
                    return NULL;
            values = asdl_seq_new(size);
            if (!values) {
                    asdl_seq_free(keys);
                    return NULL;
            }
            for (i = 0; i < NCH(ch); i += 4) {
                expr_ty expression;

                expression = ast_for_expr(CHILD(ch, i));
                if (!expression)
                    return NULL;
                    
                asdl_seq_SET(keys, i / 4, expression);

                expression = ast_for_expr(CHILD(ch, i + 2));
                if (!expression)
                    return NULL;
                    
                asdl_seq_SET(values, i / 4, expression);
            }
            return Dict(keys, values);
        }
        case BACKQUOTE: { /* repr */
            expr_ty expression = ast_for_testlist(CHILD(n, 1));

            if (!expression)
                return NULL;

            return Repr(expression);
        }
        default:
            PyErr_Format(PyExc_Exception, "unhandled atom %d", TYPE(ch));
            return NULL;
    }
}

static slice_ty
ast_for_slice(const node *n)
{
    node *ch;
    expr_ty lower = NULL, upper = NULL, step = NULL;

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
        step = ast_for_expr(ch); 
        if (!step)
            return NULL;
            
	return Index(step);
    }

    if (TYPE(ch) == test) {
	lower = ast_for_expr(ch);
        if (!lower)
            return NULL;
    }

    /* If there's an upper bound it's in the second or third position. */
    if (TYPE(ch) == COLON) {
	if (NCH(n) > 1) {
	    node *n2 = CHILD(n, 1);

	    if (TYPE(n2) == test) {
		upper = ast_for_expr(n2);
                if (!upper)
                    return NULL;
            }
	}
    } else if (NCH(n) > 2) {
	node *n2 = CHILD(n, 2);

	if (TYPE(n2) == test) {
	    upper = ast_for_expr(n2);
            if (!upper)
                return NULL;
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
	    step = ast_for_expr(ch);
            if (!step)
                return NULL;
        }
    }

    return Slice(lower, upper, step);
}

static expr_ty
ast_for_binop(const node *n)
{
	/* Must account for a sequence of expressions.
	   How should A op B op C by represented?  
	   BinOp(BinOp(A, op, B), op, C).
	*/

	int i, nops;
	expr_ty expr1, expr2, result;
        operator_ty operator;

        expr1 = ast_for_expr(CHILD(n, 0));
        if (!expr1)
            return NULL;

        expr2 = ast_for_expr(CHILD(n, 2));
        if (!expr2)
            return NULL;

        operator = get_operator(CHILD(n, 1));
        if (!operator)
            return NULL;

	result = BinOp(expr1, operator, expr2);
	if (!result)
            return NULL;

	nops = (NCH(n) - 1) / 2;
	for (i = 1; i < nops; i++) {
		expr_ty tmp_result, tmp;

                operator = get_operator(CHILD(n, i * 2 + 1));
                if (!operator)
                    return NULL;

                tmp = ast_for_expr(CHILD(n, i * 2 + 2));
                if (!tmp)
                    return NULL;

                tmp_result = BinOp(result, operator, tmp);
		if (!tmp) 
			return NULL;
		result = tmp_result;
	}
	return result;
}

/* Do not name a variable 'expr'!  Will cause a compile error.
*/

static expr_ty
ast_for_expr(const node *n)
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

    asdl_seq *seq;
    int i;

    /* fprintf(stderr, "ast_for_expr(%d, %d)\n", TYPE(n), NCH(n)); */
 loop:
    switch (TYPE(n)) {
        case test:
            if (TYPE(CHILD(n, 0)) == lambdef)
                return ast_for_lambdef(CHILD(n, 0));
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
                expr_ty e = ast_for_expr(CHILD(n, i));
                if (!e)
                    return NULL;
                asdl_seq_SET(seq, i / 2, e);
            }
            if (!strcmp(STR(CHILD(n, 1)), "and"))
                return BoolOp(And, seq);
            else {
                assert(!strcmp(STR(CHILD(n, 1)), "or"));
                return BoolOp(Or, seq);
            }
            break;
        case not_test:
            if (NCH(n) == 1) {
                n = CHILD(n, 0);
                goto loop;
            }
            else {
                expr_ty expression = ast_for_expr(CHILD(n, 1));
                if (!expression)
                    return NULL;

                return UnaryOp(Not, expression);
            }
        case comparison:
            if (NCH(n) == 1) {
                n = CHILD(n, 0);
                goto loop;
            }
            else {
                expr_ty expression;
                asdl_seq *ops, *cmps;
                ops = asdl_seq_new(NCH(n) / 2);
                if (!ops)
                    return NULL;
                cmps = asdl_seq_new(NCH(n) / 2);
                if (!cmps) {
                    asdl_seq_free(ops);
                    return NULL;
                }
                for (i = 1; i < NCH(n); i += 2) {
                    /* XXX cmpop_ty is just an enum */
                    cmpop_ty operator;

                    operator = ast_for_comp_op(CHILD(n, i));
                    if (!operator)
                        return NULL;

                    expression = ast_for_expr(CHILD(n, i + 1));
                    if (!expression)
                        return NULL;
                        
                    asdl_seq_SET(ops, i / 2, (void *)operator);
                    asdl_seq_SET(cmps, i / 2, expression);
                }
                expression = ast_for_expr(CHILD(n, 0));
                if (!expression)
                    return NULL;
                    
                return Compare(expression, ops, cmps);
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
            return ast_for_binop(n);
        case factor: {
            expr_ty expression;
            
            if (NCH(n) == 1) {
                n = CHILD(n, 0);
                goto loop;
            }

            expression = ast_for_expr(CHILD(n, 1));
            if (!expression)
                return NULL;

            switch (TYPE(CHILD(n, 0))) {
                case PLUS:
                    return UnaryOp(UAdd, expression);
                case MINUS:
                    return UnaryOp(USub, expression);
                case TILDE:
                    return UnaryOp(Invert, expression);
            }
            break;
        }
        case power: {
            expr_ty e = ast_for_atom(CHILD(n, 0));
            assert(e);
            if (NCH(n) == 1)
                return e;
            /* power: atom trailer* ('**' factor)*
               trailer: '(' [arglist] ')' | '[' subscriptlist ']' | '.' NAME 

               XXX What about atom trailer trailer ** factor?
            */
            for (i = 1; i < NCH(n); i++) {
                expr_ty new = e;
                node *ch = CHILD(n, i);
                if (ch->n_str && strcmp(ch->n_str, "**") == 0)
                    break;
                if (TYPE(CHILD(ch, 0)) == LPAR) {
                    if (NCH(ch) == 2)
                        new = Call(new, NULL, NULL, NULL, NULL);
                    else
                        new = ast_for_call(CHILD(ch, 1), new);

                    if (!new) {
		        /* XXX free(e); */
                        return NULL;
		    }
                }
                else if (TYPE(CHILD(ch, 0)) == LSQB) {
                    REQ(CHILD(ch, 2), RSQB);
                    ch = CHILD(ch, 1);
                    if (NCH(ch) <= 2) {
                        slice_ty slc = ast_for_slice(CHILD(ch, 0));
                        if (!slc) {
		            /* XXX free(e); */
                            return NULL;
			}

                        new = Subscript(e, slc, Load);
                        if (!new) {
		            /* XXX free(e); */
		            /* XXX free(slc); */
                            return NULL;
			}
                    }
                    else {
                        int j;
                        slice_ty slc;
                        asdl_seq *slices = asdl_seq_new(NCH(ch) / 2);
                        if (!slices) {
		            /* XXX free(e); */
                            return NULL;
			}

                        for (j = 0; j < NCH(ch); j += 2) {
                            slc = ast_for_slice(CHILD(ch, j));
                            if (!slc) {
		                /* XXX free(e); */
		                asdl_seq_free(slices);
                                return NULL;
			    }
                            asdl_seq_SET(slices, j / 2, slc);
                        }
                        new = Subscript(e, ExtSlice(slices), Load);
                        if (!new) {
		            /* XXX free(e); */
		            asdl_seq_free(slices);
                            return NULL;
			}
                    }
                }
                else {
                    assert(TYPE(CHILD(ch, 0)) == DOT);
                    new = Attribute(e, NEW_IDENTIFIER(CHILD(ch, 1)), Load);
                    if (!new) {
		        /* XXX free(e); */
                        return NULL;
		    }
                }
                e = new;
            }
            if (TYPE(CHILD(n, NCH(n) - 1)) == factor) {
                expr_ty f = ast_for_expr(CHILD(n, NCH(n) - 1));
                if (!f) {
		    /* XXX free(e); */
                    return NULL;
		}
                return BinOp(e, Pow, f);
            }
            return e;
        }
        default:
            PyErr_Format(PyExc_Exception, "unhandled expr: %d", TYPE(n));
            return NULL;
    }
    /* should never get here */
    return NULL;
}

static expr_ty
ast_for_call(const node *n, expr_ty func)
{
    /*
      arglist: (argument ',')* (argument [',']| '*' test [',' '**' test]
               | '**' test)
      argument: [test '='] test	# Really [keyword '='] test
    */

    int i, nargs, nkeywords;
    asdl_seq *args = NULL;
    asdl_seq *keywords = NULL;
    expr_ty vararg = NULL, kwarg = NULL;

    REQ(n, arglist);

    nargs = 0;
    nkeywords = 0;
    for (i = 0; i < NCH(n); i++) 
	if (TYPE(CHILD(n, i)) == argument) {
	    if (NCH(CHILD(n, i)) == 1)
		nargs++;
	    else
		nkeywords++;
	}

    args = asdl_seq_new(nargs);
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
	    expr_ty e;
	    if (NCH(ch) == 1) {
		e = ast_for_expr(CHILD(ch, 0));
                if (!e)
                    goto error;
		asdl_seq_SET(args, nargs++, e);
	    }  
	    else {
		keyword_ty kw;
		identifier key;

		/* CHILD(ch, 0) is test, but must be an identifier? */ 
		e = ast_for_expr(CHILD(ch, 0));
                if (!e)
                    goto error;
		assert(e->kind == Name_kind);
		key = e->v.Name.id;
		free(e);
		e = ast_for_expr(CHILD(ch, 2));
                if (!e)
                    goto error;
		kw = keyword(key, e);
                if (!kw)
                    goto error;
		asdl_seq_SET(keywords, nkeywords++, kw);
	    }
	}
	else if (TYPE(ch) == STAR) {
	    vararg = ast_for_expr(CHILD(n, i+1));
	    i++;
	}
	else if (TYPE(ch) == DOUBLESTAR) {
	    kwarg = ast_for_expr(CHILD(n, i+1));
	    i++;
	}
    }

    /* XXX syntax error if more than 255 arguments */
    return Call(func, args, keywords, vararg, kwarg);

 error:
    if (args)
        asdl_seq_free(args);
    if (keywords)
        asdl_seq_free(keywords);
    return NULL;
}

static expr_ty
ast_for_testlist(const node *n)
{
    /* n could be a testlist, a listmaker with no list_for, or
       a testlist1 from inside backquotes. */

    if (NCH(n) == 1)
	return ast_for_expr(CHILD(n, 0));
    else {
        asdl_seq *tmp = seq_for_testlist(n);
        if (!tmp)
            return NULL;

	return Tuple(tmp, Load);
    }
}

static stmt_ty
ast_for_expr_stmt(const node *n)
{
    REQ(n, expr_stmt);
    /* expr_stmt: testlist (augassign testlist | ('=' testlist)*)
       testlist: test (',' test)* [',']
       augassign: '+=' | '-=' | '*=' | '/=' | '%=' | '&=' | '|=' | '^='
	        | '<<=' | '>>=' | '**=' | '//='
       test: ... here starts the operator precendence dance
     */

    /* fprintf(stderr, "ast_for_expr_stmt(%d, %d)\n", TYPE(n), NCH(n)); */
    if (NCH(n) == 1) {
	expr_ty e = ast_for_testlist(CHILD(n, 0));
        if (!e)
            return NULL;

	return Expr(e, LINENO(n));
    }
    else if (TYPE(CHILD(n, 1)) == augassign) {
        expr_ty expr1, expr2;
        operator_ty operator;

        expr1 = ast_for_testlist(CHILD(n, 0));
        if (!expr1)
            return NULL;

        expr2 = ast_for_testlist(CHILD(n, 2));
        if (!expr2)
            return NULL;

        operator = ast_for_augassign(CHILD(n, 1));
        if (!operator)
            return NULL;

	return AugAssign(expr1, operator, expr2, LINENO(n));
    }
    else {
	int i, tmp;
	asdl_seq *targets;
        expr_ty expression;

	/* a normal assignment */
	REQ(CHILD(n, 1), EQUAL);
	targets = asdl_seq_new(NCH(n) / 2);
	if (!targets)
	    return NULL;
	for (i = 0; i < NCH(n) - 2; i += 2) {
	    expr_ty e = ast_for_testlist(CHILD(n, i));
	    /* set context to assign */
	    if (!e) {
		asdl_seq_free(targets);
		return NULL;
	    }
	    tmp = set_context(e, Store);
            if (tmp == -1) {
                asdl_seq_free(targets);
                return NULL;
            }
	    asdl_seq_SET(targets, i / 2, e);
	}
        expression = ast_for_testlist(CHILD(n, NCH(n) - 1));
	return Assign(targets, expression, LINENO(n));
    }
    return NULL;
}

static stmt_ty
ast_for_print_stmt(const node *n)
{
    /* print_stmt: 'print' ( [ test (',' test)* [','] ]
                             | '>>' test [ (',' test)+ [','] ] )
     */
    expr_ty dest = NULL, expression;
    asdl_seq *seq;
    bool nl;
    int i, start = 1;

    REQ(n, print_stmt);
    if (NCH(n) >= 2 && TYPE(CHILD(n, 1)) == RIGHTSHIFT) {
	dest = ast_for_expr(CHILD(n, 2));
        if (!dest)
            return NULL;
	start = 4;
    }
    seq = asdl_seq_new((NCH(n) + 1 - start) / 2);
    if (!seq)
	return NULL;
    for (i = start; i < NCH(n); i += 2) {
        expression = ast_for_expr(CHILD(n, i));
        if (!expression) {
	    asdl_seq_free(seq);
            return NULL;
	}

	asdl_seq_APPEND(seq, expression);
    }
    nl = (TYPE(CHILD(n, NCH(n) - 1)) == COMMA) ? false : true;
    return Print(dest, seq, nl, LINENO(n));
}

static asdl_seq *
ast_for_exprlist(const node *n, int context)
{
    asdl_seq *seq;
    int i;
    expr_ty e;

    /* fprintf(stderr, "ast_for_exprlist(%d, %d)\n", TYPE(n), context); */
    REQ(n, exprlist);

    seq = asdl_seq_new((NCH(n) + 1) / 2);
    if (!seq)
	return NULL;
    for (i = 0; i < NCH(n); i += 2) {
	e = ast_for_expr(CHILD(n, i));
	if (!e) {
	    asdl_seq_free(seq);
	    return NULL;
	}
	if (context) {
            int context_result = set_context(e, context);
	    if (context_result == -1)
                return NULL;
        }
	asdl_seq_SET(seq, i / 2, e);
    }
    return seq;
}

static stmt_ty
ast_for_del_stmt(const node *n)
{
    asdl_seq *expr_list;
    
    /* del_stmt: 'del' exprlist */
    REQ(n, del_stmt);

    expr_list = ast_for_exprlist(CHILD(n, 1), Del);
    if (!expr_list)
        return NULL;
    return Delete(expr_list, LINENO(n));
}

static stmt_ty
ast_for_flow_stmt(const node *n)
{
    /*
      flow_stmt: break_stmt | continue_stmt | return_stmt | raise_stmt
                 | yield_stmt
      break_stmt: 'break'
      continue_stmt: 'continue'
      return_stmt: 'return' [testlist]
      yield_stmt: 'yield' testlist
      raise_stmt: 'raise' [test [',' test [',' test]]]
    */
    node *ch;

    REQ(n, flow_stmt);
    ch = CHILD(n, 0);
    switch (TYPE(ch)) {
        case break_stmt:
            return Break(LINENO(n));
        case continue_stmt:
            return Continue(LINENO(n));
        case yield_stmt: {
            expr_ty expression = ast_for_testlist(CHILD(ch, 1));
            if (!expression)
                return NULL;
            return Yield(expression, LINENO(n));
        }
        case return_stmt:
            if (NCH(ch) == 1)
                return Return(NULL, LINENO(n));
            else {
                expr_ty expression = ast_for_testlist(CHILD(ch, 1));
                if (!expression)
                    return NULL;
                return Return(expression, LINENO(n));
            }
        case raise_stmt:
            if (NCH(ch) == 1)
                return Raise(NULL, NULL, NULL, LINENO(n));
            else if (NCH(ch) == 2) {
                expr_ty expression = ast_for_expr(CHILD(ch, 1));
                if (!expression)
                    return NULL;
                return Raise(expression, NULL, NULL, LINENO(n));
            }
            else if (NCH(ch) == 4) {
                expr_ty expr1, expr2;

                expr1 = ast_for_expr(CHILD(ch, 1));
                if (!expr1)
                    return NULL;
                expr2 = ast_for_expr(CHILD(ch, 3));
                if (!expr2)
                    return NULL;

                return Raise(expr1, expr2, NULL, LINENO(n));
            }
            else if (NCH(ch) == 6) {
                expr_ty expr1, expr2, expr3;

                expr1 = ast_for_expr(CHILD(ch, 1));
                if (!expr1)
                    return NULL;
                expr2 = ast_for_expr(CHILD(ch, 3));
                if (!expr2)
                    return NULL;
                expr3 = ast_for_expr(CHILD(ch, 5));
                if (!expr3)
                    return NULL;
                    
                return Raise(expr1, expr2, expr3, LINENO(n));
            }
        default:
            PyErr_Format(PyExc_Exception,
                         "unexpected flow_stmt: %d", TYPE(ch));
            return NULL;
    }
}

static alias_ty
alias_for_import_name(const node *n)
{
    /*
      import_as_name: NAME [NAME NAME]
      dotted_as_name: dotted_name [NAME NAME]
      dotted_name: NAME ('.' NAME)*
    */
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
            PyErr_Format(PyExc_Exception,
                         "unexpected import name: %d", TYPE(n));
            return NULL;
    }
    return NULL;
}

static stmt_ty
ast_for_import_stmt(const node *n)
{
    /*
      import_stmt: 'import' dotted_as_name (',' dotted_as_name)*
                 | 'from' dotted_name 'import' ('*'
                              | import_as_name (',' import_as_name)*)
    */
    int i;
    asdl_seq *aliases;

    REQ(n, import_stmt);
    if (STR(CHILD(n, 0))[0] == 'i') { /* import */
	aliases = asdl_seq_new(NCH(n) / 2);
	if (!aliases)
		return NULL;
	for (i = 1; i < NCH(n); i += 2) {
            alias_ty import_alias = alias_for_import_name(CHILD(n, i));
            if (!import_alias) {
                asdl_seq_free(aliases);
                return NULL;
            }
	    asdl_seq_SET(aliases, i / 2, import_alias);
        }
	return Import(aliases, LINENO(n));
    }
    else if (STR(CHILD(n, 0))[0] == 'f') { /* from */
	stmt_ty import;
	alias_ty mod = alias_for_import_name(CHILD(n, 1));
	if (!mod)
            return NULL;
	aliases = asdl_seq_new((NCH(n) - 2) / 2);
	if (!aliases) {
            free(mod); /* XXX proper way to free alias_ty structs? */
            return NULL;
	}
	for (i = 3; i <= NCH(n); i += 2) {
            alias_ty import_alias = alias_for_import_name(CHILD(n, i));
            if (!import_alias) {
                asdl_seq_free(aliases);
                return NULL;
            }
	    asdl_seq_APPEND(aliases, import_alias);
        }
	import = ImportFrom(mod->name, aliases, LINENO(n));
	free(mod);
	return import;
    }
    PyErr_Format(PyExc_Exception,
                 "unknown import statement: starts with command '%s'",
                 STR(CHILD(n, 0)));
    return NULL;
}

static stmt_ty
ast_for_global_stmt(const node *n)
{
    /* global_stmt: 'global' NAME (',' NAME)* */
    identifier name;
    asdl_seq *s;
    int i;

    REQ(n, global_stmt);
    s = asdl_seq_new(NCH(n) / 2);
    if (!s)
    	return NULL;
    for (i = 1; i < NCH(n); i += 2) {
	name = NEW_IDENTIFIER(CHILD(n, i));
	if (!name) {
	    asdl_seq_free(s);
	    return NULL;
	}
	asdl_seq_SET(s, i / 2, name);
    }
    return Global(s, LINENO(n));
}

static stmt_ty
ast_for_exec_stmt(const node *n)
{
    /* exec_stmt: 'exec' expr ['in' test [',' test]] */
    REQ(n, exec_stmt);
    if (NCH(n) == 2) {
        expr_ty expression = ast_for_expr(CHILD(n, 1));
        if (!expression)
            return NULL;
	return Exec(expression, NULL, NULL, LINENO(n));
    }
    else if (NCH(n) == 4) {
        expr_ty expr1, expr2;

        expr1 = ast_for_expr(CHILD(n, 1));
        if (!expr1)
            return NULL;
        if (!expr2)
            return NULL;

	return Exec(expr1, expr2, NULL, LINENO(n));
    }
    else if (NCH(n) == 6) {
        expr_ty expr1, expr2, expr3;

        expr1 = ast_for_expr(CHILD(n, 1));
        if (!expr1)
            return NULL;
        expr2 = ast_for_expr(CHILD(n, 3));
        if (!expr2)
            return NULL;
        expr3 = ast_for_expr(CHILD(n, 5));
        if (!expr3)
            return NULL;

	return Exec(expr1, expr2, expr3, LINENO(n));
    }
    PyErr_Format(PyExc_Exception,
                 "poorly formed 'exec' statement: %d parts to statement",
                 NCH(n));
    return NULL;
}

static stmt_ty
ast_for_assert_stmt(const node *n)
{
    /* assert_stmt: 'assert' test [',' test] */
    REQ(n, assert_stmt);
    if (NCH(n) == 2) {
        expr_ty expression = ast_for_expr(CHILD(n, 1));
        if (!expression)
            return NULL;
	return Assert(expression, NULL, LINENO(n));
    }
    else if (NCH(n) == 4) {
        expr_ty expr1, expr2;

        expr1 = ast_for_expr(CHILD(n, 1));
        if (!expr1)
            return NULL;
        expr2 = ast_for_expr(CHILD(n, 3));
        if (!expr2)
            return NULL;
            
	return Assert(expr1, expr2, LINENO(n));
    }
    PyErr_Format(PyExc_Exception,
                 "improper number of parts to 'assert' statement: %d",
                 NCH(n));
    return NULL;
}

static asdl_seq *
ast_for_suite(const node *n)
{
    /* suite: simple_stmt | NEWLINE INDENT stmt+ DEDENT */
    asdl_seq *seq = NULL;
    stmt_ty s;
    int i, total, num, pos = 0;
    node *ch;

    /* fprintf(stderr, "ast_for_suite(%d) lineno=%d\n", TYPE(n), n->n_lineno); */
    REQ(n, suite);

    total = num_stmts(n);
    seq = asdl_seq_new(total);
    if (!seq)
    	return NULL;
    if (TYPE(CHILD(n, 0)) == simple_stmt) {
	n = CHILD(n, 0);
        /* loop by 2 to skip semi-colons */
	for (i = 0; i < NCH(n); i += 2) {
	    ch = CHILD(n, i);
	    s = ast_for_stmt(ch);
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
		s = ast_for_stmt(ch);
		if (!s)
		    goto error;
		asdl_seq_SET(seq, pos++, s);
	    }
	    else {
		int j;
		ch = CHILD(ch, 0);
		REQ(ch, simple_stmt);
		for (j = 0; j < NCH(ch); j += 2) {
		    s = ast_for_stmt(CHILD(ch, j));
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
	asdl_seq_free(seq);
    return NULL;
}

static stmt_ty
ast_for_if_stmt(const node *n)
{
    /* if_stmt: 'if' test ':' suite ('elif' test ':' suite)*
       ['else' ':' suite]
    */
    char *s;

    REQ(n, if_stmt);

    if (NCH(n) == 4) {
        expr_ty expression;
        asdl_seq *suite_seq;

        expression = ast_for_expr(CHILD(n, 1));
        if (!expression)
            return NULL;
        suite_seq = ast_for_suite(CHILD(n, 3)); 
        if (!suite_seq)
            return NULL;
            
	return If(expression, suite_seq, NULL, LINENO(n));
    }
    s = STR(CHILD(n, 4));
    /* s[2], the third character in the string, will be
       's' for el_s_e, or
       'i' for el_i_f
    */
    if (s[2] == 's') {
        expr_ty expression;
        asdl_seq *seq1, *seq2;

        expression = ast_for_expr(CHILD(n, 1));
        if (!expression)
            return NULL;
        seq1 = ast_for_suite(CHILD(n, 3));
        if (!seq1)
            return NULL;
        seq2 = ast_for_suite(CHILD(n, 6));
        if (!seq2)
            return NULL;

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
            expr_ty expression;
            asdl_seq *seq1, *seq2;
        
	    orelse = asdl_seq_new(1);
	    if (!orelse)
		return NULL;
            expression = ast_for_expr(CHILD(n, NCH(n) - 6));
            if (!expression) {
                asdl_seq_free(orelse);
                return NULL;
            }
            seq1 = ast_for_suite(CHILD(n, NCH(n) - 4));
            if (!seq1) {
                asdl_seq_free(orelse);
                return NULL;
            }
            seq2 = ast_for_suite(CHILD(n, NCH(n) - 1));
            if (!seq2) {
                asdl_seq_free(orelse);
                return NULL;
            }

	    asdl_seq_SET(orelse, 0, If(expression, seq1, seq2, LINENO(n)));
	    /* the just-created orelse handled the last elif */
	    n_elif--;
	}
        else
            orelse  = NULL;

	for (i = 0; i < n_elif; i++) {
	    int off = 5 + (n_elif - i - 1) * 4;
            expr_ty expression;
            asdl_seq *suite_seq;
	    asdl_seq *new = asdl_seq_new(1);
	    if (!new)
		return NULL;
            expression = ast_for_expr(CHILD(n, off));
            if (!expression) {
                asdl_seq_free(new);
                return NULL;
            }
            suite_seq = ast_for_suite(CHILD(n, off + 2));
            if (!suite_seq) {
                asdl_seq_free(new);
                return NULL;
            }

	    asdl_seq_SET(new, 0,
			 If(expression, suite_seq, orelse, LINENO(n)));
	    orelse = new;
	}
	return If(ast_for_expr(CHILD(n, 1)),
		  ast_for_suite(CHILD(n, 3)),
		  orelse, LINENO(n));
    }
    else {
        PyErr_Format(PyExc_Exception,
                     "unexpected token in 'if' statement: %s", s);
        return NULL;
    }
}

static stmt_ty
ast_for_while_stmt(const node *n)
{
    /* while_stmt: 'while' test ':' suite ['else' ':' suite] */
    REQ(n, while_stmt);

    if (NCH(n) == 4) {
        expr_ty expression;
        asdl_seq *suite_seq;

        expression = ast_for_expr(CHILD(n, 1));
        if (!expression)
            return NULL;
        suite_seq = ast_for_suite(CHILD(n, 3));
        if (!suite_seq)
            return NULL;
	return While(expression, suite_seq, NULL, LINENO(n));
    }
    else if (NCH(n) == 7) {
        expr_ty expression;
        asdl_seq *seq1, *seq2;

        expression = ast_for_expr(CHILD(n, 1));
        if (!expression)
            return NULL;
        seq1 = ast_for_suite(CHILD(n, 3));
        if (!seq1)
            return NULL;
        seq2 = ast_for_suite(CHILD(n, 6));
        if (!seq2)
            return NULL;

	return While(expression, seq1, seq2, LINENO(n));
    }
    else {
        PyErr_Format(PyExc_Exception,
                     "wrong number of tokens for 'while' statement: %d",
                     NCH(n));
        return NULL;
    }
}

static stmt_ty
ast_for_for_stmt(const node *n)
{
    asdl_seq *_target = NULL, *seq = NULL, *suite_seq = NULL;
    expr_ty expression;
    expr_ty target;
    /* for_stmt: 'for' exprlist 'in' testlist ':' suite ['else' ':' suite] */
    REQ(n, for_stmt);

    if (NCH(n) == 9) {
	seq = ast_for_suite(CHILD(n, 8));
        if (!seq)
            return NULL;
    }

    _target = ast_for_exprlist(CHILD(n, 1), Store);
    if (!_target)
        return NULL;
    if (asdl_seq_LEN(_target) == 1) {
	target = asdl_seq_GET(_target, 0);
	asdl_seq_free(_target);
    }
    else
	target = Tuple(_target, Store);

    expression = ast_for_testlist(CHILD(n, 3));
    if (!expression)
        return NULL;
    suite_seq = ast_for_suite(CHILD(n, 5));
    if (!suite_seq)
        return NULL;

    return For(target, expression, suite_seq, seq, LINENO(n));
}

static excepthandler_ty
ast_for_except_clause(const node *exc, node *body)
{
    /* except_clause: 'except' [test [',' test]] */
    REQ(exc, except_clause);
    REQ(body, suite);

    if (NCH(exc) == 1) {
        asdl_seq *suite_seq = ast_for_suite(body);
        if (!suite_seq)
            return NULL;

	return excepthandler(NULL, NULL, suite_seq);
    }
    else if (NCH(exc) == 2) {
        expr_ty expression;
        asdl_seq *suite_seq;

        expression = ast_for_expr(CHILD(exc, 1));
        if (!expression)
            return NULL;
        suite_seq = ast_for_suite(body);
        if (!suite_seq)
            return NULL;

	return excepthandler(expression, NULL, suite_seq);
    }
    else if (NCH(exc) == 4) {
        asdl_seq *suite_seq;
        expr_ty expression;
	expr_ty e = ast_for_expr(CHILD(exc, 3));
	if (!e)
            return NULL;
	if (set_context(e, Store) == -1)
            return NULL;
        expression = ast_for_expr(CHILD(exc, 1));
        if (!expression)
            return NULL;
        suite_seq = ast_for_suite(body);
        if (!suite_seq)
            return NULL;

	return excepthandler(expression, e, suite_seq);
    }
    else {
        PyErr_Format(PyExc_Exception,
                     "wrong number of children for 'except' clause: %d",
                     NCH(exc));
        return NULL;
    }
}

static stmt_ty
ast_for_try_stmt(const node *n)
{
    REQ(n, try_stmt);

    if (TYPE(CHILD(n, 3)) == NAME) {/* must be 'finally' */
	/* try_stmt: 'try' ':' suite 'finally' ':' suite) */
        asdl_seq *s1, *s2;
        s1 = ast_for_suite(CHILD(n, 2));
        if (!s1)
            return NULL;
        s2 = ast_for_suite(CHILD(n, 5));
        if (!s2)
            return NULL;
            
	return TryFinally(s1, s2, LINENO(n));
    }
    else if (TYPE(CHILD(n, 3)) == except_clause) {
	/* try_stmt: ('try' ':' suite (except_clause ':' suite)+
           ['else' ':' suite]
	*/
        asdl_seq *suite_seq1, *suite_seq2;
	asdl_seq *handlers;
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
            excepthandler_ty e = ast_for_except_clause(CHILD(n, 3 + i * 3),
                                                       CHILD(n, 5 + i * 3));
            if (!e)
                return NULL;
	    asdl_seq_SET(handlers, i, e);
        }

        suite_seq1 = ast_for_suite(CHILD(n, 2));
        if (!suite_seq1)
            return NULL;
        if (has_else) {
            suite_seq2 = ast_for_suite(CHILD(n, NCH(n) - 1));
            if (!suite_seq2)
                return NULL;
        }
        else
            suite_seq2 = NULL;

	return TryExcept(suite_seq1, handlers, suite_seq2, LINENO(n));
    }
    else {
        PyErr_SetString(PyExc_Exception, "malformed 'try' statement");
        return NULL;
    }
}

static stmt_ty
ast_for_classdef(const node *n)
{
    /* classdef: 'class' NAME ['(' testlist ')'] ':' suite */
    expr_ty _bases;
    asdl_seq *bases, *s;
    
    REQ(n, classdef);

    if (NCH(n) == 4) {
        s = ast_for_suite(CHILD(n, 3));
        if (!s)
            return NULL;
	return ClassDef(NEW_IDENTIFIER(CHILD(n, 1)), NULL, s, LINENO(n));
    }
    /* else handle the base class list */
    _bases = ast_for_testlist(CHILD(n, 3));
    if (!_bases)
        return NULL;
    if (_bases->kind == Tuple_kind)
	bases = _bases->v.Tuple.elts;
    else {
	bases = asdl_seq_new(1);
	if (!bases)
            return NULL;
	asdl_seq_SET(bases, 0, _bases);
    }
    s = ast_for_suite(CHILD(n, 6));
    if (!s) {
        asdl_seq_free(bases);   /* XXX is this right for Tuples??? */
        return NULL;
    }
    return ClassDef(NEW_IDENTIFIER(CHILD(n, 1)), bases, s, LINENO(n));
}

static stmt_ty
ast_for_stmt(const node *n)
{
    /* fprintf(stderr, "ast_for_stmt(%d) lineno=%d\n",
       TYPE(n), n->n_lineno); */
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
                return ast_for_expr_stmt(n);
            case print_stmt:
                return ast_for_print_stmt(n);
            case del_stmt:
                return ast_for_del_stmt(n);
            case pass_stmt:
                return Pass(LINENO(n));
            case flow_stmt:
                return ast_for_flow_stmt(n);
            case import_stmt:
                return ast_for_import_stmt(n);
            case global_stmt:
                return ast_for_global_stmt(n);
            case exec_stmt:
                return ast_for_exec_stmt(n);
            case assert_stmt:
                return ast_for_assert_stmt(n);
            default:
                PyErr_Format(PyExc_Exception,
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
                return ast_for_if_stmt(ch);
            case while_stmt:
                return ast_for_while_stmt(ch);
            case for_stmt:
                return ast_for_for_stmt(ch);
            case try_stmt:
                return ast_for_try_stmt(ch);
            case funcdef:
                return ast_for_funcdef(ch);
            case classdef:
                return ast_for_classdef(ch);
            default:
                PyErr_Format(PyExc_Exception,
                             "unhandled small_stmt: TYPE=%d NCH=%d\n",
                             TYPE(n), NCH(n));
                return NULL;
	}
    }
}


static PyObject *
parsenumber(const char *s)
{
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
	if (s[0] == '0')
		x = (long) PyOS_strtoul((char *)s, (char **)&end, 0);
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

/* s is a Python string literal, including the bracketing quote characters,
 * and r &/or u prefixes (if any), and embedded escape sequences (if any).
 * parsestr parses it, and returns the decoded Python string object.
 */
static PyObject *
parsestr(const char *s)
{
	PyObject *v;
	size_t len;
	char *buf;
	char *p;
	const char *end;
	int c;
	int first = *s;
	int quote = first;
	int rawmode = 0;
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
		if (rawmode)
			v = PyUnicode_DecodeRawUnicodeEscape(
				 s, len, NULL);
		else
			v = PyUnicode_DecodeUnicodeEscape(
				s, len, NULL);
		return v;

	}
#endif
	if (rawmode || strchr(s, '\\') == NULL)
		return PyString_FromStringAndSize(s, len);
	v = PyString_FromStringAndSize((char *)NULL, len);
	if (v == NULL)
		return NULL;
	p = buf = PyString_AsString(v);
	end = s + len;
	while (s < end) {
		if (*s != '\\') {
			*p++ = *s++;
			continue;
		}
		s++;
		switch (*s++) {
		/* XXX This assumes ASCII! */
		case '\n': break;
		case '\\': *p++ = '\\'; break;
		case '\'': *p++ = '\''; break;
		case '\"': *p++ = '\"'; break;
		case 'b': *p++ = '\b'; break;
		case 'f': *p++ = '\014'; break; /* FF */
		case 't': *p++ = '\t'; break;
		case 'n': *p++ = '\n'; break;
		case 'r': *p++ = '\r'; break;
		case 'v': *p++ = '\013'; break; /* VT */
		case 'a': *p++ = '\007'; break; /* BEL, not classic C */
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			c = s[-1] - '0';
			if ('0' <= *s && *s <= '7') {
				c = (c<<3) + *s++ - '0';
				if ('0' <= *s && *s <= '7')
					c = (c<<3) + *s++ - '0';
			}
			*p++ = c;
			break;
		case 'x':
			if (isxdigit(Py_CHARMASK(s[0]))
			    && isxdigit(Py_CHARMASK(s[1]))) {
				unsigned int x = 0;
				c = Py_CHARMASK(*s);
				s++;
				if (isdigit(c))
					x = c - '0';
				else if (islower(c))
					x = 10 + c - 'a';
				else
					x = 10 + c - 'A';
				x = x << 4;
				c = Py_CHARMASK(*s);
				s++;
				if (isdigit(c))
					x += c - '0';
				else if (islower(c))
					x += 10 + c - 'a';
				else
					x += 10 + c - 'A';
				*p++ = x;
				break;
			}
			Py_DECREF(v);
			PyErr_SetString(PyExc_ValueError,
				  "invalid \\x escape");
			return NULL;
#ifndef Py_USING_UNICODE
		case 'u':
		case 'U':
		case 'N':
			if (unicode) {
				Py_DECREF(v);
				PyErr_SetString(PyExc_ValueError,
					  "Unicode escapes not legal "
					  "when Unicode disabled");
				return NULL;
			}
#endif
		default:
			*p++ = '\\';
			*p++ = s[-1];
			break;
		}
	}
	_PyString_Resize(&v, (int)(p - buf));
	return v;
}

/* Build a Python string object out of a STRING atom.  This takes care of
 * compile-time literal catenation, calling parsestr() on each piece, and
 * pasting the intermediate results together.
 */
static PyObject *
parsestrplus(const node *n)
{
	PyObject *v;
	int i;
	REQ(CHILD(n, 0), STRING);
	if ((v = parsestr(STR(CHILD(n, 0)))) != NULL) {
		/* String literal concatenation */
		for (i = 1; i < NCH(n); i++) {
			PyObject *s;
			s = parsestr(STR(CHILD(n, i)));
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
