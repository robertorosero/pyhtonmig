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
   re-indent this file
   internal error checking
   syntax errors
*/

static asdl_seq *seq_for_testlist(node *);
static expr_ty ast_for_expr(node *);
static stmt_ty ast_for_stmt(node *);
static asdl_seq *ast_for_suite(node *);
static asdl_seq *ast_for_exprlist(node *, int);
static expr_ty ast_for_testlist(node *);

extern grammar _PyParser_Grammar; /* From graminit.c */

mod_ty PyAST_FromNode(node *n)
{
    int i;
    asdl_seq *stmts;
    stmt_ty s;

    REQ(n, file_input);
    stmts = asdl_seq_new(NCH(n) / 2);
    for (i = 0; i < NCH(n); i++) {
	if (TYPE(CHILD(n, i)) == stmt) {
	    s = ast_for_stmt(CHILD(n, i));
	    if (!s) {
		asdl_seq_free(stmts);
		return NULL;
	    }
	    if (asdl_seq_append(stmts, s) < 0) {
		return NULL;
	    }
	}
	else
	    fprintf(stderr, "skipping %d\n", TYPE(CHILD(n, i)));
    }
    return Module(stmts);
}

#ifndef LINENO
#define LINENO(n)	((n)->n_lineno)
#endif

#define NEW_IDENTIFIER(n) PyString_InternFromString(STR(n))

static operator_ty
get_operator(node *n)
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
	    /* This should trigger syntax error */
	fprintf(stderr, "can't set context for %d\n", e->kind);
	return -1;
    }
    if (s) {
	int i;
	for (i = 0; i < asdl_seq_LEN(s); i++) {
	    if (set_context(asdl_seq_get(s, i), ctx) < 0)
		return -1;
	}
    }
    return 0;
}

static operator_ty
ast_for_augassign(node *n)
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
	fprintf(stderr, "invalid augassign: %s", STR(n));
	return 0;
    }
}

static cmpop_ty
ast_for_comp_op(node *n)
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
	}
    }
    return 0;
}

static asdl_seq *
seq_for_testlist(node *n)
{
    asdl_seq *seq;
    int i;

    seq = asdl_seq_new(NCH(n) / 2);
    for (i = 0; i < NCH(n); i += 2) {
	asdl_seq_append(seq, ast_for_expr(CHILD(n, i)));
    }
    return seq;
}

static arguments_ty
ast_for_arguments(node *n)
{

    /* XXX TO DO
       check for invalid argument lists like normal after default
       handle nested tuple arguments
    */

    /* parameters: '(' [varargslist] ')'
       varargslist: (fpdef ['=' test] ',')* ('*' NAME [',' '**' NAME] 
            | '**' NAME) | fpdef ['=' test] (',' fpdef ['=' test])* [',']
    */
    int i, n_args = 0, n_defaults = 0;
    asdl_seq *args, *defaults;
    identifier vararg = NULL, kwarg = NULL;
    node *ch;

    if (TYPE(n) == parameters) {
	if (NCH(n) == 2)
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
    args = n_args ? asdl_seq_new(n_args) : NULL;
    defaults = n_defaults? asdl_seq_new(n_defaults) : NULL;

    /* fpdef: NAME | '(' fplist ')'
       fplist: fpdef (',' fpdef)* [',']
    */
    i = 0;
    while (i < NCH(n)) {
	ch = CHILD(n, i);
	switch (TYPE(ch)) {
	case fpdef:
	    if (NCH(ch) == 3) {
		/* XXX don't handle fplist yet */
		return NULL;
	    }
	    if (TYPE(CHILD(ch, 0)) == NAME)
		asdl_seq_append(args, NEW_IDENTIFIER(CHILD(ch, 0)));
	    if (i + 1 < NCH(n) && TYPE(CHILD(n, i + 1)) == EQUAL) {
		asdl_seq_append(defaults, ast_for_expr(CHILD(n, i + 2)));
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
	    fprintf(stderr, "unexpected node in varargslist: %d @ %d\n",
		    TYPE(ch), i);
	}
    }

    return arguments(args, vararg, kwarg, defaults);
}

static stmt_ty
ast_for_funcdef(node *n)
{
    /* funcdef: 'def' NAME parameters ':' suite */
    identifier name = NEW_IDENTIFIER(CHILD(n, 1));
    REQ(n, funcdef);
    return FunctionDef(name, ast_for_arguments(CHILD(n, 2)),
		       ast_for_suite(CHILD(n, 4)), LINENO(n));
}

static expr_ty
ast_for_lambdef(node *n)
{
    /* lambdef: 'lambda' [varargslist] ':' test */
    if (NCH(n) == 3)
	return Lambda(arguments(NULL, NULL, NULL, NULL), 
		      ast_for_expr(CHILD(n, 2)));
    else
	return Lambda(ast_for_arguments(CHILD(n, 1)),
		      ast_for_expr(CHILD(n, 3)));
}

static int
count_list_fors(node *n)
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
	} else
	    return n_fors;
    }
    assert(0); /* can't get here */
    return -1;
}

