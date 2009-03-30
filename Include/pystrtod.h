#ifndef Py_STRTOD_H
#define Py_STRTOD_H

#ifdef __cplusplus
extern "C" {
#endif


PyAPI_FUNC(double) PyOS_ascii_strtod(const char *str, char **ptr);
PyAPI_FUNC(double) PyOS_ascii_atof(const char *str);
PyAPI_FUNC(char *) PyOS_ascii_formatd(char *buffer, size_t buf_len,  const char *format, double d);
PyAPI_FUNC(char *) PyOS_double_to_string(double val,
                                         int mode,
                                         char format_code,
                                         int precision,
                                         int sign,
                                         int add_dot_0_if_integer);



#ifdef __cplusplus
}
#endif

#endif /* !Py_STRTOD_H */
