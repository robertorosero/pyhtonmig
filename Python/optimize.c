#include "Python.h"
#include "Python-ast.h"
#include "pyarena.h"
#include "pyerrors.h"
#include "node.h"
#include "ast.h"
#include "symtable.h"

static int optimize_expr(expr_ty* expr_ptr, PySTEntryObject* ste,
                            PyArena* arena);
static int optimize_stmt(stmt_ty* stmt_ptr, PySTEntryObject* ste,
                            PyArena* arena);
static int optimize_comprehension(comprehension_ty* comp_ptr,
                                    PySTEntryObject* ste, PyArena* arena);
static int optimize_excepthandler(excepthandler_ty* exc_ptr,
                                    PySTEntryObject* ste, PyArena* arena);
static int optimize_keyword(keyword_ty* kwd_ptr, PySTEntryObject* ste,
                            PyArena* arena);
static int optimize_arguments(arguments_ty* args_ptr, PySTEntryObject* ste,
                                PyArena* arena);
static int optimize_slice(slice_ty* slice_ptr, PySTEntryObject* ste,
                            PyArena* arena);

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
    else if (expr->kind == Name_kind) {
        const char* name = PyString_AS_STRING(expr->v.Name.id);
        if (strcmp(name, "True") == 0)
            return Py_True;
        else if (strcmp(name, "False") == 0)
            return Py_False;
        else if (strcmp(name, "None") == 0)
            return Py_None;
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
        PyObject* value;
        expr_ty expr = (expr_ty)asdl_seq_GET(seq, i);
        value = _expr_constant_value(expr);
        Py_INCREF(value);
        PyTuple_SetItem(result, i, value);
    }

    return result;
}

/**
 * Optimize a sequence of expressions.
 */
static int
optimize_expr_seq(asdl_seq** seq_ptr, PySTEntryObject* ste, PyArena* arena)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++)
        if (!optimize_expr((expr_ty*)&asdl_seq_GET(seq, n), ste, arena))
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
_asdl_seq_replace_with_pass(asdl_seq* seq, int n, int lineno, int col_offset,
                                PyArena* arena)
{
    stmt_ty pass = Pass(lineno, col_offset, arena);
    if (pass == NULL)
        return NULL;
    asdl_seq_SET(seq, n, pass);
    return seq;
}

/**
 * Optimize a sequence of statements.
 */
static int
optimize_stmt_seq(asdl_seq** seq_ptr, PySTEntryObject* ste, PyArena* arena)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++) {
        stmt_ty stmt = asdl_seq_GET(seq, n);
        if (!optimize_stmt((stmt_ty*)&asdl_seq_GET(seq, n), ste, arena))
            return 0;

        if (stmt->kind == If_kind) {
            PyObject* test = _expr_constant_value(stmt->v.If.test);
            /* eliminate branches that can never be reached */
            if (test != NULL) {
                if (PyObject_IsTrue(test))
                    seq = _asdl_seq_replace(seq, n, stmt->v.If.body, arena);
                else {
                    if (stmt->v.If.orelse == NULL) {
                        /* no "else:" body: use a Pass() */
                        seq = _asdl_seq_replace_with_pass(seq, n,
                                stmt->lineno, stmt->col_offset, arena);
                    }
                    else {
                        seq = _asdl_seq_replace(seq, n, stmt->v.If.orelse,
                                arena);
                    }
                }
                if (seq == NULL)
                    return 0;
                *seq_ptr = seq;
            }
        }
        else if (stmt->kind == Return_kind && n < (asdl_seq_LEN(seq) - 1)) {
            /* eliminate all nodes after a return */
            seq = _asdl_seq_replace_with_pass(seq, n + 1,
                    stmt->lineno, stmt->col_offset, arena);
            if (seq == NULL)
                return 0;
            *seq_ptr = seq;
        }
    }
    return 1;
}

static int
optimize_comprehension_seq(asdl_seq** seq_ptr, PySTEntryObject* ste,
                            PyArena* arena)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++) {
        comprehension_ty* comp;
        comp = (comprehension_ty*)&asdl_seq_GET(seq, n);
        if (!optimize_comprehension(comp, ste, arena))
            return 0;
    }
    return 1;
}

static int
optimize_excepthandler_seq(asdl_seq** seq_ptr, PySTEntryObject* ste,
                            PyArena* arena)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++) {
        excepthandler_ty* excepthandler;
        excepthandler = (excepthandler_ty*)&asdl_seq_GET(seq, n);
        if (!optimize_excepthandler(excepthandler, ste, arena))
            return 0;
    }
    return 1;
}

