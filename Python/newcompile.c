#include "Python.h"

#include "Python-ast.h"
#include "code.h"
#include "compile.h"
#include "symtable.h"
#include "opcode.h"

int Py_OptimizeFlag = 0;

#if 1
#define fprintf if (0) fprintf
#endif

/*
   KNOWN BUGS:

   Seg Faults:
     #: exec generally still has problems
     #: do something about memory management!

   Inappropriate Exceptions:
     #: Get this err msg: XXX rd_object called with exception set
        From Python/marshal.c::PyMarshal_ReadLastObjectFromFile()
        This looks like it may be related to encoding not being implemented.
     #: These don't work right (from test_grammar):
           def f4(two, (compound, (argument, list))): pass
           def v3(a, (b, c), *rest): return a, b, c, rest

   Invalid behaviour:
     #: Source encoding (via encoding_decl) is ignored.  decode_unicode()
        seems to be running into a memory management bug.
     #: Ellipsis isn't handled properly
     #: co_names doesn't contain locals, only globals, co_varnames may work
     #: ref leaks in interpreter when press return on empty line
     #: line numbers are off a bit (may just need to add calls to set lineno)
        In some cases, the line numbers for generated code aren't strictly
        increasing.  This breaks the lnotab.
     #: Modules/parsermodule.c:496: warning: implicit declaration 
                                    of function `PyParser_SimpleParseString'
     #: compile.h::b_return is only set, never used

    ISSUES:

     Tim mentioned that it's more common for a basic blocks representation
     to use real pointers for jump targets rather than indexes into an
     array of basic blocks.

     opcode_stack_effect() function should be reviewed since stack depth bugs
     could be really hard to find later.

     Dead code is being generated (i.e. after unconditional jumps).
    
*/

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

#define DEFAULT_BLOCK_SIZE 16
#define DEFAULT_BLOCKS 8
#define DEFAULT_CODE_SIZE 128
#define DEFAULT_LNOTAB_SIZE 16

struct instr {
	int i_jabs : 1;
	int i_jrel : 1;
	int i_hasarg : 1;
	unsigned char i_opcode;
	int i_oparg;
	int i_target; /* target block index (if jump instruction) */
	int i_lineno;
};

struct basicblock {
	int b_iused;
	int b_ialloc;
	/* If b_next is non-zero, it is the block id of the next
	   block reached by normal control flow.
	   Since a valid b_next must always be > 0,
	   0 can be reserved to mean no next block. */
	int b_next;
	/* b_seen is used to perform a DFS of basicblocks. */
	int b_seen : 1;
	/* b_return is true if a RETURN_VALUE opcode is inserted. */
	int b_return : 1;
	int b_startdepth; /* depth of stack upon entry of block */
	struct instr b_instr[DEFAULT_BLOCK_SIZE];
};

/* The following items change on entry and exit of code blocks.
   They must be saved and restored when returning to a block.
*/
struct compiler_unit {
	PySTEntryObject *u_ste;

	PyObject *u_name;
	/* The following fields are dicts that map objects to
	   the index of them in co_XXX.  The index is used as
	   the argument for opcodes that refer to those collections.
	*/
	PyObject *u_consts;    /* all constants */
	PyObject *u_names;     /* all names */
	PyObject *u_varnames;  /* local variables */
	PyObject *u_cellvars;  /* cell variables */
	PyObject *u_freevars;  /* free variables */

	PyObject *u_private;	/* for private name mangling */

	int u_argcount;    /* number of arguments for block */ 
	int u_nblocks;     /* number of used blocks in u_blocks
			      u_nblocks < u_nalloc */
	int u_nalloc;      /* current alloc count for u_blocks */
	int u_curblock;    /* index of current block in u_blocks */
	int u_tmpname;     /* temporary variables for list comps */
	struct basicblock **u_blocks;

	int u_nfblocks;
	struct fblockinfo u_fblock[CO_MAXBLOCKS];

	int u_firstlineno; /* the first lineno of the block */
	int u_lineno;      /* the lineno for the current stmt */
	bool u_lineno_set; /* boolean to indicate whether instr
			      has been generated with current lineno */
};

struct compiler {
	const char *c_filename;
	struct symtable *c_st;
        PyFutureFeatures *c_future; /* pointer to module's __future__ */
	PyCompilerFlags *c_flags;

	int c_interactive;
        int c_nestlevel;

	struct compiler_unit *u;
	PyObject *c_stack;
	char *c_encoding;	/* source encoding (a borrowed reference) */
};

struct assembler {
	PyObject *a_bytecode;  /* string containing bytecode */
	int a_offset;          /* offset into bytecode */
	int a_nblocks;         /* number of reachable blocks */
	int *a_postorder;      /* list of block indices in dfs postorder */
	PyObject *a_lnotab;    /* string containing lnotab */
	int a_lnotab_off;      /* offset into lnotab */
	int a_lineno;          /* last lineno of emitted instruction */
	int a_lineno_off;      /* bytecode offset of last lineno */
};

static int compiler_enter_scope(struct compiler *, identifier, void *, int);
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
static int compiler_visit_keyword(struct compiler *, keyword_ty);
static int compiler_visit_expr(struct compiler *, expr_ty);
static int compiler_augassign(struct compiler *, stmt_ty);
static int compiler_visit_slice(struct compiler *, slice_ty,
				expr_context_ty);

static int compiler_push_fblock(struct compiler *, enum fblocktype, int);
static void compiler_pop_fblock(struct compiler *, enum fblocktype, int);

static int inplace_binop(struct compiler *, operator_ty);
static int expr_constant(expr_ty e);

static PyCodeObject *assemble(struct compiler *, int addNone);

static char *opnames[];
static PyObject *__doc__;

PyObject *
_Py_Mangle(PyObject *private, PyObject *ident)
{
	/* Name mangling: __private becomes _classname__private.
	   This is independent from how the name is used. */
        const char *p, *name = PyString_AsString(ident);
        char *buffer;
	size_t nlen, plen;
	if (private == NULL || name == NULL || name[0] != '_' || name[1] != '_') {
                Py_INCREF(ident);
		return ident;
        }
        p = PyString_AsString(private);
	nlen = strlen(name);
	if (name[nlen-1] == '_' && name[nlen-2] == '_') {
                Py_INCREF(ident);
		return ident; /* Don't mangle __whatever__ */
        }
	/* Strip leading underscores from class name */
	while (*p == '_')
		p++;
	if (*p == '\0') {
                Py_INCREF(ident);
		return ident; /* Don't mangle if class is just underscores */
        }
	plen = strlen(p);
        ident = PyString_FromStringAndSize(NULL, 1 + nlen + plen);
        if (!ident)
            return 0;
	/* ident = "_" + p[:plen] + name # i.e. 1+plen+nlen bytes */
        buffer = PyString_AS_STRING(ident);
        buffer[0] = '_';
	strncpy(buffer+1, p, plen);
	strcpy(buffer+1+plen, name);
	return ident;
}

static int
compiler_init(struct compiler *c)
{
	memset(c, 0, sizeof(struct compiler));

	c->c_stack = PyList_New(0);
	if (!c->c_stack)
		return 0;

	return 1;
}

PyCodeObject *
PyAST_Compile(mod_ty mod, const char *filename, PyCompilerFlags *flags)
{
	struct compiler c;
	PyCodeObject *co = NULL;
        PyCompilerFlags local_flags;
        int merged;

        if (!__doc__) {
            __doc__ = PyString_InternFromString("__doc__");
            if (!__doc__)
                goto error;
        }

	if (!compiler_init(&c))
		goto error;
	c.c_filename = filename;
	c.c_future = PyFuture_FromAST(mod, filename);
	if (c.c_future == NULL)
		goto error;
	if (!flags) {
            local_flags.cf_flags = 0;
            flags = &local_flags;
        }
        merged = c.c_future->ff_features | flags->cf_flags;
        c.c_future->ff_features = merged;
        flags->cf_flags = merged;
        c.c_flags = flags;
        c.c_nestlevel = 0;

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

	/* XXX initialize to NULL for now, need to handle */
	c.c_encoding = NULL;

	co = compiler_mod(&c, mod);

	fprintf(stderr, "code %s\n", filename);
 error:
	compiler_free(&c);
	return co;
}

static void
compiler_free(struct compiler *c)
{
	if (c->c_st)
		PySymtable_Free(c->c_st);
	if (c->c_future)
		PyObject_Free((void *)c->c_future);
	Py_DECREF(c->c_stack);
}

static PyObject *
list2dict(PyObject *list)
{
	int i, n;
	PyObject *v, *k, *dict = PyDict_New();

	n = PyList_Size(list);
	for (i = 0; i < n; i++) {
		v = PyInt_FromLong(i);
		if (!v) {
			Py_DECREF(dict);
			return NULL;
		}
                k = PyList_GET_ITEM(list, i);
                k = Py_BuildValue("(OO)", k, k->ob_type);
		if (k == NULL || PyDict_SetItem(dict, k, v) < 0) {
			Py_XDECREF(k);
			Py_DECREF(v);
			Py_DECREF(dict);
			return NULL;
		}
		Py_DECREF(v);
	}
	return dict;
}

/* Return new dict containing names from src that match scope(s).

   src is a symbol table dictionary.  If the scope of a name matches
   either scope_type or flag is set, insert it into the new dict.  The
   values are integers, starting at offset and increasing by one for
   each key.
*/

static PyObject *
dictbytype(PyObject *src, int scope_type, int flag, int offset)
{
	int pos = 0, i = offset, scope;
	PyObject *k, *v, *dest = PyDict_New();

        assert(offset >= 0);
        if (dest == NULL)
            return NULL;

	while (PyDict_Next(src, &pos, &k, &v)) {
            /* XXX this should probably be a macro in symtable.h */
            assert(PyInt_Check(v));
            scope = (PyInt_AS_LONG(v) >> SCOPE_OFF) & SCOPE_MASK;

            if (scope == scope_type || PyInt_AS_LONG(v) & flag) {
                PyObject *tuple, *item = PyInt_FromLong(i);
                if (item == NULL) {
			Py_DECREF(dest);
			return NULL;
		}
                i++;
                tuple = Py_BuildValue("(OO)", k, k->ob_type);
		if (!tuple || PyDict_SetItem(dest, tuple, item) < 0) {
			Py_DECREF(item);
			Py_DECREF(dest);
			Py_XDECREF(tuple);
			return NULL;
		}
		Py_DECREF(item);
		Py_DECREF(tuple);
            }
	}
	return dest;
}

static void
compiler_display_symbols(PyObject *name, PyObject *symbols)
{
	PyObject *key, *value;
	int flags, pos = 0;

	fprintf(stderr, "block %s\n", PyString_AS_STRING(name));
	while (PyDict_Next(symbols, &pos, &key, &value)) {
		flags = PyInt_AsLong(value);
		fprintf(stderr, "var %s:", PyString_AS_STRING(key));
		if (flags & DEF_GLOBAL)
			fprintf(stderr, " declared_global");
		if (flags & DEF_LOCAL)
			fprintf(stderr, " local");
		if (flags & DEF_PARAM)
			fprintf(stderr, " param");
		if (flags & DEF_STAR)
			fprintf(stderr, " stararg");
		if (flags & DEF_DOUBLESTAR)
			fprintf(stderr, " starstar");
		if (flags & DEF_INTUPLE)
			fprintf(stderr, " tuple");
		if (flags & DEF_FREE)
			fprintf(stderr, " free");
		if (flags & DEF_FREE_GLOBAL)
			fprintf(stderr, " global");
		if (flags & DEF_FREE_CLASS)
			fprintf(stderr, " free/class");
		if (flags & DEF_IMPORT)
			fprintf(stderr, " import");
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "\n");
}

