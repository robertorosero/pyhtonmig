#include "Python.h"
#include "Python-ast.h"
#include "pyarena.h"
#include "pyerrors.h"
#include "symtable.h"
#include "node.h"
#include "ast.h"

typedef struct _optimizer_block {
    struct _optimizer_block* b_next; /* next block on the stack */
    PySTEntryObject*         b_ste;  /* symtable entry */

    /* we don't want to optimize away "try" blocks containing a
     * "continue" statement in its finally clause. This is illegal syntax,
     * and we need to make sure it falls through to the compiler where it
     * will be detected. The following fields are used to that end.
     */
    unsigned int             b_finally; /* non-zero if we're in a "finally" */
    unsigned int             b_eliminate; /* non-zero if elimination is OK */
} optimizer_block;

typedef struct _optimizer {
    struct symtable* opt_symtable;
    PyArena*         opt_arena;
    optimizer_block* opt_current; /* the current block */
} optimizer;

static int optimize_expr(optimizer* opt, expr_ty* expr_ptr);
static int optimize_stmt(optimizer* opt, stmt_ty* stmt_ptr);
static int optimize_comprehension(optimizer* opt, comprehension_ty* comp_ptr);
static int optimize_excepthandler(optimizer* opt, excepthandler_ty* exc_ptr);
static int optimize_keyword(optimizer* opt, keyword_ty* kwd_ptr);
static int optimize_arguments(optimizer* opt, arguments_ty* args_ptr);
static int optimize_slice(optimizer* opt, slice_ty* slice_ptr);

/**
 * Enter a block/namespace.
 */
static int
_enter_block(optimizer* opt, PySTEntryObject* ste)
{
    optimizer_block* block = malloc(sizeof *block);
    if (block == NULL)
        return 0;
    block->b_next      = opt->opt_current;
    block->b_finally   = 0;
    block->b_eliminate = 1;
    block->b_ste       = ste;

    Py_INCREF(ste);

    opt->opt_current = block;

    return 1;
}

/**
 * Leave a block/namespace.
 */
static void
_leave_block(optimizer* opt)
{
    optimizer_block* next;
    assert(opt->opt_current != NULL);
    next = opt->opt_current->b_next;

    Py_DECREF(opt->opt_current->b_ste);
    free(opt->opt_current);

    opt->opt_current = next;
}

/**
 * Determine the constant value of a given expression. It's assumed that
 * constants have been folded.
 */
static PyObject*
_expr_constant_value(expr_ty expr)
{
    if (expr->kind == Str_kind) {
        return expr->v.Str.s;
    }
    else if (expr->kind == Num_kind) {
        return expr->v.Num.n;
    }
    else if (expr->kind == Const_kind) {
        return expr->v.Const.value;
    }
    return NULL;
}

/**
 * Construct an expr_ty instance from the given constant PyObject value.
 */
static expr_ty
_expr_from_object(PyObject* object, int lineno, int col_offset, PyArena* arena)
{
    expr_ty expr = NULL;

    if (PyString_Check(object) || PyUnicode_Check(object)) {
        Py_INCREF(object);
        expr = Str(object, lineno, col_offset, arena);
    }
    else if (PyNumber_Check(object)) {
        Py_INCREF(object);
        expr = Num(object, lineno, col_offset, arena);
    }
    else if (object == Py_None ||
                object == Py_True ||
                object == Py_False ||
                PyTuple_Check(object)) {
        Py_INCREF(object);
        expr = Const(object, lineno, col_offset, arena);
    }
    else {
        PyErr_Format(PyExc_TypeError, "unknown constant value");
        return NULL;
    }

    if (expr == NULL)
        return NULL;

    if (PyArena_AddPyObject(arena, object) == -1) {
        /* exception will be set in PyArena_AddPyObject */
        Py_DECREF(object);
        return NULL;
    }

    /* PyArena_AddPyObject decrements the refcount for us */
    return expr;
}

static int
_is_sequence_of_constants(asdl_seq* seq)
{
    int i;
    int length = asdl_seq_LEN(seq);
    for (i = 0; i < length; i++) {
        PyObject* value;
        expr_ty expr;
        
        expr = (expr_ty)asdl_seq_GET(seq, i);
        value = _expr_constant_value(expr);
        if (value == NULL)
            return 0;
    }
    return 1;
}

/**
 * Build a tuple of constants from an expression sequence.
 * A precondition is that the given seq returns a true
 * value for _is_sequence_of_constants.
 */
static PyObject*
_build_tuple_of_constants(asdl_seq* seq, PyArena* arena)
{
    PyObject* result;
    int i;
    int length = asdl_seq_LEN(seq);

    result = PyTuple_New(length);
    if (result == NULL)
        return NULL;

    if (PyArena_AddPyObject(arena, result) == -1) {
        Py_DECREF(result);
        return NULL;
    }

    for (i = 0; i < length; i++) {
        expr_ty expr = asdl_seq_GET(seq, i);
        PyObject* value = _expr_constant_value(expr);
        Py_INCREF(value);
        PyTuple_SET_ITEM(result, i, value);
    }

    return result;
}

/**
 * Optimize a sequence of expressions.
 */
