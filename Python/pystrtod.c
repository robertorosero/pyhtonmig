/* -*- Mode: C; c-file-style: "python" -*- */

#include <Python.h>
#include <locale.h>

/* ascii character tests (as opposed to locale tests) */
#define ISSPACE(c)  ((c) == ' ' || (c) == '\f' || (c) == '\n' || \
                     (c) == '\r' || (c) == '\t' || (c) == '\v')
#define ISDIGIT(c)  ((c) >= '0' && (c) <= '9')


/**
 * PyOS_ascii_strtod:
 * @nptr:    the string to convert to a numeric value.
 * @endptr:  if non-%NULL, it returns the character after
 *           the last character used in the conversion.
 * 
 * Converts a string to a #gdouble value.
 * This function behaves like the standard strtod() function
 * does in the C locale. It does this without actually
 * changing the current locale, since that would not be
 * thread-safe.
 *
 * This function is typically used when reading configuration
 * files or other non-user input that should be locale independent.
 * To handle input from the user you should normally use the
 * locale-sensitive system strtod() function.
 *
 * If the correct value would cause overflow, plus or minus %HUGE_VAL
 * is returned (according to the sign of the value), and %ERANGE is
 * stored in %errno. If the correct value would cause underflow,
 * zero is returned and %ERANGE is stored in %errno.
 * If memory allocation fails, %ENOMEM is stored in %errno.
 * 
 * This function resets %errno before calling strtod() so that
 * you can reliably detect overflow and underflow.
 *
 * Return value: the #gdouble value.
 **/
double
PyOS_ascii_strtod(const char *nptr, char **endptr)
{
	assert(nptr != NULL);
	/* Set errno to zero, so that we can distinguish zero results
	   and underflows */
	errno = 0;
	return _Py_dg_strtod(nptr, endptr);
}

double
PyOS_ascii_strtod_fallback(const char *nptr, char **endptr)
{
	char *fail_pos;
	double val = -1.0;
	struct lconv *locale_data;
	const char *decimal_point;
	size_t decimal_point_len;
	const char *p, *decimal_point_pos;
	const char *end = NULL; /* Silence gcc */
	const char *digits_pos = NULL;
	int negate = 0;

	assert(nptr != NULL);

	fail_pos = NULL;

	locale_data = localeconv();
	decimal_point = locale_data->decimal_point;
	decimal_point_len = strlen(decimal_point);

	assert(decimal_point_len != 0);

	decimal_point_pos = NULL;

	/* We process any leading whitespace and the optional sign manually,
	   then pass the remainder to the system strtod.  This ensures that
	   the result of an underflow has the correct sign. (bug #1725)  */

	p = nptr;
	/* Skip leading space */
	while (ISSPACE(*p))
		p++;

	/* Process leading sign, if present */
	if (*p == '-') {
		negate = 1;
		p++;
	} else if (*p == '+') {
		p++;
	}

	/* What's left should begin with a digit, a decimal point, or one of
	   the letters i, I, n, N. It should not begin with 0x or 0X */
	if ((!ISDIGIT(*p) &&
	     *p != '.' && *p != 'i' && *p != 'I' && *p != 'n' && *p != 'N')
	    ||
	    (*p == '0' && (p[1] == 'x' || p[1] == 'X')))
	{
		if (endptr)
			*endptr = (char*)nptr;
		errno = EINVAL;
		return val;
	}
	digits_pos = p;

	if (decimal_point[0] != '.' || 
	    decimal_point[1] != 0)
	{
		while (ISDIGIT(*p))
			p++;

		if (*p == '.')
		{
			decimal_point_pos = p++;

			while (ISDIGIT(*p))
				p++;

			if (*p == 'e' || *p == 'E')
				p++;
			if (*p == '+' || *p == '-')
				p++;
			while (ISDIGIT(*p))
				p++;
			end = p;
		}
		else if (strncmp(p, decimal_point, decimal_point_len) == 0)
		{
			/* Python bug #1417699 */
			if (endptr)
				*endptr = (char*)nptr;
			errno = EINVAL;
			return val;
		}
		/* For the other cases, we need not convert the decimal
		   point */
	}

	/* Set errno to zero, so that we can distinguish zero results
	   and underflows */
	errno = 0;

	if (decimal_point_pos)
	{
		char *copy, *c;

		/* We need to convert the '.' to the locale specific decimal
		   point */
		copy = (char *)PyMem_MALLOC(end - digits_pos +
					    1 + decimal_point_len);
		if (copy == NULL) {
			if (endptr)
				*endptr = (char *)nptr;
			errno = ENOMEM;
			return val;
		}

		c = copy;
		memcpy(c, digits_pos, decimal_point_pos - digits_pos);
		c += decimal_point_pos - digits_pos;
		memcpy(c, decimal_point, decimal_point_len);
		c += decimal_point_len;
		memcpy(c, decimal_point_pos + 1,
		       end - (decimal_point_pos + 1));
		c += end - (decimal_point_pos + 1);
		*c = 0;

		val = strtod(copy, &fail_pos);

		if (fail_pos)
		{
			if (fail_pos > decimal_point_pos)
				fail_pos = (char *)digits_pos +
					(fail_pos - copy) -
					(decimal_point_len - 1);
			else
				fail_pos = (char *)digits_pos +
					(fail_pos - copy);
		}

		PyMem_FREE(copy);

	}
	else {
		val = strtod(digits_pos, &fail_pos);
	}

	if (fail_pos == digits_pos)
		fail_pos = (char *)nptr;

	if (negate && fail_pos != nptr)
		val = -val;

	if (endptr)
		*endptr = fail_pos;

	return val;
}

