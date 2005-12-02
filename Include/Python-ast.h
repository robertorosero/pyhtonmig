/* File automatically generated by ./Parser/asdl_c.py */

/* For convenience, this header provides several
   macro, type and constant names which are not Py_-prefixed.
   Therefore, the file should not be included in Python.h;
   all symbols relevant to linkage are Py_-prefixed. */
PyAPI_DATA(PyTypeObject) Py_mod_Type;
#define mod_Check(op) PyObject_TypeCheck(op, &Py_mod_Type)

struct _mod{
        PyObject_HEAD
        enum {Module_kind, Interactive_kind, Expression_kind, Suite_kind} _kind;
};

PyAPI_DATA(PyTypeObject) Py_Module_Type;
#define Module_Check(op) PyObject_TypeCheck(op, &Py_Module_Type)

struct _Module{
        struct _mod _base;
        PyObject* body; /* stmt */
};
PyObject *Py_Module_New(PyObject*);
#define Module Py_Module_New

PyAPI_DATA(PyTypeObject) Py_Interactive_Type;
#define Interactive_Check(op) PyObject_TypeCheck(op, &Py_Interactive_Type)

struct _Interactive{
        struct _mod _base;
        PyObject* body; /* stmt */
};
PyObject *Py_Interactive_New(PyObject*);
#define Interactive Py_Interactive_New

PyAPI_DATA(PyTypeObject) Py_Expression_Type;
#define Expression_Check(op) PyObject_TypeCheck(op, &Py_Expression_Type)

struct _Expression{
        struct _mod _base;
        PyObject* body; /* expr */
};
PyObject *Py_Expression_New(PyObject*);
#define Expression Py_Expression_New

PyAPI_DATA(PyTypeObject) Py_Suite_Type;
#define Suite_Check(op) PyObject_TypeCheck(op, &Py_Suite_Type)

struct _Suite{
        struct _mod _base;
        PyObject* body; /* stmt */
};
PyObject *Py_Suite_New(PyObject*);
#define Suite Py_Suite_New

PyAPI_DATA(PyTypeObject) Py_stmt_Type;
#define stmt_Check(op) PyObject_TypeCheck(op, &Py_stmt_Type)

struct _stmt{
        PyObject_HEAD
        enum {FunctionDef_kind, ClassDef_kind, Return_kind, Delete_kind,
               Assign_kind, AugAssign_kind, Print_kind, For_kind, While_kind,
               If_kind, Raise_kind, TryExcept_kind, TryFinally_kind,
               Assert_kind, Import_kind, ImportFrom_kind, Exec_kind,
               Global_kind, Expr_kind, Pass_kind, Break_kind, Continue_kind}
               _kind;
        int lineno;
};

PyAPI_DATA(PyTypeObject) Py_FunctionDef_Type;
#define FunctionDef_Check(op) PyObject_TypeCheck(op, &Py_FunctionDef_Type)

struct _FunctionDef{
        struct _stmt _base;
        PyObject* name; /* identifier */
        PyObject* args; /* arguments */
        PyObject* body; /* stmt */
        PyObject* decorators; /* expr */
};
PyObject *Py_FunctionDef_New(PyObject*, PyObject*, PyObject*, PyObject*, int);
#define FunctionDef Py_FunctionDef_New

PyAPI_DATA(PyTypeObject) Py_ClassDef_Type;
#define ClassDef_Check(op) PyObject_TypeCheck(op, &Py_ClassDef_Type)

struct _ClassDef{
        struct _stmt _base;
        PyObject* name; /* identifier */
        PyObject* bases; /* expr */
        PyObject* body; /* stmt */
};
PyObject *Py_ClassDef_New(PyObject*, PyObject*, PyObject*, int);
#define ClassDef Py_ClassDef_New

PyAPI_DATA(PyTypeObject) Py_Return_Type;
#define Return_Check(op) PyObject_TypeCheck(op, &Py_Return_Type)

struct _Return{
        struct _stmt _base;
        PyObject* value; /* expr */
};
PyObject *Py_Return_New(PyObject*, int);
#define Return Py_Return_New

PyAPI_DATA(PyTypeObject) Py_Delete_Type;
#define Delete_Check(op) PyObject_TypeCheck(op, &Py_Delete_Type)

struct _Delete{
        struct _stmt _base;
        PyObject* targets; /* expr */
};
PyObject *Py_Delete_New(PyObject*, int);
#define Delete Py_Delete_New

