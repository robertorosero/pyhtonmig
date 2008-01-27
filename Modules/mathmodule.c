/* Math module -- standard C math library functions, pi and e */

/* Here are some comments from Tim Peters, extracted from the
   discussion attached to http://bugs.python.org/issue1640.  They
   describe the general aims of the math module with respect to
   special values, IEEE-754 floating-point exceptions, and Python
   exceptions.

These are the "spirit of 754" rules:

1. If the mathematical result is a real number, but of magnitude too
large to approximate by a machine float, overflow is signaled and the
result is an infinity (with the appropriate sign).

2. If the mathematical result is a real number, but of magnitude too
small to approximate by a machine float, underflow is signaled and the
result is a zero (with the appropriate sign).

3. At a singularity (a value x such that the limit of f(y) as y
approaches x exists and is an infinity), "divide by zero" is signaled
and the result is an infinity (with the appropriate sign).  This is
complicated a little by that the left-side and right-side limits may
not be the same; e.g., 1/x approaches +inf or -inf as x approaches 0
from the positive or negative directions.  In that specific case, the
sign of the zero determines the result of 1/0.

4. At a point where a function has no defined result in the extended
reals (i.e., the reals plus an infinity or two), invalid operation is
signaled and a NaN is returned.

And these are what Python has historically /tried/ to do (but not
always successfully, as platform libm behavior varies a lot):

For #1, raise OverflowError.

For #2, return a zero (with the appropriate sign if that happens by
accident ;-)).

For #3 and #4, raise ValueError.  It may have made sense to raise
Python's ZeroDivisionError in #3, but historically that's only been
raised for division by zero and mod by zero.

*/

/*
   In general, on an IEEE-754 platform the aim is to follow the C99
   standard, including Annex 'F', whenever possible.  Where the
   standard recommends raising the 'divide-by-zero' or 'invalid'
   floating-point exceptions, Python should raise a ValueError.  Where
   the standard recommends raising 'overflow', Python should raise an
   OverflowError.  In all other circumstances a value should be
   returned.
 */

#include "Python.h"
#include "longintrepr.h" /* just for SHIFT */

/* Call is_error when errno != 0, and where x is the result libm
 * returned.  is_error will usually set up an exception and return
 * true (1), but may return false (0) without setting up an exception.
 */
static int
is_error(double x)
{
	int result = 1;	/* presumption of guilt */
	assert(errno);	/* non-zero errno is a precondition for calling */
	if (errno == EDOM)
		PyErr_SetString(PyExc_ValueError, "math domain error");

	else if (errno == ERANGE) {
		/* ANSI C generally requires libm functions to set ERANGE
		 * on overflow, but also generally *allows* them to set
		 * ERANGE on underflow too.  There's no consistency about
		 * the latter across platforms.
		 * Alas, C99 never requires that errno be set.
		 * Here we suppress the underflow errors (libm functions
		 * should return a zero on underflow, and +- HUGE_VAL on
		 * overflow, so testing the result for zero suffices to
		 * distinguish the cases).
		 */
		if (x)
			PyErr_SetString(PyExc_OverflowError,
					"math range error");
		else
			result = 0;
	}
	else
                /* Unexpected math error */
		PyErr_SetFromErrno(PyExc_ValueError);
	return result;
}

static PyObject *
math_1(PyObject *arg, double (*func) (double), int can_overflow)
{
	int x_is_infinity;
	double x = PyFloat_AsDouble(arg);
	if (x == -1.0 && PyErr_Occurred())
		return NULL;
	/* a NaN input should be returned unscathed */
	if (Py_IS_NAN(x))
		return PyFloat_FromDouble(x);
	x_is_infinity = Py_IS_INFINITY(x);
	errno = 0;
	PyFPE_START_PROTECT("in math_1", return 0);
	x = (*func)(x);
	PyFPE_END_PROTECT(x);

	/* if the result was a NaN then we should be signalling a ValueError;
	   this should only happen in the cases where C99 recommends raising
	   invalid */
	if (Py_IS_NAN(x))
		errno = EDOM;

	/* if the input was finite and the result is an infinity, then either
	   overflow occurred and we should be setting errno to ERANGE so that
	   Python raises an OverflowError, or we were evaluating at a
	   singularity, and we should setting errno to EDOM so that Python
	   raises ValueError.  Currently, none of the functions using math_1
	   have singularities *and* the possibility of overflow :-) */
	if (!x_is_infinity && Py_IS_INFINITY(x))
		errno = can_overflow ? ERANGE : EDOM;

	if (errno && is_error(x))
		return NULL;
	else
		return PyFloat_FromDouble(x);
}

