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

	/* Bail out if an exception is set */
	if (PyErr_Occurred())
		goto exitUnchanged;

	/* Bypass optimization when the lineno table is too complex */
	assert(PyBytes_Check(lineno_obj));
	lineno = (unsigned char*)PyBytes_AS_STRING(lineno_obj);
	tabsiz = PyBytes_GET_SIZE(lineno_obj);
	if (memchr(lineno, 255, tabsiz) != NULL)
		goto exitUnchanged;

	/* Avoid situations where jump retargeting could overflow */
	assert(PyBytes_Check(code));
	codelen = PyBytes_GET_SIZE(code);
	if (codelen > 32700)
		goto exitUnchanged;

	/* Make a modifiable copy of the code string */
	codestr = (unsigned char *)PyMem_Malloc(codelen);
	if (codestr == NULL)
		goto exitUnchanged;
	codestr = (unsigned char *)memcpy(codestr, 
					  PyBytes_AS_STRING(code), codelen);

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

	code = PyBytes_FromStringAndSize((char *)codestr, h);
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