PyAPI_DATA(PyTypeObject) Py_Assign_Type;
#define Assign_Check(op) PyObject_TypeCheck(op, &Py_Assign_Type)

struct _Assign{
        struct _stmt _base;
        PyObject* targets; /* expr */
        PyObject* value; /* expr */
};
PyObject *Py_Assign_New(PyObject*, PyObject*, int);
#define Assign Py_Assign_New

PyAPI_DATA(PyTypeObject) Py_AugAssign_Type;
#define AugAssign_Check(op) PyObject_TypeCheck(op, &Py_AugAssign_Type)

struct _AugAssign{
        struct _stmt _base;
        PyObject* target; /* expr */
        PyObject* op; /* operator */
        PyObject* value; /* expr */
};
PyObject *Py_AugAssign_New(PyObject*, PyObject*, PyObject*, int);
#define AugAssign Py_AugAssign_New

PyAPI_DATA(PyTypeObject) Py_Print_Type;
#define Print_Check(op) PyObject_TypeCheck(op, &Py_Print_Type)

struct _Print{
        struct _stmt _base;
        PyObject* dest; /* expr */
        PyObject* values; /* expr */
        PyObject* nl; /* bool */
};
PyObject *Py_Print_New(PyObject*, PyObject*, PyObject*, int);
#define Print Py_Print_New

PyAPI_DATA(PyTypeObject) Py_For_Type;
#define For_Check(op) PyObject_TypeCheck(op, &Py_For_Type)

struct _For{
        struct _stmt _base;
        PyObject* target; /* expr */
        PyObject* iter; /* expr */
        PyObject* body; /* stmt */
        PyObject* orelse; /* stmt */
};
PyObject *Py_For_New(PyObject*, PyObject*, PyObject*, PyObject*, int);
#define For Py_For_New

PyAPI_DATA(PyTypeObject) Py_While_Type;
#define While_Check(op) PyObject_TypeCheck(op, &Py_While_Type)

struct _While{
        struct _stmt _base;
        PyObject* test; /* expr */
        PyObject* body; /* stmt */
        PyObject* orelse; /* stmt */
};
PyObject *Py_While_New(PyObject*, PyObject*, PyObject*, int);
#define While Py_While_New

PyAPI_DATA(PyTypeObject) Py_If_Type;
#define If_Check(op) PyObject_TypeCheck(op, &Py_If_Type)

struct _If{
        struct _stmt _base;
        PyObject* test; /* expr */
        PyObject* body; /* stmt */
        PyObject* orelse; /* stmt */
};
PyObject *Py_If_New(PyObject*, PyObject*, PyObject*, int);
#define If Py_If_New

PyAPI_DATA(PyTypeObject) Py_Raise_Type;
#define Raise_Check(op) PyObject_TypeCheck(op, &Py_Raise_Type)

struct _Raise{
        struct _stmt _base;
        PyObject* type; /* expr */
        PyObject* inst; /* expr */
        PyObject* tback; /* expr */
};
PyObject *Py_Raise_New(PyObject*, PyObject*, PyObject*, int);
#define Raise Py_Raise_New

PyAPI_DATA(PyTypeObject) Py_TryExcept_Type;
#define TryExcept_Check(op) PyObject_TypeCheck(op, &Py_TryExcept_Type)

struct _TryExcept{
        struct _stmt _base;
        PyObject* body; /* stmt */
        PyObject* handlers; /* excepthandler */
        PyObject* orelse; /* stmt */
};
PyObject *Py_TryExcept_New(PyObject*, PyObject*, PyObject*, int);
#define TryExcept Py_TryExcept_New

PyAPI_DATA(PyTypeObject) Py_TryFinally_Type;
#define TryFinally_Check(op) PyObject_TypeCheck(op, &Py_TryFinally_Type)

struct _TryFinally{
        struct _stmt _base;
        PyObject* body; /* stmt */
        PyObject* finalbody; /* stmt */
};
PyObject *Py_TryFinally_New(PyObject*, PyObject*, int);
#define TryFinally Py_TryFinally_New

PyAPI_DATA(PyTypeObject) Py_Assert_Type;
#define Assert_Check(op) PyObject_TypeCheck(op, &Py_Assert_Type)

struct _Assert{
        struct _stmt _base;
        PyObject* test; /* expr */
        PyObject* msg; /* expr */
};
PyObject *Py_Assert_New(PyObject*, PyObject*, int);
#define Assert Py_Assert_New

