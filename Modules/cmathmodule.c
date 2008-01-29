/* Complex math module */

/* much code borrowed from mathmodule.c */

#include "Python.h"

/* we need DBL_MAX, DBL_MIN, DBL_EPSILON and DBL_MANT_DIG from float.h */
/* We assume that FLT_RADIX is 2, not 10 or 16. */
#include <float.h>

#ifndef M_LN2
#define M_LN2 (0.6931471805599453094) /* natural log of 2 */
#endif

#ifndef M_LN10
#define M_LN10 (2.302585092994045684) /* natural log of 10 */
#endif

/*
   CM_LARGE_DOUBLE is used to avoid spurious overflow in the sqrt, log,
   inverse trig and inverse hyperbolic trig functions.  Its log is used in the
   evaluation of exp, cos, cosh, sin, sinh, tan, and tanh to avoid unecessary
   overflow.
 */

#define CM_LARGE_DOUBLE (DBL_MAX/4.)
#define CM_SQRT_LARGE_DOUBLE (sqrt(CM_LARGE_DOUBLE))
#define CM_LOG_LARGE_DOUBLE (log(CM_LARGE_DOUBLE))
#define CM_SQRT_DBL_MIN (sqrt(DBL_MIN))

/* CM_SCALE_UP defines the power of 2 to multiply by to turn a subnormal into
   a normal; used in sqrt.  must be odd */
#define CM_SCALE_UP 2*(DBL_MANT_DIG/2) + 1
#define CM_SCALE_DOWN -(DBL_MANT_DIG/2 + 1)

/* forward declarations */
static Py_complex c_asinh(Py_complex);
static Py_complex c_atanh(Py_complex);
static Py_complex c_cosh(Py_complex);
static Py_complex c_sinh(Py_complex);
static Py_complex c_sqrt(Py_complex);
static Py_complex c_tanh(Py_complex);
static PyObject * math_error(void);

/* First, the C functions that do the real work */

static Py_complex
c_acos(Py_complex z)
{
	Py_complex s1, s2, r;
        if (fabs(z.real) > CM_LARGE_DOUBLE || fabs(z.imag) > CM_LARGE_DOUBLE) {
		/* avoid unnecessary overflow for large arguments */
		r.real = atan2(fabs(z.imag), z.real);
		/* split into cases to make sure that the branch cut has the
		   correct continuity on systems with unsigned zeros */
		if (z.real < 0.) {
			r.imag = -copysign(log(hypot(z.real/2., z.imag/2.)) + M_LN2*2., z.imag);
		} else {
			r.imag = copysign(log(hypot(z.real/2., z.imag/2.)) + M_LN2*2., -z.imag);
		}
	} else {
		s1.real = 1.-z.real;
		s1.imag = -z.imag;
		s1 = c_sqrt(s1);
		s2.real = 1.+z.real;
		s2.imag = z.imag;
		s2 = c_sqrt(s2);
		r.real = 2.*atan2(s1.real, s2.real);
		r.imag = asinh(s2.real*s1.imag - s2.imag*s1.real);
	}
	return r;
}

PyDoc_STRVAR(c_acos_doc,
"acos(x)\n"
"\n"
"Return the arc cosine of x.");


static Py_complex
c_acosh(Py_complex z)
{
	Py_complex s1, s2, r;

        if (fabs(z.real) > CM_LARGE_DOUBLE || fabs(z.imag) > CM_LARGE_DOUBLE) {
		/* avoid unnecessary overflow for large arguments */
		r.real = log(hypot(z.real/2., z.imag/2.)) + M_LN2*2.;
		r.imag = atan2(z.imag, z.real);
	} else {
		s1.real = z.real - 1.;
		s1.imag = z.imag;
		s1 = c_sqrt(s1);
		s2.real = z.real + 1.;
		s2.imag = z.imag;
		s2 = c_sqrt(s2);
		r.real = asinh(s1.real*s2.real + s1.imag*s2.imag);
		r.imag = 2.*atan2(s1.imag, s2.real);
	}
	return r;
}