static int
compiler_enter_scope(struct compiler *c, identifier name, void *key,
		     int lineno)
{
	struct compiler_unit *u;

	u = PyObject_Malloc(sizeof(struct compiler_unit));
	u->u_argcount = 0;
	u->u_ste = PySymtable_Lookup(c->c_st, key);
	if (!u->u_ste) {
		return 0;
	}
	Py_INCREF(name);
	u->u_name = name;
	u->u_varnames = list2dict(u->u_ste->ste_varnames);
	u->u_cellvars = dictbytype(u->u_ste->ste_symbols, CELL, 0, 0);
	u->u_freevars = dictbytype(u->u_ste->ste_symbols, FREE, DEF_FREE_CLASS,
                                   PyDict_Size(u->u_cellvars));

	u->u_nblocks = 0;
	u->u_nalloc = DEFAULT_BLOCKS;
	u->u_blocks = (struct basicblock **)PyObject_Malloc(
		sizeof(struct basicblock *) * DEFAULT_BLOCKS);
	if (!u->u_blocks)
		return 0;
	u->u_tmpname = 0;
	u->u_nfblocks = 0;
	u->u_firstlineno = lineno;
	u->u_lineno = 0;
	u->u_lineno_set = false;
	memset(u->u_blocks, 0, sizeof(struct basicblock *) * DEFAULT_BLOCKS);
	u->u_consts = PyDict_New();
	if (!u->u_consts)
		return 0;
	u->u_names = PyDict_New();
	if (!u->u_names)
		return 0;

        u->u_private = NULL;

	/* A little debugging output */
	compiler_display_symbols(name, u->u_ste->ste_symbols);

	/* Push the old compiler_unit on the stack. */
	if (c->u) {
		PyObject *wrapper = PyCObject_FromVoidPtr(c->u, NULL);
		if (PyList_Append(c->c_stack, wrapper) < 0)
			return 0;
		Py_DECREF(wrapper);
		fprintf(stderr, "stack = %s\n", PyObject_REPR(c->c_stack));
                u->u_private = c->u->u_private;
                Py_XINCREF(u->u_private);
	}
	c->u = u;

        c->c_nestlevel++;
	if (compiler_use_new_block(c) < 0)
		return 0;

	return 1;
}

static void
compiler_unit_check(struct compiler_unit *u)
{
	int i;
	assert(u->u_curblock < u->u_nblocks);
	assert(u->u_nblocks <= u->u_nalloc);
	for (i = 0; i < u->u_nblocks; i++) {
		struct basicblock *block = u->u_blocks[i];
		assert(block);
		assert(block != (void *)0xcbcbcbcb);
		assert(block != (void *)0xfbfbfbfb);
		assert(block->b_ialloc > 0);
		assert(block->b_iused >= 0 
		       && block->b_ialloc >= block->b_iused);
	}
}

static void
compiler_unit_free(struct compiler_unit *u)
{
	int i;

	compiler_unit_check(u);
	for (i = 0; i < u->u_nblocks; i++)
		PyObject_Free((void *)u->u_blocks[i]);
	if (u->u_blocks)
		PyObject_Free((void *)u->u_blocks);
	Py_XDECREF(u->u_ste);
	Py_XDECREF(u->u_name);
	Py_XDECREF(u->u_consts);
	Py_XDECREF(u->u_names);
	Py_XDECREF(u->u_varnames);
	Py_XDECREF(u->u_freevars);
	Py_XDECREF(u->u_cellvars);
	Py_XDECREF(u->u_private);
	PyObject_Free(u);
}

static int
compiler_exit_scope(struct compiler *c)
{
	int n;
	PyObject *wrapper;

        c->c_nestlevel--;
	compiler_unit_free(c->u);
	/* Restore c->u to the parent unit. */
	n = PyList_GET_SIZE(c->c_stack) - 1;
	if (n >= 0) {
		wrapper = PyList_GET_ITEM(c->c_stack, n);
		c->u = (struct compiler_unit *)PyCObject_AsVoidPtr(wrapper);
		if (PySequence_DelItem(c->c_stack, n) < 0)
			return 0;
		compiler_unit_check(c->u);
	}
	else
		c->u = NULL;

	return 1; /* XXX void? */
}

/* Allocate a new block and return its index in c_blocks.
   Returns -1 on error.
*/

static int
compiler_new_block(struct compiler *c)
{
	struct basicblock *b;
	struct compiler_unit *u;
	int block;

	u = c->u;
	if (u->u_nblocks == u->u_nalloc) {
		int newsize = ((u->u_nalloc + u->u_nalloc)
			       * sizeof(struct basicblock *));
		u->u_blocks = (struct basicblock **)PyObject_Realloc(
			u->u_blocks, newsize);
		if (u->u_blocks == NULL)
			return -1;
		memset(u->u_blocks + u->u_nalloc, 0,
		       sizeof(struct basicblock *) * u->u_nalloc);
		u->u_nalloc += u->u_nalloc;
	}
	b = (struct basicblock *)PyObject_Malloc(sizeof(struct basicblock));
	if (b == NULL)
		return -1;
	memset((void *)b, 0, sizeof(struct basicblock));
	b->b_ialloc = DEFAULT_BLOCK_SIZE;
	block = u->u_nblocks++;
	assert(u->u_blocks[block] == NULL);
	u->u_blocks[block] = b;
	return block;
}

static void
compiler_use_block(struct compiler *c, int block)
{
	assert(block < c->u->u_nblocks);
	c->u->u_curblock = block;
	assert(c->u->u_blocks[block]);
}

static int
compiler_use_new_block(struct compiler *c)
{
	int block = compiler_new_block(c);
	if (block < 0)
		return 0;
	c->u->u_curblock = block;
	return block;
}

static int
compiler_next_block(struct compiler *c)
{
	int block = compiler_new_block(c);
	if (block < 0)
		return 0;
	c->u->u_blocks[c->u->u_curblock]->b_next = block;
	c->u->u_curblock = block;
	return block;
}

static int
compiler_use_next_block(struct compiler *c, int block)
{
	assert(block < c->u->u_nblocks);
	c->u->u_blocks[c->u->u_curblock]->b_next = block;
	assert(c->u->u_blocks[block]);
	c->u->u_curblock = block;
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
	assert(block < c->u->u_nblocks);
	b = c->u->u_blocks[block];
	assert(b);
	if (b->b_iused == b->b_ialloc) {
		void *ptr;
		int oldsize, newsize;
		oldsize = sizeof(struct basicblock);
		if (b->b_ialloc > DEFAULT_BLOCK_SIZE)
			oldsize += ((b->b_ialloc - DEFAULT_BLOCK_SIZE)
				    * sizeof(struct instr));
		newsize = oldsize + b->b_ialloc * sizeof(struct instr);
		if (newsize <= 0) {
			PyErr_NoMemory();
			return -1;
		}
		b->b_ialloc <<= 1;
		ptr = PyObject_Realloc((void *)b, newsize);
		if (ptr == NULL)
			return -1;
		memset((char *)ptr + oldsize, 0, newsize - oldsize);
		if (ptr != (void *)b) {
			fprintf(stderr, "resize block %d\n", block);
			c->u->u_blocks[block] = (struct basicblock *)ptr;
			b = ptr;
		}
	}
	return b->b_iused++;
}

/* Add an opcode with no argument.
   Returns 0 on failure, 1 on success.
*/

static void
compiler_set_lineno(struct compiler *c, int off)
{
	struct basicblock *b;
	if (c->u->u_lineno_set)
		return;
	c->u->u_lineno_set = true;
	b = c->u->u_blocks[c->u->u_curblock];
 	b->b_instr[off].i_lineno = c->u->u_lineno;
}

static int
opcode_stack_effect(int opcode, int oparg)
{
	switch (opcode) {
		case POP_TOP:
			return -1;
		case ROT_TWO:
		case ROT_THREE:
			return 0;
		case DUP_TOP:
			return 1;
		case ROT_FOUR:
			return 0;

		case UNARY_POSITIVE:
		case UNARY_NEGATIVE:
		case UNARY_NOT:
		case UNARY_CONVERT:
		case UNARY_INVERT:
			return 0;

		case BINARY_POWER:
		case BINARY_MULTIPLY:
		case BINARY_DIVIDE:
		case BINARY_MODULO:
		case BINARY_ADD:
		case BINARY_SUBTRACT:
		case BINARY_SUBSCR:
		case BINARY_FLOOR_DIVIDE:
		case BINARY_TRUE_DIVIDE:
			return -1;
		case INPLACE_FLOOR_DIVIDE:
		case INPLACE_TRUE_DIVIDE:
			return -1;

		case SLICE+0:
			return 1;
		case SLICE+1:
			return 0;
		case SLICE+2:
			return 0;
		case SLICE+3:
			return -1;

		case STORE_SLICE+0:
			return -2;
		case STORE_SLICE+1:
			return -3;
		case STORE_SLICE+2:
			return -3;
		case STORE_SLICE+3:
			return -4;

		case DELETE_SLICE+0:
			return -1;
		case DELETE_SLICE+1:
			return -2;
		case DELETE_SLICE+2:
			return -2;
		case DELETE_SLICE+3:
			return -3;

		case INPLACE_ADD:
		case INPLACE_SUBTRACT:
		case INPLACE_MULTIPLY:
		case INPLACE_DIVIDE:
		case INPLACE_MODULO:
			return -1;
		case STORE_SUBSCR:
			return -3;
		case DELETE_SUBSCR:
			return -2;

		case BINARY_LSHIFT:
		case BINARY_RSHIFT:
		case BINARY_AND:
		case BINARY_XOR:
		case BINARY_OR:
			return -1;
		case INPLACE_POWER:
			return -1;
		case GET_ITER:
			return 0;

		case PRINT_EXPR:
			return -1;
		case PRINT_ITEM:
			return -1;
		case PRINT_NEWLINE:
			return 0;
		case PRINT_ITEM_TO:
			return -2;
		case PRINT_NEWLINE_TO:
			return -1;
		case INPLACE_LSHIFT:
		case INPLACE_RSHIFT:
		case INPLACE_AND:
		case INPLACE_XOR:
		case INPLACE_OR:
			return -1;
		case BREAK_LOOP:
			return 0;

		case LOAD_LOCALS:
			return 1;
		case RETURN_VALUE:
			return -1;
		case IMPORT_STAR:
			return -1;
		case EXEC_STMT:
			return -3;
		case YIELD_VALUE:
			return -1;

		case POP_BLOCK:
			return 0;
		case END_FINALLY:
			return -1; /* or -2 or -3 if exception occurred */
		case BUILD_CLASS:
			return -2;

		case STORE_NAME:
			return -1;
		case DELETE_NAME:
			return 0;
		case UNPACK_SEQUENCE:
			return oparg-1;
		case FOR_ITER:
			return 1;

		case STORE_ATTR:
			return -2;
		case DELETE_ATTR:
			return -1;
		case STORE_GLOBAL:
			return -1;
		case DELETE_GLOBAL:
			return 0;
		case DUP_TOPX:
			return oparg;
		case LOAD_CONST:
			return 1;
		case LOAD_NAME:
			return 1;
		case BUILD_TUPLE:
		case BUILD_LIST:
			return 1-oparg;
		case BUILD_MAP:
			return 1;
		case LOAD_ATTR:
			return 0;
		case COMPARE_OP:
			return -1;
		case IMPORT_NAME:
			return 0;
		case IMPORT_FROM:
			return 1;

		case JUMP_FORWARD:
		case JUMP_IF_FALSE:
		case JUMP_IF_TRUE:
		case JUMP_ABSOLUTE:
			return 0;

		case LOAD_GLOBAL:
			return 1;

		case CONTINUE_LOOP:
			return 0;
		case SETUP_LOOP:
			return 0;
		case SETUP_EXCEPT:
		case SETUP_FINALLY:
			return 3; /* actually pushed by an exception */

		case LOAD_FAST:
			return 1;
		case STORE_FAST:
			return -1;
		case DELETE_FAST:
			return 0;

		case RAISE_VARARGS:
			return -oparg;
#define NARGS(o) (((o) % 256) + 2*((o) / 256))
		case CALL_FUNCTION:
			return -NARGS(oparg);
		case CALL_FUNCTION_VAR:
		case CALL_FUNCTION_KW:
			return -NARGS(oparg)-1;
		case CALL_FUNCTION_VAR_KW:
			return -NARGS(oparg)-2;
#undef NARGS
		case MAKE_FUNCTION:
			return -oparg;
		case BUILD_SLICE:
			if (oparg == 3)
				return -2;
			else
				return -1;

		case MAKE_CLOSURE:
			return -oparg;
		case LOAD_CLOSURE:
			return 1;
		case LOAD_DEREF:
			return 1;
		case STORE_DEREF:
			return -1;
		default:
			fprintf(stderr, "opcode = %d\n", opcode);
			Py_FatalError("opcode_stack_effect()");

	}
	return 0; /* not reachable */
}