static int
count_list_ifs(node *n)
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
ast_for_listcomp(node *n)
{
    /* listmaker: test ( list_for | (',' test)* [','] ) 
       list_for: 'for' exprlist 'in' testlist_safe [list_iter]
       list_iter: list_for | list_if
       list_if: 'if' test [list_iter]
       testlist_safe: test [(',' test)+ [',']]
    */
    expr_ty target;
    asdl_seq *listcomps;
    int i, n_fors;
    node *ch;

    REQ(n, listmaker);
    assert(NCH(n) > 1);
    
    target = ast_for_expr(CHILD(n, 0));
    set_context(target, Store);

    n_fors = count_list_fors(n);
    listcomps = asdl_seq_new(n_fors);
    ch = CHILD(n, 1);
    for (i = 0; i < n_fors; i++) {
	listcomp_ty c;
	asdl_seq *t;
	REQ(ch, list_for);
	t = ast_for_exprlist(CHILD(ch, 1), Store);
	if (asdl_seq_LEN(t) == 1)
	    c = listcomp(asdl_seq_get(t, 0), 
			 ast_for_testlist(CHILD(ch, 3)), NULL);
	else
	    c = listcomp(Tuple(t, Store),
			 ast_for_testlist(CHILD(ch, 3)), NULL);
	if (NCH(ch) == 5) {
	    int j, n_ifs;
	    asdl_seq *ifs;
	    ch = CHILD(ch, 4);
	    n_ifs = count_list_ifs(ch);
	    ifs = asdl_seq_new(n_ifs);
	    for (j = 0; j < n_ifs; j++) {
		REQ(ch, list_iter);
		ch = CHILD(ch, 0);
		REQ(ch, list_if);
		asdl_seq_append(ifs, CHILD(ch, 1));
		if (NCH(ch) == 3)
		    ch = CHILD(ch, 2);
	    }
	    /* on exit, must guarantee that ch is a list_for */
	    if (TYPE(ch) == list_iter)
		ch = CHILD(ch, 0);
	}
	asdl_seq_append(listcomps, c);
    }

    return ListComp(target, listcomps);
}