/* Given a string that may have a decimal point in the current
   locale, change it back to a dot.  Since the string cannot get
   longer, no need for a maximum buffer size parameter. */
Py_LOCAL_INLINE(void)
change_decimal_from_locale_to_dot(char* buffer)
{
	struct lconv *locale_data = localeconv();
	const char *decimal_point = locale_data->decimal_point;

	if (decimal_point[0] != '.' || decimal_point[1] != 0) {
		size_t decimal_point_len = strlen(decimal_point);

		if (*buffer == '+' || *buffer == '-')
			buffer++;
		while (isdigit(Py_CHARMASK(*buffer)))
			buffer++;
		if (strncmp(buffer, decimal_point, decimal_point_len) == 0) {
			*buffer = '.';
			buffer++;
			if (decimal_point_len > 1) {
				/* buffer needs to get smaller */
				size_t rest_len = strlen(buffer +
						     (decimal_point_len - 1));
				memmove(buffer,
					buffer + (decimal_point_len - 1),
					rest_len);
				buffer[rest_len] = 0;
			}
		}
	}
}


/* From the C99 standard, section 7.19.6:
The exponent always contains at least two digits, and only as many more digits
as necessary to represent the exponent.
*/
#define MIN_EXPONENT_DIGITS 2

/* Ensure that any exponent, if present, is at least MIN_EXPONENT_DIGITS
   in length. */