static int
compiler_addop(struct compiler *c, int opcode)
{
	struct basicblock *b;
	struct instr *i;
	int off;
	off = compiler_next_instr(c, c->u->u_curblock);
	if (off < 0)
		return 0;
	b = c->u->u_blocks[c->u->u_curblock];
	i = &b->b_instr[off];
	i->i_opcode = opcode;
	i->i_hasarg = 0;
	if (opcode == RETURN_VALUE)
		b->b_return = 1;
	compiler_set_lineno(c, off);
	return 1;
}

static int
compiler_add_o(struct compiler *c, PyObject *dict, PyObject *o)
{
	PyObject *t, *v;
	int arg;

        /* necessary to make sure types aren't coerced (e.g., int and long) */
        /* XXX should use: t = PyTuple_Pack(2, o, o->ob_type); */
        t = Py_BuildValue("(OO)", o, o->ob_type);
        if (t == NULL)
            return -1;

	v = PyDict_GetItem(dict, t);
	if (!v) {
		arg = PyDict_Size(dict);
		v = PyInt_FromLong(arg);
		if (!v) {
			Py_DECREF(t);
			return -1;
                }
		if (PyDict_SetItem(dict, t, v) < 0) {
			Py_DECREF(t);
			Py_DECREF(v);
			return -1;
		}
		Py_DECREF(v);
	}
	else
		arg = PyInt_AsLong(v);
	Py_DECREF(t);
        return arg;
}

static int
compiler_addop_o(struct compiler *c, int opcode, PyObject *dict,
		     PyObject *o)
{
    int arg = compiler_add_o(c, dict, o);
    if (arg < 0)
        return 0;
    return compiler_addop_i(c, opcode, arg);
}

static int
compiler_addop_name(struct compiler *c, int opcode, PyObject *dict,
                    PyObject *o)
{
    int arg;
    PyObject *mangled = _Py_Mangle(c->u->u_private, o);
    if (!mangled)
        return 0;
    arg = compiler_add_o(c, dict, mangled);
    Py_DECREF(mangled);
    if (arg < 0)
        return 0;
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
	off = compiler_next_instr(c, c->u->u_curblock);
	if (off < 0)
		return 0;
	i = &c->u->u_blocks[c->u->u_curblock]->b_instr[off];
	i->i_opcode = opcode;
	i->i_oparg = oparg;
	i->i_hasarg = 1;
	compiler_set_lineno(c, off);
	return 1;
}