static int
optimize_expr_seq(optimizer* opt, asdl_seq** seq_ptr)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++)
        if (!optimize_expr(opt, (expr_ty*)&asdl_seq_GET(seq, n)))
            return 0;
    return 1;
}

/**
 * Replace an AST node at position `n' with the node(s) in `replacement'.
 */
static asdl_seq*
_asdl_seq_replace(asdl_seq* seq, int n, asdl_seq* replacement, PyArena* arena)
{
    asdl_seq* new;
    int newlen, replen, i;

    assert(replacement != NULL);
    /* at the very least, we should have a single node */
    assert(asdl_seq_LEN(replacement) > 0);

    newlen = asdl_seq_LEN(seq) - 1;
    replen = asdl_seq_LEN(replacement);
    newlen += replen;

    assert(newlen > 0);

    new = asdl_seq_new(newlen, arena);
    if (new == NULL)
        return NULL;

    /* copy everything before position `n` into the new seq */
    for (i = 0; i < n; i++)
        asdl_seq_SET(new, i, asdl_seq_GET(seq, i));

    /* if we have a replacement, append it to the new seq */
    if (replacement != NULL)
        for (i = n; i < n + replen; i++)
            asdl_seq_SET(new, i, asdl_seq_GET(replacement, i - n));

    /* append everything after position `n` to the new seq */
    for (i = n + replen; i < newlen; i++)
        asdl_seq_SET(new, i, asdl_seq_GET(seq, (i - replen) + 1));

    return new;
}

/**
 * Replaces the AST node at `n' with a Pass() node.
 */
static asdl_seq*
_asdl_seq_replace_with_pass(asdl_seq* seq, int n, int lineno, int col_offset, PyArena* arena)
{
    stmt_ty pass = Pass(lineno, col_offset, arena);
    if (pass == NULL)
        return NULL;
    asdl_seq_SET(seq, n, pass);
    return seq;
}

#define LAST_IN_SEQ(seq) (asdl_seq_LEN((seq)) - 1)

/**
 * Eliminate code that we can determine will never be executed.
 */
static int
_eliminate_unreachable_code(optimizer* opt, asdl_seq** seq_ptr, int n)
{
    asdl_seq* seq = *seq_ptr;
    stmt_ty stmt = asdl_seq_GET(seq, n);

    /* eliminate unreachable branches in an "if" statement */
    if (stmt->kind == If_kind) {
        PyObject* test = _expr_constant_value(stmt->v.If.test);
        if (test != NULL) {
            if (PyObject_IsTrue(test)) {
                seq = _asdl_seq_replace(seq, n, stmt->v.If.body,
                            opt->opt_arena);
            }
            else {
                if (stmt->v.If.orelse == NULL) {
                    /* no "else:" body: use a Pass() */
                    seq = _asdl_seq_replace_with_pass(seq, n, stmt->lineno,
                            stmt->col_offset, opt->opt_arena);
                }
                else {
                    seq = _asdl_seq_replace(seq, n, stmt->v.If.orelse,
                            opt->opt_arena);
                }
            }
            if (seq == NULL)
                return 0;
            *seq_ptr = seq;
        }
    }
    /* eliminate unreachable while loops? */
    else if (stmt->kind == While_kind) {
        PyObject* test = _expr_constant_value(stmt->v.While.test);
        if (test != NULL) {
            if (!PyObject_IsTrue(test)) {
                if (stmt->v.While.orelse != NULL) {
                    seq = _asdl_seq_replace(seq, n, stmt->v.While.orelse,
                                            opt->opt_arena);
                }
                else {
                    seq = _asdl_seq_replace_with_pass(seq, n, stmt->lineno,
                            stmt->col_offset, opt->opt_arena);
                }
                if (seq == NULL)
                    return 0;
                *seq_ptr = seq;
            }
        }
    }
    /* eliminate unreachable for loops? */
    else if (stmt->kind == For_kind) {
        PyObject* iter = _expr_constant_value(stmt->v.For.iter);
        if (iter != NULL) {
            if (PyObject_Size(iter) == 0) {
                if (stmt->v.For.orelse != NULL) {
                    seq = _asdl_seq_replace(seq, n, stmt->v.For.orelse,
                            opt->opt_arena);
                }
                else {
                    seq = _asdl_seq_replace_with_pass(seq, n, stmt->lineno,
                            stmt->col_offset, opt->opt_arena);
                }
                if (seq == NULL)
                    return 0;
                *seq_ptr = seq;
            }
        }
    }
    /* eliminate all code after a "return" statement */
    else if (stmt->kind == Return_kind && n < LAST_IN_SEQ(seq)) {
        /* eliminate all nodes after a return */
        seq = _asdl_seq_replace_with_pass(seq, n + 1,
                stmt->lineno, stmt->col_offset, opt->opt_arena);
        if (seq == NULL)
            return 0;
        *seq_ptr = seq;
    }

    *seq_ptr = seq;

    return 1;
}

/**
 * Adds all ASDL nodes from seq1 with offset n1 to a new list, then
 * appends the nodes in seq2 from offset n2. The resulting list is
 * returned.
 */
