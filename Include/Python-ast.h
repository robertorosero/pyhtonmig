/* File automatically generated by ../Parser/asdl_c.py */

#include "asdl.h"

typedef struct _mod *mod_ty;

typedef struct _stmt *stmt_ty;

typedef struct _expr *expr_ty;

typedef enum _expr_context { Load=1, Store=2, Del=3, AugStore=4 }
                             expr_context_ty;

typedef struct _slice *slice_ty;

typedef enum _boolop { And=1, Or=2 } boolop_ty;

typedef enum _operator { Add=1, Sub=2, Mult=3, Div=4, Mod=5, Pow=6,
                         LShift=7, RShift=8, BitOr=9, BitXor=10, BitAnd=11,
                         FloorDiv=12 } operator_ty;

typedef enum _unaryop { Invert=1, Not=2, UAdd=3, USub=4 } unaryop_ty;

typedef enum _cmpop { Eq=1, NotEq=2, Lt=3, LtE=4, Gt=5, GtE=6, Is=7,
                      IsNot=8, In=9, NotIn=10 } cmpop_ty;

typedef struct _listcomp *listcomp_ty;

typedef struct _excepthandler *excepthandler_ty;

typedef struct _arguments *arguments_ty;

typedef struct _keyword *keyword_ty;

typedef struct _alias *alias_ty;

struct _mod {
        enum { Module_kind=1, Interactive_kind=2, Expression_kind=3,
               Suite_kind=4 } kind;
        union {
                struct {
                        asdl_seq *body;
                } Module;
                
                struct {
                        stmt_ty body;
                } Interactive;
                
                struct {
                        expr_ty body;
                } Expression;
                
                struct {
                        asdl_seq *body;
                } Suite;
                
        } v;
};

struct _stmt {
        enum { FunctionDef_kind=1, ClassDef_kind=2, Return_kind=3,
               Yield_kind=4, Delete_kind=5, Assign_kind=6,
               AugAssign_kind=7, Print_kind=8, For_kind=9, While_kind=10,
               If_kind=11, Raise_kind=12, TryExcept_kind=13,
               TryFinally_kind=14, Assert_kind=15, Import_kind=16,
               ImportFrom_kind=17, Exec_kind=18, Global_kind=19,
               Expr_kind=20, Pass_kind=21, Break_kind=22, Continue_kind=23
               } kind;
        union {
                struct {
                        identifier name;
                        arguments_ty args;
                        asdl_seq *body;
                } FunctionDef;
                
                struct {
                        identifier name;
                        asdl_seq *bases;
                        asdl_seq *body;
                } ClassDef;
                
                struct {
                        expr_ty value;
                } Return;
                
                struct {
                        expr_ty value;
                } Yield;
                
                struct {
                        asdl_seq *targets;
                } Delete;
                
                struct {
                        asdl_seq *targets;
                        expr_ty value;
                } Assign;
                
                struct {
                        expr_ty target;
                        operator_ty op;
                        expr_ty value;
                } AugAssign;
                
                struct {
                        expr_ty dest;
                        asdl_seq *value;
                        bool nl;
                } Print;
                
                struct {
                        expr_ty target;
                        expr_ty iter;
                        asdl_seq *body;
                        asdl_seq *orelse;
                } For;
                
                struct {
                        expr_ty test;
                        asdl_seq *body;
                        asdl_seq *orelse;
                } While;
                
                struct {
                        expr_ty test;
                        asdl_seq *body;
                        asdl_seq *orelse;
                } If;
                
                struct {
                        expr_ty type;
                        expr_ty inst;
                        expr_ty tback;
                } Raise;
                
                struct {
                        asdl_seq *body;
                        asdl_seq *handlers;
                        asdl_seq *orelse;
                } TryExcept;
                
                struct {
                        asdl_seq *body;
                        asdl_seq *finalbody;
                } TryFinally;
                
                struct {
                        expr_ty test;
                        expr_ty msg;
                } Assert;
                
                struct {
                        asdl_seq *names;
                } Import;
                
                struct {
                        identifier module;
                        asdl_seq *names;
                } ImportFrom;
                
                struct {
                        expr_ty body;
                        expr_ty globals;
                        expr_ty locals;
                } Exec;
                
                struct {
                        asdl_seq *names;
                } Global;
                
                struct {
                        expr_ty value;
                } Expr;
                
        } v;
        int lineno;
};

struct _expr {
        enum { BoolOp_kind=1, BinOp_kind=2, UnaryOp_kind=3, Lambda_kind=4,
               Dict_kind=5, ListComp_kind=6, Compare_kind=7, Call_kind=8,
               Repr_kind=9, Num_kind=10, Str_kind=11, Attribute_kind=12,
               Subscript_kind=13, Name_kind=14, List_kind=15, Tuple_kind=16
               } kind;
        union {
                struct {
                        boolop_ty op;
                        asdl_seq *values;
                } BoolOp;
                
                struct {
                        expr_ty left;
                        operator_ty op;
                        expr_ty right;
                } BinOp;
                
                struct {
                        unaryop_ty op;
                        expr_ty operand;
                } UnaryOp;
                
                struct {
                        arguments_ty args;
                        expr_ty body;
                } Lambda;
                
                struct {
                        asdl_seq *keys;
                        asdl_seq *values;
                } Dict;
                
                struct {
                        expr_ty target;
                        asdl_seq *generators;
                } ListComp;
                
                struct {
                        expr_ty left;
                        asdl_seq *ops;
                        asdl_seq *comparators;
                } Compare;
                