static expr_ty
ast_for_atom(node *n)
{
    /* atom: '(' [testlist] ')' | '[' [listmaker] ']' 
           | '{' [dictmaker] '}' | '`' testlist '`' | NAME | NUMBER | STRING+ 
    */
    node *ch = CHILD(n, 0);
    fprintf(stderr, "ast_for_atom((%d, %d))\n", TYPE(ch), NCH(ch));
    switch (TYPE(ch)) {
    case NAME:
	/* by default, mark all names as Load context.
	   change later if needed.
	*/
	return Name(NEW_IDENTIFIER(ch), Load);
	break;
    case STRING:
	return Str(PyString_FromString(STR(ch)));
	break;
    case NUMBER:
	return Num(PyString_FromString(STR(ch)));
	break;
	/* XXX other cases... */
    case LPAR: /* tuple */
	return Tuple(seq_for_testlist(CHILD(n, 1)), Load);
	break;
    case LSQB: /* list (or list comprehension) */
	ch = CHILD(n, 1);
	REQ(ch, listmaker);
	if (NCH(ch) == 1 || TYPE(CHILD(ch, 1)) == COMMA)
	    return List(ast_for_testlist(ch), Load);
	else
	    return ast_for_listcomp(ch);
	break;
    case LBRACE: {
        /* dictmaker: test ':' test (',' test ':' test)* [','] */
	int i, size;
	asdl_seq *keys, *values;
	ch = CHILD(n, 1);
	size = (NCH(ch) + 1) / 4; /* plus one in case no trailing comma */
	keys = asdl_seq_new(size);
	values = asdl_seq_new(size);
	for (i = 0; i < NCH(ch); i += 4) {
	    asdl_seq_append(keys, ast_for_expr(CHILD(ch, i)));
	    asdl_seq_append(values, ast_for_expr(CHILD(ch, i + 2)));
	}
	return Dict(keys, values);
	break;
    }
    case BACKQUOTE: /* repr */
	return Repr(ast_for_expr(CHILD(n, 1)));
	break;
    default:
	fprintf(stderr, "unhandled atom %d\n", TYPE(ch));
    }
    return NULL;
}

static slice_ty
ast_for_slice(node *n)
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
    if (NCH(n) == 1 && TYPE(ch) == test)
	return Index(ast_for_expr(ch));
    
    if (TYPE(ch) == test)
	lower = ast_for_expr(ch);

    /* If there's an upper bound it's in the second or third position. */
    if (TYPE(ch) == COLON) {
	if (NCH(n) > 1) {
	    node *n2 = CHILD(n, 1);
	    if (TYPE(n2) == test)
		upper = ast_for_expr(n2);
	}
    } else if (NCH(n) > 2) {
	node *n2 = CHILD(n, 2);
	if (TYPE(n2) == test)
	    upper = ast_for_expr(n2);
    }

    ch = CHILD(n, NCH(n) - 1);
    if (TYPE(ch) == sliceop) {
	if (NCH(ch) == 1)
	    ch = CHILD(ch, 0);
	else
	    ch = CHILD(ch, 1);
	if (TYPE(ch) == test)
	    step = ast_for_expr(ch);
    }
    
    return Slice(lower, upper, step);
}

static expr_ty
ast_for_subscript(node *n)
{
    node *t, *s;
    int nch;

    /* power: atom trailer* ('**' factor)* 
       trailer: '(' [arglist] ')' | '[' subscriptlist ']' | '.' NAME 
       subscriptlist: subscript (',' subscript)* [',']
    */

    REQ(n, power);
    REQ(CHILD(n, 0), atom);
    t = CHILD(n, 1);
    REQ(t, trailer);

    REQ(CHILD(t, 0), LSQB);
    s = CHILD(t, 1);
    REQ(CHILD(t, 2), RSQB);

    nch = NCH(s);
    if (nch <= 2) {
	return Subscript(ast_for_atom(CHILD(n, 0)), 
			 ast_for_slice(CHILD(s, 0)), Load);
    } else {
	int i;
	asdl_seq *slices = asdl_seq_new(nch / 2);
	for (i = 0; i < nch ; i += 2)
	    asdl_seq_append(slices, ast_for_slice(CHILD(s, i)));
	return Subscript(ast_for_atom(CHILD(n, 0)),
			 ExtSlice(slices), Load);
    }

    return NULL;
}