Py_LOCAL_INLINE(void)
ensure_minumim_exponent_length(char* buffer, size_t buf_size)
{
	char *p = strpbrk(buffer, "eE");
	if (p && (*(p + 1) == '-' || *(p + 1) == '+')) {
		char *start = p + 2;
		int exponent_digit_cnt = 0;
		int leading_zero_cnt = 0;
		int in_leading_zeros = 1;
		int significant_digit_cnt;

		/* Skip over the exponent and the sign. */
		p += 2;

		/* Find the end of the exponent, keeping track of leading
		   zeros. */
		while (*p && isdigit(Py_CHARMASK(*p))) {
			if (in_leading_zeros && *p == '0')
				++leading_zero_cnt;
			if (*p != '0')
				in_leading_zeros = 0;
			++p;
			++exponent_digit_cnt;
		}

		significant_digit_cnt = exponent_digit_cnt - leading_zero_cnt;
		if (exponent_digit_cnt == MIN_EXPONENT_DIGITS) {
			/* If there are 2 exactly digits, we're done,
			   regardless of what they contain */
		}
		else if (exponent_digit_cnt > MIN_EXPONENT_DIGITS) {
			int extra_zeros_cnt;

			/* There are more than 2 digits in the exponent.  See
			   if we can delete some of the leading zeros */
			if (significant_digit_cnt < MIN_EXPONENT_DIGITS)
				significant_digit_cnt = MIN_EXPONENT_DIGITS;
			extra_zeros_cnt = exponent_digit_cnt -
				significant_digit_cnt;

			/* Delete extra_zeros_cnt worth of characters from the
			   front of the exponent */
			assert(extra_zeros_cnt >= 0);

			/* Add one to significant_digit_cnt to copy the
			   trailing 0 byte, thus setting the length */
			memmove(start,
				start + extra_zeros_cnt,
				significant_digit_cnt + 1);
		}
		else {
			/* If there are fewer than 2 digits, add zeros
			   until there are 2, if there's enough room */
			int zeros = MIN_EXPONENT_DIGITS - exponent_digit_cnt;
			if (start + zeros + exponent_digit_cnt + 1
			      < buffer + buf_size) {
				memmove(start + zeros, start,
					exponent_digit_cnt + 1);
				memset(start, '0', zeros);
			}
		}
	}
}

/* Ensure that buffer has a decimal point in it.  The decimal point
   will not be in the current locale, it will always be '.' */
Py_LOCAL_INLINE(void)
ensure_decimal_point(char* buffer, size_t buf_size)
{
	int insert_count = 0;
	char* chars_to_insert;

	/* search for the first non-digit character */
	char *p = buffer;
	if (*p == '-' || *p == '+')
		/* Skip leading sign, if present.  I think this could only
		   ever be '-', but it can't hurt to check for both. */
		++p;
	while (*p && isdigit(Py_CHARMASK(*p)))
		++p;

	if (*p == '.') {
		if (isdigit(Py_CHARMASK(*(p+1)))) {
			/* Nothing to do, we already have a decimal
			   point and a digit after it */
		}
		else {
			/* We have a decimal point, but no following
			   digit.  Insert a zero after the decimal. */
			++p;
			chars_to_insert = "0";
			insert_count = 1;
		}
	}
	else {
		chars_to_insert = ".0";
		insert_count = 2;
	}
	if (insert_count) {
		size_t buf_len = strlen(buffer);
		if (buf_len + insert_count + 1 >= buf_size) {
			/* If there is not enough room in the buffer
			   for the additional text, just skip it.  It's
			   not worth generating an error over. */
		}
		else {
			memmove(p + insert_count, p,
				buffer + strlen(buffer) - p + 1);
			memcpy(p, chars_to_insert, insert_count);
		}
	}
}

/* Add the locale specific grouping characters to buffer.  Note
   that any decimal point (if it's present) in buffer is already
   locale-specific.  Return 0 on error, else 1. */
Py_LOCAL_INLINE(int)
add_thousands_grouping(char* buffer, size_t buf_size)
{
	Py_ssize_t len = strlen(buffer);
	struct lconv *locale_data = localeconv();
	const char *decimal_point = locale_data->decimal_point;

	/* Find the decimal point, if any.  We're only concerned
	   about the characters to the left of the decimal when
	   adding grouping. */
	char *p = strstr(buffer, decimal_point);
	if (!p) {
		/* No decimal, use the entire string. */

		/* If any exponent, adjust p. */
		p = strpbrk(buffer, "eE");
		if (!p)
			/* No exponent and no decimal.  Use the entire
			   string. */
			p = buffer + len;
	}
	/* At this point, p points just past the right-most character we
	   want to format.  We need to add the grouping string for the
	   characters between buffer and p. */
	return _PyBytes_InsertThousandsGrouping(buffer, len, p-buffer,
						buf_size, NULL, 1);
}

