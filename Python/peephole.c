/* Peephole optimizations for bytecode compiler. */

#include "Python.h"

#include "Python-ast.h"
#include "node.h"
#include "pyarena.h"
#include "ast.h"
#include "code.h"
#include "compile.h"
#include "symtable.h"
#include "opcode.h"

#define GETARG(arr, i) ((int)((arr[i+2]<<8) + arr[i+1]))
#define UNCONDITIONAL_JUMP(op)	(op==JUMP_ABSOLUTE || op==JUMP_FORWARD)
#define ABSOLUTE_JUMP(op) (op==JUMP_ABSOLUTE || op==CONTINUE_LOOP)
#define GETJUMPTGT(arr, i) (GETARG(arr,i) + (ABSOLUTE_JUMP(arr[i]) ? 0 : i+3))
#define SETARG(arr, i, val) arr[i+2] = val>>8; arr[i+1] = val & 255
#define CODESIZE(op)  (HAS_ARG(op) ? 3 : 1)
#define ISBASICBLOCK(blocks, start, bytes) \
	(blocks[start]==blocks[start+bytes-1])

/* Replace LOAD_CONST c1. LOAD_CONST c2 ... LOAD_CONST cn BUILD_TUPLE n
   with	   LOAD_CONST (c1, c2, ... cn).
   The consts table must still be in list form so that the
   new constant (c1, c2, ... cn) can be appended.
   Called with codestr pointing to the first LOAD_CONST.
   Bails out with no change if one or more of the LOAD_CONSTs is missing. 
   Also works for BUILD_LIST when followed by an "in" or "not in" test.
*/
static int
tuple_of_constants(unsigned char *codestr, Py_ssize_t n, PyObject *consts)
{
	PyObject *newconst, *constant;
	Py_ssize_t i, arg, len_consts;

	/* Pre-conditions */
	assert(PyList_CheckExact(consts));
	assert(codestr[n*3] == BUILD_TUPLE || codestr[n*3] == BUILD_LIST);
	assert(GETARG(codestr, (n*3)) == n);
	for (i=0 ; i<n ; i++)
		assert(codestr[i*3] == LOAD_CONST);

	/* Buildup new tuple of constants */
	newconst = PyTuple_New(n);
	if (newconst == NULL)
		return 0;
	len_consts = PyList_GET_SIZE(consts);
	for (i=0 ; i<n ; i++) {
		arg = GETARG(codestr, (i*3));
		assert(arg < len_consts);
		constant = PyList_GET_ITEM(consts, arg);
		Py_INCREF(constant);
		PyTuple_SET_ITEM(newconst, i, constant);
	}

	/* Append folded constant onto consts */
	if (PyList_Append(consts, newconst)) {
		Py_DECREF(newconst);
		return 0;
	}
	Py_DECREF(newconst);

	/* Write NOPs over old LOAD_CONSTS and
	   add a new LOAD_CONST newconst on top of the BUILD_TUPLE n */
	memset(codestr, NOP, n*3);
	codestr[n*3] = LOAD_CONST;
	SETARG(codestr, (n*3), len_consts);
	return 1;
}