static expr_ty
ast_for_expr(node *n)
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

    fprintf(stderr, "ast_for_expr(%d, %d)\n", TYPE(n), NCH(n));
 loop:
    switch (TYPE(n)) {
    case test:
	if (TYPE(CHILD(n, 0)) == lambdef)
	    return ast_for_lambdef(CHILD(n, 0));
	if (NCH(n) == 1) {
	    n = CHILD(n, 0);
	    goto loop;
	}
	seq = asdl_seq_new(NCH(n) / 2 + 1);
	for (i = 0; i < NCH(n); i += 2)
	    asdl_seq_append(seq, ast_for_expr(CHILD(n, i)));
	return BoolOp(Or, seq);
	break;
    case and_test:
	if (NCH(n) == 1) {
	    n = CHILD(n, 0);
	    goto loop;
	}
	seq = asdl_seq_new(NCH(n) / 2 + 1);
	for (i = 0; i < NCH(n); i += 2)
	    asdl_seq_append(seq, ast_for_expr(CHILD(n, i)));
	return BoolOp(And, seq);
	break;
    case not_test:
	if (NCH(n) == 1) {
	    n = CHILD(n, 0);
	    goto loop;
	}
	else
	    return UnaryOp(Not, ast_for_expr(CHILD(n, 1)));
    case comparison:
	if (NCH(n) == 1) {
	    n = CHILD(n, 0);
	    goto loop;
	} else {
	    asdl_seq *ops, *cmps;
	    ops = asdl_seq_new(NCH(n) / 2 + 1);
	    cmps = asdl_seq_new(NCH(n) / 2 + 1);
	    for (i = 1; i < NCH(n); i += 2) {
		asdl_seq_append(ops, (void *)ast_for_comp_op(CHILD(n, i)));
		asdl_seq_append(cmps, (void *)ast_for_expr(CHILD(n, i + 1)));
	    }
	    return Compare(ast_for_expr(CHILD(n, 0)), ops, cmps);
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
	return BinOp(ast_for_expr(CHILD(n, 0)), get_operator(CHILD(n, 1)),
		     ast_for_expr(CHILD(n, 2)));
	break;
    case factor:
	if (NCH(n) == 1) {
	    n = CHILD(n, 0);
	    goto loop;
	}
	switch (TYPE(CHILD(n, 0))) {
	case PLUS:
	    return UnaryOp(UAdd, ast_for_expr(CHILD(n, 1)));
	    break;
	case MINUS:
	    return UnaryOp(USub, ast_for_expr(CHILD(n, 1)));
	    break;
	case TILDE:
	    return UnaryOp(Invert, ast_for_expr(CHILD(n, 1)));
	    break;
	}
	break;
    case power:
	if (NCH(n) == 1)
	    return ast_for_atom(CHILD(n, 0));
	/* power: atom trailer* ('**' factor)* 
           trailer: '(' [arglist] ')' | '[' subscriptlist ']' | '.' NAME */
	if (NCH(n) == 2 && TYPE(CHILD(n, 1)) == trailer) {
	    node *ch = CHILD(n, 1);
	    if (TYPE(CHILD(ch, 0)) == LPAR) {
		/* XXX a call */
	    } else if (TYPE(CHILD(ch, 0)) == LSQB) {
		return ast_for_subscript(n);
	    } else if (TYPE(CHILD(ch, 0)) == DOT) {
		return Attribute(ast_for_atom(CHILD(n, 0)),
				 NEW_IDENTIFIER(CHILD(ch, 1)), Load);
	    }
	}
	return NULL;
	break;
    }
    /* should never get here */
    return NULL;
}

static expr_ty
ast_for_testlist(node *n)
{
    /* could be a testlist or a listmaker with no list_for */
    if (NCH(n) == 1)
	return ast_for_expr(CHILD(n, 0));
    else 
	return Tuple(seq_for_testlist(n), Load);
}