PyDoc_STRVAR(c_acosh_doc,
"acosh(x)\n"
"\n"
"Return the hyperbolic arccosine of x.");


static Py_complex
c_asin(Py_complex z)
{
	/* asin(z) = -i asinh(iz) */
	Py_complex s, r;
	s.real = -z.imag;
	s.imag = z.real;
	s = c_asinh(s);
	r.real = s.imag;
	r.imag = -s.real;
	return r;
}

PyDoc_STRVAR(c_asin_doc,
"asin(x)\n"
"\n"
"Return the arc sine of x.");


static Py_complex
c_asinh(Py_complex z)
{
	Py_complex s1, s2, r;

        if (fabs(z.real) > CM_LARGE_DOUBLE || fabs(z.imag) > CM_LARGE_DOUBLE) {
		if (z.imag >= 0.) {
			r.real = copysign(log(hypot(z.real/2., z.imag/2.)) + M_LN2*2., z.real);
		} else {
			r.real = -copysign(log(hypot(z.real/2., z.imag/2.)) + M_LN2*2., -z.real);
		}
		r.imag = atan2(z.imag, fabs(z.real));
	} else {
		s1.real = 1.+z.imag;
		s1.imag = -z.real;
		s1 = c_sqrt(s1);
		s2.real = 1.-z.imag;
		s2.imag = z.real;
		s2 = c_sqrt(s2);
		r.real = asinh(s1.real*s2.imag-s2.real*s1.imag);
		r.imag = atan2(z.imag, s1.real*s2.real-s1.imag*s2.imag);
	}
	return r;
}

PyDoc_STRVAR(c_asinh_doc,
"asinh(x)\n"
"\n"
"Return the hyperbolic arc sine of x.");


static Py_complex
c_atan(Py_complex z)
{
	/* atan(z) = -i atanh(iz) */
	Py_complex s, r;
	s.real = -z.imag;
	s.imag = z.real;
	s = c_atanh(s);
	r.real = s.imag;
	r.imag = -s.real;
	return r;
}

PyDoc_STRVAR(c_atan_doc,
"atan(x)\n"
"\n"
"Return the arc tangent of x.");


static Py_complex
c_atanh(Py_complex z)
{
	Py_complex r;
	double ay, h;

	/* Reduce to case where z.real >= 0., using atanh(z) = -atanh(-z). */
	if (z.real < 0.) {
		return c_neg(c_atanh(c_neg(z)));
	}

	ay = fabs(z.imag);
	if (z.real > CM_SQRT_LARGE_DOUBLE || ay > CM_SQRT_LARGE_DOUBLE) {
		/* 
		   if abs(z) is large then we use the approximation
		   atanh(z) ~ 1/z +/- i*pi/2 (+/- depending on the sign
		   of z.imag)
		*/
		h = hypot(z.real/2., z.imag/2.);  /* safe from overflow */
		r.real = z.real/4./h/h;
		/* the two negations in the next line cancel each other out
		   except when working with unsigned zeros: they're there to
		   ensure that the branch cut has the correct continuity on
		   systems that don't support signed zeros */
		r.imag = -copysign(Py_MATH_PI/2., -z.imag);
	} else if (z.real == 1. && ay < CM_SQRT_DBL_MIN) {
		/* C99 standard says:  atanh(1+/-0.) should be inf +/- 0i */
		r.real = -log(sqrt(ay)/sqrt(hypot(ay, 2.)));
		if (ay == 0.) {
			r.imag = z.imag;
		} else {
			r.imag = copysign(atan2(2., -ay)/2, z.imag);
		}
	} else {
		r.real = log1p(4.*z.real/((1-z.real)*(1-z.real) + ay*ay))/4.;
		r.imag = -atan2(-2.*z.imag, (1-z.real)*(1+z.real) - ay*ay)/2.;
	}
	return r;
}

PyDoc_STRVAR(c_atanh_doc,
"atanh(x)\n"
"\n"
"Return the hyperbolic arc tangent of x.");


