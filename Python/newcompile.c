#include "Python.h"

#include "Python-ast.h"
#include "code.h"
#include "compile.h"
#include "symtable.h"
#include "opcode.h"

int Py_OptimizeFlag = 0;

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
	PySTEntryObject *c_ste;

	PyObject *c_name;
	PyObject *c_consts;
	PyObject *c_names;
	PyObject *c_varnames;

	int c_argcount;
	int c_nblocks;
	int c_curblock;
	struct basicblock c_entry;
	struct basicblock c_exit;
	struct basicblock **c_blocks;

	int c_nfblocks;
	struct fblockinfo c_fblock[CO_MAXBLOCKS];

	int c_lineno;
};

struct assembler {
	PyObject *a_bytecode;  /* string containing bytecode */
	int a_offset;          /* offset into bytecode */
	int a_nblocks;         /* number of reachable blocks */
	int *a_postorder;      /* list of block indices in dfs postorder */
};

static int compiler_enter_scope(struct compiler *, identifier, void *);
static void compiler_free(struct compiler *);
static int compiler_new_block(struct compiler *);
static int compiler_next_instr(struct compiler *, int);
static int compiler_addop(struct compiler *, int);
static int compiler_addop_o(struct compiler *, int, PyObject *, PyObject *);
static int compiler_addop_i(struct compiler *, int, int);
static int compiler_addop_j(struct compiler *, int, int, int);
static void compiler_use_block(struct compiler *, int);
static int compiler_use_new_block(struct compiler *);
static int compiler_error(struct compiler *, const char *);
static int compiler_nameop(struct compiler *, identifier, expr_context_ty);

static PyCodeObject *compiler_mod(struct compiler *, mod_ty);
static int compiler_visit_stmt(struct compiler *, stmt_ty);
static int compiler_visit_expr(struct compiler *, expr_ty);
static int compiler_visit_slice(struct compiler *, slice_ty);

static int compiler_push_fblock(struct compiler *, enum fblocktype, int);
static void compiler_pop_fblock(struct compiler *, enum fblocktype, int);

static PyCodeObject *assemble(struct compiler *);

#define IS_JUMP(I) ((I)->i_jrel || (I)->i_jabs)

int
_Py_Mangle(char *p, char *name, char *buffer, size_t maxlen)
{
	/* Name mangling: __private becomes _classname__private.
	   This is independent from how the name is used. */
	size_t nlen, plen;
	if (p == NULL || name == NULL || name[0] != '_' || name[1] != '_')
		return 0;
	nlen = strlen(name);
	if (nlen+2 >= maxlen)
		return 0; /* Don't mangle __extremely_long_names */
	if (name[nlen-1] == '_' && name[nlen-2] == '_')
		return 0; /* Don't mangle __whatever__ */
	/* Strip leading underscores from class name */
	while (*p == '_')
		p++;
	if (*p == '\0')
		return 0; /* Don't mangle if class is just underscores */
	plen = strlen(p);
	if (plen + nlen >= maxlen)
		plen = maxlen-nlen-2; /* Truncate class name if too long */
	/* buffer = "_" + p[:plen] + name # i.e. 1+plen+nlen bytes */
	buffer[0] = '_';
	strncpy(buffer+1, p, plen);
	strcpy(buffer+1+plen, name);
	return 1;
}

static int
compiler_init(struct compiler *c)
{
	memset(c, 0, sizeof(struct compiler));

	return 1;
}

PyCodeObject *
PyAST_Compile(mod_ty mod, const char *filename, PyCompilerFlags *flags)
{
	struct compiler c;
	PyCodeObject *co = NULL;

	if (!compiler_init(&c))
		goto error;
	c.c_filename = filename;
	c.c_future = PyFuture_FromAST(mod, filename);
	if (c.c_future == NULL)
		goto error;
	if (flags) {
		int merged = c.c_future->ff_features | flags->cf_flags;
		c.c_future->ff_features = merged;
		flags->cf_flags = merged;
	}

	/* Trivial test of marshal code for now. */
	{
		PyObject *buf = PyString_FromStringAndSize(NULL, 1024);
		int offset = 0;
		assert(marshal_write_mod(&buf, &offset, mod));
		if (!_PyString_Resize(&buf, offset) < 0) {
			fprintf(stderr, "resize failed!\n");
			goto error;
		}
	}

	fprintf(stderr, "ast %s\n", filename);

	c.c_st = PySymtable_Build(mod, filename, c.c_future);
	if (c.c_st == NULL) {
		if (!PyErr_Occurred())
			PyErr_SetString(PyExc_SystemError, "no symtable");
		goto error;
	}

	fprintf(stderr, "symtable %s\n", filename);

	co = compiler_mod(&c, mod);

	fprintf(stderr, "code %s\n", filename);
 error:
	compiler_free(&c);
	return co;
}