PyAPI_DATA(PyTypeObject) Py_Import_Type;
#define Import_Check(op) PyObject_TypeCheck(op, &Py_Import_Type)

struct _Import{
        struct _stmt _base;
        PyObject* names; /* alias */
};
PyObject *Py_Import_New(PyObject*, int);
#define Import Py_Import_New

PyAPI_DATA(PyTypeObject) Py_ImportFrom_Type;
#define ImportFrom_Check(op) PyObject_TypeCheck(op, &Py_ImportFrom_Type)

struct _ImportFrom{
        struct _stmt _base;
        PyObject* module; /* identifier */
        PyObject* names; /* alias */
};
PyObject *Py_ImportFrom_New(PyObject*, PyObject*, int);
#define ImportFrom Py_ImportFrom_New

PyAPI_DATA(PyTypeObject) Py_Exec_Type;
#define Exec_Check(op) PyObject_TypeCheck(op, &Py_Exec_Type)

struct _Exec{
        struct _stmt _base;
        PyObject* body; /* expr */
        PyObject* globals; /* expr */
        PyObject* locals; /* expr */
};
PyObject *Py_Exec_New(PyObject*, PyObject*, PyObject*, int);
#define Exec Py_Exec_New

PyAPI_DATA(PyTypeObject) Py_Global_Type;
#define Global_Check(op) PyObject_TypeCheck(op, &Py_Global_Type)

struct _Global{
        struct _stmt _base;
        PyObject* names; /* identifier */
};
PyObject *Py_Global_New(PyObject*, int);
#define Global Py_Global_New

PyAPI_DATA(PyTypeObject) Py_Expr_Type;
#define Expr_Check(op) PyObject_TypeCheck(op, &Py_Expr_Type)

struct _Expr{
        struct _stmt _base;
        PyObject* value; /* expr */
};
PyObject *Py_Expr_New(PyObject*, int);
#define Expr Py_Expr_New

PyAPI_DATA(PyTypeObject) Py_Pass_Type;
#define Pass_Check(op) PyObject_TypeCheck(op, &Py_Pass_Type)

struct _Pass{
        struct _stmt _base;
};
PyObject *Py_Pass_New(int);
#define Pass Py_Pass_New

PyAPI_DATA(PyTypeObject) Py_Break_Type;
#define Break_Check(op) PyObject_TypeCheck(op, &Py_Break_Type)

struct _Break{
        struct _stmt _base;
};
PyObject *Py_Break_New(int);
#define Break Py_Break_New

PyAPI_DATA(PyTypeObject) Py_Continue_Type;
#define Continue_Check(op) PyObject_TypeCheck(op, &Py_Continue_Type)

struct _Continue{
        struct _stmt _base;
};
PyObject *Py_Continue_New(int);
#define Continue Py_Continue_New

PyAPI_DATA(PyTypeObject) Py_expr_Type;
#define expr_Check(op) PyObject_TypeCheck(op, &Py_expr_Type)

struct _expr{
        PyObject_HEAD
        enum {BoolOp_kind, BinOp_kind, UnaryOp_kind, Lambda_kind, Dict_kind,
               ListComp_kind, GeneratorExp_kind, Yield_kind, Compare_kind,
               Call_kind, Repr_kind, Num_kind, Str_kind, Attribute_kind,
               Subscript_kind, Name_kind, List_kind, Tuple_kind} _kind;
        int lineno;
};

PyAPI_DATA(PyTypeObject) Py_BoolOp_Type;
#define BoolOp_Check(op) PyObject_TypeCheck(op, &Py_BoolOp_Type)

struct _BoolOp{
        struct _expr _base;
        PyObject* op; /* boolop */
        PyObject* values; /* expr */
};
PyObject *Py_BoolOp_New(PyObject*, PyObject*, int);
#define BoolOp Py_BoolOp_New

PyAPI_DATA(PyTypeObject) Py_BinOp_Type;
#define BinOp_Check(op) PyObject_TypeCheck(op, &Py_BinOp_Type)

struct _BinOp{
        struct _expr _base;
        PyObject* left; /* expr */
        PyObject* op; /* operator */
        PyObject* right; /* expr */
};
PyObject *Py_BinOp_New(PyObject*, PyObject*, PyObject*, int);
#define BinOp Py_BinOp_New

PyAPI_DATA(PyTypeObject) Py_UnaryOp_Type;
#define UnaryOp_Check(op) PyObject_TypeCheck(op, &Py_UnaryOp_Type)