/* see FORMATBUFLEN in unicodeobject.c */
#define FLOAT_FORMATBUFLEN 120

/**
 * PyOS_ascii_formatd:
 * @buffer: A buffer to place the resulting string in
 * @buf_size: The length of the buffer.
 * @format: The printf()-style format to use for the
 *          code to use for converting. 
 * @d: The #gdouble to convert
 *
 * Converts a #gdouble to a string, using the '.' as
 * decimal point. To format the number you pass in
 * a printf()-style format string. Allowed conversion
 * specifiers are 'e', 'E', 'f', 'F', 'g', 'G', and 'n'.
 * 
 * 'n' is the same as 'g', except it uses the current locale.
 * 'Z' is the same as 'g', except it always has a decimal and
 *     at least one digit after the decimal.
 *
 * Return value: The pointer to the buffer with the converted string.
 **/
char *
PyOS_ascii_formatd(char       *buffer, 
		   size_t      buf_size, 
		   const char *format, 
		   double      d)
{
	char format_char;
	size_t format_len = strlen(format);

	/* For type 'n', we need to make a copy of the format string, because
	   we're going to modify 'n' -> 'g', and format is const char*, so we
	   can't modify it directly.  FLOAT_FORMATBUFLEN should be longer than
	   we ever need this to be.  There's an upcoming check to ensure it's
	   big enough. */
	/* Issue 2264: code 'Z' requires copying the format.  'Z' is 'g', but
	   also with at least one character past the decimal. */
	char tmp_format[FLOAT_FORMATBUFLEN];

	/* The last character in the format string must be the format char */
	format_char = format[format_len - 1];

	if (format[0] != '%')
		return NULL;

	/* I'm not sure why this test is here.  It's ensuring that the format
	   string after the first character doesn't have a single quote, a
	   lowercase l, or a percent. This is the reverse of the commented-out
	   test about 10 lines ago. */
	if (strpbrk(format + 1, "'l%"))
		return NULL;

	/* Also curious about this function is that it accepts format strings
	   like "%xg", which are invalid for floats.  In general, the
	   interface to this function is not very good, but changing it is
	   difficult because it's a public API. */

	if (!(format_char == 'e' || format_char == 'E' || 
	      format_char == 'f' || format_char == 'F' || 
	      format_char == 'g' || format_char == 'G' ||
	      format_char == 'n' || format_char == 'Z'))
		return NULL;

	/* Map 'n' or 'Z' format_char to 'g', by copying the format string and
	   replacing the final char with a 'g' */
	if (format_char == 'n' || format_char == 'Z') {
		if (format_len + 1 >= sizeof(tmp_format)) {
			/* The format won't fit in our copy.  Error out.  In
			   practice, this will never happen and will be
			   detected by returning NULL */
			return NULL;
		}
		strcpy(tmp_format, format);
		tmp_format[format_len - 1] = 'g';
		format = tmp_format;
	}


	/* Have PyOS_snprintf do the hard work */
	PyOS_snprintf(buffer, buf_size, format, d);

	/* Do various fixups on the return string */

	/* Get the current locale, and find the decimal point string.
	   Convert that string back to a dot.  Do not do this if using the
	   'n' (number) format code, since we want to keep the localized
	   decimal point in that case. */
	if (format_char != 'n')
		change_decimal_from_locale_to_dot(buffer);

	/* If an exponent exists, ensure that the exponent is at least
	   MIN_EXPONENT_DIGITS digits, providing the buffer is large enough
	   for the extra zeros.  Also, if there are more than
	   MIN_EXPONENT_DIGITS, remove as many zeros as possible until we get
	   back to MIN_EXPONENT_DIGITS */
	ensure_minumim_exponent_length(buffer, buf_size);

	/* If format_char is 'Z', make sure we have at least one character
	   after the decimal point (and make sure we have a decimal point). */
	if (format_char == 'Z')
		ensure_decimal_point(buffer, buf_size);

	/* If format_char is 'n', add the thousands grouping. */
	if (format_char == 'n')
		if (!add_thousands_grouping(buffer, buf_size))
			return NULL;

	return buffer;
}