static int
compiler_addop_j(struct compiler *c, int opcode, int block, int absolute)
{
	struct instr *i;
	int off;

	assert(block >= 0);
	off = compiler_next_instr(c, c->u->u_curblock);
	if (off < 0)
		return 0;
	compiler_set_lineno(c, off);
	i = &c->u->u_blocks[c->u->u_curblock]->b_instr[off];
	i->i_opcode = opcode;
	i->i_target = block;
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

/* XXX The returns inside these macros make it impossible to decref
   objects created in the local function.
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
	if (!compiler_addop_o((C), (OP), (C)->u->u_ ## TYPE, (O))) \
		return 0; \
}

#define ADDOP_NAME(C, OP, O, TYPE) { \
	if (!compiler_addop_name((C), (OP), (C)->u->u_ ## TYPE, (O))) \
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

#define VISIT_SLICE(C, V, CTX) {\
	if (!compiler_visit_slice((C), (V), (CTX))) \
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

static int
compiler_isdocstring(stmt_ty s)
{
    if (s->kind != Expr_kind)
        return 0;
    return s->v.Expr.value->kind == Str_kind;
}

/* Compile a sequence of statements, checking for a docstring. */

static int
compiler_body(struct compiler *c, asdl_seq *stmts)
{
	int i = 0;
	stmt_ty st;

	if (!asdl_seq_LEN(stmts))
		return 1;
	st = asdl_seq_GET(stmts, 0);
	if (compiler_isdocstring(st)) {
		i = 1;
		VISIT(c, expr, st->v.Expr.value);
		if (!compiler_nameop(c, __doc__, Store))
			return 0;
	}
        for (; i < asdl_seq_LEN(stmts); i++)
            VISIT(c, stmt, asdl_seq_GET(stmts, i));
	return 1;
}

static PyCodeObject *
compiler_mod(struct compiler *c, mod_ty mod)
{
	PyCodeObject *co;
        int addNone = 1;
	static PyObject *module;
	if (!module) {
		module = PyString_FromString("<module>");
		if (!module)
			return NULL;
	}
	if (!compiler_enter_scope(c, module, mod, 1))
		return NULL;
	switch (mod->kind) {
	case Module_kind: 
		if (!compiler_body(c, mod->v.Module.body))
			return 0;
		break;
	case Interactive_kind:
		c->c_interactive = 1;
		VISIT_SEQ(c, stmt, mod->v.Interactive.body);
		break;
	case Expression_kind:
		VISIT(c, expr, mod->v.Expression.body);
                addNone = 0;
		break;
	case Suite_kind:
		assert(0);      /* XXX: what should we do here? */
		VISIT_SEQ(c, stmt, mod->v.Suite.body);
		break;
        default:
            assert(0);
	}
	co = assemble(c, addNone);
	compiler_exit_scope(c);
	return co;
}

/* The test for LOCAL must come before the test for FREE in order to
   handle classes where name is both local and free.  The local var is
   a method and the free var is a free var referenced within a method.
*/

static int
get_ref_type(struct compiler *c, PyObject *name)
{
	int scope = PyST_GetScope(c->u->u_ste, name);
        if (scope == 0) {
            char buf[350];
            PyOS_snprintf(buf, sizeof(buf),
                          "unknown scope for %.100s in %.100s(%s) in %s\n"
                          "symbols: %s\nlocals: %s\nglobals: %s\n",
                          PyString_AS_STRING(name), 
                          PyString_AS_STRING(c->u->u_name), 
                          PyObject_REPR(c->u->u_ste->ste_id),
                          c->c_filename,
                          PyObject_REPR(c->u->u_ste->ste_symbols),
                          PyObject_REPR(c->u->u_varnames),
                          PyObject_REPR(c->u->u_names)
		);
            Py_FatalError(buf);
        }

        return scope;
}

static int
compiler_lookup_arg(PyObject *dict, PyObject *name)
{
    PyObject *k, *v;
    k = Py_BuildValue("(OO)", name, name->ob_type);
    if (k == NULL)
        return -1;
    v = PyDict_GetItem(dict, k);
    if (v == NULL)
        return -1;
    return PyInt_AS_LONG(v);
}

static int
compiler_make_closure(struct compiler *c, PyCodeObject *co, int args)
{
	int i, free = PyCode_GetNumFree(co);
	if (free == 0) {
            ADDOP_O(c, LOAD_CONST, (PyObject*)co, consts);
            ADDOP_I(c, MAKE_FUNCTION, args);
            return 1;
        }
	for (i = 0; i < free; ++i) {
		/* Bypass com_addop_varname because it will generate
		   LOAD_DEREF but LOAD_CLOSURE is needed. 
		*/
		PyObject *name = PyTuple_GET_ITEM(co->co_freevars, i);
		int arg, reftype;

		/* Special case: If a class contains a method with a
		   free variable that has the same name as a method,
		   the name will be considered free *and* local in the
		   class.  It should be handled by the closure, as
		   well as by the normal name loookup logic.
		*/
		reftype = get_ref_type(c, name);
		if (reftype == CELL)
			arg = compiler_lookup_arg(c->u->u_cellvars, name);
		else /* (reftype == FREE) */
			arg = compiler_lookup_arg(c->u->u_freevars, name);
		if (arg == -1) {
			printf("lookup %s in %s %d %d\n"
				"freevars of %s: %s\n",
				PyObject_REPR(name), 
				PyString_AS_STRING(c->u->u_name), 
				reftype, arg,
				PyString_AS_STRING(co->co_name),
				PyObject_REPR(co->co_freevars));
			Py_FatalError("compiler_make_closure()");
		}
		ADDOP_I(c, LOAD_CLOSURE, arg);
	}
        ADDOP_I(c, BUILD_TUPLE, free);
	ADDOP_O(c, LOAD_CONST, (PyObject*)co, consts);
        ADDOP_I(c, MAKE_CLOSURE, args);
        return 1;
}

static int
compiler_function(struct compiler *c, stmt_ty s)
{
	PyCodeObject *co;
        PyObject *first_const = Py_None;
	arguments_ty args = s->v.FunctionDef.args;
        stmt_ty st;
	int i, n, docstring;
	assert(s->kind == FunctionDef_kind);

	if (args->defaults)
		VISIT_SEQ(c, expr, args->defaults);
	if (!compiler_enter_scope(c, s->v.FunctionDef.name, (void *)s,
				  s->lineno))
		return 0;

        st = asdl_seq_GET(s->v.FunctionDef.body, 0);
        docstring = compiler_isdocstring(st);
        if (docstring)
            first_const = st->v.Expr.value->v.Str.s;
        if (compiler_add_o(c, c->u->u_consts, first_const) < 0)
            return 0;

        /* unpack nested arguments */
	n = asdl_seq_LEN(args->args);
        for (i = 0; i < n; i++) {
            expr_ty arg = asdl_seq_GET(args->args, i);
            if (arg->kind == Tuple_kind) {
                PyObject *id = PyString_FromFormat(".%d", i);
                if (id == NULL)
			return 0;
		if (!compiler_nameop(c, id, Load)) {
			Py_DECREF(id);
			return 0;
		}
		Py_DECREF(id);
                VISIT(c, expr, arg);
            }
        }

	c->u->u_argcount = asdl_seq_LEN(args->args);
	n = asdl_seq_LEN(s->v.FunctionDef.body);
        /* if there was a docstring, we need to skip the first statement */
	for (i = docstring; i < n; i++) {
		stmt_ty s2 = asdl_seq_GET(s->v.FunctionDef.body, i);
		if (i == 0 && s2->kind == Expr_kind &&
		    s2->v.Expr.value->kind == Str_kind)
			continue;
		VISIT(c, stmt, s2);
	}
	co = assemble(c, 1);
	if (co == NULL)
		return 0;
	compiler_exit_scope(c);

        compiler_make_closure(c, co, asdl_seq_LEN(args->defaults));
	return compiler_nameop(c, s->v.FunctionDef.name, Store);
}

static int
compiler_class(struct compiler *c, stmt_ty s)
{
	int n;
	PyCodeObject *co;
        PyObject *str;
	/* push class name on stack, needed by BUILD_CLASS */
	ADDOP_O(c, LOAD_CONST, s->v.ClassDef.name, consts);
	/* push the tuple of base classes on the stack */
	n = asdl_seq_LEN(s->v.ClassDef.bases);
	if (n > 0)
		VISIT_SEQ(c, expr, s->v.ClassDef.bases);
	ADDOP_I(c, BUILD_TUPLE, n);
	if (!compiler_enter_scope(c, s->v.ClassDef.name, (void *)s,
				  s->lineno))
		return 0;
        c->u->u_private = s->v.ClassDef.name;
        Py_INCREF(c->u->u_private);
        str = PyString_InternFromString("__name__");
	if (!str || !compiler_nameop(c, str, Load)) {
		Py_XDECREF(str);
		return 0;
        }
        
        Py_DECREF(str);
        str = PyString_InternFromString("__module__");
	if (!str || !compiler_nameop(c, str, Store)) {
		Py_XDECREF(str);
		return 0;
        }
        Py_DECREF(str);

	if (!compiler_body(c, s->v.ClassDef.body))
		return 0;

	ADDOP(c, LOAD_LOCALS);
	ADDOP(c, RETURN_VALUE);
	co = assemble(c, 1);
	if (co == NULL)
		return 0;
	compiler_exit_scope(c);

        compiler_make_closure(c, co, 0);
	ADDOP_I(c, CALL_FUNCTION, 0);
	ADDOP(c, BUILD_CLASS);
	if (!compiler_nameop(c, s->v.ClassDef.name, Store))
		return 0;
	return 1;
}

static int
compiler_lambda(struct compiler *c, expr_ty e)
{
	PyCodeObject *co;
	identifier name;
	arguments_ty args = e->v.Lambda.args;
	assert(e->kind == Lambda_kind);

	name = PyString_InternFromString("lambda");
	if (!name)
		return 0;

	if (args->defaults)
		VISIT_SEQ(c, expr, args->defaults);
	if (!compiler_enter_scope(c, name, (void *)e, c->u->u_lineno))
		return 0;
	c->u->u_argcount = asdl_seq_LEN(args->args);
	VISIT(c, expr, e->v.Lambda.body);
	ADDOP(c, RETURN_VALUE);
	co = assemble(c, 1);
	if (co == NULL)
		return 0;
	compiler_exit_scope(c);

        compiler_make_closure(c, co, asdl_seq_LEN(args->defaults));
	Py_DECREF(name);

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
	int end, next;

	assert(s->kind == If_kind);
	end = compiler_new_block(c);
	if (end < 0)
		return 0;
        next = compiler_new_block(c);
        if (next < 0)
            return 0;
        VISIT(c, expr, s->v.If.test);
        ADDOP_JREL(c, JUMP_IF_FALSE, next);
        ADDOP(c, POP_TOP);
        VISIT_SEQ(c, stmt, s->v.If.body);
        ADDOP_JREL(c, JUMP_FORWARD, end);
        compiler_use_next_block(c, next);
        ADDOP(c, POP_TOP);
        if (s->v.If.orelse)
            VISIT_SEQ(c, stmt, s->v.If.orelse);
	compiler_use_next_block(c, end);
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
	compiler_use_next_block(c, start);
	ADDOP_JREL(c, FOR_ITER, cleanup);
	VISIT(c, expr, s->v.For.target);
	VISIT_SEQ(c, stmt, s->v.For.body);
	ADDOP_JABS(c, JUMP_ABSOLUTE, start);
	compiler_use_next_block(c, cleanup);
	ADDOP(c, POP_BLOCK);
	compiler_pop_fblock(c, LOOP, start);
	VISIT_SEQ(c, stmt, s->v.For.orelse);
	compiler_use_next_block(c, end);
	return 1;
}

static int
compiler_while(struct compiler *c, stmt_ty s)
{
	int loop, orelse, end, anchor;
	int constant = expr_constant(s->v.While.test);

	if (constant == 0)
		return 1;
	loop = compiler_new_block(c);
	end = compiler_new_block(c);
	if (constant == -1) {
		anchor = compiler_new_block(c);
		if (anchor < 0)
			return 0;
	}
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
	if (constant == -1) {
		VISIT(c, expr, s->v.While.test);
		ADDOP_JREL(c, JUMP_IF_FALSE, anchor);
		ADDOP(c, POP_TOP);
	}
	VISIT_SEQ(c, stmt, s->v.While.body);
	ADDOP_JABS(c, JUMP_ABSOLUTE, loop);

	/* XXX should the two POP instructions be in a separate block
	   if there is no else clause ?
	*/

	if (constant == -1) {
		compiler_use_next_block(c, anchor);
		ADDOP(c, POP_TOP);
		ADDOP(c, POP_BLOCK);
	}
	compiler_pop_fblock(c, LOOP, loop);
	if (orelse != -1)
		VISIT_SEQ(c, stmt, s->v.While.orelse);
	compiler_use_next_block(c, end);

	return 1;
}

static int
compiler_continue(struct compiler *c)
{
	static const char LOOP_ERROR_MSG[] = "'continue' not properly in loop";
	int i;

	if (!c->u->u_nfblocks)
		return compiler_error(c, LOOP_ERROR_MSG);
	i = c->u->u_nfblocks - 1;
	switch (c->u->u_fblock[i].fb_type) {
	case LOOP:
		ADDOP_JABS(c, JUMP_ABSOLUTE, c->u->u_fblock[i].fb_block);
		break;
	case EXCEPT:
	case FINALLY_TRY:
		while (--i >= 0 && c->u->u_fblock[i].fb_type != LOOP)
			;
		if (i == -1)
			return compiler_error(c, LOOP_ERROR_MSG);
		ADDOP_JABS(c, CONTINUE_LOOP, c->u->u_fblock[i].fb_block);
		break;
	case FINALLY_END:
	        return compiler_error(c,
			"'continue' not supported inside 'finally' clause");
	}

	return 1;
}

/* Code generated for "try: <body> finally: <finalbody>" is as follows:
   
		SETUP_FINALLY	L
		<code for body>
		POP_BLOCK
		LOAD_CONST	<None>
	L:	<code for finalbody>
		END_FINALLY
   
   The special instructions use the block stack.  Each block
   stack entry contains the instruction that created it (here
   SETUP_FINALLY), the level of the value stack at the time the
   block stack entry was created, and a label (here L).
   
   SETUP_FINALLY:
	Pushes the current value stack level and the label
	onto the block stack.
   POP_BLOCK:
	Pops en entry from the block stack, and pops the value
	stack until its level is the same as indicated on the
	block stack.  (The label is ignored.)
   END_FINALLY:
	Pops a variable number of entries from the *value* stack
	and re-raises the exception they specify.  The number of
	entries popped depends on the (pseudo) exception type.
   
   The block stack is unwound when an exception is raised:
   when a SETUP_FINALLY entry is found, the exception is pushed
   onto the value stack (and the exception condition is cleared),
   and the interpreter jumps to the label gotten from the block
   stack.
*/

static int
compiler_try_finally(struct compiler *c, stmt_ty s)
{
	int body, end;
	body = compiler_new_block(c);
	end = compiler_new_block(c);
	if (body < 0 || end < 0)
		return 0;

	ADDOP_JREL(c, SETUP_FINALLY, end);
	compiler_use_next_block(c, body);
	if (!compiler_push_fblock(c, FINALLY_TRY, body))
		return 0;
	VISIT_SEQ(c, stmt, s->v.TryFinally.body);
	ADDOP(c, POP_BLOCK);
	compiler_pop_fblock(c, FINALLY_TRY, body);

	ADDOP_O(c, LOAD_CONST, Py_None, consts);
	compiler_use_next_block(c, end);
	if (!compiler_push_fblock(c, FINALLY_END, end))
		return 0;
	VISIT_SEQ(c, stmt, s->v.TryFinally.finalbody);
	ADDOP(c, END_FINALLY);
	compiler_pop_fblock(c, FINALLY_END, end);

	return 1;
}

/*
   Code generated for "try: S except E1, V1: S1 except E2, V2: S2 ...":
   (The contents of the value stack is shown in [], with the top
   at the right; 'tb' is trace-back info, 'val' the exception's
   associated value, and 'exc' the exception.)
   
   Value stack		Label	Instruction	Argument
   []				SETUP_EXCEPT	L1
   []				<code for S>
   []				POP_BLOCK
   []				JUMP_FORWARD	L0
   
   [tb, val, exc]	L1:	DUP				)
   [tb, val, exc, exc]		<evaluate E1>			)
   [tb, val, exc, exc, E1]	COMPARE_OP	EXC_MATCH	) only if E1
   [tb, val, exc, 1-or-0]	JUMP_IF_FALSE	L2		)
   [tb, val, exc, 1]		POP				)
   [tb, val, exc]		POP
   [tb, val]			<assign to V1>	(or POP if no V1)
   [tb]				POP
   []				<code for S1>
   				JUMP_FORWARD	L0
   
   [tb, val, exc, 0]	L2:	POP
   [tb, val, exc]		DUP
   .............................etc.......................

   [tb, val, exc, 0]	Ln+1:	POP
   [tb, val, exc]	   	END_FINALLY	# re-raise exception
   
   []			L0:	<next statement>
   
   Of course, parts are not generated if Vi or Ei is not present.
*/
static int
compiler_try_except(struct compiler *c, stmt_ty s)
{
	int i, n, body, orelse, except, end;

	body = compiler_new_block(c);
	except = compiler_new_block(c);
	orelse = compiler_new_block(c);
	end = compiler_new_block(c);
	if (body < 0 || except < 0 || orelse < 0 || end < 0)
		return 0;
	ADDOP_JREL(c, SETUP_EXCEPT, except);
	compiler_use_next_block(c, body);
	if (!compiler_push_fblock(c, EXCEPT, body))
		return 0;
	VISIT_SEQ(c, stmt, s->v.TryExcept.body);
	ADDOP(c, POP_BLOCK);
	compiler_pop_fblock(c, EXCEPT, body);
	ADDOP_JREL(c, JUMP_FORWARD, orelse);
	n = asdl_seq_LEN(s->v.TryExcept.handlers);
	compiler_use_next_block(c, except);
	for (i = 0; i < n; i++) {
		excepthandler_ty handler = asdl_seq_GET(
						s->v.TryExcept.handlers, i);
		if (!handler->type && i < n-1)
		    return compiler_error(c, "default 'except:' must be last");
		except = compiler_new_block(c);
		if (except < 0)
			return 0;
		if (handler->type) {
			ADDOP(c, DUP_TOP);
			VISIT(c, expr, handler->type);
			ADDOP_I(c, COMPARE_OP, PyCmp_EXC_MATCH);
			ADDOP_JREL(c, JUMP_IF_FALSE, except);
			ADDOP(c, POP_TOP);
		}
		ADDOP(c, POP_TOP);
		if (handler->name) {
			VISIT(c, expr, handler->name);
		}
		else {
			ADDOP(c, POP_TOP);
		}
		ADDOP(c, POP_TOP);
		VISIT_SEQ(c, stmt, handler->body);
		ADDOP_JREL(c, JUMP_FORWARD, end);
		compiler_use_next_block(c, except);
		if (handler->type)
			ADDOP(c, POP_TOP);
	}
	ADDOP(c, END_FINALLY);
	compiler_use_next_block(c, orelse);
	VISIT_SEQ(c, stmt, s->v.TryExcept.orelse);
	compiler_use_next_block(c, end);
	return 1;
}

static int
compiler_import(struct compiler *c, stmt_ty s)
{
	int i, n = asdl_seq_LEN(s->v.Import.names);
	for (i = 0; i < n; i++) {
		alias_ty alias = asdl_seq_GET(s->v.Import.names, i);
		identifier store_name;
		ADDOP_O(c, LOAD_CONST, Py_None, consts);
		ADDOP_NAME(c, IMPORT_NAME, alias->name, names);

                /* XXX: handling of store_name should be cleaned up */
		if (alias->asname) {
			store_name = alias->asname;
                        Py_INCREF(store_name);
                }
                else {
                    const char *base = PyString_AS_STRING(alias->name);
                    char *dot = strchr(base, '.');
                    if (dot)
                        store_name = PyString_FromStringAndSize(base,
                                                                dot - base);
                    else {
                        store_name = alias->name;
                        Py_INCREF(store_name);
                    }
                }

		if (!compiler_nameop(c, store_name, Store)) {
                    Py_DECREF(store_name);
                    return 0;
                }
                Py_DECREF(store_name);
	}
	return 1;
}

static int
compiler_from_import(struct compiler *c, stmt_ty s)
{
	int i, n = asdl_seq_LEN(s->v.ImportFrom.names);
	int star = 0;

	PyObject *names = PyTuple_New(n);
	if (!names)
		return 0;

	/* build up the names */
	for (i = 0; i < n; i++) {
		alias_ty alias = asdl_seq_GET(s->v.ImportFrom.names, i);
		Py_INCREF(alias->name);
		PyTuple_SET_ITEM(names, i, alias->name);
	}

	if (s->lineno > c->c_future->ff_lineno) {
		if (!strcmp(PyString_AS_STRING(s->v.ImportFrom.module),
			    "__future__")) {
			Py_DECREF(names);
			return compiler_error(c, 
				      "from __future__ imports must occur "
                                      "at the beginning of the file");

		}
	}

	ADDOP_O(c, LOAD_CONST, names, consts);
	ADDOP_NAME(c, IMPORT_NAME, s->v.ImportFrom.module, names);
	for (i = 0; i < n; i++) {
		alias_ty alias = asdl_seq_GET(s->v.ImportFrom.names, i);
		identifier store_name;

		if (i == 0 && *PyString_AS_STRING(alias->name) == '*') {
			assert(n == 1);
			ADDOP(c, IMPORT_STAR);
			star = 1;
			break;
		}
		    
		ADDOP_NAME(c, IMPORT_FROM, alias->name, names);
		store_name = alias->name;
		if (alias->asname)
			store_name = alias->asname;

		if (!compiler_nameop(c, store_name, Store)) {
			Py_DECREF(names);
			return 0;
		}
	}
	if (!star) 
		/* remove imported module */
		ADDOP(c, POP_TOP);
	return 1;
}

static int
compiler_assert(struct compiler *c, stmt_ty s)
{
	static PyObject *assertion_error = NULL;
	int end;

	if (Py_OptimizeFlag)
		return 1;
	if (assertion_error == NULL) {
		assertion_error = PyString_FromString("AssertionError");
		if (assertion_error == NULL)
			return 0;
	}
	VISIT(c, expr, s->v.Assert.test);
	end = compiler_new_block(c);
	if (end < 0)
		return 0;
	ADDOP_JREL(c, JUMP_IF_TRUE, end);
	ADDOP(c, POP_TOP);
	ADDOP_O(c, LOAD_GLOBAL, assertion_error, names);
	if (s->v.Assert.msg) {
		VISIT(c, expr, s->v.Assert.msg);
		ADDOP_I(c, RAISE_VARARGS, 2);
	}
	else {
		ADDOP_I(c, RAISE_VARARGS, 1);
	}
	compiler_use_block(c, end);
	ADDOP(c, POP_TOP);
	return 1;
}

static int
compiler_visit_stmt(struct compiler *c, stmt_ty s)
{
	int i, n;

	fprintf(stderr, "compile stmt %d lineno %d\n", s->kind, s->lineno);
	c->u->u_lineno = s->lineno;
	c->u->u_lineno_set = false;
	switch (s->kind) {
        case FunctionDef_kind:
		return compiler_function(c, s);
        case ClassDef_kind:
		return compiler_class(c, s);
        case Return_kind:
                if (c->c_nestlevel <= 1)
                        return compiler_error(c, "'return' outside function");
		if (s->v.Return.value) {
			if (c->u->u_ste->ste_generator) {
				return compiler_error(c,
				    "'return' with argument inside generator");
			}
			VISIT(c, expr, s->v.Return.value);
		}
		else
			ADDOP_O(c, LOAD_CONST, Py_None, consts);
		ADDOP(c, RETURN_VALUE);
		break;
        case Yield_kind:
                if (c->c_nestlevel <= 1)
                        return compiler_error(c, "'yield' outside function");
		for (i = 0; i < c->u->u_nfblocks; i++) {
			if (c->u->u_fblock[i].fb_type == FINALLY_TRY)
				return compiler_error(
					c, "'yield' not allowed in a 'try' "
					"block with a 'finally' clause");
		}
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
		return compiler_augassign(c, s);
        case Print_kind:
		return compiler_print(c, s);
        case For_kind:
		return compiler_for(c, s);
        case While_kind:
		return compiler_while(c, s);
        case If_kind:
		return compiler_if(c, s);
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
		return compiler_try_except(c, s);
        case TryFinally_kind:
		return compiler_try_finally(c, s);
        case Assert_kind:
		return compiler_assert(c, s);
        case Import_kind:
		return compiler_import(c, s);
        case ImportFrom_kind:
		return compiler_from_import(c, s);
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
			ADDOP(c, POP_TOP);
		}
		break;
        case Pass_kind:
		break;
        case Break_kind:
		if (!c->u->u_nfblocks)
			return compiler_error(c, "'break' outside loop");
		ADDOP(c, BREAK_LOOP);
		break;
        case Continue_kind:
		return compiler_continue(c);
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
cmpop(cmpop_ty op)
{
	switch (op) {
	case Eq:
		return PyCmp_EQ;
	case NotEq:
		return PyCmp_NE;
	case Lt:
		return PyCmp_LT;
	case LtE:
		return PyCmp_LE;
	case Gt:
		return PyCmp_GT;
	case GtE:
		return PyCmp_GE;
	case Is:
		return PyCmp_IS;
	case IsNot:
		return PyCmp_IS_NOT;
	case In:
		return PyCmp_IN;
	case NotIn:
		return PyCmp_NOT_IN;
	}
	return PyCmp_BAD;
}

static int
inplace_binop(struct compiler *c, operator_ty op)
{
	switch (op) {
	case Add:
		return INPLACE_ADD;
	case Sub:
		return INPLACE_SUBTRACT;
	case Mult:
		return INPLACE_MULTIPLY;
	case Div:
		if (c->c_flags && c->c_flags->cf_flags & CO_FUTURE_DIVISION)
			return INPLACE_TRUE_DIVIDE;
		else
			return INPLACE_DIVIDE;
	case Mod:
		return INPLACE_MODULO;
	case Pow:
		return INPLACE_POWER;
	case LShift:
		return INPLACE_LSHIFT;
	case RShift:
		return INPLACE_RSHIFT;
	case BitOr:
		return INPLACE_OR;
	case BitXor:
		return INPLACE_XOR;
	case BitAnd:
		return INPLACE_AND;
	case FloorDiv:
		return INPLACE_FLOOR_DIVIDE;
	}
	assert(0);
	return 0;
}

static int
compiler_nameop(struct compiler *c, identifier name, expr_context_ty ctx)
{
	int op, scope;
	enum { OP_FAST, OP_GLOBAL, OP_DEREF, OP_NAME } optype;

        PyObject *dict = c->u->u_names;
	/* XXX AugStore isn't used anywhere! */

	/* First check for assignment to __debug__. Param? */
	if ((ctx == Store || ctx == AugStore || ctx == Del)
	    && !strcmp(PyString_AS_STRING(name), "__debug__")) {
		return compiler_error(c, "can not assign to __debug__");
	}

	op = 0;
	optype = OP_NAME;
	scope = PyST_GetScope(c->u->u_ste, name);
	switch (scope) {
	case FREE:
                dict = c->u->u_freevars;
		optype = OP_DEREF;
		break;
	case CELL:
                dict = c->u->u_cellvars;
		optype = OP_DEREF;
		break;
	case LOCAL:
		if (c->u->u_ste->ste_type == FunctionBlock)
			optype = OP_FAST;
		break;
	case GLOBAL_IMPLICIT:
		if (c->u->u_ste->ste_optimized)
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
		case AugLoad:
		case AugStore:
			break;
		case Del:
			PyErr_Format(PyExc_SyntaxError,
				     "can not delete variable '%s' referenced "
				     "in nested scope",
				     PyString_AS_STRING(name));
			return 0;
			break;
		case Param:
			assert(0); /* impossible */
		}
		break;
	case OP_FAST:
		switch (ctx) {
		case Load: op = LOAD_FAST; break;
		case Store: op = STORE_FAST; break;
		case Del: op = DELETE_FAST; break;
		case AugLoad:
		case AugStore:
			break;
		case Param:
			assert(0); /* impossible */
		}
		ADDOP_O(c, op, name, varnames);
		return 1;
	case OP_GLOBAL:
		switch (ctx) {
		case Load: op = LOAD_GLOBAL; break;
		case Store: op = STORE_GLOBAL; break;
		case Del: op = DELETE_GLOBAL; break;
		case AugLoad:
		case AugStore:
			break;
		case Param:
			assert(0); /* impossible */
		}
		break;
	case OP_NAME:
		switch (ctx) {
		case Load: op = LOAD_NAME; break;
		case Store: op = STORE_NAME; break;
		case Del: op = DELETE_NAME; break;
		case AugLoad:
		case AugStore:
			break;
		case Param:
			assert(0); /* impossible */
		}
		break;
	}

	assert(op);
	return compiler_addop_name(c, op, dict, name);
}

static int
compiler_boolop(struct compiler *c, expr_ty e)
{
	int end, jumpi, i, n;
	asdl_seq *s;

	assert(e->kind == BoolOp_kind);
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
		ADDOP(c, POP_TOP)
	}
	VISIT(c, expr, asdl_seq_GET(s, n));
	compiler_use_next_block(c, end);
	return 1;
}