static int
optimize_keyword_seq(asdl_seq** seq_ptr, PySTEntryObject* ste, PyArena* arena)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++)
        if (!optimize_keyword((keyword_ty*)&asdl_seq_GET(seq, n), ste, arena))
            return 0;
    return 1;
}

static int
optimize_slice_seq(asdl_seq** seq_ptr, PySTEntryObject* ste, PyArena* arena)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++)
        if (!optimize_slice((slice_ty*)&asdl_seq_GET(seq, n), ste, arena))
            return 0;
    return 1;
}

static int
optimize_mod(mod_ty* mod_ptr, PySTEntryObject* ste, PyArena* arena)
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
                return optimize_expr(&mod->v.Expression.body, ste, arena);
            }
        default:
            PyErr_Format(PyExc_ValueError, "unknown mod_ty kind: %d",
                    mod->kind);
            return 0;
    };

    return optimize_stmt_seq(body, ste, arena);
}

static int
optimize_bool_op(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr_seq(&expr->v.BoolOp.values, ste, arena))
        return 0;
    return 1;
}

static int
optimize_bin_op(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    PyObject* left;
    PyObject* right;
    expr_ty expr = *expr_ptr;

    if (!optimize_expr(&expr->v.BinOp.left, ste, arena))
        return 0;
    if (!optimize_expr(&expr->v.BinOp.right, ste, arena))
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
                    /* avoid divide-by-zero errors */
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
        
        expr = _expr_from_object(res, expr->lineno, expr->col_offset, arena);
        if (expr == NULL) {
            Py_DECREF(res);
            return 0;
        }
        *expr_ptr = expr;
    }

    return 1;
}

static int
optimize_unary_op(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    PyObject* operand;
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.UnaryOp.operand, ste, arena))
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

        expr = _expr_from_object(res, expr->lineno, expr->col_offset, arena);
        if (!expr) {
            Py_DECREF(res);
            return 0;
        }
        *expr_ptr = expr;
    }
    return 1;
}

static int
optimize_lambda(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    /* XXX: do we need to look up ste again? */
    if (!optimize_expr(&expr->v.Lambda.body, ste, arena))
        return 0;
    return 1;
}

static int optimize_if_exp(expr_ty* expr_ptr, PySTEntryObject* ste,
                            PyArena* arena) {
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.IfExp.test, ste, arena))
        return 0;
    if (!optimize_expr(&expr->v.IfExp.body, ste, arena))
        return 0;
    if (!optimize_expr(&expr->v.IfExp.orelse, ste, arena))
        return 0;
    return 1;
}

static int optimize_dict(expr_ty* expr_ptr, PySTEntryObject* ste,
                            PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr_seq(&expr->v.Dict.keys, ste, arena))
        return 0;
    if (!optimize_expr_seq(&expr->v.Dict.values, ste, arena))
        return 0;
    return 1;
}

static int
optimize_comprehension(comprehension_ty* comp_ptr, PySTEntryObject* ste,
                        PyArena* arena)
{
    comprehension_ty comp = *comp_ptr;
    if (!optimize_expr(&comp->target, ste, arena))
        return 0;
    if (!optimize_expr(&comp->iter, ste, arena))
        return 0;
    if (!optimize_expr_seq(&comp->ifs, ste, arena))
        return 0;
    return 1;
}

static int
optimize_list_comp(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.ListComp.elt, ste, arena))
        return 0;
    if (!optimize_comprehension_seq(&expr->v.ListComp.generators, ste, arena))
        return 0;
    return 1;
}

static int
optimize_generator_exp(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.GeneratorExp.elt, ste, arena))
        return 0;
    if (!optimize_comprehension_seq(&expr->v.GeneratorExp.generators, ste,
                                        arena))
        return 0;
    return 1;
}

static int
optimize_yield(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (expr->v.Yield.value != NULL) {
        expr_ty value;
        if (!optimize_expr(&expr->v.Yield.value, ste, arena))
            return 0;
        value = expr->v.Yield.value;
        if (value->kind == Const_kind && value->v.Const.value == Py_None)
            expr->v.Yield.value = NULL;
    }
    return 1;
}

static int
optimize_compare(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.Compare.left, ste, arena))
        return 0;
    if (!optimize_expr_seq(&expr->v.Compare.comparators, ste, arena))
        return 0;
    return 1;
}

static int
optimize_keyword(keyword_ty* keyword_ptr, PySTEntryObject* ste, PyArena* arena)
{
    keyword_ty keyword = *keyword_ptr;
    if (!optimize_expr(&keyword->value, ste, arena))
        return 0;
    return 1;
}

