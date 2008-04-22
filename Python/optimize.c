#include "Python.h"
#include "Python-ast.h"
#include "pyarena.h"
#include "pyerrors.h"
#include "node.h"
#include "ast.h"

static int optimize_expr(expr_ty* expr_ptr, PyArena* arena);
static int optimize_stmt(stmt_ty* stmt_ptr, PyArena* arena);
static int optimize_comprehension(comprehension_ty* comp_ptr, PyArena* arena);
static int optimize_excepthandler(excepthandler_ty* exc_ptr, PyArena* arena);
static int optimize_keyword(keyword_ty* kwd_ptr, PyArena* arena);
static int optimize_arguments(arguments_ty* args_ptr, PyArena* arena);
static int optimize_slice(slice_ty* slice_ptr, PyArena* arena);

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
    return NULL;
}

/**
 * Determine whether or not the given expression represents a constant value.
 * This makes the assumption that constants have already been folded.
 */
static int
_expr_is_constant(expr_ty expr)
{
    return _expr_constant_value(expr) != NULL;
}

/*
 * builds a Name() node with a Load context from the given id.
 */
static expr_ty
_make_name(PyObject* id, int lineno, int col_offset, PyArena* arena)
{
    expr_ty expr;
    if (id == NULL)
        return NULL;
    expr = Name(id, Load, lineno, col_offset, arena);
    if (expr == NULL)
        return NULL;
    return expr;
}

/**
 * Builds an expr from the given constant value. Constant values can be
 * any Str or Num, or any one of True/False/None.
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
    else if (object == Py_None) {
        object = PyString_FromString("None");
        expr = _make_name(object, lineno, col_offset, arena);
    }
    else if (object == Py_True) {
        object = PyString_FromString("True");
        expr = _make_name(object, lineno, col_offset, arena);
    }
    else if (object == Py_False) {
        object = PyString_FromString("False");
        expr = _make_name(object, lineno, col_offset, arena);
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

/**
 * Returns 1 if the given expression evaluates to a true value. Otherwise,
 * returns 0. This function assumes that the given expr is constant.
 */
static int
_expr_is_true(expr_ty expr)
{
    assert(_expr_is_constant(expr));
    return PyObject_IsTrue(_expr_constant_value(expr));
}

/**
 * Optimize a sequence of expressions.
 */
static int
optimize_expr_seq(asdl_seq** seq_ptr, PyArena* arena)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++)
        if (!optimize_expr((expr_ty*)&asdl_seq_GET(seq, n), arena))
            return 0;
    return 1;
}

#if 0
/**
 * Shrink an ASDL sequence by trimming off the last few elements.
 */
static asdl_seq*
_asdl_seq_shrink(asdl_seq* seq, int newlen, PyArena* arena)
{
    asdl_seq* new;
    int n;
    new = asdl_seq_new(newlen, arena);
    if (new == NULL)
        return NULL;
    for (n = 0; n < newlen; n++)
        asdl_seq_SET(new, n, asdl_seq_GET(seq, n));
    return new;
}
#endif

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
    stmt_ty pass;
    asdl_seq* new;

    pass = Pass(lineno, col_offset, arena);
    if (pass == NULL)
        return NULL;
    new = asdl_seq_new(1, arena);
    if (new == NULL)
        return NULL;
    asdl_seq_SET(new, 0, pass);
    
    return _asdl_seq_replace(seq, n, new, arena);
}

/**
 * Optimize a sequence of statements.
 */
static int
optimize_stmt_seq(asdl_seq** seq_ptr, PyArena* arena)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++) {
        stmt_ty stmt = asdl_seq_GET(seq, n);
        if (!optimize_stmt((stmt_ty*)&asdl_seq_GET(seq, n), arena))
            return 0;

        if (stmt->kind == If_kind) {
            /* eliminate branches that can never be reached */
            if (_expr_is_constant(stmt->v.If.test)) {
                if (_expr_is_true(stmt->v.If.test))
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
        else if (stmt->kind == Return_kind) {
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
optimize_comprehension_seq(asdl_seq** seq_ptr, PyArena* arena)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++) {
        comprehension_ty* comp;
        comp = (comprehension_ty*)&asdl_seq_GET(seq, n);
        if (!optimize_comprehension(comp, arena))
            return 0;
    }
    return 1;
}

static int
optimize_excepthandler_seq(asdl_seq** seq_ptr, PyArena* arena)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++) {
        excepthandler_ty* excepthandler;
        excepthandler = (excepthandler_ty*)&asdl_seq_GET(seq, n);
        if (!optimize_excepthandler(excepthandler, arena))
            return 0;
    }
    return 1;
}

