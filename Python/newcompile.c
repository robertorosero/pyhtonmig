#include "Python.h"

#include "Python-ast.h"
#include "code.h"
#include "compile.h"
#include "symtable.h"
#include "opcode.h"

/* fblockinfo tracks the current frame block.

   A frame block is used to handle loops, try/except, and try/finally.
   It's called a frame block to distinguish it from a basic block in the
   compiler IR.
*/

enum fblocktype { LOOP, EXCEPT, FINALLY_TRY, FINALLY_END };

struct fblockinfo {
        enum fblocktype fb_type;
	int fb_block;
};

struct compiler {
	const char *c_filename;
	struct symtable *c_st;
        PyFutureFeatures *c_future; /* pointer to module's __future__ */
	PyCompilerFlags *c_flags;

	int c_interactive;

	/* info that changes for each code block */
	PySymtableEntryObject *c_symbols;
	int c_nblocks;
	int c_curblock;
	struct basicblock c_entry;
	struct basicblock c_exit;
	struct basicblock **c_blocks;

	int c_nfblocks;
	struct fblockinfo c_fblock[CO_MAXBLOCKS];

	int c_lineno;
};

static int compiler_enter_scope(struct compiler *, identifier, void *);
static int compiler_exit_scope(struct compiler *, identifier, void *);
static void compiler_free(struct compiler *);
static PyCodeObject *compiler_get_code(struct compiler *);
static int compiler_new_block(struct compiler *);
static int compiler_next_instr(struct compiler *, int);
static int compiler_addop(struct compiler *, int);
static int compiler_addop_o(struct compiler *, int, PyObject *);
static int compiler_addop_i(struct compiler *, int, int);
static void compiler_use_block(struct compiler *, int);
static int compiler_use_new_block(struct compiler *);
static int compiler_error(struct compiler *, const char *);

static int compiler_mod(struct compiler *, mod_ty);
static int compiler_visit_stmt(struct compiler *, stmt_ty);
static int compiler_visit_expr(struct compiler *, expr_ty);
static int compiler_visit_arguments(struct compiler *, arguments_ty);
static int compiler_visit_slice(struct compiler *, slice_ty);

static int compiler_push_fblock(struct compiler *, enum fblocktype, int);
static void compiler_pop_fblock(struct compiler *, enum fblocktype, int);

PyCodeObject *
PyAST_Compile(mod_ty mod, const char *filename, PyCompilerFlags *flags)
{
	struct compiler c;

	c.c_filename = filename;
	c.c_future = PyFuture_FromAST(mod, filename);
	if (c.c_future == NULL)
		goto error;
	if (flags) {
		int merged = c.c_future->ff_features | flags->cf_flags;
		c.c_future->ff_features = merged;
		flags->cf_flags = merged;
	}
	
	c.c_st = PySymtable_Build(mod, filename, c.c_future);
	if (c.c_st == NULL)
		goto error;
	return NULL;

	compiler_mod(&c, mod);

 error:
	compiler_free(&c);
	return NULL;
}

static void
compiler_free(struct compiler *c)
{
	int i;

	if (c->c_st)
		PySymtable_Free(c->c_st);
	if (c->c_future)
		PyMem_Free((void *)c->c_future);
	for (i = 0; i < c->c_nblocks; i++) 
		free((void *)c->c_blocks[i]);
	if (c->c_blocks)
		free((void *)c->c_blocks);
}


static int
compiler_enter_scope(struct compiler *c, identifier name, void *key)
{
	/* XXX need stack of info */
	PyObject *k, *v;

	k = PyLong_FromVoidPtr(key);
	if (k == NULL)
		return 0;
	v = PyDict_GetItem(c->c_st->st_symbols, k);
	if (!v) {
		/* XXX */
		PyErr_SetObject(PyExc_KeyError, name);
		return 0;
	}
	assert(PySymtableEntry_Check(v));
	c->c_symbols = (PySymtableEntryObject *)v;

	c->c_nblocks = 0;
	c->c_blocks = (struct basicblock **)malloc(sizeof(struct basicblock *) 
						   * DEFAULT_BLOCKS);
	if (!c->c_blocks)
		return 0;
	return 1;
}

