
/* Memory object interface */

#ifndef Py_MEMORYOBJECT_H
#define Py_MEMORYOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    PyObject_HEAD
    PyObject *base;
    int ndims;
    Py_ssize_t *starts;  /* slice starts */
    Py_ssize_t *stops;   /* slice stops */
    Py_ssize_t *steps;   /* slice steps */
} PyMemoryViewObject;


PyAPI_DATA(PyTypeObject) PyMemoryView_Type;

#define PyMemory_Check(op) (Py_Type(op) == &PyMemoryView_Type)

#define Py_END_OF_MEMORY	(-1)

PyAPI_FUNC(PyObject *) PyObject_GetMemoryView(PyObject *base);

PyAPI_FUNC(PyObject *) PyMemoryView_FromMemory(PyBuffer *info);  
	/* create new if bufptr is NULL 
	    will be a new bytesobject in base */

#ifdef __cplusplus
}
#endif
#endif /* !Py_BUFFEROBJECT_H */