double
PyOS_ascii_atof(const char *nptr)
{
	return PyOS_ascii_strtod(nptr, NULL);
}


/* I'm using a lookup table here so that I don't have to invent a non-locale
   specific way to convert to uppercase */
#define OFS_INF 0
#define OFS_NAN 1
#define OFS_E 2

/* The lengths of these are known to the code below, so don't change them */
static char *lc_float_strings[] = {
	"inf",
	"nan",
	"e",
};
static char *uc_float_strings[] = {
	"INF",
	"NAN",
	"E",
};


/* Convert a Python float to a minimal string that evaluates back to that
   float.  The output is minimal in the sense of having the fewest possible
   number of significant digits. */

static void
format_float_short(char *buf, Py_ssize_t buflen, double d, char format_code,
		   int mode, Py_ssize_t precision,
		   Py_ssize_t n_wanted_digits_after_decimal,
		   int always_add_sign, int add_dot_0_if_integer,
		   int use_alt_formatting, char **float_strings)
{
	char *digits, *digits_end;
	int decpt, sign, exp_len, dec_pos;
	int use_exp = 0;
	int is_integer;  /* is the output produced so far just an integer? */
	Py_ssize_t n_digits, trailing_zeros;

	/* precision of 0 makes no sense for 'g' format; interpret as 1 */
	if (precision == 0 && format_code == 'g')
		precision = 1;

	/* _Py_dg_dtoa returns a digit string (no decimal point or
	   exponent) */
	digits = _Py_dg_dtoa(d, mode, precision, &decpt, &sign, &digits_end);
	assert(digits_end != NULL && digits_end >= digits);
	n_digits = digits_end - digits;

	if (n_digits && !isdigit(digits[0])) {
		/* Infinities and nans here; adapt Gay's output,
		   so convert Infinity to inf and NaN to nan, and
		   ignore sign of nan. Then return. */
		if (digits[0] == 'i' || digits[0] == 'I') {
			if (sign == 1) {
				*buf++ = '-';
			}
			else if (always_add_sign) {
				*buf++ = '+';
			}
			strncpy(buf, float_strings[OFS_INF], 3);
			buf += 3;
		}
		else if (digits[0] == 'n' || digits[0] == 'N') {
			strncpy(buf, float_strings[OFS_NAN], 3);
			buf += 3;
		}
		else {
			/* shouldn't get here: Gay's code should always return
			   something starting with a digit, an 'I', or an
			   'N' */
			/*printf("Help! dtoa returned: %.*s\n",
			  (int)n_digits, digits);*/
			strncpy(buf, "ERR", 3);
			buf += 3;
			assert(0);
		}
		*buf = '\0';
		return;
	}

	/* We got digits back, format them. */

	/* this replaces the various tests in other places like:
	    if (type == 'f' && fabs(x) >= 1e50)
		type = 'g';
	   over time, those tests should be deleted
	*/
/*	if (decpt > 50 && format_code == 'f')
		format_code = 'g'; */

	/* Detect if we're using exponents or not */
	switch (format_code) {
	case 'e':
		use_exp = 1;
		trailing_zeros = precision - n_digits;
		break;
	case 'f':
		use_exp = 0;
		trailing_zeros = decpt + precision - n_digits;
		break;
	case 'g':
		if ((mode != 0) && (decpt > precision || decpt <= -4))
			use_exp = 1;
		else {
			use_exp = 0;
		}
		if (use_alt_formatting)
			trailing_zeros = precision - n_digits;
		else
			trailing_zeros = 0;
		break;
	case 'r':
		/* use exponent for values >= 1e16 or < 1e-4 */
		if ((-4 < decpt) && (decpt <= 16))
			use_exp = 0;
		else
			use_exp = 1;
		trailing_zeros = 0;
		break;
	}

	/* Always add a negative sign, and a plus sign if always_add_sign. */
	if (sign == 1)
		*buf++ = '-';
	else if (always_add_sign)
		*buf++ = '+';

	/* dec_pos = position of decimal point in buffer */
	if (use_exp)
		dec_pos = 1;
	else
		dec_pos = decpt;

	/* zero padding on left of digit string */
	if (dec_pos <= 0) {
		*buf++ = '0';
		*buf++ = '.';
		memset(buf, '0', -dec_pos);
		buf -= dec_pos;
	}

	/* digits, with included decimal point */
	if (0 < dec_pos && dec_pos <= n_digits) {
		strncpy(buf, digits, dec_pos);
		buf += dec_pos;
		*buf++ = '.';
		strncpy(buf, digits+dec_pos, n_digits-dec_pos);
		buf += n_digits-dec_pos;
	}
	else {
		strncpy(buf, digits, n_digits);
		buf += n_digits;
	}
	/* and zeros on the right up to the decimal point */
	if (n_digits < dec_pos) {
		memset(buf, '0', dec_pos-n_digits);
		buf += dec_pos-n_digits;
		*buf++ = '.';
		trailing_zeros -= dec_pos-n_digits;
	}
	/* and more trailing zeros, when necessary */
	if (trailing_zeros > 0) {
		memset(buf, '0', trailing_zeros);
		buf += trailing_zeros;
	}

	/* If we're at a trailing decimal, delete it. We are then just an integer. */
	if (buf[-1] == '.' && !(format_code == 'g' && use_alt_formatting)) {
		buf--;
		is_integer = 1;
	}
	else
		/* The decimal isn't at the end, so it's somewhere else in the
		   string. We are therefore not an integer. */
		is_integer = 0;

	/* Now that we've done zero padding, add an exponent if needed. */
	if (use_exp) {
		*buf++ = float_strings[OFS_E][0];
		exp_len = sprintf(buf, "%+.02d", decpt-1);
		buf += exp_len;
		is_integer = 0;
	}

	/* Add ".0" if we're an integer? */
	if (add_dot_0_if_integer && is_integer) {
		*buf++ = '.';
		*buf++ = '0';
	}

	*buf++ = '\0';
}