static int
optimize_keyword_seq(asdl_seq** seq_ptr, PyArena* arena)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++)
        if (!optimize_keyword((keyword_ty*)&asdl_seq_GET(seq, n), arena))
            return 0;
    return 1;
}

static int
optimize_slice_seq(asdl_seq** seq_ptr, PyArena* arena)
{
    int n;
    asdl_seq* seq = *seq_ptr;
    for (n = 0; n < asdl_seq_LEN(seq); n++)
        if (!optimize_slice((slice_ty*)&asdl_seq_GET(seq, n), arena))
            return 0;
    return 1;
}

static int
optimize_mod(mod_ty* mod_ptr, PyArena* arena)
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
                return optimize_expr(&mod->v.Expression.body, arena);
            }
        default:
            PyErr_Format(PyExc_ValueError, "unknown mod_ty kind: %d",
                    mod->kind);
            return 0;
    };

    return optimize_stmt_seq(body, arena);
}

static int
optimize_bool_op(expr_ty* expr_ptr, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr_seq(&expr->v.BoolOp.values, arena))
        return 0;
    return 1;
}

static int
optimize_bin_op(expr_ty* expr_ptr, PyArena* arena)
{
    PyObject* left;
    PyObject* right;
    expr_ty expr = *expr_ptr;

    if (!optimize_expr(&expr->v.BinOp.left, arena))
        return 0;
    if (!optimize_expr(&expr->v.BinOp.right, arena))
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
                /* if we're out of memory, we need to complain about it now! */
                if (PyErr_ExceptionMatches(PyExc_MemoryError)) {
                    return 0;
                }
                /* otherwise, the binop failed: clear the error */
                else
                    PyErr_Clear();
            }
            /* do not optimize this expression */
            return 1;
        }

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

        Py_DECREF(res);
    }

    return 1;
}

static int
optimize_unary_op(expr_ty* expr_ptr, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.UnaryOp.operand, arena))
        return 0;
    if (_expr_is_constant(expr->v.UnaryOp.operand)) {
        PyObject* operand;
        PyObject* res;

        operand = _expr_constant_value(expr->v.UnaryOp.operand);
        if (operand == NULL) {
            /* XXX this should never happen ... */
            PyErr_Format(PyExc_ValueError, "unknown constant type: %d",
                    expr->v.UnaryOp.operand->kind);
            return 0;
        }

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

        if (res == NULL)
            return 0;

        expr = _expr_from_object(res, expr->lineno, expr->col_offset, arena);
        if (!expr) {
            Py_DECREF(res);
            return 0;
        }
        *expr_ptr = expr;

        Py_DECREF(res);
    }
    return 1;
}

static int
optimize_lambda(expr_ty* expr_ptr, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.Lambda.body, arena))
        return 0;
    return 1;
}

static int optimize_if_exp(expr_ty* expr_ptr, PyArena* arena) {
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.IfExp.test, arena))
        return 0;
    if (!optimize_expr(&expr->v.IfExp.body, arena))
        return 0;
    if (!optimize_expr(&expr->v.IfExp.orelse, arena))
        return 0;
    return 1;
}

static int optimize_dict(expr_ty* expr_ptr, PyArena* arena) {
    expr_ty expr = *expr_ptr;
    if (!optimize_expr_seq(&expr->v.Dict.keys, arena))
        return 0;
    if (!optimize_expr_seq(&expr->v.Dict.values, arena))
        return 0;
    return 1;
}

static int
optimize_comprehension(comprehension_ty* comp_ptr, PyArena* arena)
{
    comprehension_ty comp = *comp_ptr;
    if (!optimize_expr(&comp->target, arena))
        return 0;
    if (!optimize_expr(&comp->iter, arena))
        return 0;
    if (!optimize_expr_seq(&comp->ifs, arena))
        return 0;
    return 1;
}