static int
compiler_list(struct compiler *c, expr_ty e)
{
	int n = asdl_seq_LEN(e->v.List.elts);
	if (e->v.List.ctx == Store) {
		ADDOP_I(c, UNPACK_SEQUENCE, n);
	}
	VISIT_SEQ(c, expr, e->v.List.elts);
	if (e->v.List.ctx == Load) {
		ADDOP_I(c, BUILD_LIST, n);
	}
	return 1;
}

static int
compiler_tuple(struct compiler *c, expr_ty e)
{
	int n = asdl_seq_LEN(e->v.Tuple.elts);
	if (e->v.Tuple.ctx == Store) {
		ADDOP_I(c, UNPACK_SEQUENCE, n);
	}
	VISIT_SEQ(c, expr, e->v.Tuple.elts);
	if (e->v.Tuple.ctx == Load) {
		ADDOP_I(c, BUILD_TUPLE, n);
	}
	return 1;
}

static int
compiler_compare(struct compiler *c, expr_ty e)
{
	int i, n, cleanup = -1;

	/* XXX the logic can be cleaned up for 1 or multiple comparisons */
	VISIT(c, expr, e->v.Compare.left);
	n = asdl_seq_LEN(e->v.Compare.ops);
	assert(n > 0);
	if (n > 1) {
		cleanup = compiler_new_block(c);
		VISIT(c, expr, asdl_seq_GET(e->v.Compare.comparators, 0));
	}
	for (i = 1; i < n; i++) {
		ADDOP(c, DUP_TOP);
		ADDOP(c, ROT_THREE);
		/* XXX We're casting a void* to cmpop_ty in the next stmt. */
		ADDOP_I(c, COMPARE_OP,
			cmpop((cmpop_ty)asdl_seq_GET(e->v.Compare.ops, i - 1)));
		ADDOP_JREL(c, JUMP_IF_FALSE, cleanup);
		NEXT_BLOCK(c);
		ADDOP(c, POP_TOP);
		if (i < (n - 1))
		    VISIT(c, expr, asdl_seq_GET(e->v.Compare.comparators, i));
	}
	VISIT(c, expr, asdl_seq_GET(e->v.Compare.comparators, n - 1));
	ADDOP_I(c, COMPARE_OP,
		/* XXX We're casting a void* to cmpop_ty in the next stmt. */
	       cmpop((cmpop_ty)asdl_seq_GET(e->v.Compare.ops, n - 1)));
	if (n > 1) {
		int end = compiler_new_block(c);
		ADDOP_JREL(c, JUMP_FORWARD, end);
		compiler_use_next_block(c, cleanup);
		ADDOP(c, ROT_TWO);
		ADDOP(c, POP_TOP);
		compiler_use_next_block(c, end);
	}
	return 1;
}