static asdl_seq*
_asdl_seq_append(asdl_seq* seq1, int n1, asdl_seq* seq2, int n2,
                    PyArena* arena)
{
    asdl_seq* new;
    int i;
    int len1 = asdl_seq_LEN(seq1) - n1;
    int len2 = asdl_seq_LEN(seq2) - n2;
    int newlen = len1 + len2;

    new = asdl_seq_new(newlen, arena);
    if (new == NULL)
        return NULL;

    for (i = 0; i < len1; i++)
        asdl_seq_SET(new, i, asdl_seq_GET(seq1, n1 + i));

    for (i = 0; i < len2; i++)
        asdl_seq_SET(new, len1 + i, asdl_seq_GET(seq2, n2 + i));

    return new;
}

/**
 * Appends a Return node to the end of `seq' using the given
 * value.
 */
static asdl_seq*
_asdl_seq_append_return(asdl_seq* seq, expr_ty value, PyArena* arena)
{
    stmt_ty ret;
    stmt_ty last;
    asdl_seq* retseq = asdl_seq_new(1, arena);
    if (retseq == NULL)
        return NULL;
    last = asdl_seq_GET(seq, asdl_seq_LEN(seq)-1);
    ret = Return(value, last->lineno, last->col_offset, arena);
    if (ret == NULL)
        return NULL;
    asdl_seq_SET(retseq, 0, ret);
    return _asdl_seq_append(seq, 0, retseq, 0, arena);
}

#if 0
static void
_expr_incref(expr_ty expr)
{
    if (expr != NULL) {
        if (expr->kind == Num_kind)
            Py_INCREF(expr->v.Num.n);
        else if (expr->kind == Str_kind)
            Py_INCREF(expr->v.Str.s);
        else if (expr->kind == Const_kind)
            Py_INCREF(expr->v.Const.value);
    }
}
#endif

static int
_inject_compound_stmt_return(stmt_ty stmt, stmt_ty next, PyArena* arena)
{
    expr_ty value = NULL;
    if (next != NULL)
        value = next->v.Return.value;

    /* if the else body is not present, there will be no jump anyway */
    if (stmt->kind == If_kind && stmt->v.If.orelse != NULL) {
        stmt_ty inner = asdl_seq_GET(stmt->v.If.body,
                                    LAST_IN_SEQ(stmt->v.If.body));
        
        if (inner->kind != Return_kind) {
            stmt->v.If.body =
                _asdl_seq_append_return(stmt->v.If.body, value, arena);

            if (stmt->v.If.body == NULL)
                return 0;
        }
    }
    /* TODO: we probably want to append a return to all but
     *       the last handler too
     */
    if (stmt->kind == TryExcept_kind) {
        stmt_ty inner = asdl_seq_GET(stmt->v.TryExcept.body,
                            LAST_IN_SEQ(stmt->v.TryExcept.body));
        if (inner->kind != Return_kind && inner->kind != Raise_kind) {
            /* if we inject a return into the "try" body of a
             * "try..except..else", we will miss the "else"!
             * We need to take a different path if the "else" is
             * present.
             */
            if (stmt->v.TryExcept.orelse == NULL) {
                stmt->v.TryExcept.body =
                    _asdl_seq_append_return(stmt->v.TryExcept.body,
                                            value, arena);
                if (stmt->v.TryExcept.body == NULL)
                    return 0;
            }
            else {
                stmt->v.TryExcept.orelse =
                    _asdl_seq_append_return(stmt->v.TryExcept.orelse,
                                            value, arena);
                if (stmt->v.TryExcept.orelse == NULL)
                    return 0;
            }
        }
    }
    else if (stmt->kind == TryFinally_kind) {
        stmt_ty inner = asdl_seq_GET(stmt->v.TryFinally.body,
                            LAST_IN_SEQ(stmt->v.TryFinally.body));
        if (inner->kind != Return_kind && inner->kind != Raise_kind) {
            stmt->v.TryFinally.body =
                _asdl_seq_append_return(stmt->v.TryFinally.body, value, arena);
            if (stmt->v.TryFinally.body == NULL)
                return 0;
        }
    }

    return 1;
}

/**
 * Simplify any branches that converge on a "return" statement such that
 * they immediately return rather than jump.
 */
static int
_simplify_jumps(optimizer* opt, asdl_seq* seq, int top)
{
    int n, len;

    len = asdl_seq_LEN(seq);

    for (n = 0; n < len; n++) {
        stmt_ty stmt = asdl_seq_GET(seq, n);
        stmt_ty next = NULL;
        if (n < len-1)
            next = asdl_seq_GET(seq, n+1);
        /* XXX: handle the implicit return only if top-level function seq */
        if ((top && next == NULL) || (next && next->kind == Return_kind))
            if (!_inject_compound_stmt_return(stmt, next, opt->opt_arena))
                return 0;
    }

    return 1;
}

/**
 * Optimize a sequence of statements.
 */
static int
optimize_stmt_seq(optimizer* opt, asdl_seq** seq_ptr)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++) {
        if (!optimize_stmt(opt, (stmt_ty*)&asdl_seq_GET(seq, n)))
            return 0;
        if (opt->opt_current->b_eliminate)
            if (!_eliminate_unreachable_code(opt, seq_ptr, n))
                return 0;
        if (opt->opt_current->b_ste->ste_type == FunctionBlock)
            if (!_simplify_jumps(opt, *seq_ptr, 0))
                return 0;
    }
    return 1;
}

