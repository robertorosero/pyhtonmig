#ifndef Py_ASDL_H
#define Py_ASDL_H

typedef PyObject * identifier;
typedef PyObject * string;
typedef PyObject * object;

#include <stdbool.h>

/* It would be nice if the code generated by asdl_c.py was completely
   independent of Python, but it is a goal the requires too much work
   at this stage.  So, for example, I'll represent identifiers as
   interned Python strings.
*/

/* XXX A sequence should be typed so that its use can be typechecked. */

/* XXX We shouldn't pay for offset when we don't need APPEND. */

typedef struct {
    int size;
    int offset;
    void *elements[1];
} asdl_seq;

asdl_seq *asdl_seq_new(int size);
void asdl_seq_free(asdl_seq *);

#define asdl_seq_GET(S, I) (S)->elements[(I)]
#define asdl_seq_SET(S, I, V) (S)->elements[I] = (V)
#define asdl_seq_APPEND(S, V) (S)->elements[(S)->offset++] = (V)
#define asdl_seq_LEN(S) ((S) == NULL ? 0 : (S)->size)

/* Routines to marshal the basic types. */
int marshal_write_int(PyObject **, int *, int);
int marshal_write_bool(PyObject **, int *, bool);
int marshal_write_identifier(PyObject **, int *, identifier);
int marshal_write_string(PyObject **, int *, string);
int marshal_write_object(PyObject **, int *, object);

#endif /* !Py_ASDL_H */
