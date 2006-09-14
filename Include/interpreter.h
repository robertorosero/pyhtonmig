/*
   interpreter.h
*/

#ifndef HAVE_INTERPRETER_H
#define HAVE_INTERPRETER_H

#ifdef __cpluspus
extern "C" {
#endif

typedef struct
{
    PyObject_HEAD;
    PyThreadState *tstate;  /* Main thread state for interpreter */
    PyInterpreterState *istate;  /* Interpreter state; for convenience to avoid
				    going through thread state every time. */
} PyInterpreterObject;

#ifdef __cplusplus
}
#endif

#endif /* !HAVE_INTERPRETER_H */