static void
compiler_free(struct compiler *c)
{
	int i;

	if (c->c_st)
		PySymtable_Free(c->c_st);
	if (c->c_future)
		PyObject_Free((void *)c->c_future);
	for (i = 0; i < c->c_nblocks; i++) 
		PyObject_Free((void *)c->c_blocks[i]);
	if (c->c_blocks)
		PyObject_Free((void *)c->c_blocks);
	Py_XDECREF(c->c_name);
	Py_XDECREF(c->c_consts);
	Py_XDECREF(c->c_names);
	Py_XDECREF(c->c_varnames);
}

static int
compiler_enter_scope(struct compiler *c, identifier name, void *key)
{
	c->c_ste = PySymtable_Lookup(c->c_st, key);
	if (!c->c_ste) {
		return 0;
	}
	Py_INCREF(name);
	c->c_name = name;
	c->c_varnames = c->c_ste->ste_varnames;
	Py_INCREF(c->c_varnames);
	fprintf(stderr, "block %s varnames %s\n",
		PyObject_REPR(name),
		PyObject_REPR(c->c_varnames));
	c->c_nblocks = 0;
	c->c_blocks = (struct basicblock **)PyObject_Malloc(
	    sizeof(struct basicblock *) * DEFAULT_BLOCKS);
	if (!c->c_blocks)
		return 0;
	memset(c->c_blocks, 0, sizeof(struct basicblock *) * DEFAULT_BLOCKS);
	if (compiler_use_new_block(c) < 0)
		return 0;
	c->c_consts = PyDict_New();
	if (!c->c_consts)
		return 0;
	c->c_names = PyDict_New();
	if (!c->c_names)
		return 0;
	return 1;
}

static int
compiler_exit_scope(struct compiler *c)
{
	Py_DECREF(c->c_name);
	c->c_name = NULL;
	Py_DECREF(c->c_consts);
	c->c_consts = NULL;
	Py_DECREF(c->c_names);
	c->c_names = NULL;
	Py_DECREF(c->c_varnames);
	c->c_varnames = NULL;
	return 1; /* XXX void? */
}

/* Allocate a new block and return its index in c_blocks. 
   Returns -1 on error.
*/

static int
compiler_new_block(struct compiler *c)
{
	struct basicblock *b;
	int block;

	if (c->c_nblocks && c->c_nblocks % DEFAULT_BLOCKS == 0) {
		/* XXX should double */
		int newsize = c->c_nblocks + DEFAULT_BLOCKS;
		c->c_blocks = (struct basicblock **)PyObject_Realloc(
			c->c_blocks, newsize);
		if (c->c_blocks == NULL)
			return -1;
	}
	b = (struct basicblock *)PyObject_Malloc(sizeof(struct basicblock));
	if (b == NULL)
		return -1;
	memset((void *)b, 0, sizeof(struct basicblock));
	b->b_ialloc = DEFAULT_BLOCK_SIZE;
	block = c->c_nblocks++;
	c->c_blocks[block] = b;
	return block;
}

static void
compiler_use_block(struct compiler *c, int block)
{
	assert(block < c->c_nblocks);
	c->c_curblock = block;
	assert(c->c_blocks[block]);
}

static int
compiler_use_new_block(struct compiler *c)
{
	int block = compiler_new_block(c);
	if (block < 0)
		return 0;
	c->c_curblock = block;
	return block;
}