PyAPI_FUNC(char *) PyOS_double_to_string(double val,
                                         char format_code,
                                         int precision,
                                         int flags)
{
	char* buf = (char *)PyMem_Malloc(512);
	char lc_format_code = format_code;
	char** float_strings = lc_float_strings;
	Py_ssize_t n_wanted_digits_after_decimal = precision;
	int mode = 0;

	/* Validate format_code, and map upper and lower case */
	switch (format_code) {
	case 'e':          /* exponent */
	case 'f':          /* fixed */
	case 'g':          /* general */
	case 'r':          /* repr format */
		break;
	case 'E':
		lc_format_code = 'e';
		break;
	case 'F':
		lc_format_code = 'f';
		break;
	case 'G':
		lc_format_code = 'g';
		break;
	default:
		PyErr_BadInternalCall();
		return NULL;
	}

	if (format_code != lc_format_code)
		float_strings = uc_float_strings;

	/* From the format code, compute the mode and make any adjustments as
	   needed. */
	switch (lc_format_code) {
	case 'e':
		mode = 2;
		precision++;
		break;
	case 'f':
		mode = 3;
		break;
	case 'g':
		mode = 2;
		if (flags & Py_DTSF_ALT)
			n_wanted_digits_after_decimal--;
		break;
	case 'r':
		/* "repr" pseudo-mode */
		mode = 0;
		break;
	}

	if (!buf)
		return NULL;

	/* XXX validate format_code */

	format_float_short(buf, 512, val, lc_format_code, mode, precision,
			   n_wanted_digits_after_decimal, flags & Py_DTSF_SIGN,
			   flags & Py_DTSF_ADD_DOT_0, flags & Py_DTSF_ALT,
			   float_strings);

	return buf;
}