struct _UnaryOp{
        struct _expr _base;
        PyObject* op; /* unaryop */
        PyObject* operand; /* expr */
};
PyObject *Py_UnaryOp_New(PyObject*, PyObject*, int);
#define UnaryOp Py_UnaryOp_New

PyAPI_DATA(PyTypeObject) Py_Lambda_Type;
#define Lambda_Check(op) PyObject_TypeCheck(op, &Py_Lambda_Type)

struct _Lambda{
        struct _expr _base;
        PyObject* args; /* arguments */
        PyObject* body; /* expr */
};
PyObject *Py_Lambda_New(PyObject*, PyObject*, int);
#define Lambda Py_Lambda_New

PyAPI_DATA(PyTypeObject) Py_Dict_Type;
#define Dict_Check(op) PyObject_TypeCheck(op, &Py_Dict_Type)

struct _Dict{
        struct _expr _base;
        PyObject* keys; /* expr */
        PyObject* values; /* expr */
};
PyObject *Py_Dict_New(PyObject*, PyObject*, int);
#define Dict Py_Dict_New

PyAPI_DATA(PyTypeObject) Py_ListComp_Type;
#define ListComp_Check(op) PyObject_TypeCheck(op, &Py_ListComp_Type)

struct _ListComp{
        struct _expr _base;
        PyObject* elt; /* expr */
        PyObject* generators; /* comprehension */
};
PyObject *Py_ListComp_New(PyObject*, PyObject*, int);
#define ListComp Py_ListComp_New

PyAPI_DATA(PyTypeObject) Py_GeneratorExp_Type;
#define GeneratorExp_Check(op) PyObject_TypeCheck(op, &Py_GeneratorExp_Type)

struct _GeneratorExp{
        struct _expr _base;
        PyObject* elt; /* expr */
        PyObject* generators; /* comprehension */
};
PyObject *Py_GeneratorExp_New(PyObject*, PyObject*, int);
#define GeneratorExp Py_GeneratorExp_New

PyAPI_DATA(PyTypeObject) Py_Yield_Type;
#define Yield_Check(op) PyObject_TypeCheck(op, &Py_Yield_Type)

struct _Yield{
        struct _expr _base;
        PyObject* value; /* expr */
};
PyObject *Py_Yield_New(PyObject*, int);
#define Yield Py_Yield_New

PyAPI_DATA(PyTypeObject) Py_Compare_Type;
#define Compare_Check(op) PyObject_TypeCheck(op, &Py_Compare_Type)

struct _Compare{
        struct _expr _base;
        PyObject* left; /* expr */
        PyObject* ops; /* cmpop */
        PyObject* comparators; /* expr */
};
PyObject *Py_Compare_New(PyObject*, PyObject*, PyObject*, int);
#define Compare Py_Compare_New

PyAPI_DATA(PyTypeObject) Py_Call_Type;
#define Call_Check(op) PyObject_TypeCheck(op, &Py_Call_Type)

struct _Call{
        struct _expr _base;
        PyObject* func; /* expr */
        PyObject* args; /* expr */
        PyObject* keywords; /* keyword */
        PyObject* starargs; /* expr */
        PyObject* kwargs; /* expr */
};
PyObject *Py_Call_New(PyObject*, PyObject*, PyObject*, PyObject*, PyObject*,
                      int);
#define Call Py_Call_New

PyAPI_DATA(PyTypeObject) Py_Repr_Type;
#define Repr_Check(op) PyObject_TypeCheck(op, &Py_Repr_Type)

struct _Repr{
        struct _expr _base;
        PyObject* value; /* expr */
};
PyObject *Py_Repr_New(PyObject*, int);
#define Repr Py_Repr_New

PyAPI_DATA(PyTypeObject) Py_Num_Type;
#define Num_Check(op) PyObject_TypeCheck(op, &Py_Num_Type)

struct _Num{
        struct _expr _base;
        PyObject* n; /* object */
};
PyObject *Py_Num_New(PyObject*, int);
#define Num Py_Num_New

PyAPI_DATA(PyTypeObject) Py_Str_Type;
#define Str_Check(op) PyObject_TypeCheck(op, &Py_Str_Type)

struct _Str{
        struct _expr _base;
        PyObject* s; /* string */
};
PyObject *Py_Str_New(PyObject*, int);
#define Str Py_Str_New

PyAPI_DATA(PyTypeObject) Py_Attribute_Type;
#define Attribute_Check(op) PyObject_TypeCheck(op, &Py_Attribute_Type)