static stmt_ty
ast_for_expr_stmt(node *n)
{
    REQ(n, expr_stmt);
    /* expr_stmt: testlist (augassign testlist | ('=' testlist)*) 
       testlist: test (',' test)* [',']
       augassign: '+=' | '-=' | '*=' | '/=' | '%=' | '&=' | '|=' | '^=' 
	        | '<<=' | '>>=' | '**=' | '//='
       test: ... here starts the operator precendence dance 
     */

    fprintf(stderr, "ast_for_expr_stmt(%d, %d)\n", TYPE(n), NCH(n));
    if (NCH(n) == 1) {
	return Expr(ast_for_testlist(CHILD(n, 0)), LINENO(n));
    } else if (TYPE(CHILD(n, 1)) == augassign) {
	return AugAssign(ast_for_testlist(CHILD(n, 0)),
			 ast_for_augassign(CHILD(n, 1)),
			 ast_for_testlist(CHILD(n, 2)),
			 LINENO(n));
    } else {
	int i;
	asdl_seq *targets;

	/* a normal assignment */
	REQ(CHILD(n, 1), EQUAL);
	targets = asdl_seq_new(NCH(n) / 2 + 1);
	for (i = 0; i < NCH(n) - 2; i += 2) {
	    expr_ty e = ast_for_testlist(CHILD(n, i));
	    /* set context to assign */
	    set_context(e, Store);
	    asdl_seq_append(targets, e);
	}
	return Assign(targets, ast_for_testlist(CHILD(n, NCH(n) - 1)),
		      LINENO(n));
    }
    return NULL;
}

static stmt_ty
ast_for_print_stmt(node *n)
{
    /* print_stmt: 'print' ( [ test (',' test)* [','] ] 
                             | '>>' test [ (',' test)+ [','] ] ) 
     */
    expr_ty dest = NULL;
    asdl_seq *seq;
    bool nl;
    int i, start = 1;

    REQ(n, print_stmt);
    if (TYPE(CHILD(n, 1)) == RIGHTSHIFT) {
	dest = ast_for_expr(CHILD(n, 2));
	start = 4;
    }
    seq = asdl_seq_new((NCH(n) + 1 - start) / 2);
    for (i = start; i < NCH(n); i += 2) {
	asdl_seq_append(seq, ast_for_expr(CHILD(n, i)));
    }
    nl = (TYPE(CHILD(n, NCH(n) - 1)) == COMMA) ? false : true;
    return Print(dest, seq, nl, LINENO(n));
}

static asdl_seq *
ast_for_exprlist(node *n, int context)
{
    asdl_seq *seq;
    int i;
    expr_ty e;

    fprintf(stderr, "ast_for_exprlist(%d, %d)\n", TYPE(n), context);
    REQ(n, exprlist);

    seq = asdl_seq_new((NCH(n) + 1) / 2);
    for (i = 0; i < NCH(n); i += 2) {
	e = ast_for_expr(CHILD(n, i));
	if (context)
	    set_context(e, context);
	asdl_seq_append(seq, e);
    }
    return seq;
}

static stmt_ty
ast_for_del_stmt(node *n)
{
    /* del_stmt: 'del' exprlist */
    REQ(n, del_stmt);
    return Delete(ast_for_exprlist(CHILD(n, 1), Del), LINENO(n));
}

static stmt_ty
ast_for_flow_stmt(node *n)
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
    case yield_stmt:
	return Yield(ast_for_testlist(CHILD(ch, 1)), LINENO(n));
    case return_stmt:
	if (NCH(ch) == 1)
	    return Return(NULL, LINENO(n));
	else
	    return Return(ast_for_testlist(CHILD(ch, 1)), LINENO(n));
    case raise_stmt:
	if (NCH(ch) == 1)
	    return Raise(NULL, NULL, NULL, LINENO(n));
	else if (NCH(ch) == 2)
	    return Raise(ast_for_expr(CHILD(n, 1)), NULL, NULL, LINENO(n));
	else if (NCH(ch) == 4)
	    return Raise(ast_for_expr(CHILD(n, 1)), 
			 ast_for_expr(CHILD(n, 3)), 
			 NULL, LINENO(n));
	else if (NCH(ch) == 6)
	    return Raise(ast_for_expr(CHILD(n, 1)), 
			 ast_for_expr(CHILD(n, 3)), 
			 ast_for_expr(CHILD(n, 5)), LINENO(n));
    default:
	fprintf(stderr, "unexpected flow_stmt: %d\n", TYPE(ch));
	return NULL;
    }
}