static int
compiler_call(struct compiler *c, expr_ty e)
{
	int n, code = 0;

	VISIT(c, expr, e->v.Call.func);
	n = asdl_seq_LEN(e->v.Call.args);
	VISIT_SEQ(c, expr, e->v.Call.args);
	if (e->v.Call.keywords) {
		VISIT_SEQ(c, keyword, e->v.Call.keywords);
		n |= asdl_seq_LEN(e->v.Call.keywords) << 8;
	}
	if (e->v.Call.starargs) {
		VISIT(c, expr, e->v.Call.starargs);
		code |= 1;
	}
	if (e->v.Call.kwargs) {
		VISIT(c, expr, e->v.Call.kwargs);
		code |= 2;
	}
	switch (code) {
	case 0:
		ADDOP_I(c, CALL_FUNCTION, n);
		break;
	case 1:
		ADDOP_I(c, CALL_FUNCTION_VAR, n);
		break;
	case 2:
		ADDOP_I(c, CALL_FUNCTION_KW, n);
		break;
	case 3:
		ADDOP_I(c, CALL_FUNCTION_VAR_KW, n);
		break;
	}
	return 1;
}

static int
compiler_listcomp_generator(struct compiler *c, PyObject *tmpname,
                            asdl_seq *generators, int gen_index, 
                            expr_ty elt)
{
	/* generate code for the iterator, then each of the ifs,
	   and then write to the element */

	listcomp_ty l;
	int start, anchor, skip, if_cleanup, i, n;

	start = compiler_new_block(c);
	skip = compiler_new_block(c);
	if_cleanup = compiler_new_block(c);
	anchor = compiler_new_block(c);

        if (start < 0 || skip < 0 || if_cleanup < 0 || anchor < 0)
            return 0;

	l = asdl_seq_GET(generators, gen_index);
	VISIT(c, expr, l->iter);
	ADDOP(c, GET_ITER);
	compiler_use_next_block(c, start);
	ADDOP_JREL(c, FOR_ITER, anchor);
	NEXT_BLOCK(c);
	VISIT(c, expr, l->target);

        /* XXX this needs to be cleaned up...a lot! */
	n = asdl_seq_LEN(l->ifs);
	for (i = 0; i < n; i++) {
		expr_ty e = asdl_seq_GET(l->ifs, i);
		VISIT(c, expr, e);
		ADDOP_JREL(c, JUMP_IF_FALSE, if_cleanup);
		NEXT_BLOCK(c);
		ADDOP(c, POP_TOP);
	} 

        if (++gen_index < asdl_seq_LEN(generators))
            if (!compiler_listcomp_generator(c, tmpname, 
                                             generators, gen_index, elt))
                return 0;

        /* only append after the last for generator */
        if (gen_index >= asdl_seq_LEN(generators)) {
            if (!compiler_nameop(c, tmpname, Load))
		return 0;
            VISIT(c, expr, elt);
            ADDOP_I(c, CALL_FUNCTION, 1);
            ADDOP(c, POP_TOP);

            compiler_use_next_block(c, skip);
        }
	for (i = 0; i < n; i++) {
		ADDOP_I(c, JUMP_FORWARD, 1);
                if (i == 0)
                    compiler_use_next_block(c, if_cleanup);
		ADDOP(c, POP_TOP);
	} 
	ADDOP_JABS(c, JUMP_ABSOLUTE, start);
	compiler_use_next_block(c, anchor);
        /* delete the append method added to locals */
	if (gen_index == 1)
            if (!compiler_nameop(c, tmpname, Del))
		return 0;
	
	return 1;
}

static int
compiler_listcomp(struct compiler *c, expr_ty e)
{
	char tmpname[256];
	identifier tmp;
        int rc = 0;
	static identifier append;
	asdl_seq *generators = e->v.ListComp.generators;

	assert(e->kind == ListComp_kind);
	if (!append) {
		append = PyString_InternFromString("append");
		if (!append)
			return 0;
	}
	PyOS_snprintf(tmpname, sizeof(tmpname), "_[%d]", ++c->u->u_tmpname);
	tmp = PyString_FromString(tmpname);
	if (!tmp)
		return 0;
	ADDOP_I(c, BUILD_LIST, 0);
	ADDOP(c, DUP_TOP);
	ADDOP_O(c, LOAD_ATTR, append, names);
	if (compiler_nameop(c, tmp, Store))
            rc = compiler_listcomp_generator(c, tmp, generators, 0, 
                                             e->v.ListComp.elt);
        Py_DECREF(tmp);
	return rc;
}

static int
compiler_visit_keyword(struct compiler *c, keyword_ty k)
{
	ADDOP_O(c, LOAD_CONST, k->arg, consts);
	VISIT(c, expr, k->value);
	return 1;
}

/* Test whether expression is constant.  For constants, report
   whether they are true or false.

   Return values: 1 for true, 0 for false, -1 for non-constant.
 */

static int
expr_constant(expr_ty e)
{
	switch (e->kind) {
	case Num_kind:
		return PyObject_IsTrue(e->v.Num.n);
	case Str_kind:
		return PyObject_IsTrue(e->v.Str.s);
	default:
		return -1;
	}
}

static int
compiler_visit_expr(struct compiler *c, expr_ty e)
{
	int i, n;

	switch (e->kind) {
        case BoolOp_kind:
		return compiler_boolop(c, e);
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
		return compiler_lambda(c, e);
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
		return compiler_listcomp(c, e);
        case Compare_kind:
		return compiler_compare(c, e);
        case Call_kind:
		return compiler_call(c, e);
        case Repr_kind:
		VISIT(c, expr, e->v.Repr.value);
		ADDOP(c, UNARY_CONVERT);
		break;
        case Num_kind:
		ADDOP_O(c, LOAD_CONST, e->v.Num.n, consts);
		break;
        case Str_kind:
		ADDOP_O(c, LOAD_CONST, e->v.Str.s, consts);
		break;
	/* The following exprs can be assignment targets. */
        case Attribute_kind:
		if (e->v.Attribute.ctx != AugStore)
			VISIT(c, expr, e->v.Attribute.value);
		switch (e->v.Attribute.ctx) {
		case AugLoad:
			ADDOP(c, DUP_TOP);
			/* Fall through to load */
		case Load:
			ADDOP_NAME(c, LOAD_ATTR, e->v.Attribute.attr, names);
			break;
		case AugStore:
			ADDOP(c, ROT_TWO);
			/* Fall through to save */
		case Store:
			ADDOP_NAME(c, STORE_ATTR, e->v.Attribute.attr, names);
			break;
		case Del:
			ADDOP_NAME(c, DELETE_ATTR, e->v.Attribute.attr, names);
			break;
		case Param:
			assert(0);
			break;
		}
		break;
        case Subscript_kind:
		switch (e->v.Subscript.ctx) {
		case AugLoad:
			VISIT(c, expr, e->v.Subscript.value);
			VISIT_SLICE(c, e->v.Subscript.slice, AugLoad);
			break;
		case Load:
			VISIT(c, expr, e->v.Subscript.value);
			VISIT_SLICE(c, e->v.Subscript.slice, Load);
			break;
		case AugStore:
			VISIT_SLICE(c, e->v.Subscript.slice, AugStore);
			break;
		case Store:
			VISIT(c, expr, e->v.Subscript.value);
			VISIT_SLICE(c, e->v.Subscript.slice, Store);
			break;
		case Del:
			VISIT(c, expr, e->v.Subscript.value);
			VISIT_SLICE(c, e->v.Subscript.slice, Del);
			break;
		case Param:
			assert(0);
			break;
		}
		break;
        case Name_kind:
		return compiler_nameop(c, e->v.Name.id, e->v.Name.ctx);
	/* child nodes of List and Tuple will have expr_context set */
        case List_kind:
		return compiler_list(c, e);
        case Tuple_kind:
		return compiler_tuple(c, e);
	}
	return 1;
}

static int
compiler_augassign(struct compiler *c, stmt_ty s)
{
	expr_ty e = s->v.AugAssign.target;
	expr_ty auge;

	assert(s->kind == AugAssign_kind);

	switch (e->kind) {
	case Attribute_kind:
		auge = Attribute(e->v.Attribute.value, e->v.Attribute.attr,
				 AugLoad);
                if (auge == NULL)
                    return 0;
		VISIT(c, expr, auge);
		VISIT(c, expr, s->v.AugAssign.value);
		ADDOP(c, inplace_binop(c, s->v.AugAssign.op));
		auge->v.Attribute.ctx = AugStore;
		VISIT(c, expr, auge);
		free(auge);
		break;
	case Subscript_kind:
		auge = Subscript(e->v.Subscript.value, e->v.Subscript.slice,
				 AugLoad);
                if (auge == NULL)
                    return 0;
		VISIT(c, expr, auge);
		VISIT(c, expr, s->v.AugAssign.value);
		ADDOP(c, inplace_binop(c, s->v.AugAssign.op));
                auge->v.Subscript.ctx = AugStore;
		VISIT(c, expr, auge);
		free(auge);
	    break;
	case Name_kind:
		VISIT(c, expr, s->v.AugAssign.target);
		VISIT(c, expr, s->v.AugAssign.value);
		ADDOP(c, inplace_binop(c, s->v.AugAssign.op));
		return compiler_nameop(c, e->v.Name.id, Store);
	default:
	    fprintf(stderr, "invalid node type for augmented assignment\n");
	    return 0;
	}
	return 1;
}

static int
compiler_push_fblock(struct compiler *c, enum fblocktype t, int b)
{
	struct fblockinfo *f;
	if (c->u->u_nfblocks >= CO_MAXBLOCKS)
		return 0;
	f = &c->u->u_fblock[c->u->u_nfblocks++];
	f->fb_type = t;
	f->fb_block = b;
	return 1;
}

static void
compiler_pop_fblock(struct compiler *c, enum fblocktype t, int b)
{
	struct compiler_unit *u = c->u;
	assert(u->u_nfblocks > 0);
	u->u_nfblocks--;
	assert(u->u_fblock[u->u_nfblocks].fb_type == t);
	assert(u->u_fblock[u->u_nfblocks].fb_block == b);
}

/* Raises a SyntaxError and returns 0.
   If something goes wrong, a different exception may be raised.
*/