static Py_complex
c_cos(Py_complex z)
{
	/* cos(z) = cosh(iz) */
	Py_complex r;
	r.real = -z.imag;
	r.imag = z.real;
	r = c_cosh(r);
	return r;
}

PyDoc_STRVAR(c_cos_doc,
"cos(x)\n"
"n"
"Return the cosine of x.");


static Py_complex
c_cosh(Py_complex z)
{
	Py_complex r;
	double x_minus_one;

	if (fabs(z.real) > CM_LOG_LARGE_DOUBLE) {
		/* deal correctly with cases where cosh(z.real) overflows but
		   cosh(z) does not. */
		x_minus_one = z.real - copysign(1., z.real);
		r.real = cos(z.imag) * cosh(x_minus_one) * Py_MATH_E;
		r.imag = sin(z.imag) * sinh(x_minus_one) * Py_MATH_E;
	} else {
		r.real = cos(z.imag) * cosh(z.real);
		r.imag = sin(z.imag) * sinh(z.real);
	}
	return r;
}

PyDoc_STRVAR(c_cosh_doc,
"cosh(x)\n"
"n"
"Return the hyperbolic cosine of x.");


static Py_complex
c_exp(Py_complex z)
{
	Py_complex r;
	double l;

	if (z.real > CM_LOG_LARGE_DOUBLE) {
		l = exp(z.real-1.);
		r.real = l*cos(z.imag)*Py_MATH_E;
		r.imag = l*sin(z.imag)*Py_MATH_E;
	} else {
		l = exp(z.real);
		r.real = l*cos(z.imag);
		r.imag = l*sin(z.imag);
	}
	return r;
}

PyDoc_STRVAR(c_exp_doc,
"exp(x)\n"
"\n"
"Return the exponential value e**x.");


static Py_complex
c_log(Py_complex z)
{
	/*
	   The usual formula for the real part is log(hypot(z.real, z.imag)).
	   There are four situations where this formula is potentially
	   problematic:

	   (1) the absolute value of z is subnormal.  Then hypot is subnormal,
	   so has fewer than the usual number of bits of accuracy, hence may
	   have large relative error.  This then gives a large absolute error
	   in the log.  This can be solved by rescaling z by a suitable power
	   of 2.

	   (2) the absolute value of z is greater than DBL_MAX (e.g. when both
	   z.real and z.imag are within a factor of 1/sqrt(2) of DBL_MAX)
	   Again, rescaling solves this.

	   (3) the absolute value of z is close to 1.  In this case it's
	   difficult to achieve good accuracy, at least in part because a
	   change of 1ulp in the real or imaginary part of z can result in a
	   change of billions of ulps in the correctly rounded answer.

	   (4) z = 0.  The simplest thing to do here is to call the
	   floating-point log with an argument of 0, and let its behaviour
	   (returning -infinity, signaling a floating-point exception, setting
	   errno, or whatever) determine that of c_log.  So the usual formula
	   is fine here.

	 */

	Py_complex r;
	double ax, ay, am, an, h;

	ax = fabs(z.real);
	ay = fabs(z.imag);

	if (ax > CM_LARGE_DOUBLE || ay > CM_LARGE_DOUBLE) {
		r.real = log(hypot(ax/2., ay/2.)) + M_LN2;
	} else if (ax < DBL_MIN && ay < DBL_MIN && (ax > 0. || ay > 0.)) {
		/* catch cases where hypot(ax, ay) is subnormal */
		r.real = log(hypot(ldexp(ax, DBL_MANT_DIG), ldexp(ay, DBL_MANT_DIG))) - DBL_MANT_DIG*M_LN2;
	} else {
		h = hypot(ax, ay);
		if (0.71 <= h && h <= 1.73) {
			am = ax > ay ? ax : ay;  /* max(ax, ay) */
			an = ax > ay ? ay : ax;  /* min(ax, ay) */
			r.real = log1p((am-1)*(am+1)+an*an)/2.;
		} else {
			r.real = log(h);
		}
	}
	r.imag = atan2(z.imag, z.real);
	return r;
}