static int
compiler_exit_scope(struct compiler *c, identifier name, void *key)
{
	/* pop current scope off stack & reinit */
	return 1;
}

static PyCodeObject *
compiler_get_code(struct compiler *c)
{
	/* get the code object for the current block.
	   XXX may want to return a thunk insttead
	       to allow later passes
	*/
	return NULL;
}

/* Allocate a new block and return its index in c_blocks. 
   Returns 0 on error.
*/

static int
compiler_new_block(struct compiler *c)
{
	int i;
	struct basicblock *b;

	if (c->c_nblocks && c->c_nblocks % DEFAULT_BLOCKS == 0) {
		/* XXX should double */
		int newsize = c->c_nblocks + DEFAULT_BLOCKS;
		c->c_blocks = (struct basicblock **)realloc(c->c_blocks,
							    newsize);
		if (c->c_blocks == NULL)
			return 0;
	}
	i = c->c_nblocks++;
	b = (struct basicblock *)malloc(sizeof(struct basicblock));
	if (b == NULL)
		return 0;
	memset((void *)b, 0, sizeof(struct basicblock));
	b->b_ialloc = DEFAULT_BLOCK_SIZE;
	c->c_blocks[i] = b;
	return i;
}

static void
compiler_use_block(struct compiler *c, int block)
{
	assert(block < c->c_nblocks);
	c->c_curblock = block;
}

static int
compiler_use_new_block(struct compiler *c)
{
	int block = compiler_new_block(c);
	if (!block)
		return 0;
	c->c_curblock = block;
	return block;
}

/* Returns the offset of the next instruction in the current block's
   b_instr array.  Resizes the b_instr as necessary.
   Returns -1 on failure.
 */

static int
compiler_next_instr(struct compiler *c, int block)
{
	struct basicblock *b;
	assert(block < c->c_nblocks);
	b = c->c_blocks[block];
	if (b->b_iused == b->b_ialloc) {
		void *ptr;
		int newsize;
		b->b_ialloc *= 2;
		/* XXX newsize is wrong */
		ptr = realloc((void *)b, newsize);
		if (ptr == NULL)
			return -1;
		if (ptr != (void *)b)
			c->c_blocks[block] = (struct basicblock *)ptr;
	}
	return b->b_iused++;
}

/* Add an opcode with no argument.
   Returns 0 on failure, 1 on success.
*/

static int
compiler_addop(struct compiler *c, int opcode)
{
	int off;
	off = compiler_next_instr(c, c->c_curblock);
	if (off < 0)
		return 0;
	c->c_blocks[c->c_curblock]->b_instr[off].i_opcode = opcode;
	return 1;
}

/* Add an opcode with a PyObject * argument.
   Returns 0 on failure, 1 on success.
*/

static int
compiler_addop_o(struct compiler *c, int opcode, PyObject *o)
{
	struct instr *i;
	int off;
	off = compiler_next_instr(c, c->c_curblock);
	if (off < 0)
		return 0;
	i = &c->c_blocks[c->c_curblock]->b_instr[off];
	i->i_opcode = i->i_opcode;
	i->i_arg = o;
	return 1;
}

/* Add an opcode with an integer argument.
   Returns 0 on failure, 1 on success.
*/

static int
compiler_addop_i(struct compiler *c, int opcode, int oparg)
{
	struct instr *i;
	int off;
	off = compiler_next_instr(c, c->c_curblock);
	if (off < 0)
		return 0;
	i = &c->c_blocks[c->c_curblock]->b_instr[off];
	i->i_opcode = i->i_opcode;
	i->i_oparg = oparg;
	return 1;
}

#define NEW_BLOCK(C) { \
        if (!compiler_use_new_block((C))) \
	        return 0; \
}

#define ADDOP(C, OP) { \
	if (!compiler_addop((C), (OP))) \
		return 0; \
}

