/*
 * Declarations shared between the different parts of the io module
 */

extern PyTypeObject PyFileIO_Type;
extern PyTypeObject PyBytesIO_Type;
extern PyTypeObject PyIOBase_Type;
extern PyTypeObject PyRawIOBase_Type;
extern PyTypeObject PyBufferedIOBase_Type;
extern PyTypeObject PyBufferedReader_Type;
extern PyTypeObject PyBufferedWriter_Type;
extern PyTypeObject PyBufferedRWPair_Type;
extern PyTypeObject PyBufferedRandom_Type;
extern PyTypeObject PyTextIOWrapper_Type;
extern PyTypeObject PyIncrementalNewlineDecoder_Type;

/* These functions are used as METH_NOARGS methods, are normally called
 * with args=NULL, and return a new reference.
 * BUT when args=Py_True is passed, they return a borrowed reference.
 */
extern PyObject* _PyIOBase_checkReadable(PyObject *self, PyObject *args);
extern PyObject* _PyIOBase_checkWritable(PyObject *self, PyObject *args);
extern PyObject* _PyIOBase_checkSeekable(PyObject *self, PyObject *args);
extern PyObject* _PyIOBase_checkClosed(PyObject *self, PyObject *args);

extern PyObject* PyIOExc_UnsupportedOperation;

/* Helper for finalization.
   This function will revive an object ready to be deallocated and try to
   close() it. It returns 0 if the object can be destroyed, or -1 if it
   is alive again. */
extern int _PyIOBase_finalize(PyObject *self);

#define DEFAULT_BUFFER_SIZE (8 * 1024)  /* bytes */

typedef struct {
    PyException_HEAD
    PyObject *myerrno;
    PyObject *strerror;
    PyObject *filename; /* Not used, but part of the IOError object */
    Py_ssize_t written;
} PyBlockingIOErrorObject;
PyObject *PyExc_BlockingIOError;

/*
 * Offset type for positioning.
 */

#if defined(MS_WIN64) || defined(MS_WINDOWS)

/* Windows uses long long for offsets */
typedef PY_LONG_LONG Py_off_t;
# define PyLong_AsOff_t     PyLong_AsLongLong
# define PyLong_FromOff_t   PyLong_FromLongLong
# define PY_OFF_T_MAX       PY_LLONG_MAX
# define PY_OFF_T_MIN       PY_LLONG_MIN

#else

/* Other platforms use off_t */
typedef off_t Py_off_t;
#if (SIZEOF_OFF_T == SIZEOF_SIZE_T)
# define PyLong_AsOff_t     PyLong_AsSsize_t
# define PyLong_FromOff_t   PyLong_FromSsize_t
# define PY_OFF_T_MAX       PY_SSIZE_T_MAX
# define PY_OFF_T_MIN       PY_SSIZE_T_MIN
#elif (SIZEOF_OFF_T == SIZEOF_LONG_LONG)
# define PyLong_AsOff_t     PyLong_AsLongLong
# define PyLong_FromOff_t   PyLong_FromLongLong
# define PY_OFF_T_MAX       PY_LLONG_MAX
# define PY_OFF_T_MIN       PY_LLONG_MIN
#elif (SIZEOF_OFF_T == SIZEOF_LONG)
# define PyLong_AsOff_t     PyLong_AsLong
# define PyLong_FromOff_t   PyLong_FromLong
# define PY_OFF_T_MAX       LONG_MAX
# define PY_OFF_T_MIN       LONG_MIN
#else
# error off_t does not match either size_t, long, or long long!
#endif

#endif

extern Py_off_t PyNumber_AsOff_t(PyObject *item, PyObject *err);

/* Implementation details */

extern PyObject *_PyIO_str_close;
extern PyObject *_PyIO_str_closed;
extern PyObject *_PyIO_str_decode;
extern PyObject *_PyIO_str_encode;
extern PyObject *_PyIO_str_fileno;
extern PyObject *_PyIO_str_flush;
extern PyObject *_PyIO_str_getstate;
extern PyObject *_PyIO_str_isatty;
extern PyObject *_PyIO_str_newlines;
extern PyObject *_PyIO_str_read;
extern PyObject *_PyIO_str_read1;
extern PyObject *_PyIO_str_readable;
extern PyObject *_PyIO_str_readinto;
extern PyObject *_PyIO_str_readline;
extern PyObject *_PyIO_str_seek;
extern PyObject *_PyIO_str_seekable;
extern PyObject *_PyIO_str_tell;
extern PyObject *_PyIO_str_truncate;
extern PyObject *_PyIO_str_writable;
extern PyObject *_PyIO_str_write;