static int
optimize_comprehension_seq(optimizer* opt, asdl_seq** seq_ptr)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++) {
        comprehension_ty* comp;
        comp = (comprehension_ty*)&asdl_seq_GET(seq, n);
        if (!optimize_comprehension(opt, comp))
            return 0;
    }
    return 1;
}

static int
optimize_excepthandler_seq(optimizer* opt, asdl_seq** seq_ptr)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++) {
        excepthandler_ty* excepthandler;
        excepthandler = (excepthandler_ty*)&asdl_seq_GET(seq, n);
        if (!optimize_excepthandler(opt, excepthandler))
            return 0;
    }
    return 1;
}

static int
optimize_keyword_seq(optimizer* opt, asdl_seq** seq_ptr)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++)
        if (!optimize_keyword(opt, (keyword_ty*)&asdl_seq_GET(seq, n)))
            return 0;
    return 1;
}

static int
optimize_slice_seq(optimizer* opt, asdl_seq** seq_ptr)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++)
        if (!optimize_slice(opt, (slice_ty*)&asdl_seq_GET(seq, n)))
            return 0;
    return 1;
}

static int
optimize_mod(optimizer* opt, mod_ty* mod_ptr)
{
    asdl_seq** body;
    mod_ty mod = *mod_ptr;

    switch (mod->kind) {
        case Module_kind:
            {
                body = &mod->v.Module.body;
                break;
            }
        case Interactive_kind:
            {
                body = &mod->v.Interactive.body;
                break;
            }
        case Suite_kind:
            {
                body = &mod->v.Suite.body;
                break;
            }
        case Expression_kind:
            {
                return optimize_expr(opt, &mod->v.Expression.body);
            }
        default:
            PyErr_Format(PyExc_ValueError, "unknown mod_ty kind: %d",
                    mod->kind);
            return 0;
    };

    return optimize_stmt_seq(opt, body);
}

static int
optimize_bool_op(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr_seq(opt, &expr->v.BoolOp.values))
        return 0;
    return 1;
}

static int
optimize_bin_op(optimizer* opt, expr_ty* expr_ptr)
{
    PyObject* left;
    PyObject* right;
    expr_ty expr = *expr_ptr;

    if (!optimize_expr(opt, &expr->v.BinOp.left))
        return 0;
    if (!optimize_expr(opt, &expr->v.BinOp.right))
        return 0;

    /* 
     * TODO: aggressively rearrange binop grouping so that as many constants
     * as possible are grouped together
     */

    left = _expr_constant_value(expr->v.BinOp.left);
    right = _expr_constant_value(expr->v.BinOp.right);

    /* if the left & right hand side are constant values, we can fold them */
    if (left != NULL && right != NULL) {
        PyObject* res = NULL;
        int size;

        switch (expr->v.BinOp.op) {
            case Add:
                {
                    res = PyNumber_Add(left, right);
                    break;
                }
            case Sub:
                {
                    res = PyNumber_Subtract(left, right);
                    break;
                }
            case Mult:
                {
                    res = PyNumber_Multiply(left, right);
                    break;
                }
            case Div:
                {
                    /* XXX: -Qnew will break this. Fixes test_binop ... */
                    /* TODO: this should be okay in Python-3000 */
#if 0
                    /* avoid divide-by-zero errors */
                    if (PyObject_IsTrue(right))
                        res = PyNumber_TrueDivide(left, right);
#endif
                    break;
                }
            case Mod:
                {
                    /* avoid divide-by-zero errors */
                    if (PyObject_IsTrue(right))
                        res = PyNumber_Remainder(left, right);
                    break;
                }
            case Pow:
                {
                    res = PyNumber_Power(left, right, Py_None);
                    break;
                }
            case LShift:
                {
                    res = PyNumber_Lshift(left, right);
                    break;
                }
            case RShift:
                {
                    res = PyNumber_Rshift(left, right);
                    break;
                }
            case BitOr:
                {
                    res = PyNumber_Or(left, right);
                    break;
                }
            case BitXor:
                {
                    res = PyNumber_Xor(left, right);
                    break;
                }
            case BitAnd:
                {
                    res = PyNumber_And(left, right);
                    break;
                }
            case FloorDiv:
                {
                    /* raise divide-by-zero errors at runtime */
                    if (PyObject_IsTrue(right))
                        res = PyNumber_FloorDivide(left, right);
                    break;
                }
            default:
                PyErr_Format(PyExc_ValueError, "unknown binary operator: %d",
                        expr->v.BinOp.op);
                return 0;
        }
        
        if (res == NULL) {
            if (PyErr_Occurred()) {
                /* complain about out of memory errors right away */
                if (PyErr_ExceptionMatches(PyExc_MemoryError))
                    return 0;
                /* leave all other errors for runtime */
                else
                    PyErr_Clear();
            }
            return 1;
        }

        /* XXX: is this check still necessary? */
        size = PyObject_Size(res);
        if (size == -1) {
            PyErr_Clear();
        }
        else if (size >= 20) {
            Py_DECREF(res);
            return 1;
        }
        
        expr = _expr_from_object(res, expr->lineno, expr->col_offset,
                    opt->opt_arena);
        if (expr == NULL) {
            Py_DECREF(res);
            return 0;
        }
        *expr_ptr = expr;
    }

    return 1;
}