#define ADDOP_O(C, OP, O) { \
	if (!compiler_addop_o((C), (OP), (O))) \
		return 0; \
}

#define ADDOP_I(C, OP, O) { \
	if (!compiler_addop_i((C), (OP), (O))) \
		return 0; \
}

/* VISIT and VISIT_SEQ takes an ASDL type as their second argument.  They use
   the ASDL name to synthesize the name of the C type and the visit function. 
*/

#define VISIT(C, TYPE, V) {\
	if (!compiler_visit_ ## TYPE((C), (V))) \
		return 0; \
}
						    
#define VISIT_SEQ(C, TYPE, SEQ) { \
	int i; \
	asdl_seq *seq = (SEQ); /* avoid variable capture */ \
	for (i = 0; i < asdl_seq_LEN(seq); i++) { \
		TYPE ## _ty elt = asdl_seq_get(seq, i); \
		if (!compiler_visit_ ## TYPE((C), elt)) \
			return 0; \
	} \
}

static int
compiler_mod(struct compiler *c, mod_ty mod)
{
	switch (mod->kind) {
	case Module_kind:
		break;
	case Interactive_kind:
		break;
	case Expression_kind:
		break;
	case Suite_kind:
		break;
	}
	return 1;
}

static int
compiler_function(struct compiler *c, stmt_ty s)
{
	PyCodeObject *co;
	assert(s->kind == FunctionDef_kind);

	if (s->v.FunctionDef.args->defaults)
		VISIT_SEQ(c, expr, s->v.FunctionDef.args->defaults);
	if (!compiler_enter_scope(c, s->v.FunctionDef.name, (void *)s))
		return 0;
	VISIT(c, arguments, s->v.FunctionDef.args);
	VISIT_SEQ(c, stmt, s->v.FunctionDef.body);
	co = compiler_get_code(c);
	if (co == NULL)
		return 0;
		
	return 1;
}

static int
compiler_print(struct compiler *c, stmt_ty s)
{
	int i, n;
	bool dest;

	assert(s->kind == Print_kind);
	n = asdl_seq_LEN(s->v.Print.values);
	dest = false;
	if (s->v.Print.dest) {
		VISIT(c, expr, s->v.Print.dest);
		dest = true;
	}
	for (i = 0; i < n; i++) {
		if (dest) {
			ADDOP(c, DUP_TOP);
			VISIT(c, expr, 
			      (expr_ty)asdl_seq_get(s->v.Print.values, i));
			ADDOP(c, ROT_TWO);
			ADDOP(c, PRINT_ITEM_TO);
		}
		else {
			VISIT(c, expr, 
			      (expr_ty)asdl_seq_get(s->v.Print.values, i));
			ADDOP(c, PRINT_ITEM);
		}
	}
	if (s->v.Print.nl) {
		if (dest)
			ADDOP(c, PRINT_NEWLINE_TO)
		else
			ADDOP(c, PRINT_NEWLINE)
	}
	else if (dest)
		ADDOP(c, POP_TOP);
	return 1;
}

static int
compiler_if(struct compiler *c, stmt_ty s)
{
	int end, next, elif = 1;
	
	assert(s->kind == If_kind);
	end = compiler_new_block(c);
	if (!end)
		return 0;
	while (elif) {
		next = compiler_new_block(c);
		if (!next)
			return 0;
		VISIT(c, expr, s->v.If.test);
		ADDOP_I(c, JUMP_IF_FALSE, next);
		NEW_BLOCK(c);
		ADDOP(c, POP_TOP);
		VISIT_SEQ(c, stmt, s->v.If.body);
		ADDOP_I(c, JUMP_FORWARD, end);
		compiler_use_block(c, next);
		ADDOP(c, POP_TOP);
		if (s->v.If.orelse) {
			stmt_ty t = asdl_seq_get(s->v.If.orelse, 0);
			if (t->kind == If_kind) {
				elif = 1;
				s = t;
				c->c_lineno = t->lineno;
			}
		}
		if (!elif)
			VISIT_SEQ(c, stmt, s->v.If.orelse);
	}
	compiler_use_block(c, end);
	return 1;
}