static PyObject *
math_2(PyObject *args, double (*func) (double, double), char *funcname)
{
	PyObject *ox, *oy;
	double x, y;
	if (! PyArg_UnpackTuple(args, funcname, 2, 2, &ox, &oy))
		return NULL;
	x = PyFloat_AsDouble(ox);
	y = PyFloat_AsDouble(oy);
	if ((x == -1.0 || y == -1.0) && PyErr_Occurred())
		return NULL;
        if (Py_IS_NAN(x))
		return PyFloat_FromDouble(x);
	if (Py_IS_NAN(y))
		return PyFloat_FromDouble(y);
	errno = 0;
	PyFPE_START_PROTECT("in math_2", return 0)
	x = (*func)(x, y);
	PyFPE_END_PROTECT(x);
	if (errno && is_error(x))
		return NULL;
	else
		return PyFloat_FromDouble(x);
}

#define FUNC1(funcname, func, can_overflow, docstring)			\
	static PyObject * math_##funcname(PyObject *self, PyObject *args) { \
		return math_1(args, func, can_overflow);		    \
	}\
        PyDoc_STRVAR(math_##funcname##_doc, docstring);

#define FUNC2(funcname, func, docstring) \
	static PyObject * math_##funcname(PyObject *self, PyObject *args) { \
		return math_2(args, func, #funcname); \
	}\
        PyDoc_STRVAR(math_##funcname##_doc, docstring);

FUNC1(acos, acos, 0,
      "acos(x)\n\nReturn the arc cosine (measured in radians) of x.")
FUNC1(acosh, acosh, 0,
      "acosh(x)\n\nReturn the hyperbolic arc cosine (measured in radians) of x.")
FUNC1(asin, asin, 0,
      "asin(x)\n\nReturn the arc sine (measured in radians) of x.")
FUNC1(asinh, asinh, 0,
      "asinh(x)\n\nReturn the hyperbolic arc sine (measured in radians) of x.")
FUNC1(atan, atan, 0,
      "atan(x)\n\nReturn the arc tangent (measured in radians) of x.")
FUNC2(atan2, atan2,
      "atan2(y, x)\n\nReturn the arc tangent (measured in radians) of y/x.\n"
      "Unlike atan(y/x), the signs of both x and y are considered.")
FUNC1(atanh, atanh, 0,
      "atanh(x)\n\nReturn the hyperbolic arc tangent (measured in radians) of x.")
FUNC1(ceil, ceil, 0,
      "ceil(x)\n\nReturn the ceiling of x as a float.\n"
      "This is the smallest integral value >= x.")
FUNC1(cos, cos, 0,
      "cos(x)\n\nReturn the cosine of x (measured in radians).")
FUNC1(cosh, cosh, 1,
      "cosh(x)\n\nReturn the hyperbolic cosine of x.")
FUNC2(copysign, copysign,
      "copysign(x,y)\n\nReturn x with the sign of y.");
FUNC1(exp, exp, 1,
      "exp(x)\n\nReturn e raised to the power of x.")
FUNC1(fabs, fabs, 0,
      "fabs(x)\n\nReturn the absolute value of the float x.")
FUNC1(floor, floor, 0,
      "floor(x)\n\nReturn the floor of x as a float.\n"
      "This is the largest integral value <= x.")
FUNC2(fmod, fmod,
      "fmod(x,y)\n\nReturn fmod(x, y), according to platform C."
      "  x % y may differ.")
FUNC1(sin, sin, 0,
      "sin(x)\n\nReturn the sine of x (measured in radians).")
FUNC1(sinh, sinh, 1,
      "sinh(x)\n\nReturn the hyperbolic sine of x.")
FUNC1(sqrt, sqrt, 0,
      "sqrt(x)\n\nReturn the square root of x.")
FUNC1(tan, tan, 0,
      "tan(x)\n\nReturn the tangent of x (measured in radians).")
FUNC1(tanh, tanh, 0,
      "tanh(x)\n\nReturn the hyperbolic tangent of x.")

static PyObject *
math_frexp(PyObject *self, PyObject *arg)
{
	int i;
	double x = PyFloat_AsDouble(arg);
	if (x == -1.0 && PyErr_Occurred())
		return NULL;
	errno = 0;
	x = frexp(x, &i);
	Py_SET_ERRNO_ON_MATH_ERROR(x);
	if (errno && is_error(x))
		return NULL;
	else
		return Py_BuildValue("(di)", x, i);
}

PyDoc_STRVAR(math_frexp_doc,
"frexp(x)\n"
"\n"
"Return the mantissa and exponent of x, as pair (m, e).\n"
"m is a float and e is an int, such that x = m * 2.**e.\n"
"If x is 0, m and e are both 0.  Else 0.5 <= abs(m) < 1.0.");

static PyObject *
math_ldexp(PyObject *self, PyObject *args)
{
	double x;
	int exp;
	if (! PyArg_ParseTuple(args, "di:ldexp", &x, &exp))
		return NULL;
	errno = 0;
	PyFPE_START_PROTECT("ldexp", return 0)
	x = ldexp(x, exp);
	PyFPE_END_PROTECT(x)
	Py_SET_ERRNO_ON_MATH_ERROR(x);
	if (errno && is_error(x))
		return NULL;
	else
		return PyFloat_FromDouble(x);
}

PyDoc_STRVAR(math_ldexp_doc,
"ldexp(x, i) -> x * (2**i)");

static PyObject *
math_modf(PyObject *self, PyObject *arg)
{
	double y, x = PyFloat_AsDouble(arg);
	if (x == -1.0 && PyErr_Occurred())
		return NULL;
	errno = 0;
	x = modf(x, &y);
	Py_SET_ERRNO_ON_MATH_ERROR(x);
	if (errno && is_error(x))
		return NULL;
	else
		return Py_BuildValue("(dd)", x, y);
}

PyDoc_STRVAR(math_modf_doc,
"modf(x)\n"
"\n"
"Return the fractional and integer parts of x.  Both results carry the sign\n"
"of x.  The integer part is returned as a real.");

/* A decent logarithm is easy to compute even for huge longs, but libm can't
   do that by itself -- loghelper can.  func is log or log10, and name is
   "log" or "log10".  Note that overflow isn't possible:  a long can contain
   no more than INT_MAX * SHIFT bits, so has value certainly less than
   2**(2**64 * 2**16) == 2**2**80, and log2 of that is 2**80, which is
   small enough to fit in an IEEE single.  log and log10 are even smaller.
*/

static PyObject*
loghelper(PyObject* arg, double (*func)(double), char *funcname)
{
	/* If it is long, do it ourselves. */
	if (PyLong_Check(arg)) {
		double x;
		int e;
		x = _PyLong_AsScaledDouble(arg, &e);
		if (x <= 0.0) {
			PyErr_SetString(PyExc_ValueError,
					"math domain error");
			return NULL;
		}
		/* Value is ~= x * 2**(e*SHIFT), so the log ~=
		   log(x) + log(2) * e * SHIFT.
		   CAUTION:  e*SHIFT may overflow using int arithmetic,
		   so force use of double. */
		x = func(x) + (e * (double)SHIFT) * func(2.0);
		return PyFloat_FromDouble(x);
	}

	/* Else let libm handle it by itself. */
	return math_1(arg, func, 0);
}

static PyObject *
math_log(PyObject *self, PyObject *args)
{
	PyObject *arg;
	PyObject *base = NULL;
	PyObject *num, *den;
	PyObject *ans;

	if (!PyArg_UnpackTuple(args, "log", 1, 2, &arg, &base))
		return NULL;

	num = loghelper(arg, log, "log");
	if (num == NULL || base == NULL)
		return num;

	den = loghelper(base, log, "log");
	if (den == NULL) {
		Py_DECREF(num);
		return NULL;
	}

	ans = PyNumber_Divide(num, den);
	Py_DECREF(num);
	Py_DECREF(den);
	return ans;
}

PyDoc_STRVAR(math_log_doc,
"log(x[, base]) -> the logarithm of x to the given base.\n\
If the base not specified, returns the natural logarithm (base e) of x.");

static PyObject *
math_log1p(PyObject *self, PyObject *args)
{
	PyObject *arg;
	PyObject *base = NULL;
	PyObject *num, *den;
	PyObject *ans;

	if (!PyArg_UnpackTuple(args, "log1p", 1, 2, &arg, &base))
		return NULL;

	num = loghelper(arg, log1p, "log");
	if (num == NULL || base == NULL)
		return num;

	den = loghelper(base, log1p, "log");
	if (den == NULL) {
		Py_DECREF(num);
		return NULL;
	}

	ans = PyNumber_Divide(num, den);
	Py_DECREF(num);
	Py_DECREF(den);
	return ans;
}

PyDoc_STRVAR(math_log1p_doc,
"log1p(x[, base]) -> the logarithm of 1+x to the given base.\n\
If the base not specified, returns the natural logarithm (base e) of x.\n\
The result is computed in a way which is accurate for x near zero.");

static PyObject *
math_log10(PyObject *self, PyObject *arg)
{
	return loghelper(arg, log10, "log10");
}

PyDoc_STRVAR(math_log10_doc,
"log10(x) -> the base 10 logarithm of x.");

static PyObject *
math_pow(PyObject *self, PyObject *args)
{
	PyObject *ox, *oy;
	double x, y;
	int x_is_infinity, y_is_infinity;

	if (! PyArg_UnpackTuple(args, "pow", 2, 2, &ox, &oy))
		return NULL;
	x = PyFloat_AsDouble(ox);
	y = PyFloat_AsDouble(oy);
	if ((x == -1.0 || y == -1.0) && PyErr_Occurred())
		return NULL;

	/* 1**x and x**0 return 1., even if x is a NaN or infinity */
	if (x == 1.0 || y == 0.0)
		return PyFloat_FromDouble(1.);

	/* with the above two exceptions out of the way, usual rules for NaNs
	   apply: if either x or y is a NaN then NaN should be returned. */
	if (Py_IS_NAN(x))
		return PyFloat_FromDouble(x);
	if (Py_IS_NAN(y))
		return PyFloat_FromDouble(y);

	x_is_infinity = Py_IS_INFINITY(x);
	y_is_infinity = Py_IS_INFINITY(y);

	/* 0 ** negative should give a divide-by-zero exception according
	   to IEEE-754;  we want errno = EDOM here to get a ValueError.
	   Note: for 0.0 ** positive we let the libm routine take care of
	   evaluation, to give maximum chance of getting the sign of the result
	   correct. (e.g. (-0.0)**27 should be -0.0.) */
	if (x == 0.0 && y < 0.0) {
		errno = EDOM;
		is_error(x);
		return NULL;
	}

	/* Finally, call the libm pow. */
	errno = 0;
	PyFPE_START_PROTECT("in math_pow", return 0);
	x = pow(x, y);
	PyFPE_END_PROTECT(x);

	/* a NaN at this point should result in ValueError being raised */
	if (Py_IS_NAN(x))
		errno = EDOM;

	/* if neither x nor y was infinity, then an infinite result here
	   indicates overflow. */
	if (!x_is_infinity && !y_is_infinity && Py_IS_INFINITY(x))
		errno = ERANGE;

	if (errno && is_error(x))
		return NULL;
	else
		return PyFloat_FromDouble(x);
}

PyDoc_STRVAR(math_pow_doc,
"pow(x,y)\n\nReturn x**y (x to the power of y).");

static PyObject *
math_hypot(PyObject *self, PyObject *args)
{
	PyObject *ox, *oy;
	double x, y;
	int x_is_infinity, y_is_infinity;

	if (! PyArg_UnpackTuple(args, "hypot", 2, 2, &ox, &oy))
		return NULL;
	x = PyFloat_AsDouble(ox);
	y = PyFloat_AsDouble(oy);
	if ((x == -1.0 || y == -1.0) && PyErr_Occurred())
		return NULL;

	/* hypot (+/-infinity, x) returns infinity, even if x is a NaN */
	if (Py_IS_INFINITY(x) || Py_IS_INFINITY(y))
		return PyFloat_FromDouble(Py_HUGE_VAL);

	/* Now the usual rules apply: if either argument is a NaN, return NaN */
	if (Py_IS_NAN(x))
		return PyFloat_FromDouble(x);
	if (Py_IS_NAN(y))
		return PyFloat_FromDouble(y);

	x_is_infinity = Py_IS_INFINITY(x);
	y_is_infinity = Py_IS_INFINITY(y);

	errno = 0;
	PyFPE_START_PROTECT("in math_hypot", return 0);
	x = hypot(x, y);
	PyFPE_END_PROTECT(x);

	/* we can get an infinity here either as an exact result, or
	   as a result of overflow */
	if (!x_is_infinity && !y_is_infinity && Py_IS_INFINITY(x))
		errno = ERANGE;

	if (errno && is_error(x))
		return NULL;
	else
		return PyFloat_FromDouble(x);
}

PyDoc_STRVAR(math_hypot_doc,
	     "hypot(x,y)\n\nReturn the Euclidean distance, sqrt(x*x + y*y).");

static const double degToRad = Py_MATH_PI / 180.0;
static const double radToDeg = 180.0 / Py_MATH_PI;

static PyObject *
math_degrees(PyObject *self, PyObject *arg)
{
	double x = PyFloat_AsDouble(arg);
	if (x == -1.0 && PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble(x * radToDeg);
}

PyDoc_STRVAR(math_degrees_doc,
"degrees(x) -> converts angle x from radians to degrees");

static PyObject *
math_radians(PyObject *self, PyObject *arg)
{
	double x = PyFloat_AsDouble(arg);
	if (x == -1.0 && PyErr_Occurred())
		return NULL;
	return PyFloat_FromDouble(x * degToRad);
}

PyDoc_STRVAR(math_radians_doc,
"radians(x) -> converts angle x from degrees to radians");

static PyObject *
math_isnan(PyObject *self, PyObject *arg)
{
	double x = PyFloat_AsDouble(arg);
	if (x == -1.0 && PyErr_Occurred())
		return NULL;
	return PyBool_FromLong((long)Py_IS_NAN(x));
}

PyDoc_STRVAR(math_isnan_doc,
"isnan(x) -> bool\n\
Checks if float x is not a number (NaN)");

static PyObject *
math_isinf(PyObject *self, PyObject *arg)
{
	double x = PyFloat_AsDouble(arg);
	if (x == -1.0 && PyErr_Occurred())
		return NULL;
	return PyBool_FromLong((long)Py_IS_INFINITY(x));
}

PyDoc_STRVAR(math_isinf_doc,
"isinf(x) -> bool\n\
Checks if float x is infinite (positive or negative)");


static PyMethodDef math_methods[] = {
	{"acos",	math_acos,	METH_O,		math_acos_doc},
	{"acosh",	math_acosh,	METH_O,		math_acosh_doc},
	{"asin",	math_asin,	METH_O,		math_asin_doc},
	{"asinh",	math_asinh,	METH_O,		math_asinh_doc},
	{"atan",	math_atan,	METH_O,		math_atan_doc},
	{"atan2",	math_atan2,	METH_VARARGS,	math_atan2_doc},
	{"atanh",	math_atanh,	METH_O,		math_atanh_doc},
	{"ceil",	math_ceil,	METH_O,		math_ceil_doc},
	{"copysign",	math_copysign,	METH_VARARGS,	math_copysign_doc},
	{"cos",		math_cos,	METH_O,		math_cos_doc},
	{"cosh",	math_cosh,	METH_O,		math_cosh_doc},
	{"degrees",	math_degrees,	METH_O,		math_degrees_doc},
	{"exp",		math_exp,	METH_O,		math_exp_doc},
	{"fabs",	math_fabs,	METH_O,		math_fabs_doc},
	{"floor",	math_floor,	METH_O,		math_floor_doc},
	{"fmod",	math_fmod,	METH_VARARGS,	math_fmod_doc},
	{"frexp",	math_frexp,	METH_O,		math_frexp_doc},
	{"hypot",	math_hypot,	METH_VARARGS,	math_hypot_doc},
	{"isinf",	math_isinf,	METH_O,		math_isinf_doc},
	{"isnan",	math_isnan,	METH_O,		math_isnan_doc},
	{"ldexp",	math_ldexp,	METH_VARARGS,	math_ldexp_doc},
	{"log",		math_log,	METH_VARARGS,	math_log_doc},
	{"log1p",	math_log1p,	METH_VARARGS,	math_log1p_doc},
	{"log10",	math_log10,	METH_O,		math_log10_doc},
	{"modf",	math_modf,	METH_O,		math_modf_doc},
	{"pow",		math_pow,	METH_VARARGS,	math_pow_doc},
	{"radians",	math_radians,	METH_O,		math_radians_doc},
	{"sin",		math_sin,	METH_O,		math_sin_doc},
	{"sinh",	math_sinh,	METH_O,		math_sinh_doc},
	{"sqrt",	math_sqrt,	METH_O,		math_sqrt_doc},
	{"tan",		math_tan,	METH_O,		math_tan_doc},
	{"tanh",	math_tanh,	METH_O,		math_tanh_doc},
	{NULL,		NULL}		/* sentinel */
};


PyDoc_STRVAR(module_doc,
"This module is always available.  It provides access to the\n"
"mathematical functions defined by the C standard.");

PyMODINIT_FUNC
initmath(void)
{
	PyObject *m, *d, *v;

	m = Py_InitModule3("math", math_methods, module_doc);
	if (m == NULL)
		goto finally;
	d = PyModule_GetDict(m);
	if (d == NULL)
		goto finally;

        if (!(v = PyFloat_FromDouble(Py_MATH_PI)))
                goto finally;
	if (PyDict_SetItemString(d, "pi", v) < 0)
                goto finally;
	Py_DECREF(v);

        if (!(v = PyFloat_FromDouble(Py_MATH_E)))
                goto finally;
	if (PyDict_SetItemString(d, "e", v) < 0)
                goto finally;
	Py_DECREF(v);

  finally:
	return;
}