static int
compiler_next_block(struct compiler *c)
{
	int block = compiler_new_block(c);
	if (block < 0)
		return 0;
	c->c_blocks[c->c_curblock]->b_next = block;
	c->c_curblock = block;
	return block;
}

static int
compiler_use_next_block(struct compiler *c, int block)
{
	assert(block < c->c_nblocks);
	c->c_blocks[c->c_curblock]->b_next = block;
	assert(c->c_blocks[block]);
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
	assert(b);
	if (b->b_iused == b->b_ialloc) {
		void *ptr;
		int newsize;
		b->b_ialloc *= 2;
		newsize = sizeof(struct basicblock) 
			+ ((b->b_ialloc - DEFAULT_BLOCK_SIZE) 
			   * sizeof(struct instr));
		ptr = PyObject_Realloc((void *)b, newsize);
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
	struct instr *i;
	int off;
	off = compiler_next_instr(c, c->c_curblock);
	if (off < 0)
		return 0;
	i = &c->c_blocks[c->c_curblock]->b_instr[off];
	i->i_opcode = opcode;
	i->i_hasarg = 0;
	return 1;
}

static int
compiler_addop_o(struct compiler *c, int opcode, PyObject *dict, 
		     PyObject *o)
{
	PyObject *v;
	int arg;

	v = PyDict_GetItem(dict, o);
	if (!v) {
		arg = PyDict_Size(dict);
		v = PyInt_FromLong(arg);
		if (!v)
			return 0;
		if (PyDict_SetItem(dict, o, v) < 0) {
			Py_DECREF(v);
			return 0;
		}
		Py_DECREF(v);
	}
	else 
		arg = PyInt_AsLong(v);
	return compiler_addop_i(c, opcode, arg);
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
	i->i_opcode = opcode;
	i->i_oparg = oparg;
	i->i_hasarg = 1;
	return 1;
}

static int
compiler_addop_j(struct compiler *c, int opcode, int block, int absolute)
{
	struct instr *i;
	int off;
	off = compiler_next_instr(c, c->c_curblock);
	if (off < 0)
		return 0;
	i = &c->c_blocks[c->c_curblock]->b_instr[off];
	i->i_opcode = opcode;
	i->i_oparg = block;
	i->i_hasarg = 1;
	if (absolute)
		i->i_jabs = 1;
	else
		i->i_jrel = 1;
	return 1;
}

/* The distinction between NEW_BLOCK and NEXT_BLOCK is subtle.  (I'd
   like to find better names.)  NEW_BLOCK() creates a new block and sets
   it as the current block.  NEXT_BLOCK() also creates an implicit jump
   from the current block to the new block.
*/
   

#define NEW_BLOCK(C) { \
        if (compiler_use_new_block((C)) < 0) \
	        return 0; \
}

#define NEXT_BLOCK(C) { \
        if (compiler_next_block((C)) < 0) \
	        return 0; \
}

#define ADDOP(C, OP) { \
	if (!compiler_addop((C), (OP))) \
		return 0; \
}

#define ADDOP_O(C, OP, O, TYPE) { \
	if (!compiler_addop_o((C), (OP), (C)->c_ ## TYPE, (O))) \
		return 0; \
}

#define ADDOP_I(C, OP, O) { \
	if (!compiler_addop_i((C), (OP), (O))) \
		return 0; \
}

#define ADDOP_JABS(C, OP, O) { \
	if (!compiler_addop_j((C), (OP), (O), 1)) \
		return 0; \
}

#define ADDOP_JREL(C, OP, O) { \
	if (!compiler_addop_j((C), (OP), (O), 0)) \
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
		TYPE ## _ty elt = asdl_seq_GET(seq, i); \
		if (!compiler_visit_ ## TYPE((C), elt)) \
			return 0; \
	} \
}

static PyCodeObject *
compiler_mod(struct compiler *c, mod_ty mod)
{
	PyCodeObject *co;
	static PyObject *module;
	if (!module) {
		module = PyString_FromString("<module>");
		if (!module)
			return NULL;
	}
	if (!compiler_enter_scope(c, module, mod))
		return NULL;
	switch (mod->kind) {
	case Module_kind:
		VISIT_SEQ(c, stmt, mod->v.Module.body);
		break;
	case Interactive_kind:
		c->c_interactive = 1;
		VISIT_SEQ(c, stmt, mod->v.Interactive.body);
		break;
	case Expression_kind:
		VISIT(c, expr, mod->v.Expression.body);
		break;
	case Suite_kind:
		assert(0);
		VISIT_SEQ(c, stmt, mod->v.Suite.body);
		break;
	}
	co = assemble(c);
	compiler_exit_scope(c);
	return co;
}