static int
compiler_for(struct compiler *c, stmt_ty s)
{
	int start, cleanup, end;

	start = compiler_new_block(c);
	cleanup = compiler_new_block(c);
	end = compiler_new_block(c);
	if (!(start && end && cleanup))
		return 0;
	ADDOP_I(c, SETUP_LOOP, end);
	if (!compiler_push_fblock(c, LOOP, start))
		return 0;
	VISIT(c, expr, s->v.For.iter);
	ADDOP(c, GET_ITER);
	compiler_use_block(c, start);
	ADDOP_I(c, FOR_ITER, cleanup);
	VISIT(c, expr, s->v.For.target);
	VISIT_SEQ(c, stmt, s->v.For.body);
	ADDOP_I(c, JUMP_ABSOLUTE, start);
	compiler_use_block(c, cleanup);
	ADDOP(c, POP_BLOCK);
	compiler_pop_fblock(c, LOOP, start);
	VISIT_SEQ(c, stmt, s->v.For.orelse);
	compiler_use_block(c, end);
	return 1;
}

static int
compiler_while(struct compiler *c, stmt_ty s)
{
	int loop, orelse, end;
	loop = compiler_new_block(c);
	end = compiler_new_block(c);
	if (!(loop && end))
		return 0;
	if (s->v.While.orelse) {
		orelse = compiler_new_block(c);
		if (!orelse)
			return 0;
	}
	else
		orelse = -1;

	ADDOP_I(c, SETUP_LOOP, end);
	compiler_use_block(c, loop);
	if (!compiler_push_fblock(c, LOOP, loop))
		return 0;
	VISIT(c, expr, s->v.While.test);
	ADDOP_I(c, JUMP_IF_FALSE, orelse == -1 ? end : orelse);
	NEW_BLOCK(c);
	ADDOP(c, POP_TOP);
	VISIT_SEQ(c, stmt, s->v.While.body);
	ADDOP_I(c, JUMP_ABSOLUTE, loop);

	/* XXX should the two POP instructions be in a separate block
	   if there is no else clause ? 
	*/
	if (orelse == -1)
		compiler_use_block(c, end);
	else
		compiler_use_block(c, orelse);
	ADDOP(c, POP_TOP);
	ADDOP(c, POP_BLOCK);
	compiler_pop_fblock(c, LOOP, loop);
	if (orelse != -1)
		VISIT_SEQ(c, stmt, s->v.While.orelse);
	compiler_use_block(c, end);
	
	return 1;
}

static int
compiler_continue(struct compiler *c)
{
	int i;

	if (!c->c_nfblocks)
		return compiler_error(c, "'continue' outside loop");
	i = c->c_nfblocks - 1;
	switch (c->c_fblock[i].fb_type) {
	case LOOP:
		ADDOP_I(c, JUMP_ABSOLUTE, c->c_fblock[i].fb_block);
		NEW_BLOCK(c);
		break;
	case EXCEPT:
	case FINALLY_TRY:
		while (--i > 0 && c->c_fblock[i].fb_type != LOOP)
			;
		if (i == -1)
			return compiler_error(c, "'continue' outside loop");
		ADDOP_I(c, CONTINUE_LOOP, c->c_fblock[i].fb_block);
		NEW_BLOCK(c);
		break;
	case FINALLY_END:
	        return compiler_error(c,
			"'continue' not allowed in 'finally' block");
		break;
	}
	
	return 1;
}

