/* XXX getter, setter, and struct getsetlist need 'Py'-prefixed names */

typedef PyObject *(*getter)(PyObject *, void *);
typedef int (*setter)(PyObject *, PyObject *, void *);

struct getsetlist {
	char *name;
	getter get;
	setter set;
	void *closure;
};

typedef PyObject *(*wrapperfunc)(PyObject *self, PyObject *args, void *wrapped);

struct wrapperbase {
	char *name;
	wrapperfunc wrapper;
	char *doc;
};

extern PyTypeObject PyDescr_Type;

#define PyDescr_Check(d) ((d)->ob_type == &PyDescr_Type)

typedef struct PyDescrObject PyDescrObject;

extern DL_IMPORT(PyObject *) PyDescr_NewMethod(PyTypeObject *, PyMethodDef *);
extern DL_IMPORT(PyObject *) PyDescr_NewMember(PyTypeObject *,
					       struct memberlist *);
extern DL_IMPORT(PyObject *) PyDescr_NewGetSet(PyTypeObject *,
					       struct getsetlist *);
extern DL_IMPORT(PyObject *) PyDescr_NewWrapper(PyTypeObject *,
						struct wrapperbase *, void *);
extern DL_IMPORT(int) PyDescr_IsMethod(PyObject *);
extern DL_IMPORT(int) PyDescr_IsData(PyObject *);

extern DL_IMPORT(PyObject *) PyDictProxy_New(PyObject *);
extern DL_IMPORT(PyObject *) PyWrapper_New(PyDescrObject *, PyObject *);
