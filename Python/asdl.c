#include "Python.h"
#include "asdl.h"

asdl_seq *
asdl_seq_new(int size)
{
    asdl_seq *seq = (asdl_seq *)PyObject_Malloc(sizeof(asdl_seq)
					     + sizeof(void *) * (size - 1));
    if (!seq)
	return NULL;
    seq->size = size;
    seq->offset = 0;
    return seq;
}

void
asdl_seq_free(asdl_seq *seq)
{
    PyObject_Free(seq);
}

#define CHECKSIZE(BUF, OFF, MIN) \
	if ((*OFF + (MIN)) >= PyString_GET_SIZE(*BUF)) { \
		if (_PyString_Resize(BUF, PyString_GET_SIZE(*BUF) * 2) < 0) \
			return 0; \
	} \

int 
marshal_write_int(PyObject **buf, int *offset, int x)
{
	char *s;

	CHECKSIZE(buf, offset, 4)
	s = PyString_AS_STRING(*buf) + (*offset);
	s[0] = (x & 0xff);
	s[1] = (x >> 8) & 0xff;
	s[2] = (x >> 16) & 0xff;
	s[3] = (x >> 24) & 0xff;
	*offset += 4;
	return 1;
}

int 
marshal_write_bool(PyObject **buf, int *offset, bool b)
{
	if (b)
		marshal_write_int(buf, offset, 1);
	else
		marshal_write_int(buf, offset, 0);
	return 1;
}

int 
marshal_write_identifier(PyObject **buf, int *offset, identifier id)
{
	int l = PyString_GET_SIZE(id);
	marshal_write_int(buf, offset, l);
	CHECKSIZE(buf, offset, l);
	memcpy(PyString_AS_STRING(*buf) + *offset,
	       PyString_AS_STRING(id), l);
	*offset += l;
	return 1;
}

int 
marshal_write_string(PyObject **buf, int *offset, string s)
{
	int l = PyString_GET_SIZE(s);
	marshal_write_int(buf, offset, l);
	CHECKSIZE(buf, offset, l);
	memcpy(PyString_AS_STRING(*buf) + *offset,
	       PyString_AS_STRING(s), l);
	*offset += l;
	return 1;
}

int 
marshal_write_object(PyObject **buf, int *offset, object s)
{
	/* XXX */
	return 0;
}