static int
compiler_visit_stmt(struct compiler *c, stmt_ty s)
{
	int i, n;

	c->c_lineno = s->lineno; 	/* XXX this isn't right */
	switch (s->kind) {
        case FunctionDef_kind:
		return compiler_function(c, s);
		break;
        case ClassDef_kind:
		break;
        case Return_kind:
		if (s->v.Return.value)
			VISIT(c, expr, s->v.Return.value)
		else
			ADDOP_O(c, LOAD_CONST, Py_None);
		ADDOP(c, RETURN_VALUE);
		break;
        case Yield_kind:
		VISIT(c, expr, s->v.Yield.value);
		ADDOP(c, YIELD_VALUE);
		break;
        case Delete_kind:
		VISIT_SEQ(c, expr, s->v.Delete.targets)
		break;
        case Assign_kind:
		n = asdl_seq_LEN(s->v.Assign.targets);
		VISIT(c, expr, s->v.Assign.value);
		for (i = 0; i < n; i++) {
			if (i < n - 1) 
				ADDOP(c, DUP_TOP);
			VISIT(c, expr, 
			      (expr_ty)asdl_seq_get(s->v.Assign.targets, i));
		}
		break;
        case AugAssign_kind:
		break;
        case Print_kind:
		return compiler_print(c, s);
		break;
        case For_kind:
		return compiler_for(c, s);
		break;
        case While_kind:
		return compiler_if(c, s);
		break;
        case If_kind:
		return compiler_if(c, s);
		break;
        case Raise_kind:
		if (s->v.Raise.type) {
			VISIT(c, expr, s->v.Raise.type);
			n++;
			if (s->v.Raise.inst) {
				VISIT(c, expr, s->v.Raise.inst);
				n++;
				if (s->v.Raise.tback) {
					VISIT(c, expr, s->v.Raise.tback);
					n++;
				}
			}
		}
		ADDOP_I(c, RAISE_VARARGS, n);
		break;
        case TryExcept_kind:
		break;
        case TryFinally_kind:
		break;
        case Assert_kind:
		break;
        case Import_kind:
		break;
        case ImportFrom_kind:
		break;
        case Exec_kind:
		VISIT(c, expr, s->v.Exec.body);
		if (s->v.Exec.globals) {
			VISIT(c, expr, s->v.Exec.globals);
			if (s->v.Exec.locals) {
				VISIT(c, expr, s->v.Exec.locals);
			} else {
				ADDOP(c, DUP_TOP);
			}
		} else {
			ADDOP_O(c, LOAD_CONST, Py_None);
			ADDOP(c, DUP_TOP);
		}
		ADDOP(c, EXEC_STMT);
		break;
        case Global_kind:
		break;
        case Expr_kind:
		VISIT(c, expr, s->v.Expr.value);
		if (c->c_interactive) {
			ADDOP(c, PRINT_EXPR);
		}
		else {
			ADDOP(c, DUP_TOP);
		}
		break;
        case Pass_kind:
		break;
        case Break_kind:
		if (!c->c_nfblocks)
			return compiler_error(c, "'break' outside loop");
		ADDOP(c, BREAK_LOOP);
		break;
        case Continue_kind:
		compiler_continue(c);
		break;
	}
	return 1;
}

static int
unaryop(unaryop_ty op)
{
	switch (op) {
	case Invert:
		return UNARY_INVERT;
	case Not:
		return UNARY_NOT;
	case UAdd:
		return UNARY_POSITIVE;
	case USub:
		return UNARY_NEGATIVE;
	}
	return 0;
}

static int 
binop(struct compiler *c, operator_ty op)
{
	switch (op) {
	case Add:
		return BINARY_ADD;
	case Sub:
		return BINARY_SUBTRACT;
	case Mult: 
		return BINARY_MULTIPLY;
	case Div:
		if (c->c_flags->cf_flags & CO_FUTURE_DIVISION)
			return BINARY_TRUE_DIVIDE;
		else
			return BINARY_DIVIDE;
	case Mod:
		return BINARY_MODULO;
	case Pow:
		return BINARY_POWER;
	case LShift: 
		return BINARY_LSHIFT;
	case RShift:
		return BINARY_RSHIFT;
	case BitOr:
		return BINARY_OR;
	case BitXor: 
		return BINARY_XOR;
	case BitAnd:
		return BINARY_AND;
	case FloorDiv:
		return BINARY_FLOOR_DIVIDE;
	}
	return 0;
}
	