static alias_ty
alias_for_import_name(node *n)
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
	} else {
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
	    s = PyString_AS_STRING(str);
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
	return NULL;
    }
    return NULL;
}

static stmt_ty
ast_for_import_stmt(node *n)
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
	for (i = 1; i < NCH(n); i += 2)
	    asdl_seq_append(aliases, alias_for_import_name(CHILD(n, i)));
	return Import(aliases, LINENO(n));
    } else if (STR(CHILD(n, 0))[0] == 'f') { /* from */
	alias_ty mod = alias_for_import_name(CHILD(n, 1));
	aliases = asdl_seq_new((NCH(n) - 2) / 2);
	for (i = 3; i <= NCH(n); i += 2)
	    asdl_seq_append(aliases, alias_for_import_name(CHILD(n, i)));
	return ImportFrom(mod->name, aliases, LINENO(n));
    }
    return NULL;
}

static stmt_ty
ast_for_global_stmt(node *n)
{
    /* global_stmt: 'global' NAME (',' NAME)* */
    identifier name;
    asdl_seq *s;
    int i;

    REQ(n, global_stmt);
    s = asdl_seq_new(NCH(n) / 2);
    for (i = 1; i < NCH(n); i += 2) {
	name = NEW_IDENTIFIER(CHILD(n, i));
	if (!name)
	    return NULL;
	if (asdl_seq_append(s, name) < 0)
	    return NULL;
    }
    return Global(s, LINENO(n));
}

static stmt_ty
ast_for_exec_stmt(node *n)
{
    /* exec_stmt: 'exec' expr ['in' test [',' test]] */
    REQ(n, exec_stmt);
    if (NCH(n) == 2)
	return Exec(ast_for_expr(CHILD(n, 1)), NULL, NULL, LINENO(n));
    else if (NCH(n) == 4)
	return Exec(ast_for_expr(CHILD(n, 1)), 
		    ast_for_expr(CHILD(n, 3)), NULL, LINENO(n));
    else if (NCH(n) == 6)
	return Exec(ast_for_expr(CHILD(n, 1)), 
		    ast_for_expr(CHILD(n, 3)),
		    ast_for_expr(CHILD(n, 5)), LINENO(n));
    return NULL;
}

static stmt_ty
ast_for_assert_stmt(node *n)
{
    /* assert_stmt: 'assert' test [',' test] */
    REQ(n, assert_stmt);
    if (NCH(n) == 2)
	return Assert(ast_for_expr(CHILD(n, 1)), NULL, LINENO(n));
    else if (NCH(n) == 4)
	return Assert(ast_for_expr(CHILD(n, 1)),
		      ast_for_expr(CHILD(n, 3)), LINENO(n));
    return NULL;
}

static asdl_seq *
ast_for_suite(node *n)
{
    /* suite: simple_stmt | NEWLINE INDENT stmt+ DEDENT */
    asdl_seq *seq;
    int i;
    fprintf(stderr, "ast_for_suite(%d)\n", TYPE(n));
    REQ(n, suite);

    fprintf(stderr, "ast_for_suite(CHILD(n, 0)==%d)\n", TYPE(CHILD(n, 0)));

    if (NCH(n) == 1) {
	/* XXX punt on stmt; stmt */
	REQ(CHILD(n, 0), simple_stmt);
	seq = asdl_seq_new(1);
	asdl_seq_append(seq, ast_for_stmt(CHILD(n, 0)));
	return seq;
    }
    fprintf(stderr, "\t%d\n", NCH(n));
    /* one child node for each stmt, plus 3 children for NL, {, & } */
    seq = asdl_seq_new(NCH(n) - 3);
    for (i = 2; i < NCH(n) - 1; i++) {
	fprintf(stderr, "\t%d: %d %d\n", i, TYPE(CHILD(n, i)),
		NCH(CHILD(n, i)));
	REQ(CHILD(n, i), stmt);
	asdl_seq_append(seq, ast_for_stmt(CHILD(n, i)));
    }
    return seq;
}