static int
optimize_unary_op(optimizer* opt, expr_ty* expr_ptr)
{
    PyObject* operand;
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(opt, &expr->v.UnaryOp.operand))
        return 0;
    operand = _expr_constant_value(expr->v.UnaryOp.operand);
    if (operand != NULL) {
        PyObject* res;

        switch (expr->v.UnaryOp.op) {
            case Invert:
                {
                    res = PyNumber_Invert(operand);
                    break;
                }
            case Not:
                {
                    res = PyBool_FromLong(PyObject_Not(operand));
                    break;
                }
            case UAdd:
                {
                    res = PyNumber_Positive(operand);
                    break;
                }
            case USub:
                {
                    /* ensure -0.0/+0.0 are not touched */
                    if (PyObject_IsTrue(operand))
                        res = PyNumber_Negative(operand);
                    else
                        return 1;

                    break;
                }
            default:
                PyErr_Format(PyExc_ValueError, "unknown unary op: %d",
                        expr->v.UnaryOp.op);
                return 0;
        }

        if (res == NULL) {
            if (PyErr_Occurred()) {
                /* complain about out of memory errors right away */
                if (PyErr_ExceptionMatches(PyExc_MemoryError))
                    return 0;
                /* leave all other errors for runtime */
                else
                    PyErr_Clear();
            }
            return 1;
        }

        expr = _expr_from_object(res, expr->lineno, expr->col_offset,
                    opt->opt_arena);
        if (!expr) {
            Py_DECREF(res);
            return 0;
        }
        *expr_ptr = expr;
    }
    return 1;
}

static int
optimize_lambda(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(opt, &expr->v.Lambda.body))
        return 0;
    return 1;
}

static int
optimize_if_exp(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(opt, &expr->v.IfExp.test))
        return 0;
    if (!optimize_expr(opt, &expr->v.IfExp.body))
        return 0;
    if (!optimize_expr(opt, &expr->v.IfExp.orelse))
        return 0;
    return 1;
}

static int
optimize_dict(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr_seq(opt, &expr->v.Dict.keys))
        return 0;
    if (!optimize_expr_seq(opt, &expr->v.Dict.values))
        return 0;
    return 1;
}

static int
optimize_comprehension(optimizer* opt, comprehension_ty* comp_ptr)
{
    comprehension_ty comp = *comp_ptr;
    if (!optimize_expr(opt, &comp->target))
        return 0;
    if (!optimize_expr(opt, &comp->iter))
        return 0;
    if (!optimize_expr_seq(opt, &comp->ifs))
        return 0;
    return 1;
}

static int
optimize_list_comp(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(opt, &expr->v.ListComp.elt))
        return 0;
    if (!optimize_comprehension_seq(opt, &expr->v.ListComp.generators))
        return 0;
    return 1;
}

static int
optimize_generator_exp(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(opt, &expr->v.GeneratorExp.elt))
        return 0;
    if (!optimize_comprehension_seq(opt, &expr->v.GeneratorExp.generators))
        return 0;
    return 1;
}

static int
optimize_yield(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (expr->v.Yield.value != NULL) {
        expr_ty value;
        if (!optimize_expr(opt, &expr->v.Yield.value))
            return 0;
        value = expr->v.Yield.value;
        if (value->kind == Const_kind && value->v.Const.value == Py_None)
            expr->v.Yield.value = NULL;
    }
    return 1;
}

static int
optimize_compare(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(opt, &expr->v.Compare.left))
        return 0;
    if (!optimize_expr_seq(opt, &expr->v.Compare.comparators))
        return 0;
    return 1;
}

static int
optimize_keyword(optimizer* opt, keyword_ty* keyword_ptr)
{
    keyword_ty keyword = *keyword_ptr;
    if (!optimize_expr(opt, &keyword->value))
        return 0;
    return 1;
}

static int
optimize_arguments(optimizer* opt, arguments_ty* args_ptr)
{
    arguments_ty args = *args_ptr;
    if (!optimize_expr_seq(opt, &args->args))
        return 0;
    if (!optimize_expr_seq(opt, &args->defaults))
        return 0;
    return 1;
}

static int
optimize_call(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(opt, &expr->v.Call.func))
        return 0;
    if (!optimize_expr_seq(opt, &expr->v.Call.args))
        return 0;
    if (!optimize_keyword_seq(opt, &expr->v.Call.keywords))
        return 0;
    if (expr->v.Call.starargs != NULL)
        if (!optimize_expr(opt, &expr->v.Call.starargs))
            return 0;
    if (expr->v.Call.kwargs != NULL)
        if (!optimize_expr(opt, &expr->v.Call.kwargs))
            return 0;
    return 1;
}

static int
optimize_repr(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(opt, &expr->v.Repr.value))
        return 0;
    return 1;
}

static int
optimize_attribute(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(opt, &expr->v.Attribute.value))
        return 0;
    return 1;
}