struct _Attribute{
        struct _expr _base;
        PyObject* value; /* expr */
        PyObject* attr; /* identifier */
        PyObject* ctx; /* expr_context */
};
PyObject *Py_Attribute_New(PyObject*, PyObject*, PyObject*, int);
#define Attribute Py_Attribute_New

PyAPI_DATA(PyTypeObject) Py_Subscript_Type;
#define Subscript_Check(op) PyObject_TypeCheck(op, &Py_Subscript_Type)

struct _Subscript{
        struct _expr _base;
        PyObject* value; /* expr */
        PyObject* slice; /* slice */
        PyObject* ctx; /* expr_context */
};
PyObject *Py_Subscript_New(PyObject*, PyObject*, PyObject*, int);
#define Subscript Py_Subscript_New

PyAPI_DATA(PyTypeObject) Py_Name_Type;
#define Name_Check(op) PyObject_TypeCheck(op, &Py_Name_Type)

struct _Name{
        struct _expr _base;
        PyObject* id; /* identifier */
        PyObject* ctx; /* expr_context */
};
PyObject *Py_Name_New(PyObject*, PyObject*, int);
#define Name Py_Name_New

PyAPI_DATA(PyTypeObject) Py_List_Type;
#define List_Check(op) PyObject_TypeCheck(op, &Py_List_Type)

struct _List{
        struct _expr _base;
        PyObject* elts; /* expr */
        PyObject* ctx; /* expr_context */
};
PyObject *Py_List_New(PyObject*, PyObject*, int);
#define List Py_List_New

PyAPI_DATA(PyTypeObject) Py_Tuple_Type;
#define Tuple_Check(op) PyObject_TypeCheck(op, &Py_Tuple_Type)

struct _Tuple{
        struct _expr _base;
        PyObject* elts; /* expr */
        PyObject* ctx; /* expr_context */
};
PyObject *Py_Tuple_New(PyObject*, PyObject*, int);
#define Tuple Py_Tuple_New

PyAPI_DATA(PyTypeObject) Py_expr_context_Type;
#define expr_context_Check(op) PyObject_TypeCheck(op, &Py_expr_context_Type)

struct _expr_context{
        PyObject_HEAD
        enum {Load_kind, Store_kind, Del_kind, AugLoad_kind, AugStore_kind,
               Param_kind} _kind;
};

PyAPI_DATA(PyTypeObject) Py_Load_Type;
#define Load_Check(op) PyObject_TypeCheck(op, &Py_Load_Type)

struct _Load{
        struct _expr_context _base;
};
PyObject *Py_Load_New(void);
#define Load Py_Load_New

PyAPI_DATA(PyTypeObject) Py_Store_Type;
#define Store_Check(op) PyObject_TypeCheck(op, &Py_Store_Type)

struct _Store{
        struct _expr_context _base;
};
PyObject *Py_Store_New(void);
#define Store Py_Store_New

PyAPI_DATA(PyTypeObject) Py_Del_Type;
#define Del_Check(op) PyObject_TypeCheck(op, &Py_Del_Type)

struct _Del{
        struct _expr_context _base;
};
PyObject *Py_Del_New(void);
#define Del Py_Del_New

PyAPI_DATA(PyTypeObject) Py_AugLoad_Type;
#define AugLoad_Check(op) PyObject_TypeCheck(op, &Py_AugLoad_Type)

struct _AugLoad{
        struct _expr_context _base;
};
PyObject *Py_AugLoad_New(void);
#define AugLoad Py_AugLoad_New

PyAPI_DATA(PyTypeObject) Py_AugStore_Type;
#define AugStore_Check(op) PyObject_TypeCheck(op, &Py_AugStore_Type)

struct _AugStore{
        struct _expr_context _base;
};
PyObject *Py_AugStore_New(void);
#define AugStore Py_AugStore_New

PyAPI_DATA(PyTypeObject) Py_Param_Type;
#define Param_Check(op) PyObject_TypeCheck(op, &Py_Param_Type)

struct _Param{
        struct _expr_context _base;
};
PyObject *Py_Param_New(void);
#define Param Py_Param_New

PyAPI_DATA(PyTypeObject) Py_slice_Type;
#define slice_Check(op) PyObject_TypeCheck(op, &Py_slice_Type)

struct _slice{
        PyObject_HEAD
        enum {Ellipsis_kind, Slice_kind, ExtSlice_kind, Index_kind} _kind;
};

