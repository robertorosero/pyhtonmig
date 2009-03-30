#ifdef __cplusplus
extern "C" {
#endif

PyAPI_FUNC(double) _Py_dg_strtod(const char *str, char **ptr);
PyAPI_FUNC(char *) _Py_dg_dtoa(double d, int mode, int ndigits,
                        int *decpt, int *sign, char **rve);

#ifdef __cplusplus
}
#endif