static int
optimize_slice(optimizer* opt, slice_ty* slice_ptr)
{
    slice_ty slice = *slice_ptr;
    switch (slice->kind) {
        case Slice_kind:
            {
                if (slice->v.Slice.lower != NULL)
                    if (!optimize_expr(opt, &slice->v.Slice.lower))
                        return 0;
                if (slice->v.Slice.upper != NULL)
                    if (!optimize_expr(opt, &slice->v.Slice.upper))
                        return 0;
                if (slice->v.Slice.step != NULL)
                    if (!optimize_expr(opt, &slice->v.Slice.step))
                        return 0;
                break;
            }
        case ExtSlice_kind:
            {
                if (!optimize_slice_seq(opt, &slice->v.ExtSlice.dims))
                    return 0;
                break;
            }
        case Index_kind:
            {
                if (!optimize_expr(opt, &slice->v.Index.value))
                    return 0;
                break;
            }
        case Ellipsis_kind:
            {
                return 1;
            }
        default:
            PyErr_Format(PyExc_ValueError, "unknown slice kind: %d",
                    slice->kind);
            return 0;
    }
    return 1;
}

static int
optimize_subscript(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(opt, &expr->v.Subscript.value))
        return 0;
    if (!optimize_slice(opt, &expr->v.Subscript.slice))
        return 0;
    return 1;
}

static int
optimize_tuple(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr_seq(opt, &expr->v.Tuple.elts))
        return 0;

    if (_is_sequence_of_constants(expr->v.Tuple.elts)) {
        PyObject* tuple = _build_tuple_of_constants(expr->v.Tuple.elts,
                            opt->opt_arena);
        if (tuple == NULL)
            return 0;
        *expr_ptr = Const(tuple, expr->lineno, expr->col_offset,
                        opt->opt_arena);
        if (*expr_ptr == NULL)
            return 0;
    }

    return 1;
}

static int
optimize_name(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    const char* id = PyString_AS_STRING(expr->v.Name.id);
    PyObject* constvalue = NULL;

    /* allow "assignment to none" error to naturally occur */
    if (expr->v.Name.ctx != Load)
        return 1;

    if (strcmp(id, "None") == 0) {
        Py_INCREF(Py_None);
        constvalue = Py_None;
    }
/* this is not doable in 2.x ... */
#if 0
    else if (strcmp(id, "True") == 0) {
        Py_INCREF(Py_True);
        constvalue = Py_True;
    }
    else if (strcmp(id, "False") == 0) {
        Py_INCREF(Py_False);
        constvalue = Py_False;
    }
#endif

    if (constvalue != NULL) {
        *expr_ptr = Const(constvalue, expr->lineno,
                            expr->col_offset, opt->opt_arena);
        if (*expr_ptr == NULL)
            return 0;
    }

    return 1;
}

static int
optimize_expr(optimizer* opt, expr_ty* expr_ptr)
{
    expr_ty expr = *expr_ptr;
    switch (expr->kind) {
        case BoolOp_kind:
            {
                return optimize_bool_op(opt, expr_ptr);
            }
        case BinOp_kind:
            {
                return optimize_bin_op(opt, expr_ptr);
            }
        case UnaryOp_kind:
            {
                return optimize_unary_op(opt, expr_ptr);
            }
        case Lambda_kind:
            {
                return optimize_lambda(opt, expr_ptr);
            }
        case IfExp_kind:
            {
                return optimize_if_exp(opt, expr_ptr);
            }
        case Dict_kind:
            {
                return optimize_dict(opt, expr_ptr);
            }
        case ListComp_kind:
            {
                return optimize_list_comp(opt, expr_ptr);
            }
        case GeneratorExp_kind:
            {
                return optimize_generator_exp(opt, expr_ptr);
            }
        case Yield_kind:
            {
                return optimize_yield(opt, expr_ptr);
            }
        case Compare_kind:
            {
                return optimize_compare(opt, expr_ptr);
            }
        case Call_kind:
            {
                return optimize_call(opt, expr_ptr);
            }
        case Repr_kind:
            {
                return optimize_repr(opt, expr_ptr);
            }
        case Attribute_kind:
            {
                return optimize_attribute(opt, expr_ptr);
            }
        case Subscript_kind:
            {
                return optimize_subscript(opt, expr_ptr);
            }
        case List_kind:
            {
                return optimize_expr_seq(opt, &expr->v.List.elts);
            }
        case Tuple_kind:
            {
                return optimize_tuple(opt, expr_ptr);
            }
        case Name_kind:
            {
                return optimize_name(opt, expr_ptr);
            }
        case Num_kind:
        case Str_kind:
        case Const_kind:
            {
                return 1;
            }
        default:
            PyErr_Format(PyExc_ValueError, "unknown expr_ty kind: %d",
                    expr->kind);
            return 0;
    }
}

static int
_contains_return(asdl_seq* seq)
{
    int i;
    int len = asdl_seq_LEN(seq);
    for (i = 0; i < len; i++)
        if (((stmt_ty)asdl_seq_GET(seq, i))->kind == Return_kind)
            return 1;
    return 0;
}

