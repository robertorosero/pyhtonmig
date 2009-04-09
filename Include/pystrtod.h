#ifndef Py_STRTOD_H
#define Py_STRTOD_H

#ifdef __cplusplus
extern "C" {
#endif


PyAPI_FUNC(double) PyOS_ascii_strtod(const char *str, char **ptr);
PyAPI_FUNC(double) PyOS_ascii_atof(const char *str);
PyAPI_FUNC(char *) PyOS_ascii_formatd(char *buffer, size_t buf_len,  const char *format, double d);

/* The caller is responsible for calling PyMem_Free to free the buffer
   that's is returned. */
PyAPI_FUNC(char *) PyOS_double_to_string(double val,
                                         char format_code,
                                         int precision,
                                         int flags);


#define Py_DTSF_SIGN      0x01 /* always add the sign */
#define Py_DTSF_ADD_DOT_0 0x02 /* if the result is an integer add ".0" */
#define Py_DTSF_ALT       0x04 /* "alternate" formatting. it's format_code
                                  specific */

#ifdef __cplusplus
}
#endif

#endif /* !Py_STRTOD_H */