static int
optimize_arguments(arguments_ty* args_ptr, PySTEntryObject* ste,
                    PyArena* arena)
{
    arguments_ty args = *args_ptr;
    if (!optimize_expr_seq(&args->args, ste, arena))
        return 0;
    if (!optimize_expr_seq(&args->defaults, ste, arena))
        return 0;
    return 1;
}

static int
optimize_call(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.Call.func, ste, arena))
        return 0;
    if (!optimize_expr_seq(&expr->v.Call.args, ste, arena))
        return 0;
    if (!optimize_keyword_seq(&expr->v.Call.keywords, ste, arena))
        return 0;
    if (expr->v.Call.starargs != NULL)
        if (!optimize_expr(&expr->v.Call.starargs, ste, arena))
            return 0;
    if (expr->v.Call.kwargs != NULL)
        if (!optimize_expr(&expr->v.Call.kwargs, ste, arena))
            return 0;
    return 1;
}

static int
optimize_repr(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.Repr.value, ste, arena))
        return 0;
    return 1;
}

static int
optimize_attribute(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.Attribute.value, ste, arena))
        return 0;
    return 1;
}

static int
optimize_slice(slice_ty* slice_ptr, PySTEntryObject* ste, PyArena* arena)
{
    slice_ty slice = *slice_ptr;
    switch (slice->kind) {
        case Slice_kind:
            {
                if (slice->v.Slice.lower != NULL)
                    if (!optimize_expr(&slice->v.Slice.lower, ste, arena))
                        return 0;
                if (slice->v.Slice.upper != NULL)
                    if (!optimize_expr(&slice->v.Slice.upper, ste, arena))
                        return 0;
                if (slice->v.Slice.step != NULL)
                    if (!optimize_expr(&slice->v.Slice.step, ste, arena))
                        return 0;
                break;
            }
        case ExtSlice_kind:
            {
                if (!optimize_slice_seq(&slice->v.ExtSlice.dims, ste, arena))
                    return 0;
                break;
            }
        case Index_kind:
            {
                if (!optimize_expr(&slice->v.Index.value, ste, arena))
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
optimize_subscript(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.Subscript.value, ste, arena))
        return 0;
    if (!optimize_slice(&expr->v.Subscript.slice, ste, arena))
        return 0;
    return 1;
}

static int
optimize_tuple(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr_seq(&expr->v.Tuple.elts, ste, arena))
        return 0;

    if (_is_sequence_of_constants(expr->v.Tuple.elts)) {
        PyObject* tuple = _build_tuple_of_constants(expr->v.Tuple.elts, arena);
        if (tuple == NULL)
            return 0;
        *expr_ptr = Const(tuple, expr->lineno, expr->col_offset, arena);
        if (*expr_ptr == NULL)
            return 0;
    }

    return 1;
}

static int
optimize_name(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    const char* id = PyString_AS_STRING(expr->v.Name.id);
    PyObject* constvalue = NULL;

    if (strcmp(id, "None") == 0) {
        Py_INCREF(Py_None);
        constvalue = Py_None;
    }
    else if (strcmp(id, "True") == 0) {
        Py_INCREF(Py_True);
        constvalue = Py_True;
    }
    else if (strcmp(id, "False") == 0) {
        Py_INCREF(Py_False);
        constvalue = Py_False;
    }

    if (constvalue != NULL)
        *expr_ptr = Const(constvalue, expr->lineno,
                            expr->col_offset, arena);

    return 1;
}