static int 
compiler_visit_expr(struct compiler *c, expr_ty e)
{
	int i, n;

	switch (e->kind) {
        case BoolOp_kind:
		break;
        case BinOp_kind:
		VISIT(c, expr, e->v.BinOp.left);
		VISIT(c, expr, e->v.BinOp.right);
		ADDOP(c, binop(c, e->v.BinOp.op));
		break;
        case UnaryOp_kind:
		VISIT(c, expr, e->v.UnaryOp.operand);
		ADDOP(c, unaryop(e->v.UnaryOp.op));
		break;
        case Lambda_kind:
		break;
        case Dict_kind:
		/* XXX get rid of arg? */
		ADDOP_I(c, BUILD_MAP, 0);
		n = asdl_seq_LEN(e->v.Dict.values);
		/* We must arrange things just right for STORE_SUBSCR.
		   It wants the stack to look like (value) (dict) (key) */
		for (i = 0; i < n; i++) {
			ADDOP(c, DUP_TOP);
			VISIT(c, expr, asdl_seq_get(e->v.Dict.values, i));
			ADDOP(c, ROT_TWO);
			VISIT(c, expr, asdl_seq_get(e->v.Dict.keys, i));
			ADDOP(c, STORE_SUBSCR);
		}
		break;
        case ListComp_kind:
		break;
        case Compare_kind:
		break;
        case Call_kind:
		break;
        case Repr_kind:
		VISIT(c, expr, e->v.Repr.value);
		ADDOP(c, UNARY_CONVERT);
		break;
        case Num_kind:
		break;
        case Str_kind:
		break;
	/* The following exprs can be assignment targets. */
        case Attribute_kind:
		VISIT(c, expr, e->v.Attribute.value);
		switch (e->v.Attribute.ctx) {
		case Load:
			ADDOP_O(c, LOAD_ATTR, e->v.Attribute.attr);
			break;
		case Store:
			ADDOP_O(c, STORE_ATTR, e->v.Attribute.attr);
			break;
		case Del:
			ADDOP_O(c, DELETE_ATTR, e->v.Attribute.attr);
			break;
		case AugStore:
			/* XXX */
			break;
		}
		break;
        case Subscript_kind:
		VISIT(c, expr, e->v.Subscript.value);
		VISIT(c, slice, e->v.Subscript.slice);
		break;
        case Name_kind:
		break;
	/* child nodes of List and Tuple will have expr_context set */
        case List_kind:
		VISIT_SEQ(c, expr, e->v.List.elts);
		break;
        case Tuple_kind:
		VISIT_SEQ(c, expr, e->v.Tuple.elts);
		break;
	}
	return 1;
}

static int 
compiler_push_fblock(struct compiler *c, enum fblocktype t, int b)
{
	struct fblockinfo *f;
	if (c->c_nfblocks >= CO_MAXBLOCKS)
		return 0;
	f = &c->c_fblock[c->c_nfblocks++];
	f->fb_type = t;
	f->fb_block = b;
	return 1;
}

static void
compiler_pop_fblock(struct compiler *c, enum fblocktype t, int b)
{
	assert(c->c_nfblocks > 0);
	c->c_nfblocks--;
	assert(c->c_fblock[c->c_nfblocks].fb_type == t);
	assert(c->c_fblock[c->c_nfblocks].fb_block == b);
}

/* Raises a SyntaxError and returns 0.
   If something goes wrong, a different exception may be raised.
*/

static int
compiler_error(struct compiler *c, const char *errstr)
{
	PyObject *loc;
	PyObject *u, *v;

	loc = PyErr_ProgramText(c->c_filename, c->c_lineno);
	if (!loc) {
		Py_INCREF(Py_None);
		loc = Py_None;
	}
	u = Py_BuildValue("(ziOO)", c->c_filename, c->c_lineno, Py_None, loc);
	if (!u)
		goto exit;
	v = Py_BuildValue("(zO)", errstr, u);
	if (!v)
		goto exit;
	PyErr_SetObject(PyExc_SyntaxError, v);
 exit:
	Py_DECREF(loc);
	Py_XDECREF(u);
	Py_XDECREF(v);
	return 0;
}