static stmt_ty
ast_for_if_stmt(node *n)
{
    /* if_stmt: 'if' test ':' suite ('elif' test ':' suite)* 
       ['else' ':' suite]
    */
    char *s;

    REQ(n, if_stmt);

    if (NCH(n) == 4)
	return If(ast_for_expr(CHILD(n, 1)),
		  ast_for_suite(CHILD(n, 3)), NULL, LINENO(n));
    s = STR(CHILD(n, 4));
    /* s[2], the third character in the string, will be 
       's' for el_s_e, or
       'i' for el_i_f
    */
    if (s[2] == 's') 
	return If(ast_for_expr(CHILD(n, 1)),
		  ast_for_suite(CHILD(n, 3)),
		  ast_for_suite(CHILD(n, 6)), LINENO(n));
    else {
	int i, n_elif, has_else = 0;
	asdl_seq *orelse = NULL;
	n_elif = NCH(n) - 4;
	if (TYPE(CHILD(n, n_elif)) == NAME 
	    && STR(CHILD(n, n_elif))[2] == 's') {
	    has_else = 1;
	    n_elif -= 3;
	}
	n_elif /= 4;

	if (has_else) {
	    orelse = asdl_seq_new(1);
	    asdl_seq_append(orelse,
			    If(ast_for_expr(CHILD(n, NCH(n) - 6)),
			       ast_for_suite(CHILD(n, NCH(n) - 4)),
			       ast_for_suite(CHILD(n, NCH(n) - 1)), 
			       LINENO(n)));
	    /* the just-created orelse handled the last elif */
	    n_elif--;
	} else
	    orelse  = NULL;
			
	for (i = 0; i < n_elif; i++) {
	    int off = 5 + (n_elif - i - 1) * 4;
	    orelse = asdl_seq_new(1);
	    asdl_seq_append(orelse,
			    If(ast_for_expr(CHILD(n, off)),
			       ast_for_suite(CHILD(n, off + 2)),
			       orelse, LINENO(n)));
	}
	return If(ast_for_expr(CHILD(n, 1)),
		  ast_for_suite(CHILD(n, 3)),
		  orelse, LINENO(n));
    }
    
    return NULL;
}

static stmt_ty
ast_for_while_stmt(node *n)
{
    /* while_stmt: 'while' test ':' suite ['else' ':' suite] */
    REQ(n, while_stmt);

    if (NCH(n) == 4)
	return While(ast_for_expr(CHILD(n, 1)),
		     ast_for_suite(CHILD(n, 3)), NULL, LINENO(n));
    else
	return While(ast_for_expr(CHILD(n, 1)),
		     ast_for_suite(CHILD(n, 3)), 
		     ast_for_suite(CHILD(n, 6)), LINENO(n));

    return NULL;
}

static stmt_ty
ast_for_for_stmt(node *n)
{
    asdl_seq *_target = NULL, *seq = NULL;
    expr_ty target;
    /* for_stmt: 'for' exprlist 'in' testlist ':' suite ['else' ':' suite] */
    REQ(n, for_stmt);

    if (NCH(n) == 9)
	seq = ast_for_suite(CHILD(n, 8));

    _target = ast_for_exprlist(CHILD(n, 1), 0);
    if (asdl_seq_LEN(_target) == 1) {
	target = asdl_seq_get(_target, 0);
	asdl_seq_free(_target);
    }
    else
	target = Tuple(_target, Store);

    return For(target,
	       ast_for_testlist(CHILD(n, 3)),
	       ast_for_suite(CHILD(n, 5)),
	       seq, LINENO(n));
    return NULL;
}