static int
optimize_expr(expr_ty* expr_ptr, PySTEntryObject* ste, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    switch (expr->kind) {
        case BoolOp_kind:
            {
                return optimize_bool_op(expr_ptr, ste, arena);
            }
        case BinOp_kind:
            {
                return optimize_bin_op(expr_ptr, ste, arena);
            }
        case UnaryOp_kind:
            {
                return optimize_unary_op(expr_ptr, ste, arena);
            }
        case Lambda_kind:
            {
                return optimize_lambda(expr_ptr, ste, arena);
            }
        case IfExp_kind:
            {
                return optimize_if_exp(expr_ptr, ste, arena);
            }
        case Dict_kind:
            {
                return optimize_dict(expr_ptr, ste, arena);
            }
        case ListComp_kind:
            {
                return optimize_list_comp(expr_ptr, ste, arena);
            }
        case GeneratorExp_kind:
            {
                return optimize_generator_exp(expr_ptr, ste, arena);
            }
        case Yield_kind:
            {
                return optimize_yield(expr_ptr, ste, arena);
            }
        case Compare_kind:
            {
                return optimize_compare(expr_ptr, ste, arena);
            }
        case Call_kind:
            {
                return optimize_call(expr_ptr, ste, arena);
            }
        case Repr_kind:
            {
                return optimize_repr(expr_ptr, ste, arena);
            }
        case Attribute_kind:
            {
                return optimize_attribute(expr_ptr, ste, arena);
            }
        case Subscript_kind:
            {
                return optimize_subscript(expr_ptr, ste, arena);
            }
        case List_kind:
            {
                return optimize_expr_seq(&expr->v.List.elts, ste, arena);
            }
        case Tuple_kind:
            {
                return optimize_tuple(expr_ptr, ste, arena);
            }
        case Name_kind:
            {
                return optimize_name(expr_ptr, ste, arena);
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
optimize_function_def(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_arguments(&stmt->v.FunctionDef.args, ste, arena))
        return 0;
    if (!optimize_expr_seq(&stmt->v.FunctionDef.decorator_list, ste, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.FunctionDef.body, ste, arena))
        return 0;
    return 1;
}

static int
optimize_class_def(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr_seq(&stmt->v.ClassDef.bases, ste, arena))
        return 0;
    if (!optimize_expr_seq(&stmt->v.ClassDef.decorator_list, ste, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.ClassDef.body, ste, arena))
        return 0;
    return 1;
}

static int
optimize_return(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (stmt->v.Return.value != NULL) {
        expr_ty value;
        if (!optimize_expr(&stmt->v.Return.value, ste, arena))
            return 0;
        value = stmt->v.Return.value;
        if (value->kind == Const_kind && value->v.Const.value == Py_None)
            stmt->v.Return.value = NULL;
    }
    return 1;
}

static int
optimize_delete(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr_seq(&stmt->v.Delete.targets, ste, arena))
        return 0;
    return 1;
}

static int
optimize_assign(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr_seq(&stmt->v.Assign.targets, ste, arena))
        return 0;
    if (!optimize_expr(&stmt->v.Assign.value, ste, arena))
        return 0;
    return 1;
}

static int
optimize_aug_assign(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(&stmt->v.AugAssign.target, ste, arena))
        return 0;
    if (!optimize_expr(&stmt->v.AugAssign.value, ste, arena))
        return 0;
    return 1;
}

static int
optimize_print(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;

    if (stmt->v.Print.dest != NULL)
        if (!optimize_expr(&stmt->v.Print.dest, ste, arena))
            return 0;
    if (!optimize_expr_seq(&stmt->v.Print.values, ste, arena))
        return 0;
    return 1;
}

static int
optimize_for(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(&stmt->v.For.target, ste, arena))
        return 0;
    if (!optimize_expr(&stmt->v.For.iter, ste, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.For.body, ste, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.For.orelse, ste, arena))
        return 0;
    return 1;
}

static int
optimize_while(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(&stmt->v.While.test, ste, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.While.body, ste, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.While.orelse, ste, arena))
        return 0;
    return 1;
}

static int
optimize_if(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;

    if (!optimize_expr(&stmt->v.If.test, ste, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.If.body, ste, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.If.orelse, ste, arena))
        return 0;

    if (stmt->v.If.test->kind == UnaryOp_kind &&
            stmt->v.If.test->v.UnaryOp.op == Not) {
        asdl_seq* body = stmt->v.If.orelse;
        if (body == NULL) {
            /* body can't be NULL, so let's turn this into a Pass() */
            stmt_ty pass = Pass(stmt->lineno, stmt->col_offset, arena);
            if (pass == NULL)
                return 0;

            body = asdl_seq_new(1, arena);
            if (body == NULL)
                return 0;

            asdl_seq_SET(body, 0, pass);
        }
        *stmt_ptr = If(stmt->v.If.test->v.UnaryOp.operand,
                        body,
                        stmt->v.If.body,
                        stmt->lineno, stmt->col_offset, arena);
        if (*stmt_ptr == NULL)
            return 0;
    }

    return 1;
}

static int
optimize_with(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(&stmt->v.With.context_expr, ste, arena))
        return 0;
    if (stmt->v.With.optional_vars != NULL)
        if (!optimize_expr(&stmt->v.With.optional_vars, ste, arena))
            return 0;
    if (!optimize_stmt_seq(&stmt->v.With.body, ste, arena))
        return 0;
    return 1;
}

static int
optimize_raise(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (stmt->v.Raise.type != NULL)
        if (!optimize_expr(&stmt->v.Raise.type, ste, arena))
            return 0;
    if (stmt->v.Raise.inst != NULL)
        if (!optimize_expr(&stmt->v.Raise.inst, ste, arena))
            return 0;
    if (stmt->v.Raise.tback != NULL)
        if (!optimize_expr(&stmt->v.Raise.tback, ste, arena))
            return 0;
    return 1;
}