PyAPI_DATA(PyTypeObject) Py_Ellipsis_Type;
#define Ellipsis_Check(op) PyObject_TypeCheck(op, &Py_Ellipsis_Type)

struct _Ellipsis{
        struct _slice _base;
};
PyObject *Py_Ellipsis_New(void);
#define Ellipsis Py_Ellipsis_New

PyAPI_DATA(PyTypeObject) Py_Slice_Type;
#define Slice_Check(op) PyObject_TypeCheck(op, &Py_Slice_Type)

struct _Slice{
        struct _slice _base;
        PyObject* lower; /* expr */
        PyObject* upper; /* expr */
        PyObject* step; /* expr */
};
PyObject *Py_Slice_New(PyObject*, PyObject*, PyObject*);
#define Slice Py_Slice_New

PyAPI_DATA(PyTypeObject) Py_ExtSlice_Type;
#define ExtSlice_Check(op) PyObject_TypeCheck(op, &Py_ExtSlice_Type)

struct _ExtSlice{
        struct _slice _base;
        PyObject* dims; /* slice */
};
PyObject *Py_ExtSlice_New(PyObject*);
#define ExtSlice Py_ExtSlice_New

PyAPI_DATA(PyTypeObject) Py_Index_Type;
#define Index_Check(op) PyObject_TypeCheck(op, &Py_Index_Type)

struct _Index{
        struct _slice _base;
        PyObject* value; /* expr */
};
PyObject *Py_Index_New(PyObject*);
#define Index Py_Index_New

PyAPI_DATA(PyTypeObject) Py_boolop_Type;
#define boolop_Check(op) PyObject_TypeCheck(op, &Py_boolop_Type)

struct _boolop{
        PyObject_HEAD
        enum {And_kind, Or_kind} _kind;
};

PyAPI_DATA(PyTypeObject) Py_And_Type;
#define And_Check(op) PyObject_TypeCheck(op, &Py_And_Type)

struct _And{
        struct _boolop _base;
};
PyObject *Py_And_New(void);
#define And Py_And_New

PyAPI_DATA(PyTypeObject) Py_Or_Type;
#define Or_Check(op) PyObject_TypeCheck(op, &Py_Or_Type)

struct _Or{
        struct _boolop _base;
};
PyObject *Py_Or_New(void);
#define Or Py_Or_New

PyAPI_DATA(PyTypeObject) Py_operator_Type;
#define operator_Check(op) PyObject_TypeCheck(op, &Py_operator_Type)

struct _operator{
        PyObject_HEAD
        enum {Add_kind, Sub_kind, Mult_kind, Div_kind, Mod_kind, Pow_kind,
               LShift_kind, RShift_kind, BitOr_kind, BitXor_kind, BitAnd_kind,
               FloorDiv_kind} _kind;
};

PyAPI_DATA(PyTypeObject) Py_Add_Type;
#define Add_Check(op) PyObject_TypeCheck(op, &Py_Add_Type)

struct _Add{
        struct _operator _base;
};
PyObject *Py_Add_New(void);
#define Add Py_Add_New

PyAPI_DATA(PyTypeObject) Py_Sub_Type;
#define Sub_Check(op) PyObject_TypeCheck(op, &Py_Sub_Type)

struct _Sub{
        struct _operator _base;
};
PyObject *Py_Sub_New(void);
#define Sub Py_Sub_New

PyAPI_DATA(PyTypeObject) Py_Mult_Type;
#define Mult_Check(op) PyObject_TypeCheck(op, &Py_Mult_Type)

struct _Mult{
        struct _operator _base;
};
PyObject *Py_Mult_New(void);
#define Mult Py_Mult_New

PyAPI_DATA(PyTypeObject) Py_Div_Type;
#define Div_Check(op) PyObject_TypeCheck(op, &Py_Div_Type)

struct _Div{
        struct _operator _base;
};
PyObject *Py_Div_New(void);
#define Div Py_Div_New

PyAPI_DATA(PyTypeObject) Py_Mod_Type;
#define Mod_Check(op) PyObject_TypeCheck(op, &Py_Mod_Type)

struct _Mod{
        struct _operator _base;
};
PyObject *Py_Mod_New(void);
#define Mod Py_Mod_New

PyAPI_DATA(PyTypeObject) Py_Pow_Type;
#define Pow_Check(op) PyObject_TypeCheck(op, &Py_Pow_Type)

struct _Pow{
        struct _operator _base;
};
PyObject *Py_Pow_New(void);
#define Pow Py_Pow_New