static unsigned int *
markblocks(unsigned char *code, Py_ssize_t len)
{
	unsigned int *blocks = (unsigned int *)PyMem_Malloc(len*sizeof(int));
	int i,j, opcode, blockcnt = 0;

	if (blocks == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	memset(blocks, 0, len*sizeof(int));

	/* Mark labels in the first pass */
	for (i=0 ; i<len ; i+=CODESIZE(opcode)) {
		opcode = code[i];
		switch (opcode) {
			case FOR_ITER:
			case JUMP_FORWARD:
			case JUMP_IF_FALSE:
			case JUMP_IF_TRUE:
			case JUMP_ABSOLUTE:
			case CONTINUE_LOOP:
			case SETUP_LOOP:
			case SETUP_EXCEPT:
			case SETUP_FINALLY:
				j = GETJUMPTGT(code, i);
				blocks[j] = 1;
				break;
		}
	}
	/* Build block numbers in the second pass */
	for (i=0 ; i<len ; i++) {
		blockcnt += blocks[i];	/* increment blockcnt over labels */
		blocks[i] = blockcnt;
	}
	return blocks;
}

/* Perform basic peephole optimizations to components of a code object.
   The consts object should still be in list form to allow new constants 
   to be appended.

   To keep the optimizer simple, it bails out (does nothing) for code
   containing extended arguments or that has a length over 32,700.  That 
   allows us to avoid overflow and sign issues.	 Likewise, it bails when
   the lineno table has complex encoding for gaps >= 255.

   Optimizations are restricted to simple transformations occuring within a
   single basic block.	All transformations keep the code size the same or 
   smaller.  For those that reduce size, the gaps are initially filled with 
   NOPs.  Later those NOPs are removed and the jump addresses retargeted in 
   a single pass.  Line numbering is adjusted accordingly. */

PyObject *
PyCode_Optimize(PyObject *code, PyObject* consts, PyObject *names,
                PyObject *lineno_obj)
{
	Py_ssize_t i, j, codelen;
	int nops, h, adj;
	int tgt, tgttgt, opcode;
	unsigned char *codestr = NULL;
	unsigned char *lineno;
	int *addrmap = NULL;
	int new_line, cum_orig_line, last_line, tabsiz;
	int cumlc=0, lastlc=0;	/* Count runs of consecutive LOAD_CONSTs */
	unsigned int *blocks = NULL;
	char *name;

	/* Bail out if an exception is set */
	if (PyErr_Occurred())
		goto exitUnchanged;

	/* Bypass optimization when the lineno table is too complex */
	assert(PyString_Check(lineno_obj));
	lineno = (unsigned char*)PyString_AS_STRING(lineno_obj);
	tabsiz = PyString_GET_SIZE(lineno_obj);
	if (memchr(lineno, 255, tabsiz) != NULL)
		goto exitUnchanged;

	/* Avoid situations where jump retargeting could overflow */
	assert(PyString_Check(code));
	codelen = PyString_GET_SIZE(code);
	if (codelen > 32700)
		goto exitUnchanged;

	/* Make a modifiable copy of the code string */
	codestr = (unsigned char *)PyMem_Malloc(codelen);
	if (codestr == NULL)
		goto exitUnchanged;
	codestr = (unsigned char *)memcpy(codestr, 
					  PyString_AS_STRING(code), codelen);

	/* Verify that RETURN_VALUE terminates the codestring.	This allows
	   the various transformation patterns to look ahead several
	   instructions without additional checks to make sure they are not
	   looking beyond the end of the code string.
	*/
	if (codestr[codelen-1] != RETURN_VALUE)
		goto exitUnchanged;

	/* Mapping to new jump targets after NOPs are removed */
	addrmap = (int *)PyMem_Malloc(codelen * sizeof(int));
	if (addrmap == NULL)
		goto exitUnchanged;

	blocks = markblocks(codestr, codelen);
	if (blocks == NULL)
		goto exitUnchanged;
	assert(PyList_Check(consts));

	for (i=0 ; i<codelen ; i += CODESIZE(codestr[i])) {
		opcode = codestr[i];

		lastlc = cumlc;
		cumlc = 0;

		switch (opcode) {
				/* not a is b -->  a is not b
				   not a in b -->  a not in b
				   not a is not b -->  a is b
				   not a not in b -->  a in b
				*/
			case COMPARE_OP:
				j = GETARG(codestr, i);
				if (j < 6  ||  j > 9  ||
				    codestr[i+3] != UNARY_NOT  || 
				    !ISBASICBLOCK(blocks,i,4))
					continue;
				SETARG(codestr, i, (j^1));
				codestr[i+3] = NOP;
				break;

				/* Replace LOAD_GLOBAL/LOAD_NAME None
                                   with LOAD_CONST None */
			case LOAD_NAME:
			case LOAD_GLOBAL:
				j = GETARG(codestr, i);
				name = PyString_AsString(PyTuple_GET_ITEM(names, j));
				if (name == NULL  ||  strcmp(name, "None") != 0)
					continue;
				for (j=0 ; j < PyList_GET_SIZE(consts) ; j++) {
					if (PyList_GET_ITEM(consts, j) == Py_None)
						break;
				}
				if (j == PyList_GET_SIZE(consts)) {
					if (PyList_Append(consts, Py_None) == -1)
					        goto exitUnchanged;                                        
				}
				assert(PyList_GET_ITEM(consts, j) == Py_None);
				codestr[i] = LOAD_CONST;
				SETARG(codestr, i, j);
				cumlc = lastlc + 1;
				break;

				/* Try to fold tuples of constants (includes a case for lists
				   which are only used for "in" and "not in" tests).
				   Skip over BUILD_SEQN 1 UNPACK_SEQN 1.
				   Replace BUILD_SEQN 2 UNPACK_SEQN 2 with ROT2.
				   Replace BUILD_SEQN 3 UNPACK_SEQN 3 with ROT3 ROT2. */
			case BUILD_TUPLE:
			case BUILD_LIST:
				j = GETARG(codestr, i);
				h = i - 3 * j;
				if (h >= 0  &&
				    j <= lastlc	 &&
				    ((opcode == BUILD_TUPLE && 
				      ISBASICBLOCK(blocks, h, 3*(j+1))) ||
				     (opcode == BUILD_LIST && 
				      codestr[i+3]==COMPARE_OP && 
				      ISBASICBLOCK(blocks, h, 3*(j+2)) &&
				      (GETARG(codestr,i+3)==6 ||
				       GETARG(codestr,i+3)==7))) &&
				    tuple_of_constants(&codestr[h], j, consts)) {
					assert(codestr[i] == LOAD_CONST);
					cumlc = 1;
					break;
				}
				if (codestr[i+3] != UNPACK_SEQUENCE  ||
				    !ISBASICBLOCK(blocks,i,6) ||
				    j != GETARG(codestr, i+3))
					continue;
				if (j == 1) {
					memset(codestr+i, NOP, 6);
				} else if (j == 2) {
					codestr[i] = ROT_TWO;
					memset(codestr+i+1, NOP, 5);
				} else if (j == 3) {
					codestr[i] = ROT_THREE;
					codestr[i+1] = ROT_TWO;
					memset(codestr+i+2, NOP, 4);
				}
				break;

				/* Simplify conditional jump to conditional jump where the
				   result of the first test implies the success of a similar
				   test or the failure of the opposite test.
				   Arises in code like:
				   "if a and b:"
				   "if a or b:"
				   "a and b or c"
				   "(a and b) and c"
				   x:JUMP_IF_FALSE y   y:JUMP_IF_FALSE z  -->  x:JUMP_IF_FALSE z
				   x:JUMP_IF_FALSE y   y:JUMP_IF_TRUE z	 -->  x:JUMP_IF_FALSE y+3
				   where y+3 is the instruction following the second test.
				*/
			case JUMP_IF_FALSE:
			case JUMP_IF_TRUE:
				tgt = GETJUMPTGT(codestr, i);
				j = codestr[tgt];
				if (j == JUMP_IF_FALSE	||  j == JUMP_IF_TRUE) {
					if (j == opcode) {
						tgttgt = GETJUMPTGT(codestr, tgt) - i - 3;
						SETARG(codestr, i, tgttgt);
					} else {
						tgt -= i;
						SETARG(codestr, i, tgt);
					}
					break;
				}
				/* Intentional fallthrough */  

				/* Replace jumps to unconditional jumps */
			case FOR_ITER:
			case JUMP_FORWARD:
			case JUMP_ABSOLUTE:
			case CONTINUE_LOOP:
			case SETUP_LOOP:
			case SETUP_EXCEPT:
			case SETUP_FINALLY:
				tgt = GETJUMPTGT(codestr, i);
				/* Replace JUMP_* to a RETURN into just a RETURN */
				if (UNCONDITIONAL_JUMP(opcode) &&
				    codestr[tgt] == RETURN_VALUE) {
					codestr[i] = RETURN_VALUE;
					memset(codestr+i+1, NOP, 2);
					continue;
				}
				if (!UNCONDITIONAL_JUMP(codestr[tgt]))
					continue;
				tgttgt = GETJUMPTGT(codestr, tgt);
				if (opcode == JUMP_FORWARD) /* JMP_ABS can go backwards */
					opcode = JUMP_ABSOLUTE;
				if (!ABSOLUTE_JUMP(opcode))
					tgttgt -= i + 3;     /* Calc relative jump addr */
				if (tgttgt < 0)		  /* No backward relative jumps */
					continue;
				codestr[i] = opcode;
				SETARG(codestr, i, tgttgt);
				break;

			case EXTENDED_ARG:
				goto exitUnchanged;
		}
	}

	/* Fixup linenotab */
	for (i=0, nops=0 ; i<codelen ; i += CODESIZE(codestr[i])) {
		addrmap[i] = i - nops;
		if (codestr[i] == NOP)
			nops++;
	}
	cum_orig_line = 0;
	last_line = 0;
	for (i=0 ; i < tabsiz ; i+=2) {
		cum_orig_line += lineno[i];
		new_line = addrmap[cum_orig_line];
		assert (new_line - last_line < 255);
		lineno[i] =((unsigned char)(new_line - last_line));
		last_line = new_line;
	}

	/* Remove NOPs and fixup jump targets */
	for (i=0, h=0 ; i<codelen ; ) {
		opcode = codestr[i];
		switch (opcode) {
			case NOP:
				i++;
				continue;

			case JUMP_ABSOLUTE:
			case CONTINUE_LOOP:
				j = addrmap[GETARG(codestr, i)];
				SETARG(codestr, i, j);
				break;

			case FOR_ITER:
			case JUMP_FORWARD:
			case JUMP_IF_FALSE:
			case JUMP_IF_TRUE:
			case SETUP_LOOP:
			case SETUP_EXCEPT:
			case SETUP_FINALLY:
				j = addrmap[GETARG(codestr, i) + i + 3] - addrmap[i] - 3;
				SETARG(codestr, i, j);
				break;
		}
		adj = CODESIZE(opcode);
		while (adj--)
			codestr[h++] = codestr[i++];
	}
	assert(h + nops == codelen);

	code = PyString_FromStringAndSize((char *)codestr, h);
	PyMem_Free(addrmap);
	PyMem_Free(codestr);
	PyMem_Free(blocks);
	return code;

 exitUnchanged:
	if (blocks != NULL)
		PyMem_Free(blocks);
	if (addrmap != NULL)
		PyMem_Free(addrmap);
	if (codestr != NULL)
		PyMem_Free(codestr);
	Py_INCREF(code);
	return code;
}