static Py_complex
c_log10(Py_complex z)
{
	Py_complex r;

	r = c_log(z);
	r.real = r.real / M_LN10;
	r.imag = r.imag / M_LN10;
	return r;
}

PyDoc_STRVAR(c_log10_doc,
"log10(x)\n"
"\n"
"Return the base-10 logarithm of x.");


static Py_complex
c_sin(Py_complex z)
{
	/* sin(z) = -i sin(iz) */
	Py_complex s, r;
	s.real = -z.imag;
	s.imag = z.real;
	s = c_sinh(s);
	r.real = s.imag;
	r.imag = -s.real;
	return r;
}

PyDoc_STRVAR(c_sin_doc,
"sin(x)\n"
"\n"
"Return the sine of x.");


static Py_complex
c_sinh(Py_complex z)
{
	Py_complex r;
	double x_minus_one;

	if (fabs(z.real) > CM_LOG_LARGE_DOUBLE) {
		x_minus_one = z.real - copysign(1., z.real);
		r.real = cos(z.imag) * sinh(x_minus_one) * Py_MATH_E;
		r.imag = sin(z.imag) * cosh(x_minus_one) * Py_MATH_E;
	} else {
		r.real = cos(z.imag) * sinh(z.real);
		r.imag = sin(z.imag) * cosh(z.real);
	}
	return r;

}

PyDoc_STRVAR(c_sinh_doc,
"sinh(x)\n"
"\n"
"Return the hyperbolic sine of x.");


static Py_complex
c_sqrt(Py_complex z)
{
	/*
	   Method: use symmetries to reduce to the case when x = z.real and y
	   = z.imag are nonnegative.  Then the real part of the result is
	   given by

	     s = sqrt((x + hypot(x, y))/2)

	   and the imaginary part is

	     d = (y/2)/s

	   If either x or y is very large then there's a risk of overflow in
	   computation of the expression x + hypot(x, y).  We can avoid this
	   by rewriting the formula for s as:

	     s = 2*sqrt(x/8 + hypot(x/8, y/8))

	   This costs us two extra multiplications/divisions, but avoids the
	   overhead of checking for x and y large.

	   If both x and y are subnormal then hypot(x, y) may also be
	   subnormal, so will lack full precision.  We solve this by rescaling
	   x and y by a sufficiently large power of 2 to ensure that x and y
	   are normal.
	*/


	Py_complex r;
	double s,d;
	double ax, ay;

	if (z.real == 0. && z.imag == 0.) {
		r.real = 0.;
		r.imag = z.imag;
		return r;
	}

	ax = fabs(z.real);
	ay = fabs(z.imag);

	if (ax < DBL_MIN && ay < DBL_MIN && (ax > 0. || ay > 0.)) {
		/* here we catch cases where hypot(ax, ay) is subnormal */
		ax = ldexp(ax, CM_SCALE_UP);
		s = ldexp(sqrt(ax + hypot(ax, ldexp(ay, CM_SCALE_UP))), CM_SCALE_DOWN);
	} else {
		ax /= 8.;
		s = 2.*sqrt(ax + hypot(ax, ay/8.));
	}
        d = ay/(2.*s);

	if (z.real >= 0.) {
		r.real = s;
		r.imag = copysign(d, z.imag);
	} else {
		r.real = d;
		r.imag = copysign(s, z.imag);
	}
	return r;
}

PyDoc_STRVAR(c_sqrt_doc,
"sqrt(x)\n"
"\n"
"Return the square root of x.");


static Py_complex
c_tan(Py_complex z)
{
	/* tan(z) = -i tanh(iz) */
	Py_complex s, r;
	s.real = -z.imag;
	s.imag = z.real;
	s = c_tanh(s);
	r.real = s.imag;
	r.imag = -s.real;
	return r;
}

PyDoc_STRVAR(c_tan_doc,
"tan(x)\n"
"\n"
"Return the tangent of x.");