static int
compiler_function(struct compiler *c, stmt_ty s)
{
	PyCodeObject *co;
	int ndefs = 0;
	arguments_ty args = s->v.FunctionDef.args;
	assert(s->kind == FunctionDef_kind);

	fprintf(stderr, "function %s\n", 
		PyString_AS_STRING(s->v.FunctionDef.name));

	if (args->defaults)
		VISIT_SEQ(c, expr, args->defaults);
	if (!compiler_enter_scope(c, s->v.FunctionDef.name, (void *)s))
		return 0;
	ndefs += asdl_seq_LEN(args->args);
	c->c_argcount = ndefs;
	if (args->vararg)
		ndefs++;
	if (args->kwarg)
		ndefs++;
	VISIT_SEQ(c, stmt, s->v.FunctionDef.body);
	co = assemble(c);
	if (co == NULL)
		return 0;
	compiler_exit_scope(c);

	/* XXX closure */
	ADDOP_O(c, LOAD_CONST, (PyObject *)co, consts);
	ADDOP_I(c, MAKE_FUNCTION, ndefs);
	if (!compiler_nameop(c, s->v.FunctionDef.name, Store))
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
		expr_ty e = (expr_ty)asdl_seq_GET(s->v.Print.values, i);
		if (dest) {
			ADDOP(c, DUP_TOP);
			VISIT(c, expr, e);
			ADDOP(c, ROT_TWO);
			ADDOP(c, PRINT_ITEM_TO);
		}
		else {
			VISIT(c, expr, e);
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
	if (end < 0)
		return 0;
	while (elif) {
		next = compiler_new_block(c);
		if (next < 0)
			return 0;
		VISIT(c, expr, s->v.If.test);
		ADDOP_JREL(c, JUMP_IF_FALSE, next);
		ADDOP(c, POP_TOP);
		VISIT_SEQ(c, stmt, s->v.If.body);
		ADDOP_JREL(c, JUMP_FORWARD, end);
		compiler_use_block(c, next);
		ADDOP(c, POP_TOP);
		if (s->v.If.orelse) {
			stmt_ty t = asdl_seq_GET(s->v.If.orelse, 0);
			if (t->kind == If_kind) {
				elif = 1;
				s = t;
				c->c_lineno = t->lineno;
			}
		}
		else
			elif = 0;
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
	if (start < 0 || end < 0 || cleanup < 0)
		return 0;
	ADDOP_JREL(c, SETUP_LOOP, end);
	if (!compiler_push_fblock(c, LOOP, start))
		return 0;
	VISIT(c, expr, s->v.For.iter);
	ADDOP(c, GET_ITER);
	compiler_use_block(c, start);
	ADDOP_I(c, FOR_ITER, cleanup);
	VISIT(c, expr, s->v.For.target);
	VISIT_SEQ(c, stmt, s->v.For.body);
	ADDOP_JABS(c, JUMP_ABSOLUTE, start);
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
	if (loop < 0 || end < 0)
		return 0;
	if (s->v.While.orelse) {
		orelse = compiler_new_block(c);
		if (orelse < 0)
			return 0;
	}
	else
		orelse = -1;

	ADDOP_JREL(c, SETUP_LOOP, end);
	compiler_use_next_block(c, loop);
	if (!compiler_push_fblock(c, LOOP, loop))
		return 0;
	VISIT(c, expr, s->v.While.test);
	ADDOP_JREL(c, JUMP_IF_FALSE, orelse == -1 ? end : orelse);
	ADDOP(c, POP_TOP);
	VISIT_SEQ(c, stmt, s->v.While.body);
	ADDOP_JABS(c, JUMP_ABSOLUTE, loop);

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
		ADDOP_JABS(c, JUMP_ABSOLUTE, c->c_fblock[i].fb_block);
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

	fprintf(stderr, "compile stmt %d lineno %d\n",
		s->kind, s->lineno);
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
			ADDOP_O(c, LOAD_CONST, Py_None, consts);
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
			      (expr_ty)asdl_seq_GET(s->v.Assign.targets, i));
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
		return compiler_while(c, s);
		break;
        case If_kind:
		return compiler_if(c, s);
		break;
        case Raise_kind:
		n = 0;
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
			ADDOP_O(c, LOAD_CONST, Py_None, consts);
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
		if (c->c_flags && c->c_flags->cf_flags & CO_FUTURE_DIVISION)
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
compiler_nameop(struct compiler *c, identifier name, expr_context_ty ctx)
{
	int op, scope;
	enum { OP_FAST, OP_GLOBAL, OP_DEREF, OP_NAME } optype;

	/* XXX AugStore isn't used anywhere! */
	op = 0;
	optype = OP_NAME;
	scope = PyST_GetScope(c->c_ste, name);
	switch (scope) {
	case FREE:
	case CELL:
		optype = OP_DEREF;
		break;
	case LOCAL:
		if (c->c_ste->ste_type == FunctionBlock)
			optype = OP_FAST;
		break;
	case GLOBAL_IMPLICIT:
		if (c->c_ste->ste_optimized)
			optype = OP_GLOBAL;
		break;
	case GLOBAL_EXPLICIT:
		optype = OP_GLOBAL;
		break;
	}

	switch (optype) {
	case OP_DEREF:
		switch (ctx) {
		case Load: op = LOAD_DEREF; break;
		case Store: op = STORE_DEREF; break;
		case AugStore:
			break;
		case Del:
			assert(0); /* impossible */
		}
	case OP_FAST:
		switch (ctx) {
		case Load: op = LOAD_FAST; break;
		case Store: op = STORE_FAST; break;
		case Del: op = DELETE_FAST; break;
		case AugStore:
			break;
		}
	case OP_GLOBAL:
		switch (ctx) {
		case Load: op = LOAD_GLOBAL; break;
		case Store: op = STORE_GLOBAL; break;
		case Del: op = DELETE_GLOBAL; break;
		case AugStore:
			break;
		}
	case OP_NAME:
		switch (ctx) {
		case Load: op = LOAD_NAME; break;
		case Store: op = STORE_NAME; break;
		case Del: op = DELETE_NAME; break;
		case AugStore:
			break;
		}
	}
	
	assert(op);
	ADDOP_O(c, op, name, names);
	return 1;
}

static int
compiler_name(struct compiler *c, expr_ty e)
{
	return compiler_nameop(c, e->v.Name.id, e->v.Name.ctx);
}

static int
compiler_boolop(struct compiler *c, expr_ty e)
{
	int end, jumpi, i, n;
	asdl_seq *s;
	
	if (e->v.BoolOp.op == And)
		jumpi = JUMP_IF_FALSE;
	else
		jumpi = JUMP_IF_TRUE;
	end = compiler_new_block(c);
	if (end < 0)
		return 0;
	s = e->v.BoolOp.values;
	n = asdl_seq_LEN(s) - 1;
	for (i = 0; i < n; ++i) {
		VISIT(c, expr, asdl_seq_GET(s, i));
		ADDOP_JREL(c, jumpi, end);
		NEXT_BLOCK(c);
		ADDOP(c, POP_TOP)
	}
	VISIT(c, expr, asdl_seq_GET(s, n));
	compiler_use_block(c, end);
	return 1;
}
	
static int 
compiler_visit_expr(struct compiler *c, expr_ty e)
{
	int i, n;

	switch (e->kind) {
        case BoolOp_kind: 
		return compiler_boolop(c, e);
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
			VISIT(c, expr, asdl_seq_GET(e->v.Dict.values, i));
			ADDOP(c, ROT_TWO);
			VISIT(c, expr, asdl_seq_GET(e->v.Dict.keys, i));
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
		ADDOP_O(c, LOAD_CONST, e->v.Num.n, consts);
		break;
        case Str_kind:
		break;
	/* The following exprs can be assignment targets. */
        case Attribute_kind:
		VISIT(c, expr, e->v.Attribute.value);
		switch (e->v.Attribute.ctx) {
		case Load:
			ADDOP_O(c, LOAD_ATTR, e->v.Attribute.attr, names);
			break;
		case Store:
			ADDOP_O(c, STORE_ATTR, e->v.Attribute.attr, names);
			break;
		case Del:
			ADDOP_O(c, DELETE_ATTR, e->v.Attribute.attr, names);
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
		return compiler_name(c, e);
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
	PyObject *u = NULL, *v = NULL;

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

static int
compiler_visit_slice(struct compiler *c, slice_ty s)
{
	return 1;
}

/* do depth-first search of basic block graph, starting with block.
   post records the block indices in post-order.

   XXX must handle implicit jumps from one block to next
*/

static void
dfs(struct compiler *c, int block, struct assembler *a)
{
	int i;
	struct basicblock *b;
	struct instr *instr = NULL;

	if (block >= c->c_nblocks)
		return;
	b = c->c_blocks[block];
	if (b->b_seen)
		return;
	b->b_seen = 1;
	if (b->b_next)
		dfs(c, b->b_next, a);
	for (i = 0; i < b->b_iused; i++) {
		instr = &b->b_instr[i];
		if (instr->i_jrel || instr->i_jabs)
			dfs(c, instr->i_oparg, a);
	}
	a->a_postorder[a->a_nblocks++] = block;
}

static int
stackdepth(struct compiler *c)
{
	/* XXX need to do this */
	return 100;
}

static int
assemble_init(struct assembler *a, int nblocks)
{
	memset(a, 0, sizeof(struct assembler));
	a->a_bytecode = PyString_FromStringAndSize(NULL, DEFAULT_CODE_SIZE);
	if (!a->a_bytecode)
		return 0;
	a->a_postorder = (int *)PyObject_Malloc(sizeof(int) * nblocks);
	if (!a->a_postorder)
		return 0;
	return 1;
}

static void
assemble_free(struct assembler *a)
{
	Py_XDECREF(a->a_bytecode);
	if (a->a_postorder)
		PyObject_Free(a->a_postorder);
}

/* Return the size of a basic block in bytes. */

static int
instrsize(struct instr *instr)
{
	int size = 1;
	if (instr->i_hasarg) {
		size += 2;
		if (instr->i_oparg >> 16)
			size += 2;
	}
	return size;
}

static int
blocksize(struct basicblock *b)
{
	int i, size = 0;

	for (i = 0; i < b->b_iused; i++)
		size += instrsize(&b->b_instr[i]);
	return size;
}

static int
assemble_emit(struct assembler *a, struct instr *i)
{
	int arg = 0, size = 0, ext = i->i_oparg >> 16;
	int len = PyString_GET_SIZE(a->a_bytecode);
	char *code;
	if (!i->i_hasarg)
		size = 1;
	else {
		if (ext)
			size = 6;
		else
			size = 3;
		arg = i->i_oparg;
	}
	if (a->a_offset + size >= len) {
		if (_PyString_Resize(&a->a_bytecode, len * 2) < 0)
		    return 0;
	}
	code = PyString_AS_STRING(a->a_bytecode) + a->a_offset;
	fprintf(stderr, 
		"emit %3d %5d\toffset = %2d\tsize = %d\text = %d\n",
		i->i_opcode, i->i_oparg, a->a_offset, size, ext);
	a->a_offset += size;
	if (ext > 0) {
	    *code++ = EXTENDED_ARG;
	    *code++ = ext & 0xff;
	    *code++ = ext >> 8;
	    arg &= 0xffff;
	}
	*code++ = i->i_opcode;
	if (size == 1)
		return 1;
	*code++ = arg & 0xff;
	*code++ = arg >> 8;
	return 1;
}

static int
assemble_jump_offsets(struct assembler *a, struct compiler *c)
{
	int *blockoff;
	int bsize, totsize = 0;
	int i, j;

	/* Compute the size of each block and fixup jump args.
	   Replace block index with position in bytecode. */
	blockoff = malloc(sizeof(int) * a->a_nblocks);
	if (!blockoff)
		return 0;
	/* blockoff computed in dfs postorder, but stored using
	   c_blocks[] indices.
	*/
	for (i = a->a_nblocks - 1; i >= 0; i--) {
		int block = a->a_postorder[i];
		bsize = blocksize(c->c_blocks[block]);
		blockoff[block] = totsize;
		totsize += bsize;
	}
	for (i = 0; i < c->c_nblocks; i++) {
		struct basicblock *b = c->c_blocks[i];
		bsize = blockoff[i];
		for (j = 0; j < b->b_iused; j++) {
			struct instr *instr = &b->b_instr[j];
			/* Relative jumps are computed relative to
			   the instruction pointer after fetching
			   the jump instruction.
			*/
			bsize += instrsize(instr);
			if (instr->i_jabs)
				instr->i_oparg = blockoff[instr->i_oparg];
			else if (instr->i_jrel) {
				int delta = blockoff[instr->i_oparg] - bsize;
				instr->i_oparg = delta;
			}
		}
	}
	
	return 1;
}

static PyObject *
dict_keys_inorder(PyObject *dict, int offset)
{
	PyObject *tuple, *k, *v;
	int i, pos = 0, size = PyDict_Size(dict);

	tuple = PyTuple_New(size);
	if (tuple == NULL)
		return NULL;
	while (PyDict_Next(dict, &pos, &k, &v)) {
		i = PyInt_AS_LONG(v);
		Py_INCREF(k);
		assert((i - offset) < size);
		PyTuple_SET_ITEM(tuple, i - offset, k);
	}
	return tuple;
}

static PyCodeObject *
makecode(struct compiler *c, struct assembler *a)
{
	PyCodeObject *co = NULL;
	PyObject *consts = NULL;
	PyObject *names = NULL;
	PyObject *varnames = NULL;
	PyObject *filename = NULL;
	PyObject *name = NULL;
	PyObject *nil = PyTuple_New(0);

	consts = dict_keys_inorder(c->c_consts, 0);
	if (!consts)
		goto error;
	names = dict_keys_inorder(c->c_names, 0);
	if (!names)
		goto error;
	varnames = PySequence_Tuple(c->c_varnames);
	if (!varnames)
		goto error;
	filename = PyString_FromString(c->c_filename);
	if (!filename)
		goto error;
	
	co = PyCode_New(c->c_argcount, 0, stackdepth(c), 0,
			a->a_bytecode, consts, names, varnames,
			nil, nil,
			filename, c->c_name,
			0, 
			filename); /* XXX lnotab */
 error:
	Py_XDECREF(consts);
	Py_XDECREF(names);
	Py_XDECREF(varnames);
	Py_XDECREF(filename);
	Py_XDECREF(name);
	return co;
			
}

static PyCodeObject *
assemble(struct compiler *c)
{
	struct assembler a;
	int i, j;
	PyCodeObject *co = NULL;

	/* Make sure every block that falls off the end returns None. 
	   XXX NEXT_BLOCK() isn't quite right, because if the last
	   block ends with a jump or return b_next shouldn't set.
	 */
	NEXT_BLOCK(c);
	ADDOP_O(c, LOAD_CONST, Py_None, consts);
	ADDOP(c, RETURN_VALUE);

	if (!assemble_init(&a, c->c_nblocks))
		goto error;
	dfs(c, 0, &a);

	/* Can't modify the bytecode after computing jump offsets. */
	if (!assemble_jump_offsets(&a, c))
		goto error;

	/* Emit code in reverse postorder from dfs. */
	for (i = a.a_nblocks - 1; i >= 0; i--) {
		struct basicblock *b = c->c_blocks[a.a_postorder[i]];
		fprintf(stderr, "block %d(%d): used=%d alloc=%d\n",
			i, a.a_postorder[i], b->b_iused, b->b_ialloc);
		for (j = 0; j < b->b_iused; j++) {
			if (!assemble_emit(&a, &b->b_instr[j]))
				goto error;
		}
	}

	if (_PyString_Resize(&a.a_bytecode, a.a_offset) < 0)
		goto error;

	co = makecode(c, &a);
 error:
	assemble_free(&a);
	return co;
}