static int
optimize_list_comp(expr_ty* expr_ptr, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.ListComp.elt, arena))
        return 0;
    if (!optimize_comprehension_seq(&expr->v.ListComp.generators, arena))
        return 0;
    return 1;
}

static int
optimize_generator_exp(expr_ty* expr_ptr, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.GeneratorExp.elt, arena))
        return 0;
    if (!optimize_comprehension_seq(&expr->v.GeneratorExp.generators, arena))
        return 0;
    return 1;
}

static int
optimize_yield(expr_ty* expr_ptr, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (expr->v.Yield.value != NULL)
        if (!optimize_expr(&expr->v.Yield.value, arena))
            return 0;
    return 1;
}

static int
optimize_compare(expr_ty* expr_ptr, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.Compare.left, arena))
        return 0;
    if (!optimize_expr_seq(&expr->v.Compare.comparators, arena))
        return 0;
    return 1;
}

static int
optimize_keyword(keyword_ty* keyword_ptr, PyArena* arena)
{
    keyword_ty keyword = *keyword_ptr;
    if (!optimize_expr(&keyword->value, arena))
        return 0;
    return 1;
}

static int
optimize_arguments(arguments_ty* args_ptr, PyArena* arena)
{
    arguments_ty args = *args_ptr;
    if (!optimize_expr_seq(&args->args, arena))
        return 0;
    if (!optimize_expr_seq(&args->defaults, arena))
        return 0;
    return 1;
}

static int
optimize_call(expr_ty* expr_ptr, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.Call.func, arena))
        return 0;
    if (!optimize_expr_seq(&expr->v.Call.args, arena))
        return 0;
    if (!optimize_keyword_seq(&expr->v.Call.keywords, arena))
        return 0;
    if (expr->v.Call.starargs != NULL)
        if (!optimize_expr(&expr->v.Call.starargs, arena))
            return 0;
    if (expr->v.Call.kwargs != NULL)
        if (!optimize_expr(&expr->v.Call.kwargs, arena))
            return 0;
    return 1;
}

static int
optimize_repr(expr_ty* expr_ptr, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.Repr.value, arena))
        return 0;
    return 1;
}

static int
optimize_attribute(expr_ty* expr_ptr, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.Attribute.value, arena))
        return 0;
    return 1;
}