static int
optimize_function_def(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    
    /* XXX: this breaks a bunch of tests. For now we use a second pass. */
#if 0
    /* Make any implicit returns explicit */
    if (!_contains_return(stmt->v.FunctionDef.body)) {
        stmt->v.FunctionDef.body =
            _asdl_seq_append_return(stmt->v.FunctionDef.body, NULL,
                                    opt->opt_arena);
        if (stmt->v.FunctionDef.body == NULL)
            return 0;
    }
#endif

    if (!optimize_arguments(opt, &stmt->v.FunctionDef.args))
        return 0;
    if (!optimize_expr_seq(opt, &stmt->v.FunctionDef.decorator_list))
        return 0;
    if (!optimize_stmt_seq(opt, &stmt->v.FunctionDef.body))
        return 0;
    /* XXX: shallow second pass for implicit returns */
    if (!_contains_return(stmt->v.FunctionDef.body))
        if (!_simplify_jumps(opt, stmt->v.FunctionDef.body, 1))
            return 0;
    return 1;
}

static int
optimize_class_def(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr_seq(opt, &stmt->v.ClassDef.bases))
        return 0;
    if (!optimize_expr_seq(opt, &stmt->v.ClassDef.decorator_list))
        return 0;
    if (!optimize_stmt_seq(opt, &stmt->v.ClassDef.body))
        return 0;
    return 1;
}

static int
optimize_return(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    if (stmt->v.Return.value != NULL) {
        expr_ty value;
        if (!optimize_expr(opt, &stmt->v.Return.value))
            return 0;
        value = stmt->v.Return.value;
        if (value->kind == Const_kind && value->v.Const.value == Py_None)
            stmt->v.Return.value = NULL;
    }
    return 1;
}

static int
optimize_delete(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr_seq(opt, &stmt->v.Delete.targets))
        return 0;
    return 1;
}

static int
optimize_assign(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr_seq(opt, &stmt->v.Assign.targets))
        return 0;
    if (!optimize_expr(opt, &stmt->v.Assign.value))
        return 0;
    return 1;
}

static int
optimize_aug_assign(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(opt, &stmt->v.AugAssign.target))
        return 0;
    if (!optimize_expr(opt, &stmt->v.AugAssign.value))
        return 0;
    return 1;
}

static int
optimize_print(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;

    if (stmt->v.Print.dest != NULL)
        if (!optimize_expr(opt, &stmt->v.Print.dest))
            return 0;
    if (!optimize_expr_seq(opt, &stmt->v.Print.values))
        return 0;
    return 1;
}

static int
_optimize_for_iter(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(opt, &stmt->v.For.iter))
        return 0;
    return 1;
}

static int
optimize_for(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(opt, &stmt->v.For.target))
        return 0;
    if (!_optimize_for_iter(opt, &stmt))
        return 0;
    if (!optimize_stmt_seq(opt, &stmt->v.For.body))
        return 0;
    if (!optimize_stmt_seq(opt, &stmt->v.For.orelse))
        return 0;
    return 1;
}

static int
optimize_while(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(opt, &stmt->v.While.test))
        return 0;
    if (!optimize_stmt_seq(opt, &stmt->v.While.body))
        return 0;
    if (!optimize_stmt_seq(opt, &stmt->v.While.orelse))
        return 0;
    return 1;
}

static int
optimize_if(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;

    if (!optimize_expr(opt, &stmt->v.If.test))
        return 0;
    if (!optimize_stmt_seq(opt, &stmt->v.If.body))
        return 0;
    if (!optimize_stmt_seq(opt, &stmt->v.If.orelse))
        return 0;

    /* BEFORE: if not <A>: <B>; else: <C>
     * AFTER:  if <A>: <C>; else: <B>
     */
    if (stmt->v.If.test->kind == UnaryOp_kind &&
            stmt->v.If.test->v.UnaryOp.op == Not) {
        asdl_seq* body = stmt->v.If.orelse;
        if (body == NULL) {
            /* body can't be NULL, so let's turn this into a Pass() */
            stmt_ty pass = Pass(stmt->lineno, stmt->col_offset, opt->opt_arena);
            if (pass == NULL)
                return 0;

            body = asdl_seq_new(1, opt->opt_arena);
            if (body == NULL)
                return 0;

            asdl_seq_SET(body, 0, pass);
        }
        *stmt_ptr = If(stmt->v.If.test->v.UnaryOp.operand,
                        body,
                        stmt->v.If.body,
                        stmt->lineno, stmt->col_offset, opt->opt_arena);
        if (*stmt_ptr == NULL)
            return 0;
    }

    return 1;
}

static int
optimize_with(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(opt, &stmt->v.With.context_expr))
        return 0;
    if (stmt->v.With.optional_vars != NULL)
        if (!optimize_expr(opt, &stmt->v.With.optional_vars))
            return 0;
    if (!optimize_stmt_seq(opt, &stmt->v.With.body))
        return 0;
    return 1;
}

static int
optimize_raise(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    if (stmt->v.Raise.type != NULL)
        if (!optimize_expr(opt, &stmt->v.Raise.type))
            return 0;
    if (stmt->v.Raise.inst != NULL)
        if (!optimize_expr(opt, &stmt->v.Raise.inst))
            return 0;
    if (stmt->v.Raise.tback != NULL)
        if (!optimize_expr(opt, &stmt->v.Raise.tback))
            return 0;
    return 1;
}

static int
optimize_excepthandler(optimizer* opt, excepthandler_ty* exc_ptr)
{
    excepthandler_ty exc = *exc_ptr;
    if (exc->v.ExceptHandler.type != NULL)
        if (!optimize_expr(opt, &exc->v.ExceptHandler.type))
            return 0;
    if (exc->v.ExceptHandler.name != NULL)
        if (!optimize_expr(opt, &exc->v.ExceptHandler.name))
            return 0;
    if (!optimize_stmt_seq(opt, &exc->v.ExceptHandler.body))
        return 0;
    return 1;
}