static int
optimize_excepthandler(excepthandler_ty* exc_ptr, PySTEntryObject* ste,
                        PyArena* arena)
{
    excepthandler_ty exc = *exc_ptr;
    if (exc->v.ExceptHandler.type != NULL)
        if (!optimize_expr(&exc->v.ExceptHandler.type, ste, arena))
            return 0;
    if (exc->v.ExceptHandler.name != NULL)
        if (!optimize_expr(&exc->v.ExceptHandler.name, ste, arena))
            return 0;
    if (!optimize_stmt_seq(&exc->v.ExceptHandler.body, ste, arena))
        return 0;
    return 1;
}

static int
optimize_try_except(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_stmt_seq(&stmt->v.TryExcept.body, ste, arena))
        return 0;
    if (!optimize_excepthandler_seq(&stmt->v.TryExcept.handlers, ste, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.TryExcept.orelse, ste, arena))
        return 0;
    return 1;
}

static int
optimize_try_finally(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_stmt_seq(&stmt->v.TryFinally.body, ste, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.TryFinally.finalbody, ste, arena))
        return 0;
    return 1;
}

static int
optimize_assert(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(&stmt->v.Assert.test, ste, arena))
        return 0;
    if (stmt->v.Assert.msg != NULL)
        if (!optimize_expr(&stmt->v.Assert.msg, ste, arena))
            return 0;
    return 1;
}

static int
optimize_exec(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(&stmt->v.Exec.body, ste, arena))
        return 0;
    if (stmt->v.Exec.globals != NULL)
        if (!optimize_expr(&stmt->v.Exec.globals, ste, arena))
            return 0;
    if (stmt->v.Exec.locals != NULL)
        if (!optimize_expr(&stmt->v.Exec.locals, ste, arena))
            return 0;
    return 1;
}

static int
optimize_stmt(stmt_ty* stmt_ptr, PySTEntryObject* ste, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;

    switch (stmt->kind) {
        case FunctionDef_kind:
            {
                int rc;
                ste = PySymtable_Lookup(ste->ste_table, *stmt_ptr);
                if (ste == NULL)
                    return 0;
                rc = optimize_function_def(stmt_ptr, ste, arena);
                Py_DECREF(ste);
                return rc;
            }
        case ClassDef_kind:
            {
                int rc;
                ste = PySymtable_Lookup(ste->ste_table, *stmt_ptr);
                if (ste == NULL)
                    return 0;
                rc = optimize_class_def(stmt_ptr, ste, arena);
                Py_DECREF(ste);
                return rc;
            }
        case Return_kind:
            {
                return optimize_return(stmt_ptr, ste, arena);
            }
        case Delete_kind:
            {
                return optimize_delete(stmt_ptr, ste, arena);
            }
        case Assign_kind:
            {
                return optimize_assign(stmt_ptr, ste, arena);
            }
        case AugAssign_kind:
            {
                return optimize_aug_assign(stmt_ptr, ste, arena);
            }
        case Print_kind:
            {
                return optimize_print(stmt_ptr, ste, arena);
            }
        case For_kind:
            {
                return optimize_for(stmt_ptr, ste, arena);
            }
        case While_kind:
            {
                return optimize_while(stmt_ptr, ste, arena);
            }
        case If_kind:
            {
                return optimize_if(stmt_ptr, ste, arena);
            }
        case With_kind:
            {
                return optimize_with(stmt_ptr, ste, arena);
            }
        case Raise_kind:
            {
                return optimize_raise(stmt_ptr, ste, arena);
            }
        case TryExcept_kind:
            {
                return optimize_try_except(stmt_ptr, ste, arena);
            }
        case TryFinally_kind:
            {
                return optimize_try_finally(stmt_ptr, ste, arena);
            }
        case Assert_kind:
            {
                return optimize_assert(stmt_ptr, ste, arena);
            }
        case Exec_kind:
            {
                return optimize_exec(stmt_ptr, ste, arena);
            }
        case Expr_kind:
            {
                return optimize_expr(&stmt->v.Expr.value, ste, arena);
            }
        case Import_kind:
        case ImportFrom_kind:
        case Global_kind:
        case Pass_kind:
        case Break_kind:
        case Continue_kind:
            {
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
    int rc;
    PySTEntryObject* ste = PySymtable_Lookup(st, *mod_ptr);
    if (ste == NULL)
        return 0;
    rc = optimize_mod(mod_ptr, ste, arena);
    Py_DECREF(ste);
    return rc;
}