static int
compiler_error(struct compiler *c, const char *errstr)
{
	PyObject *loc;
	PyObject *u = NULL, *v = NULL;

	loc = PyErr_ProgramText(c->c_filename, c->u->u_lineno);
	if (!loc) {
		Py_INCREF(Py_None);
		loc = Py_None;
	}
	u = Py_BuildValue("(ziOO)", c->c_filename, c->u->u_lineno,
			  Py_None, loc);
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
compiler_handle_subscr(struct compiler *c, const char *kind, 
                       expr_context_ty ctx) {
    int op;

    /* XXX this code is duplicated */
    switch (ctx) {
    case AugLoad: /* fall through to Load */
    case Load:    op = BINARY_SUBSCR; break;
    case AugStore:/* fall through to Store */
    case Store:   op = STORE_SUBSCR; break;
    case Del:     op = DELETE_SUBSCR; break;
    case Param:
        fprintf(stderr, "invalid %s kind %d in compiler_visit_slice\n", 
                kind, ctx);
        return 0;
    }
    if (ctx == AugLoad) {
        ADDOP_I(c, DUP_TOPX, 2);
    }
    else if (ctx == AugStore) {
        ADDOP(c, ROT_THREE);
    }
    ADDOP(c, op);
    return 1;
}

static int
compiler_slice(struct compiler *c, slice_ty s, expr_context_ty ctx)
{
	int n = 2;
	assert(s->kind == Slice_kind);

	/* only handles the cases where BUILD_SLICE is emitted */
	if (s->v.Slice.lower) {
                VISIT(c, expr, s->v.Slice.lower);
	}
	else {
                ADDOP_O(c, LOAD_CONST, Py_None, consts);
	}
                
	if (s->v.Slice.upper) {
                VISIT(c, expr, s->v.Slice.upper);
	}
	else {
                ADDOP_O(c, LOAD_CONST, Py_None, consts);
	}

	if (s->v.Slice.step) {
		n++;
		VISIT(c, expr, s->v.Slice.step);
	}
	ADDOP_I(c, BUILD_SLICE, n);
	return 1;
}

static int
compiler_simple_slice(struct compiler *c, slice_ty s, expr_context_ty ctx)
{
	int op, slice_offset = 0, stack_count = 0;

	assert(s->v.Slice.step == NULL);
	if (s->v.Slice.lower) {
		slice_offset++;
		stack_count++;
		if (ctx != AugStore) 
			VISIT(c, expr, s->v.Slice.lower);
	}
	if (s->v.Slice.upper) {
		slice_offset += 2;
		stack_count++;
		if (ctx != AugStore) 
			VISIT(c, expr, s->v.Slice.upper);
	}

 	if (ctx == AugLoad) {
 		switch (stack_count) {
 		case 0: ADDOP(c, DUP_TOP); break;
 		case 1: ADDOP_I(c, DUP_TOPX, 2); break;
 		case 2: ADDOP_I(c, DUP_TOPX, 3); break;
 		}
  	}
 	else if (ctx == AugStore) {
 		switch (stack_count) {
 		case 0: ADDOP(c, ROT_TWO); break;
 		case 1: ADDOP(c, ROT_THREE); break;
 		case 2: ADDOP(c, ROT_FOUR); break;
  		}
  	}

	switch (ctx) {
	case AugLoad: /* fall through to Load */
	case Load: op = SLICE; break;
	case AugStore:/* fall through to Store */
	case Store: op = STORE_SLICE; break;
	case Del: op = DELETE_SLICE; break;
	case Param:  /* XXX impossible? */
		fprintf(stderr, "param invalid\n");
		assert(0);
	}

	ADDOP(c, op + slice_offset);
	return 1;
}

static int
compiler_visit_nested_slice(struct compiler *c, slice_ty s, 
			    expr_context_ty ctx)
{
	switch (s->kind) {
	case Ellipsis_kind:
		ADDOP_O(c, LOAD_CONST, Py_Ellipsis, consts);
		break;
	case Slice_kind:
		return compiler_slice(c, s, ctx);
		break;
	case Index_kind:
		VISIT(c, expr, s->v.Index.value);
		break;
	case ExtSlice_kind:
		assert(0);
		break;
	}
	return 1;
}


static int
compiler_visit_slice(struct compiler *c, slice_ty s, expr_context_ty ctx)
{
	switch (s->kind) {
	case Ellipsis_kind:
		ADDOP_O(c, LOAD_CONST, Py_Ellipsis, consts);
		break;
	case Slice_kind:
		if (!s->v.Slice.step) 
			return compiler_simple_slice(c, s, ctx);
                if (!compiler_slice(c, s, ctx))
			return 0;
		if (ctx == AugLoad) {
			ADDOP_I(c, DUP_TOPX, 2);
		}
		else if (ctx == AugStore) {
			ADDOP(c, ROT_THREE);
		}
		return compiler_handle_subscr(c, "slice", ctx);
		break;
	case ExtSlice_kind: {
		int i, n = asdl_seq_LEN(s->v.ExtSlice.dims);
		for (i = 0; i < n; i++) {
			slice_ty sub = asdl_seq_GET(s->v.ExtSlice.dims, i);
			if (!compiler_visit_nested_slice(c, sub, ctx))
				return 0;
		}
		ADDOP_I(c, BUILD_TUPLE, n);
                return compiler_handle_subscr(c, "extended slice", ctx);
		break;
	}
	case Index_kind:
                if (ctx != AugStore)
			VISIT(c, expr, s->v.Index.value);
                return compiler_handle_subscr(c, "index", ctx);
	}
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

	if (block >= c->u->u_nblocks)
		return;
	b = c->u->u_blocks[block];
	if (b->b_seen)
		return;
	b->b_seen = 1;
	if (b->b_next)
		dfs(c, b->b_next, a);
	for (i = 0; i < b->b_iused; i++) {
		instr = &b->b_instr[i];
		if (instr->i_jrel || instr->i_jabs)
			dfs(c, instr->i_target, a);
	}
	a->a_postorder[a->a_nblocks++] = block;
}

int
stackdepth_walk(struct compiler *c, int block, int depth, int maxdepth)
{
	int i;
	struct instr *instr;
	struct basicblock *b;
	b = c->u->u_blocks[block];
	if (b->b_seen || b->b_startdepth >= depth)
		return maxdepth;
	b->b_seen = 1;
	b->b_startdepth = depth;
	fprintf(stderr, "block %d\n", block);
	for (i = 0; i < b->b_iused; i++) {
		instr = &b->b_instr[i];
		depth += opcode_stack_effect(instr->i_opcode, instr->i_oparg);
		if (depth > maxdepth)
			maxdepth = depth;
		fprintf(stderr, "  %14s %3d %3d (%d)\n", 
			opnames[instr->i_opcode], depth, maxdepth,
			instr->i_lineno);
		assert(depth >= 0); /* invalid code or bug in stackdepth() */
		if (instr->i_jrel || instr->i_jabs) {
			maxdepth = stackdepth_walk(c, instr->i_target,
						   depth, maxdepth);
			if (instr->i_opcode == JUMP_ABSOLUTE ||
			    instr->i_opcode == JUMP_FORWARD) {
				goto out; /* remaining code is dead */
			}
		}
	}
	if (b->b_next)
		maxdepth = stackdepth_walk(c, b->b_next, depth, maxdepth);
out:
	b->b_seen = 0;
	return maxdepth;
}

/* Find the flow path that needs the largest stack.  We assume that
 * cycles in the flow graph have no net effect on the stack depth.
 */
static int
stackdepth(struct compiler *c)
{
	int i;
	for (i = 0; i < c->u->u_nblocks; i++) {
		c->u->u_blocks[i]->b_seen = 0;
		c->u->u_blocks[i]->b_startdepth = INT_MIN;
	}
	return stackdepth_walk(c, 0, 0, 0);
}

static int
assemble_init(struct assembler *a, int nblocks)
{
	memset(a, 0, sizeof(struct assembler));
	a->a_lineno = 1;
	a->a_bytecode = PyString_FromStringAndSize(NULL, DEFAULT_CODE_SIZE);
	if (!a->a_bytecode)
		return 0;
	a->a_lnotab = PyString_FromStringAndSize(NULL, DEFAULT_LNOTAB_SIZE);
	if (!a->a_lnotab)
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
	Py_XDECREF(a->a_lnotab);
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
	int i;
	int size = 0;

	for (i = 0; i < b->b_iused; i++)
		size += instrsize(&b->b_instr[i]);
	return size;
}

/* Produce output that looks rather like dis module output. */

static void
assemble_display(struct assembler *a, struct instr *i)
{
    /* Dispatch the simple case first. */
    if (!i->i_hasarg) {
	    fprintf(stderr, "%5d %-20.20s     %d\n",
		    a->a_offset, opnames[i->i_opcode], i->i_lineno);
	    return;
    }
    
    fprintf(stderr, "%5d %-20.20s %3d %d",
	    a->a_offset, opnames[i->i_opcode], i->i_oparg, i->i_lineno);
    if (i->i_jrel) 
	    fprintf(stderr, " (to %d)", a->a_offset + i->i_oparg + 3);
    fprintf(stderr, "\n");
}

/* All about a_lnotab.

c_lnotab is an array of unsigned bytes disguised as a Python string.
It is used to map bytecode offsets to source code line #s (when needed
for tracebacks).

The array is conceptually a list of
    (bytecode offset increment, line number increment)
pairs.  The details are important and delicate, best illustrated by example:

    byte code offset    source code line number
        0		    1
        6		    2
       50		    7
      350                 307
      361                 308

The first trick is that these numbers aren't stored, only the increments
from one row to the next (this doesn't really work, but it's a start):

    0, 1,  6, 1,  44, 5,  300, 300,  11, 1

The second trick is that an unsigned byte can't hold negative values, or
values larger than 255, so (a) there's a deep assumption that byte code
offsets and their corresponding line #s both increase monotonically, and (b)
if at least one column jumps by more than 255 from one row to the next, more
than one pair is written to the table. In case #b, there's no way to know
from looking at the table later how many were written.  That's the delicate
part.  A user of c_lnotab desiring to find the source line number
corresponding to a bytecode address A should do something like this

    lineno = addr = 0
    for addr_incr, line_incr in c_lnotab:
        addr += addr_incr
        if addr > A:
            return lineno
        lineno += line_incr

In order for this to work, when the addr field increments by more than 255,
the line # increment in each pair generated must be 0 until the remaining addr
increment is < 256.  So, in the example above, com_set_lineno should not (as
was actually done until 2.2) expand 300, 300 to 255, 255,  45, 45, but to
255, 0,  45, 255,  0, 45.
*/

static int
assemble_lnotab(struct assembler *a, struct instr *i)
{
	int d_bytecode, d_lineno;
	int len;
	char *lnotab;

	d_bytecode = a->a_offset - a->a_lineno_off;
	d_lineno = i->i_lineno - a->a_lineno;

	assert(d_bytecode >= 0);
	assert(d_lineno >= 0);

	if (d_lineno == 0)
		return 1;

	if (d_bytecode > 255) {
		int i, nbytes, ncodes = d_bytecode / 255;
		nbytes = a->a_lnotab_off + 2 * ncodes;
		len = PyString_GET_SIZE(a->a_lnotab);
		if (nbytes >= len) {
			if (len * 2 < nbytes)
				len = nbytes;
			else
				len *= 2;
			if (_PyString_Resize(&a->a_lnotab, len) < 0)
				return 0;
		}
		lnotab = PyString_AS_STRING(a->a_lnotab) + a->a_lnotab_off;
		for (i = 0; i < ncodes; i++) {
			*lnotab++ = 255;
			*lnotab++ = 0;
		}
		d_bytecode -= ncodes * 255;
		a->a_lnotab_off += ncodes * 2;
	}
	assert(d_bytecode <= 255);
	if (d_lineno > 255) {
		int i, nbytes, ncodes = d_lineno / 255;
		nbytes = a->a_lnotab_off + 2 * ncodes;
		len = PyString_GET_SIZE(a->a_lnotab);
		if (nbytes >= len) {
			if (len * 2 < nbytes)
				len = nbytes;
			else
				len *= 2;
			if (_PyString_Resize(&a->a_lnotab, len) < 0)
				return 0;
		}
		lnotab = PyString_AS_STRING(a->a_lnotab) + a->a_lnotab_off;
		*lnotab++ = 255;
		*lnotab++ = d_bytecode;
		d_bytecode = 0;
		for (i = 1; i < ncodes; i++) {
			*lnotab++ = 255;
			*lnotab++ = 0;
		}
		d_lineno -= ncodes * 255;
		a->a_lnotab_off += ncodes * 2;
	}

	if (d_bytecode)	{
		len = PyString_GET_SIZE(a->a_lnotab);
		if (a->a_lnotab_off + 2 >= len) {
			if (_PyString_Resize(&a->a_lnotab, len * 2) < 0)
				return 0;
		}
		lnotab = PyString_AS_STRING(a->a_lnotab) + a->a_lnotab_off;
		a->a_lnotab_off += 2;
		*lnotab++ = d_bytecode;
		*lnotab++ = d_lineno;
	}
	a->a_lineno = i->i_lineno;
	a->a_lineno_off = a->a_offset;
	return 1;
}

/* assemble_emit()
   Extend the bytecode with a new instruction.
   Update lnotab if necessary.
*/

static int
assemble_emit(struct assembler *a, struct instr *i)
{
	int arg = 0, size = 0, ext = i->i_oparg >> 16;
	int len = PyString_GET_SIZE(a->a_bytecode);
	char *code;

	assemble_display(a, i);
	if (!i->i_hasarg)
		size = 1;
	else {
		if (ext)
			size = 6;
		else
			size = 3;
		arg = i->i_oparg;
	}
	if (i->i_lineno && !assemble_lnotab(a, i))
			return 0;
	if (a->a_offset + size >= len) {
		if (_PyString_Resize(&a->a_bytecode, len * 2) < 0)
		    return 0;
	}
	code = PyString_AS_STRING(a->a_bytecode) + a->a_offset;
	a->a_offset += size;
	if (ext > 0) {
	    *code++ = (char)EXTENDED_ARG;
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
	blockoff = malloc(sizeof(int) * c->u->u_nblocks);
	if (!blockoff)
		return 0;
	/* blockoff computed in dfs postorder, but stored using
	   c_blocks[] indices.
	*/
	assert(a->a_nblocks <= c->u->u_nblocks);
	for (i = a->a_nblocks - 1; i >= 0; i--) {
		int block = a->a_postorder[i];
		bsize = blocksize(c->u->u_blocks[block]);
		blockoff[block] = totsize;
		totsize += bsize;
	}
	for (i = 0; i < c->u->u_nblocks; i++) {
		struct basicblock *b = c->u->u_blocks[i];
		bsize = blockoff[i];
		for (j = 0; j < b->b_iused; j++) {
			struct instr *instr = &b->b_instr[j];
			/* Relative jumps are computed relative to
			   the instruction pointer after fetching
			   the jump instruction.
			*/
			bsize += instrsize(instr);
			if (instr->i_jabs)
				instr->i_oparg = blockoff[instr->i_target];
			else if (instr->i_jrel) {
				int delta = blockoff[instr->i_target] - bsize;
				instr->i_oparg = delta;
			}
		}
	}
	free(blockoff);

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
                k = PyTuple_GET_ITEM(k, 0);
		Py_INCREF(k);
		assert((i - offset) < size);
                assert((i - offset) >= 0);
		PyTuple_SET_ITEM(tuple, i - offset, k);
	}
	return tuple;
}

static int
compute_code_flags(struct compiler *c)
{
	PySTEntryObject *ste = c->u->u_ste;
	int flags = 0, n;
	if (ste->ste_type != ModuleBlock)
		flags |= CO_NEWLOCALS;
	if (ste->ste_type == FunctionBlock) {
		if (ste->ste_optimized)
			flags |= CO_OPTIMIZED;
		if (ste->ste_nested)
			flags |= CO_NESTED;
		if (ste->ste_generator)
			flags |= CO_GENERATOR;
	}
	if (ste->ste_varargs)
		flags |= CO_VARARGS;
	if (ste->ste_varkeywords)
		flags |= CO_VARKEYWORDS;
	if (ste->ste_generator)
		flags |= CO_GENERATOR;
        if (c->c_flags->cf_flags & CO_FUTURE_DIVISION)
                flags |= CO_FUTURE_DIVISION;
	n = PyDict_Size(c->u->u_freevars);
	if (n < 0)
	    return -1;
	if (n == 0) {
	    n = PyDict_Size(c->u->u_cellvars);
	    if (n < 0)
		return -1;
	    if (n == 0) {
		flags |= CO_NOFREE;
	    }
	}

	return flags;
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
	PyObject *freevars = NULL;
	PyObject *cellvars = NULL;
	int nlocals, flags;

	consts = dict_keys_inorder(c->u->u_consts, 0);
	names = dict_keys_inorder(c->u->u_names, 0);
	varnames = dict_keys_inorder(c->u->u_varnames, 0);
	if (!consts || !names || !varnames)
		goto error;
      
        cellvars = dict_keys_inorder(c->u->u_cellvars, 0);
        if (!cellvars)
            goto error;
        freevars = dict_keys_inorder(c->u->u_freevars, PyTuple_Size(cellvars));
        if (!freevars)
            goto error;
	filename = PyString_FromString(c->c_filename);
	if (!filename)
		goto error;

        nlocals = PyDict_Size(c->u->u_varnames);
	flags = compute_code_flags(c);
	if (flags < 0)
	    goto error;
	co = PyCode_New(c->u->u_argcount, nlocals, stackdepth(c), flags,
			a->a_bytecode, consts, names, varnames,
			freevars, cellvars,
			filename, c->u->u_name,
			c->u->u_firstlineno,
			a->a_lnotab);
 error:
	Py_XDECREF(consts);
	Py_XDECREF(names);
	Py_XDECREF(varnames);
	Py_XDECREF(filename);
	Py_XDECREF(name);
	Py_XDECREF(freevars);
	Py_XDECREF(cellvars);
	return co;
}

static PyCodeObject *
assemble(struct compiler *c, int addNone)
{
	struct assembler a;
	int i, j;
	PyCodeObject *co = NULL;

	/* Make sure every block that falls off the end returns None.
	   XXX NEXT_BLOCK() isn't quite right, because if the last
	   block ends with a jump or return b_next shouldn't set.
	 */
	NEXT_BLOCK(c);
        if (addNone)
            ADDOP_O(c, LOAD_CONST, Py_None, consts);
	ADDOP(c, RETURN_VALUE);

	if (!assemble_init(&a, c->u->u_nblocks))
		goto error;
	dfs(c, 0, &a);

	/* Can't modify the bytecode after computing jump offsets. */
	if (!assemble_jump_offsets(&a, c))
		goto error;

	/* Emit code in reverse postorder from dfs. */
	for (i = a.a_nblocks - 1; i >= 0; i--) {
		struct basicblock *b = c->u->u_blocks[a.a_postorder[i]];
		fprintf(stderr, 
                        "\nblock %d: order=%d used=%d alloc=%d next=%d\n",
			i, a.a_postorder[i], b->b_iused, b->b_ialloc,
			b->b_next);
		for (j = 0; j < b->b_iused; j++)
			if (!assemble_emit(&a, &b->b_instr[j]))
				goto error;
	}
	fprintf(stderr, "\n");

	if (_PyString_Resize(&a.a_lnotab, a.a_lnotab_off) < 0)
		goto error;
	if (_PyString_Resize(&a.a_bytecode, a.a_offset) < 0)
		goto error;

	co = makecode(c, &a);
 error:
	assemble_free(&a);
	return co;
}

static char *opnames[] = {
	"STOP_CODE",
	"POP_TOP",
	"ROT_TWO",
	"ROT_THREE",
	"DUP_TOP",
	"ROT_FOUR",
	"<6>",
	"<7>",
	"<8>",
	"<9>",
	"UNARY_POSITIVE",
	"UNARY_NEGATIVE",
	"UNARY_NOT",
	"UNARY_CONVERT",
	"<14>",
	"UNARY_INVERT",
	"<16>",
	"<17>",
	"<18>",
	"BINARY_POWER",
	"BINARY_MULTIPLY",
	"BINARY_DIVIDE",
	"BINARY_MODULO",
	"BINARY_ADD",
	"BINARY_SUBTRACT",
	"BINARY_SUBSCR",
	"BINARY_FLOOR_DIVIDE",
	"BINARY_TRUE_DIVIDE",
	"INPLACE_FLOOR_DIVIDE",
	"INPLACE_TRUE_DIVIDE",
	"SLICE+0",
	"SLICE+1",
	"SLICE+2",
	"SLICE+3",
	"<34>",
	"<35>",
	"<36>",
	"<37>",
	"<38>",
	"<39>",
	"STORE_SLICE+0",
	"STORE_SLICE+1",
	"STORE_SLICE+2",
	"STORE_SLICE+3",
	"<44>",
	"<45>",
	"<46>",
	"<47>",
	"<48>",
	"<49>",
	"DELETE_SLICE+0",
	"DELETE_SLICE+1",
	"DELETE_SLICE+2",
	"DELETE_SLICE+3",
	"<54>",
	"INPLACE_ADD",
	"INPLACE_SUBTRACT",
	"INPLACE_MULTIPLY",
	"INPLACE_DIVIDE",
	"INPLACE_MODULO",
	"STORE_SUBSCR",
	"DELETE_SUBSCR",
	"BINARY_LSHIFT",
	"BINARY_RSHIFT",
	"BINARY_AND",
	"BINARY_XOR",
	"BINARY_OR",
	"INPLACE_POWER",
	"GET_ITER",
	"<69>",
	"PRINT_EXPR",
	"PRINT_ITEM",
	"PRINT_NEWLINE",
	"PRINT_ITEM_TO",
	"PRINT_NEWLINE_TO",
	"INPLACE_LSHIFT",
	"INPLACE_RSHIFT",
	"INPLACE_AND",
	"INPLACE_XOR",
	"INPLACE_OR",
	"BREAK_LOOP",
	"<81>",
	"LOAD_LOCALS",
	"RETURN_VALUE",
	"IMPORT_STAR",
	"EXEC_STMT",
	"YIELD_VALUE",
	"POP_BLOCK",
	"END_FINALLY",
	"BUILD_CLASS",
	"STORE_NAME",
	"DELETE_NAME",
	"UNPACK_SEQUENCE",
	"FOR_ITER",
	"<94>",
	"STORE_ATTR",
	"DELETE_ATTR",
	"STORE_GLOBAL",
	"DELETE_GLOBAL",
	"DUP_TOPX",
	"LOAD_CONST",
	"LOAD_NAME",
	"BUILD_TUPLE",
	"BUILD_LIST",
	"BUILD_MAP",
	"LOAD_ATTR",
	"COMPARE_OP",
	"IMPORT_NAME",
	"IMPORT_FROM",
	"<109>",
	"JUMP_FORWARD",
	"JUMP_IF_FALSE",
	"JUMP_IF_TRUE",
	"JUMP_ABSOLUTE",
	"<114>",
	"<115>",
	"LOAD_GLOBAL",
	"<117>",
	"<118>",
	"CONTINUE_LOOP",
	"SETUP_LOOP",
	"SETUP_EXCEPT",
	"SETUP_FINALLY",
	"<123>",
	"LOAD_FAST",
	"STORE_FAST",
	"DELETE_FAST",
	"<127>",
	"<128>",
	"<129>",
	"RAISE_VARARGS",
	"CALL_FUNCTION",
	"MAKE_FUNCTION",
	"BUILD_SLICE",
	"MAKE_CLOSURE",
	"LOAD_CLOSURE",
	"LOAD_DEREF",
	"STORE_DEREF",
	"<138>",
	"<139>",
	"CALL_FUNCTION_VAR",
	"CALL_FUNCTION_KW",
	"CALL_FUNCTION_VAR_KW",
	"EXTENDED_ARG",
};