static int
optimize_try_except(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;

    if (!optimize_stmt_seq(opt, &stmt->v.TryExcept.body))
        return 0;
    if (!optimize_excepthandler_seq(opt, &stmt->v.TryExcept.handlers))
        return 0;
    if (!optimize_stmt_seq(opt, &stmt->v.TryExcept.orelse))
        return 0;

    return 1;
}

static int
optimize_try_finally(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;

    if (!optimize_stmt_seq(opt, &stmt->v.TryFinally.body))
        return 0;

    opt->opt_current->b_finally = 1;
    if (!optimize_stmt_seq(opt, &stmt->v.TryFinally.finalbody))
        return 0;
    opt->opt_current->b_finally = 0;

    return 1;
}

static int
optimize_assert(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(opt, &stmt->v.Assert.test))
        return 0;
    if (stmt->v.Assert.msg != NULL)
        if (!optimize_expr(opt, &stmt->v.Assert.msg))
            return 0;
    return 1;
}

static int
optimize_exec(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(opt, &stmt->v.Exec.body))
        return 0;
    if (stmt->v.Exec.globals != NULL)
        if (!optimize_expr(opt, &stmt->v.Exec.globals))
            return 0;
    if (stmt->v.Exec.locals != NULL)
        if (!optimize_expr(opt, &stmt->v.Exec.locals))
            return 0;
    return 1;
}

static int
optimize_stmt(optimizer* opt, stmt_ty* stmt_ptr)
{
    stmt_ty stmt = *stmt_ptr;

    switch (stmt->kind) {
        case FunctionDef_kind:
            {
                int result;
                PySTEntryObject* ste;

                ste = PySymtable_Lookup(opt->opt_symtable, stmt);
                if (ste == NULL)
                    return 0;
                if (!_enter_block(opt, ste)) {
                    Py_DECREF(ste);
                    return 0;
                }

                result = optimize_function_def(opt, stmt_ptr);

                _leave_block(opt);
                Py_DECREF(ste);
                return result;
            }
        case ClassDef_kind:
            {
                int result;
                PySTEntryObject* ste;

                ste = PySymtable_Lookup(opt->opt_symtable, stmt);
                if (ste == NULL)
                    return 0;
                if (!_enter_block(opt, ste)) {
                    Py_DECREF(ste);
                    return 0;
                }
                result = optimize_class_def(opt, stmt_ptr);
                _leave_block(opt);
                Py_DECREF(ste);
                return result;
            }
        case Return_kind:
            {
                return optimize_return(opt, stmt_ptr);
            }
        case Delete_kind:
            {
                return optimize_delete(opt, stmt_ptr);
            }
        case Assign_kind:
            {
                return optimize_assign(opt, stmt_ptr);
            }
        case AugAssign_kind:
            {
                return optimize_aug_assign(opt, stmt_ptr);
            }
        case Print_kind:
            {
                return optimize_print(opt, stmt_ptr);
            }
        case For_kind:
            {
                return optimize_for(opt, stmt_ptr);
            }
        case While_kind:
            {
                return optimize_while(opt, stmt_ptr);
            }
        case If_kind:
            {
                return optimize_if(opt, stmt_ptr);
            }
        case With_kind:
            {
                return optimize_with(opt, stmt_ptr);
            }
        case Raise_kind:
            {
                return optimize_raise(opt, stmt_ptr);
            }
        case TryExcept_kind:
            {
                return optimize_try_except(opt, stmt_ptr);
            }
        case TryFinally_kind:
            {
                return optimize_try_finally(opt, stmt_ptr);
            }
        case Assert_kind:
            {
                return optimize_assert(opt, stmt_ptr);
            }
        case Exec_kind:
            {
                return optimize_exec(opt, stmt_ptr);
            }
        case Expr_kind:
            {
                return optimize_expr(opt, &stmt->v.Expr.value);
            }
        case Import_kind:
        case ImportFrom_kind:
        case Global_kind:
        case Pass_kind:
        case Break_kind:
            {
                return 1;
            }
        case Continue_kind:
            {
                if (opt->opt_current->b_finally)
                    opt->opt_current->b_eliminate = 0;
                return 1;
            }
        default:
            PyErr_Format(PyExc_ValueError, "unknown stmt_ty kind: %d",
                    stmt->kind);
            return 0;
    };

    return 1;
}

/**
 * Optimize an AST.
 */
int
PyAST_Optimize(mod_ty* mod_ptr, struct symtable* st, PyArena* arena)
{
    int result;
    PySTEntryObject* ste;
    optimizer opt;

    opt.opt_symtable  = st;
    opt.opt_arena     = arena;
    opt.opt_current   = NULL;

    ste = PySymtable_Lookup(st, *mod_ptr);
    if (ste == NULL)
        return 0;
    if (!_enter_block(&opt, ste)) {
        Py_DECREF(ste);
        return 0;
    }
    result = optimize_mod(&opt, mod_ptr);
    _leave_block(&opt);
    Py_DECREF(ste);
    return result;
}

