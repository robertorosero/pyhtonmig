#include "Python.h"
#include "asdl.h"

asdl_seq *
asdl_seq_new(int size)
{
    asdl_seq *seq = (asdl_seq *)malloc(sizeof(asdl_seq));
    if (!seq)
	return NULL;
    seq->elements = malloc(sizeof(void *) * size);
    if (!seq->elements) {
	free(seq);
	return NULL;
    }
    seq->size = size;
    seq->used = 0;
    return seq;
}

void *
asdl_seq_get(asdl_seq *seq, int offset)
{
    if (offset > seq->used)
	return NULL;
    return seq->elements[offset];
}

int
asdl_seq_append(asdl_seq *seq, void *elt)
{
    if (seq->size == seq->used) {
	void *newptr;
	int nsize = seq->size * 2;
	newptr = realloc(seq->elements, sizeof(void *) * nsize);
	if (!newptr)
	    return 0;
	seq->elements = newptr;
    }
    seq->elements[seq->used++] = elt;
    return 1;
}

void
asdl_seq_free(asdl_seq *seq)
{
    if (seq->elements)
	free(seq->elements);
    free(seq);
}