                struct {
                        expr_ty func;
                        asdl_seq *args;
                        asdl_seq *keywords;
                        expr_ty starargs;
                        expr_ty kwargs;
                } Call;
                
                struct {
                        expr_ty value;
                } Repr;
                
                struct {
                        object n;
                } Num;
                
                struct {
                        string s;
                } Str;
                
                struct {
                        expr_ty value;
                        identifier attr;
                        expr_context_ty ctx;
                } Attribute;
                
                struct {
                        expr_ty value;
                        slice_ty slice;
                        expr_context_ty ctx;
                } Subscript;
                
                struct {
                        identifier id;
                        expr_context_ty ctx;
                } Name;
                
                struct {
                        asdl_seq *elts;
                        expr_context_ty ctx;
                } List;
                
                struct {
                        asdl_seq *elts;
                        expr_context_ty ctx;
                } Tuple;
                
        } v;
};

struct _slice {
        enum { Ellipsis_kind=1, Slice_kind=2, ExtSlice_kind=3, Index_kind=4
               } kind;
        union {
                struct {
                        expr_ty lower;
                        expr_ty upper;
                        expr_ty step;
                } Slice;
                
                struct {
                        asdl_seq *dims;
                } ExtSlice;
                
                struct {
                        expr_ty value;
                } Index;
                
        } v;
};

struct _listcomp {
        expr_ty target;
        expr_ty iter;
        asdl_seq *ifs;
};

struct _excepthandler {
        expr_ty type;
        expr_ty name;
        asdl_seq *body;
};

struct _arguments {
        asdl_seq *args;
        identifier vararg;
        identifier kwarg;
        asdl_seq *defaults;
};

struct _keyword {
        identifier arg;
        expr_ty value;
};

struct _alias {
        identifier name;
        identifier asname;
};

mod_ty Module(asdl_seq * body);
mod_ty Interactive(stmt_ty body);
mod_ty Expression(expr_ty body);
mod_ty Suite(asdl_seq * body);
stmt_ty FunctionDef(identifier name, arguments_ty args, asdl_seq * body,
                    int lineno);
stmt_ty ClassDef(identifier name, asdl_seq * bases, asdl_seq * body, int
                 lineno);
stmt_ty Return(expr_ty value, int lineno);
stmt_ty Yield(expr_ty value, int lineno);
stmt_ty Delete(asdl_seq * targets, int lineno);
stmt_ty Assign(asdl_seq * targets, expr_ty value, int lineno);
stmt_ty AugAssign(expr_ty target, operator_ty op, expr_ty value, int
                  lineno);
stmt_ty Print(expr_ty dest, asdl_seq * value, bool nl, int lineno);
stmt_ty For(expr_ty target, expr_ty iter, asdl_seq * body, asdl_seq *
            orelse, int lineno);
stmt_ty While(expr_ty test, asdl_seq * body, asdl_seq * orelse, int lineno);
stmt_ty If(expr_ty test, asdl_seq * body, asdl_seq * orelse, int lineno);
stmt_ty Raise(expr_ty type, expr_ty inst, expr_ty tback, int lineno);
stmt_ty TryExcept(asdl_seq * body, asdl_seq * handlers, asdl_seq * orelse,
                  int lineno);
stmt_ty TryFinally(asdl_seq * body, asdl_seq * finalbody, int lineno);
stmt_ty Assert(expr_ty test, expr_ty msg, int lineno);
stmt_ty Import(asdl_seq * names, int lineno);
stmt_ty ImportFrom(identifier module, asdl_seq * names, int lineno);
stmt_ty Exec(expr_ty body, expr_ty globals, expr_ty locals, int lineno);
stmt_ty Global(asdl_seq * names, int lineno);
stmt_ty Expr(expr_ty value, int lineno);
stmt_ty Pass(int lineno);
stmt_ty Break(int lineno);
stmt_ty Continue(int lineno);
expr_ty BoolOp(boolop_ty op, asdl_seq * values);
expr_ty BinOp(expr_ty left, operator_ty op, expr_ty right);
expr_ty UnaryOp(unaryop_ty op, expr_ty operand);
expr_ty Lambda(arguments_ty args, expr_ty body);
expr_ty Dict(asdl_seq * keys, asdl_seq * values);
expr_ty ListComp(expr_ty target, asdl_seq * generators);
expr_ty Compare(expr_ty left, asdl_seq * ops, asdl_seq * comparators);
expr_ty Call(expr_ty func, asdl_seq * args, asdl_seq * keywords, expr_ty
             starargs, expr_ty kwargs);
expr_ty Repr(expr_ty value);
expr_ty Num(object n);
expr_ty Str(string s);
expr_ty Attribute(expr_ty value, identifier attr, expr_context_ty ctx);
expr_ty Subscript(expr_ty value, slice_ty slice, expr_context_ty ctx);
expr_ty Name(identifier id, expr_context_ty ctx);
expr_ty List(asdl_seq * elts, expr_context_ty ctx);
expr_ty Tuple(asdl_seq * elts, expr_context_ty ctx);
slice_ty Ellipsis(void);
slice_ty Slice(expr_ty lower, expr_ty upper, expr_ty step);
slice_ty ExtSlice(asdl_seq * dims);
slice_ty Index(expr_ty value);
listcomp_ty listcomp(expr_ty target, expr_ty iter, asdl_seq * ifs);
excepthandler_ty excepthandler(expr_ty type, expr_ty name, asdl_seq * body);
arguments_ty arguments(asdl_seq * args, identifier vararg, identifier
                       kwarg, asdl_seq * defaults);
keyword_ty keyword(identifier arg, expr_ty value);
alias_ty alias(identifier name, identifier asname);
