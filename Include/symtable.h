#ifndef Py_SYMTABLE_H
#define Py_SYMTABLE_H
#ifdef __cplusplus
extern "C" {
#endif

typedef enum _block_type { FunctionBlock, ClassBlock, ModuleBlock }
    block_ty;

struct _symtable_entry;

struct symtable {
	const char *st_filename; /* name of file being compiled */
	struct _symtable_entry *st_cur; /* current symbol table entry */
	PyObject *st_symbols;    /* dictionary of symbol table entries */
        PyObject *st_stack;      /* stack of namespace info */
	PyObject *st_global;     /* borrowed ref to MODULE in st_symbols */
	int st_nblocks;          /* number of blocks */
	char *st_private;        /* name of current class or NULL */
	int st_tmpname;          /* temporary name counter */
	PyFutureFeatures *st_future; /* module's future features */
};

typedef struct _symtable_entry {
	PyObject_HEAD
	PyObject *ste_id;        /* int: key in st_symbols */
	PyObject *ste_symbols;   /* dict: name to flags */
	PyObject *ste_name;      /* string: name of block */
	PyObject *ste_varnames;  /* list of variable names */
	PyObject *ste_children;  /* list of child ids */
	block_ty ste_type;       /* module, class, or function */
	int ste_lineno;          /* first line of block */
	int ste_optimized;       /* true if namespace can't be optimized */
	int ste_nested;          /* true if block is nested */
	int ste_child_free;      /* true if a child block has free variables,
				    including free refs to globals */
	int ste_generator;       /* true if namespace is a generator */
	int ste_opt_lineno;      /* lineno of last exec or import * */
	struct symtable *ste_table;
} PySTEntryObject;

extern DL_IMPORT(PyTypeObject) PySTEntry_Type;

#define PySTEntry_Check(op) ((op)->ob_type == &PySTEntry_Type)

extern DL_IMPORT(PySTEntryObject *) \
	PySTEntry_New(struct symtable *, identifier, block_ty, void *, int);
DL_IMPORT(int) PyST_GetScope(PySTEntryObject *, PyObject *);

DL_IMPORT(struct symtable *) PySymtable_Build(mod_ty, const char *, 
					      PyFutureFeatures *);
DL_IMPORT(PySTEntryObject *) PySymtable_Lookup(struct symtable *, void *);

DL_IMPORT(void) PySymtable_Free(struct symtable *);

/* Flags for def-use information */

#define DEF_GLOBAL 1           /* global stmt */
#define DEF_LOCAL 2            /* assignment in code block */
#define DEF_PARAM 2<<1         /* formal parameter */
#define USE 2<<2               /* name is used */
#define DEF_STAR 2<<3          /* parameter is star arg */
#define DEF_DOUBLESTAR 2<<4    /* parameter is star-star arg */
#define DEF_INTUPLE 2<<5       /* name defined in tuple in parameters */
#define DEF_FREE 2<<6          /* name used but not defined in nested block */
#define DEF_FREE_GLOBAL 2<<7   /* free variable is actually implicit global */
#define DEF_FREE_CLASS 2<<8    /* free variable from class's method */
#define DEF_IMPORT 2<<9        /* assignment occurred via import */

#define DEF_BOUND (DEF_LOCAL | DEF_PARAM | DEF_IMPORT)

/* GLOBAL_EXPLICIT and GLOBAL_IMPLICIT are used internally by the symbol
   table.  GLOBAL is returned from PyST_GetScope() for either of them. 
   It is stored in ste_symbols at bits 12-14.
*/
#define SCOPE_OFF 11
#define SCOPE_MASK 7

#define LOCAL 1
#define GLOBAL_EXPLICIT 2
#define GLOBAL_IMPLICIT 3
#define FREE 4
#define CELL 5

#define OPT_IMPORT_STAR 1
#define OPT_EXEC 2
#define OPT_BARE_EXEC 4

#ifdef __cplusplus
}
#endif
#endif /* !Py_SYMTABLE_H */