PyAPI_DATA(PyTypeObject) Py_LShift_Type;
#define LShift_Check(op) PyObject_TypeCheck(op, &Py_LShift_Type)

struct _LShift{
        struct _operator _base;
};
PyObject *Py_LShift_New(void);
#define LShift Py_LShift_New

PyAPI_DATA(PyTypeObject) Py_RShift_Type;
#define RShift_Check(op) PyObject_TypeCheck(op, &Py_RShift_Type)

struct _RShift{
        struct _operator _base;
};
PyObject *Py_RShift_New(void);
#define RShift Py_RShift_New

PyAPI_DATA(PyTypeObject) Py_BitOr_Type;
#define BitOr_Check(op) PyObject_TypeCheck(op, &Py_BitOr_Type)

struct _BitOr{
        struct _operator _base;
};
PyObject *Py_BitOr_New(void);
#define BitOr Py_BitOr_New

PyAPI_DATA(PyTypeObject) Py_BitXor_Type;
#define BitXor_Check(op) PyObject_TypeCheck(op, &Py_BitXor_Type)

struct _BitXor{
        struct _operator _base;
};
PyObject *Py_BitXor_New(void);
#define BitXor Py_BitXor_New

PyAPI_DATA(PyTypeObject) Py_BitAnd_Type;
#define BitAnd_Check(op) PyObject_TypeCheck(op, &Py_BitAnd_Type)

struct _BitAnd{
        struct _operator _base;
};
PyObject *Py_BitAnd_New(void);
#define BitAnd Py_BitAnd_New

PyAPI_DATA(PyTypeObject) Py_FloorDiv_Type;
#define FloorDiv_Check(op) PyObject_TypeCheck(op, &Py_FloorDiv_Type)

struct _FloorDiv{
        struct _operator _base;
};
PyObject *Py_FloorDiv_New(void);
#define FloorDiv Py_FloorDiv_New

PyAPI_DATA(PyTypeObject) Py_unaryop_Type;
#define unaryop_Check(op) PyObject_TypeCheck(op, &Py_unaryop_Type)

struct _unaryop{
        PyObject_HEAD
        enum {Invert_kind, Not_kind, UAdd_kind, USub_kind} _kind;
};

PyAPI_DATA(PyTypeObject) Py_Invert_Type;
#define Invert_Check(op) PyObject_TypeCheck(op, &Py_Invert_Type)

struct _Invert{
        struct _unaryop _base;
};
PyObject *Py_Invert_New(void);
#define Invert Py_Invert_New

PyAPI_DATA(PyTypeObject) Py_Not_Type;
#define Not_Check(op) PyObject_TypeCheck(op, &Py_Not_Type)

struct _Not{
        struct _unaryop _base;
};
PyObject *Py_Not_New(void);
#define Not Py_Not_New

PyAPI_DATA(PyTypeObject) Py_UAdd_Type;
#define UAdd_Check(op) PyObject_TypeCheck(op, &Py_UAdd_Type)

struct _UAdd{
        struct _unaryop _base;
};
PyObject *Py_UAdd_New(void);
#define UAdd Py_UAdd_New

PyAPI_DATA(PyTypeObject) Py_USub_Type;
#define USub_Check(op) PyObject_TypeCheck(op, &Py_USub_Type)

struct _USub{
        struct _unaryop _base;
};
PyObject *Py_USub_New(void);
#define USub Py_USub_New

PyAPI_DATA(PyTypeObject) Py_cmpop_Type;
#define cmpop_Check(op) PyObject_TypeCheck(op, &Py_cmpop_Type)

struct _cmpop{
        PyObject_HEAD
        enum {Eq_kind, NotEq_kind, Lt_kind, LtE_kind, Gt_kind, GtE_kind,
               Is_kind, IsNot_kind, In_kind, NotIn_kind} _kind;
};

PyAPI_DATA(PyTypeObject) Py_Eq_Type;
#define Eq_Check(op) PyObject_TypeCheck(op, &Py_Eq_Type)

struct _Eq{
        struct _cmpop _base;
};
PyObject *Py_Eq_New(void);
#define Eq Py_Eq_New

PyAPI_DATA(PyTypeObject) Py_NotEq_Type;
#define NotEq_Check(op) PyObject_TypeCheck(op, &Py_NotEq_Type)

struct _NotEq{
        struct _cmpop _base;
};
PyObject *Py_NotEq_New(void);
#define NotEq Py_NotEq_New