static int
optimize_slice(slice_ty* slice_ptr, PyArena* arena)
{
    slice_ty slice = *slice_ptr;
    switch (slice->kind) {
        case Slice_kind:
            {
                if (slice->v.Slice.lower != NULL)
                    if (!optimize_expr(&slice->v.Slice.lower, arena))
                        return 0;
                if (slice->v.Slice.upper != NULL)
                    if (!optimize_expr(&slice->v.Slice.upper, arena))
                        return 0;
                if (slice->v.Slice.step != NULL)
                    if (!optimize_expr(&slice->v.Slice.step, arena))
                        return 0;
                break;
            }
        case ExtSlice_kind:
            {
                if (!optimize_slice_seq(&slice->v.ExtSlice.dims, arena))
                    return 0;
                break;
            }
        case Index_kind:
            {
                if (!optimize_expr(&slice->v.Index.value, arena))
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
optimize_subscript(expr_ty* expr_ptr, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    if (!optimize_expr(&expr->v.Subscript.value, arena))
        return 0;
    if (!optimize_slice(&expr->v.Subscript.slice, arena))
        return 0;
    return 1;
}

static int
optimize_expr(expr_ty* expr_ptr, PyArena* arena)
{
    expr_ty expr = *expr_ptr;
    switch (expr->kind) {
        case BoolOp_kind:
            {
                return optimize_bool_op(expr_ptr, arena);
            }
        case BinOp_kind:
            {
                return optimize_bin_op(expr_ptr, arena);
            }
        case UnaryOp_kind:
            {
                return optimize_unary_op(expr_ptr, arena);
            }
        case Lambda_kind:
            {
                return optimize_lambda(expr_ptr, arena);
            }
        case IfExp_kind:
            {
                return optimize_if_exp(expr_ptr, arena);
            }
        case Dict_kind:
            {
                return optimize_dict(expr_ptr, arena);
            }
        case ListComp_kind:
            {
                return optimize_list_comp(expr_ptr, arena);
            }
        case GeneratorExp_kind:
            {
                return optimize_generator_exp(expr_ptr, arena);
            }
        case Yield_kind:
            {
                return optimize_yield(expr_ptr, arena);
            }
        case Compare_kind:
            {
                return optimize_compare(expr_ptr, arena);
            }
        case Call_kind:
            {
                return optimize_call(expr_ptr, arena);
            }
        case Repr_kind:
            {
                return optimize_repr(expr_ptr, arena);
            }
        case Attribute_kind:
            {
                return optimize_attribute(expr_ptr, arena);
            }
        case Subscript_kind:
            {
                return optimize_subscript(expr_ptr, arena);
            }
        case List_kind:
            {
                return optimize_expr_seq(&expr->v.List.elts, arena);
            }
        case Tuple_kind:
            {
                return optimize_expr_seq(&expr->v.Tuple.elts, arena);
            }
        case Num_kind:
        case Str_kind:
        case Name_kind:
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
optimize_function_def(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_arguments(&stmt->v.FunctionDef.args, arena))
        return 0;
    if (!optimize_expr_seq(&stmt->v.FunctionDef.decorator_list, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.FunctionDef.body, arena))
        return 0;
    return 1;
}

static int
optimize_class_def(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr_seq(&stmt->v.ClassDef.bases, arena))
        return 0;
    if (!optimize_expr_seq(&stmt->v.ClassDef.decorator_list, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.ClassDef.body, arena))
        return 0;
    return 1;
}

static int
optimize_return(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (stmt->v.Return.value != NULL)
        if (!optimize_expr(&stmt->v.Return.value, arena))
            return 0;
    return 1;
}

static int
optimize_delete(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr_seq(&stmt->v.Delete.targets, arena))
        return 0;
    return 1;
}

static int
optimize_assign(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr_seq(&stmt->v.Assign.targets, arena))
        return 0;
    if (!optimize_expr(&stmt->v.Assign.value, arena))
        return 0;
    return 1;
}

static int
optimize_aug_assign(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(&stmt->v.AugAssign.target, arena))
        return 0;
    if (!optimize_expr(&stmt->v.AugAssign.value, arena))
        return 0;
    return 1;
}

static int
optimize_print(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;

    if (stmt->v.Print.dest != NULL)
        if (!optimize_expr(&stmt->v.Print.dest, arena))
            return 0;
    if (!optimize_expr_seq(&stmt->v.Print.values, arena))
        return 0;
    return 1;
}

static int
optimize_for(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(&stmt->v.For.target, arena))
        return 0;
    if (!optimize_expr(&stmt->v.For.iter, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.For.body, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.For.orelse, arena))
        return 0;
    return 1;
}

static int
optimize_while(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(&stmt->v.While.test, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.While.body, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.While.orelse, arena))
        return 0;
    return 1;
}

static int
optimize_if(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;

    if (!optimize_expr(&stmt->v.If.test, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.If.body, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.If.orelse, arena))
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
optimize_with(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(&stmt->v.With.context_expr, arena))
        return 0;
    if (stmt->v.With.optional_vars != NULL)
        if (!optimize_expr(&stmt->v.With.optional_vars, arena))
            return 0;
    if (!optimize_stmt_seq(&stmt->v.With.body, arena))
        return 0;
    return 1;
}

static int
optimize_raise(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (stmt->v.Raise.type != NULL)
        if (!optimize_expr(&stmt->v.Raise.type, arena))
            return 0;
    if (stmt->v.Raise.inst != NULL)
        if (!optimize_expr(&stmt->v.Raise.inst, arena))
            return 0;
    if (stmt->v.Raise.tback != NULL)
        if (!optimize_expr(&stmt->v.Raise.tback, arena))
            return 0;
    return 1;
}