static excepthandler_ty
ast_for_except_clause(node *exc, node *body)
{
    /* except_clause: 'except' [test [',' test]] */
    REQ(exc, except_clause);
    REQ(body, suite);

    if (NCH(exc) == 1)
	return excepthandler(NULL, NULL, ast_for_suite(body));
    else if (NCH(exc) == 2)
	return excepthandler(ast_for_expr(CHILD(exc, 1)), NULL, 
			     ast_for_suite(body));
    else {
	expr_ty e = ast_for_expr(CHILD(exc, 3));
	set_context(e, Store);
	return excepthandler(ast_for_expr(CHILD(exc, 1)), e,
		      ast_for_suite(body));
    }
}

static stmt_ty
ast_for_try_stmt(node *n)
{
    REQ(n, try_stmt);
    if (TYPE(CHILD(n, 3)) == NAME) {/* must be 'finally' */
	/* try_stmt: 'try' ':' suite 'finally' ':' suite) */
	return TryFinally(ast_for_suite(CHILD(n, 2)),
			  ast_for_suite(CHILD(n, 5)), LINENO(n));
    } else {
	/* try_stmt: ('try' ':' suite (except_clause ':' suite)+ 
           ['else' ':' suite] 
	*/
	asdl_seq *handlers;
	int i, has_else = 0, n_except = NCH(n) - 3;
	if (TYPE(CHILD(n, NCH(n) - 3)) == NAME) {
	    has_else = 1;
	    n_except -= 3;
	}
	n_except /= 3;
	handlers = asdl_seq_new(n_except);
	for (i = 0; i < n_except; i++)
	    asdl_seq_append(handlers,
			    ast_for_except_clause(CHILD(n, 3 + i * 3),
						  CHILD(n, 5 + i * 3)));
	return TryExcept(ast_for_suite(CHILD(n, 2)), handlers,
			 has_else ? ast_for_suite(CHILD(n, NCH(n) - 1)): NULL,
			 LINENO(n));
    }
    return NULL;
}

static stmt_ty
ast_for_classdef(node *n)
{
    /* classdef: 'class' NAME ['(' testlist ')'] ':' suite */
    expr_ty _bases;
    asdl_seq *bases;
    REQ(n, classdef);
    if (NCH(n) == 4) 
	return ClassDef(NEW_IDENTIFIER(CHILD(n, 1)), NULL,
			ast_for_suite(CHILD(n, 3)), LINENO(n));
    /* else handle the base class list */
    _bases = ast_for_testlist(CHILD(n, 3));
    if (_bases->kind == Tuple_kind)
	bases = _bases->v.Tuple.elts;
    else {
	bases = asdl_seq_new(1);
	asdl_seq_append(bases, _bases);
    }
    return ClassDef(NEW_IDENTIFIER(CHILD(n, 1)), bases,
		    ast_for_suite(CHILD(n, 6)), LINENO(n));
    return NULL;
}

static stmt_ty
ast_for_stmt(node *n)
{
    if (TYPE(n) == stmt) {
	assert(NCH(n) == 1);
	n = CHILD(n, 0);
    }
    fprintf(stderr, "stmt lineno %d\n", n->n_lineno);
    if (TYPE(n) == simple_stmt) {
	/* I'm explicitly punting on multiple statements joined by a
	   semicolon.  When I do handle it, ast_for_stmt() will have
	   to return a sequence of statements.  It may actually be
	   easier to put the check for a several small statements
	   joined by a semicolon in the caller.  
	*/
	assert(NCH(n) == 2);
	n = CHILD(n, 0);
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
	    fprintf(stderr, "TYPE=%d NCH=%d\n", TYPE(n), NCH(n));
	}
    } else {
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
	    return NULL;
	}
    }
    return NULL;
}