PyAPI_DATA(PyTypeObject) Py_Lt_Type;
#define Lt_Check(op) PyObject_TypeCheck(op, &Py_Lt_Type)

struct _Lt{
        struct _cmpop _base;
};
PyObject *Py_Lt_New(void);
#define Lt Py_Lt_New

PyAPI_DATA(PyTypeObject) Py_LtE_Type;
#define LtE_Check(op) PyObject_TypeCheck(op, &Py_LtE_Type)

struct _LtE{
        struct _cmpop _base;
};
PyObject *Py_LtE_New(void);
#define LtE Py_LtE_New

PyAPI_DATA(PyTypeObject) Py_Gt_Type;
#define Gt_Check(op) PyObject_TypeCheck(op, &Py_Gt_Type)

struct _Gt{
        struct _cmpop _base;
};
PyObject *Py_Gt_New(void);
#define Gt Py_Gt_New

PyAPI_DATA(PyTypeObject) Py_GtE_Type;
#define GtE_Check(op) PyObject_TypeCheck(op, &Py_GtE_Type)

struct _GtE{
        struct _cmpop _base;
};
PyObject *Py_GtE_New(void);
#define GtE Py_GtE_New

PyAPI_DATA(PyTypeObject) Py_Is_Type;
#define Is_Check(op) PyObject_TypeCheck(op, &Py_Is_Type)

struct _Is{
        struct _cmpop _base;
};
PyObject *Py_Is_New(void);
#define Is Py_Is_New

PyAPI_DATA(PyTypeObject) Py_IsNot_Type;
#define IsNot_Check(op) PyObject_TypeCheck(op, &Py_IsNot_Type)

struct _IsNot{
        struct _cmpop _base;
};
PyObject *Py_IsNot_New(void);
#define IsNot Py_IsNot_New

PyAPI_DATA(PyTypeObject) Py_In_Type;
#define In_Check(op) PyObject_TypeCheck(op, &Py_In_Type)

struct _In{
        struct _cmpop _base;
};
PyObject *Py_In_New(void);
#define In Py_In_New

PyAPI_DATA(PyTypeObject) Py_NotIn_Type;
#define NotIn_Check(op) PyObject_TypeCheck(op, &Py_NotIn_Type)

struct _NotIn{
        struct _cmpop _base;
};
PyObject *Py_NotIn_New(void);
#define NotIn Py_NotIn_New

PyAPI_DATA(PyTypeObject) Py_comprehension_Type;
#define comprehension_Check(op) PyObject_TypeCheck(op, &Py_comprehension_Type)

struct _comprehension {
        PyObject_HEAD
        PyObject* target; /* expr */
        PyObject* iter; /* expr */
        PyObject* ifs; /* expr */
};
PyObject *Py_comprehension_New(PyObject*, PyObject*, PyObject*);

PyAPI_DATA(PyTypeObject) Py_excepthandler_Type;
#define excepthandler_Check(op) PyObject_TypeCheck(op, &Py_excepthandler_Type)

struct _excepthandler {
        PyObject_HEAD
        PyObject* type; /* expr */
        PyObject* name; /* expr */
        PyObject* body; /* stmt */
};
PyObject *Py_excepthandler_New(PyObject*, PyObject*, PyObject*);

PyAPI_DATA(PyTypeObject) Py_arguments_Type;
#define arguments_Check(op) PyObject_TypeCheck(op, &Py_arguments_Type)

struct _arguments {
        PyObject_HEAD
        PyObject* args; /* expr */
        PyObject* vararg; /* identifier */
        PyObject* kwarg; /* identifier */
        PyObject* defaults; /* expr */
};
PyObject *Py_arguments_New(PyObject*, PyObject*, PyObject*, PyObject*);

PyAPI_DATA(PyTypeObject) Py_keyword_Type;
#define keyword_Check(op) PyObject_TypeCheck(op, &Py_keyword_Type)

struct _keyword {
        PyObject_HEAD
        PyObject* arg; /* identifier */
        PyObject* value; /* expr */
};
PyObject *Py_keyword_New(PyObject*, PyObject*);

PyAPI_DATA(PyTypeObject) Py_alias_Type;
#define alias_Check(op) PyObject_TypeCheck(op, &Py_alias_Type)

struct _alias {
        PyObject_HEAD
        PyObject* name; /* identifier */
        PyObject* asname; /* identifier */
};
PyObject *Py_alias_New(PyObject*, PyObject*);