static Py_complex
c_tanh(Py_complex z)
{
	/* Formula:

	   tanh(x+iy) = (tanh(x)(1+tan(y)^2) + i tan(y)(1-tanh(x))^2) /
	   (1+tan(y)^2 tanh(x)^2)

	   To avoid excessive roundoff error, 1-tanh(x)^2 is better computed
	   as 1/cosh(x)^2.  When abs(x) is large, we approximate 1-tanh(x)^2
	   by 4 exp(-2*x) instead, to avoid possible overflow in the
	   computation of cosh(x).

	*/

	Py_complex r;
	double tx, ty, cx, txty, denom;

	if (fabs(z.real) > CM_LOG_LARGE_DOUBLE) {
		r.real = copysign(1., z.real);
                r.imag = 2.*sin(2.*z.imag)*exp(-2.*fabs(z.real));
	} else {
		tx = tanh(z.real);
		ty = tan(z.imag);
		cx = 1./cosh(z.real);
		txty = tx*ty;
		denom = 1. + txty*txty;
		r.real = tx*(1.+ty*ty)/denom;
		r.imag = ((ty/denom)*cx)*cx;
	}
	return r;
}

PyDoc_STRVAR(c_tanh_doc,
"tanh(x)\n"
"\n"
"Return the hyperbolic tangent of x.");

static PyObject *
cmath_log(PyObject *self, PyObject *args)
{
	Py_complex x;
	Py_complex y;

	if (!PyArg_ParseTuple(args, "D|D", &x, &y))
		return NULL;

	errno = 0;
	PyFPE_START_PROTECT("complex function", return 0)
	x = c_log(x);
	if (PyTuple_GET_SIZE(args) == 2)
		x = c_quot(x, c_log(y));
	PyFPE_END_PROTECT(x)
	Py_ADJUST_ERANGE2(x.real, x.imag);
	if (errno != 0)
		return math_error();
	return PyComplex_FromCComplex(x);
}

PyDoc_STRVAR(cmath_log_doc,
"log(x[, base]) -> the logarithm of x to the given base.\n\
If the base not specified, returns the natural logarithm (base e) of x.");


/* And now the glue to make them available from Python: */

static PyObject *
math_error(void)
{
	if (errno == EDOM)
		PyErr_SetString(PyExc_ValueError, "math domain error");
	else if (errno == ERANGE)
		PyErr_SetString(PyExc_OverflowError, "math range error");
	else    /* Unexpected math error */
		PyErr_SetFromErrno(PyExc_ValueError);
	return NULL;
}

static PyObject *
math_1(PyObject *args, Py_complex (*func)(Py_complex))
{
	Py_complex x;
	if (!PyArg_ParseTuple(args, "D", &x))
		return NULL;
	errno = 0;
	PyFPE_START_PROTECT("complex function", return 0)
	x = (*func)(x);
	PyFPE_END_PROTECT(x)
	Py_ADJUST_ERANGE2(x.real, x.imag);
	if (errno != 0)
		return math_error();
	else
		return PyComplex_FromCComplex(x);
}

#define FUNC1(stubname, func) \
	static PyObject * stubname(PyObject *self, PyObject *args) { \
		return math_1(args, func); \
	}

FUNC1(cmath_acos, c_acos)
FUNC1(cmath_acosh, c_acosh)
FUNC1(cmath_asin, c_asin)
FUNC1(cmath_asinh, c_asinh)
FUNC1(cmath_atan, c_atan)
FUNC1(cmath_atanh, c_atanh)
FUNC1(cmath_cos, c_cos)
FUNC1(cmath_cosh, c_cosh)
FUNC1(cmath_exp, c_exp)
FUNC1(cmath_log10, c_log10)
FUNC1(cmath_sin, c_sin)
FUNC1(cmath_sinh, c_sinh)
FUNC1(cmath_sqrt, c_sqrt)
FUNC1(cmath_tan, c_tan)
FUNC1(cmath_tanh, c_tanh)

static PyObject *
cmath_arg(PyObject *self, PyObject *args)
{
	Py_complex z;
	double phi;
	if (!PyArg_ParseTuple(args, "D:arg", &z))
		return NULL;
	errno = 0;
	PyFPE_START_PROTECT("arg function", return 0)
	phi = atan2(z.imag, z.real);
	PyFPE_END_PROTECT(r)
	if (errno != 0)
		return math_error();
	else
		return PyFloat_FromDouble(phi);
}