static int
optimize_excepthandler(excepthandler_ty* exc_ptr, PyArena* arena)
{
    excepthandler_ty exc = *exc_ptr;
    if (exc->v.ExceptHandler.type != NULL)
        if (!optimize_expr(&exc->v.ExceptHandler.type, arena))
            return 0;
    if (exc->v.ExceptHandler.name != NULL)
        if (!optimize_expr(&exc->v.ExceptHandler.name, arena))
            return 0;
    if (!optimize_stmt_seq(&exc->v.ExceptHandler.body, arena))
        return 0;
    return 1;
}

static int
optimize_try_except(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_stmt_seq(&stmt->v.TryExcept.body, arena))
        return 0;
    if (!optimize_excepthandler_seq(&stmt->v.TryExcept.handlers, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.TryExcept.orelse, arena))
        return 0;
    return 1;
}

static int
optimize_try_finally(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_stmt_seq(&stmt->v.TryFinally.body, arena))
        return 0;
    if (!optimize_stmt_seq(&stmt->v.TryFinally.finalbody, arena))
        return 0;
    return 1;
}

static int
optimize_assert(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(&stmt->v.Assert.test, arena))
        return 0;
    if (stmt->v.Assert.msg != NULL)
        if (!optimize_expr(&stmt->v.Assert.msg, arena))
            return 0;
    return 1;
}

static int
optimize_import(stmt_ty* stmt_ptr, PyArena* arena)
{
    return 1;
}

static int
optimize_import_from(stmt_ty* stmt_ptr, PyArena* arena)
{
    return 1;
}

static int
optimize_exec(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;
    if (!optimize_expr(&stmt->v.Exec.body, arena))
        return 0;
    if (stmt->v.Exec.globals != NULL)
        if (!optimize_expr(&stmt->v.Exec.globals, arena))
            return 0;
    if (stmt->v.Exec.locals != NULL)
        if (!optimize_expr(&stmt->v.Exec.locals, arena))
            return 0;
    return 1;
}

static int
optimize_global(stmt_ty* stmt_ptr, PyArena* arena)
{
    return 1;
}

static int
optimize_stmt(stmt_ty* stmt_ptr, PyArena* arena)
{
    stmt_ty stmt = *stmt_ptr;

    switch (stmt->kind) {
        case FunctionDef_kind:
            {
                return optimize_function_def(stmt_ptr, arena);
            }
        case ClassDef_kind:
            {
                return optimize_class_def(stmt_ptr, arena);
            }
        case Return_kind:
            {
                return optimize_return(stmt_ptr, arena);
            }
        case Delete_kind:
            {
                return optimize_delete(stmt_ptr, arena);
            }
        case Assign_kind:
            {
                return optimize_assign(stmt_ptr, arena);
            }
        case AugAssign_kind:
            {
                return optimize_aug_assign(stmt_ptr, arena);
            }
        case Print_kind:
            {
                return optimize_print(stmt_ptr, arena);
            }
        case For_kind:
            {
                return optimize_for(stmt_ptr, arena);
            }
        case While_kind:
            {
                return optimize_while(stmt_ptr, arena);
            }
        case If_kind:
            {
                return optimize_if(stmt_ptr, arena);
            }
        case With_kind:
            {
                return optimize_with(stmt_ptr, arena);
            }
        case Raise_kind:
            {
                return optimize_raise(stmt_ptr, arena);
            }
        case TryExcept_kind:
            {
                return optimize_try_except(stmt_ptr, arena);
            }
        case TryFinally_kind:
            {
                return optimize_try_finally(stmt_ptr, arena);
            }
        case Assert_kind:
            {
                return optimize_assert(stmt_ptr, arena);
            }
        case Import_kind:
            {
                return optimize_import(stmt_ptr, arena);
            }
        case ImportFrom_kind:
            {
                return optimize_import_from(stmt_ptr, arena);
            }
        case Exec_kind:
            {
                return optimize_exec(stmt_ptr, arena);
            }
        case Global_kind:
            {
                return optimize_global(stmt_ptr, arena);
            }
        case Expr_kind:
            {
                return optimize_expr(&stmt->v.Expr.value, arena);
            }
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
PyAST_Optimize(mod_ty* mod_ptr, PyArena* arena)
{
    return optimize_mod(mod_ptr, arena);
}