PyDoc_STRVAR(cmath_arg_doc,
"arg(z) -> float\n\n\
Return argument, also known as the phase angle, of a complex.");

static PyObject *
cmath_polar(PyObject *self, PyObject *args)
{
	Py_complex z;
	double r, phi;
	if (!PyArg_ParseTuple(args, "D:polar", &z))
		return NULL;
	errno = 0;
	PyFPE_START_PROTECT("polar function", return 0)
	r = hypot(z.real, z.imag);
	phi = atan2(z.imag, z.real);
	PyFPE_END_PROTECT(r)
	if (errno != 0)
		return math_error();
	else
		return Py_BuildValue("dd", r, phi);
}

PyDoc_STRVAR(cmath_polar_doc,
"polar(z) -> r: float, phi: float\n\n\
Convert a complex from rectangular coordinates to polar coordinates. r is\n\
the distance from 0 and phi the phase angle.");

static PyObject *
cmath_rect(PyObject *self, PyObject *args)
{
	Py_complex z;
	double r, phi;
	if (!PyArg_ParseTuple(args, "dd:rect", &r, &phi))
		return NULL;
	errno = 0;
	if (r < 0.) {
		errno = EDOM;
		return math_error();
	}
	PyFPE_START_PROTECT("rect function", return 0)
	z.real = r * cos(phi);
	z.imag = r * sin(phi);
	PyFPE_END_PROTECT(z)
	if (errno != 0)
		return math_error();
	else
		return PyComplex_FromCComplex(z);
}

PyDoc_STRVAR(cmath_rect_doc,
"rect(r, phi) -> z: complex\n\n\
Convert from polar coordinates to rectangular coordinates.");

PyDoc_STRVAR(module_doc,
"This module is always available. It provides access to mathematical\n"
"functions for complex numbers.");

static PyMethodDef cmath_methods[] = {
	{"acos",   cmath_acos,  METH_VARARGS, c_acos_doc},
	{"acosh",  cmath_acosh, METH_VARARGS, c_acosh_doc},
	{"arg",    cmath_arg,   METH_VARARGS, cmath_arg_doc},
	{"asin",   cmath_asin,  METH_VARARGS, c_asin_doc},
	{"asinh",  cmath_asinh, METH_VARARGS, c_asinh_doc},
	{"atan",   cmath_atan,  METH_VARARGS, c_atan_doc},
	{"atanh",  cmath_atanh, METH_VARARGS, c_atanh_doc},
	{"cos",    cmath_cos,   METH_VARARGS, c_cos_doc},
	{"cosh",   cmath_cosh,  METH_VARARGS, c_cosh_doc},
	{"exp",    cmath_exp,   METH_VARARGS, c_exp_doc},
	{"log",    cmath_log,   METH_VARARGS, cmath_log_doc},
	{"log10",  cmath_log10, METH_VARARGS, c_log10_doc},
	{"polar",  cmath_polar, METH_VARARGS, cmath_polar_doc},
	{"rect",   cmath_rect,  METH_VARARGS, cmath_rect_doc},
	{"sin",    cmath_sin,   METH_VARARGS, c_sin_doc},
	{"sinh",   cmath_sinh,  METH_VARARGS, c_sinh_doc},
	{"sqrt",   cmath_sqrt,  METH_VARARGS, c_sqrt_doc},
	{"tan",    cmath_tan,   METH_VARARGS, c_tan_doc},
	{"tanh",   cmath_tanh,  METH_VARARGS, c_tanh_doc},
	{NULL,		NULL}		/* sentinel */
};

PyMODINIT_FUNC
initcmath(void)
{
	PyObject *m;

	m = Py_InitModule3("cmath", cmath_methods, module_doc);
	if (m == NULL)
		return;

	PyModule_AddObject(m, "pi",
                           PyFloat_FromDouble(Py_MATH_PI));
	PyModule_AddObject(m, "e", PyFloat_FromDouble(Py_MATH_E));
}
