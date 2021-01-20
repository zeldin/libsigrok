/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* Needed for POSIX.1-2008 locale functions */
/** @cond PRIVATE */
#define _XOPEN_SOURCE 700
/** @endcond */
#include <config.h>
#include <ctype.h>
#include <locale.h>
#if defined(__FreeBSD__) || defined(__APPLE__)
#include <xlocale.h>
#endif
#if defined(__FreeBSD__)
#include <sys/param.h>
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

/** @cond PRIVATE */
#define LOG_PREFIX "strutil"
/** @endcond */

/**
 * @file
 *
 * Helper functions for handling or converting libsigrok-related strings.
 */

/**
 * @defgroup grp_strutil String utilities
 *
 * Helper functions for handling or converting libsigrok-related strings.
 *
 * @{
 */

/**
 * Convert a string representation of a numeric value (base 10) to a long integer. The
 * conversion is strict and will fail if the complete string does not represent
 * a valid long integer. The function sets errno according to the details of the
 * failure.
 *
 * @param str The string representation to convert.
 * @param ret Pointer to long where the result of the conversion will be stored.
 *
 * @retval SR_OK Conversion successful.
 * @retval SR_ERR Failure.
 *
 * @private
 */
SR_PRIV int sr_atol(const char *str, long *ret)
{
	long tmp;
	char *endptr = NULL;

	errno = 0;
	tmp = strtol(str, &endptr, 10);

	while (endptr && isspace(*endptr))
		endptr++;

	if (!endptr || *endptr || errno) {
		if (!errno)
			errno = EINVAL;
		return SR_ERR;
	}

	*ret = tmp;
	return SR_OK;
}

/**
 * Convert a text to a number including support for non-decimal bases.
 * Also optionally returns the position after the number, where callers
 * can either error out, or support application specific suffixes.
 *
 * @param[in] str The input text to convert.
 * @param[out] ret The conversion result.
 * @param[out] end The position after the number.
 * @param[in] base The number format's base, can be 0.
 *
 * @retval SR_OK Conversion successful.
 * @retval SR_ERR Conversion failed.
 *
 * @private
 *
 * This routine is more general than @ref sr_atol(), which strictly
 * expects the input text to contain just a decimal number, and nothing
 * else in addition. The @ref sr_atol_base() routine accepts trailing
 * text after the number, and supports non-decimal numbers (bin, hex),
 * including automatic detection from prefix text.
 */
SR_PRIV int sr_atol_base(const char *str, long *ret, char **end, int base)
{
	long num;
	char *endptr;

	/* Add "0b" prefix support which strtol(3) may be missing. */
	while (str && isspace(*str))
		str++;
	if (!base && strncmp(str, "0b", strlen("0b")) == 0) {
		str += strlen("0b");
		base = 2;
	}

	/* Run the number conversion. Quick bail out if that fails. */
	errno = 0;
	endptr = NULL;
	num = strtol(str, &endptr, base);
	if (!endptr || errno) {
		if (!errno)
			errno = EINVAL;
		return SR_ERR;
	}
	*ret = num;

	/* Advance to optional non-space trailing suffix. */
	while (endptr && isspace(*endptr))
		endptr++;
	if (end)
		*end = endptr;

	return SR_OK;
}

/**
 * Convert a text to a number including support for non-decimal bases.
 * Also optionally returns the position after the number, where callers
 * can either error out, or support application specific suffixes.
 *
 * @param[in] str The input text to convert.
 * @param[out] ret The conversion result.
 * @param[out] end The position after the number.
 * @param[in] base The number format's base, can be 0.
 *
 * @retval SR_OK Conversion successful.
 * @retval SR_ERR Conversion failed.
 *
 * @private
 *
 * This routine is more general than @ref sr_atol(), which strictly
 * expects the input text to contain just a decimal number, and nothing
 * else in addition. The @ref sr_atoul_base() routine accepts trailing
 * text after the number, and supports non-decimal numbers (bin, hex),
 * including automatic detection from prefix text.
 */
SR_PRIV int sr_atoul_base(const char *str, unsigned long *ret, char **end, int base)
{
	unsigned long num;
	char *endptr;

	/* Add "0b" prefix support which strtol(3) may be missing. */
	while (str && isspace(*str))
		str++;
	if (!base && strncmp(str, "0b", strlen("0b")) == 0) {
		str += strlen("0b");
		base = 2;
	}

	/* Run the number conversion. Quick bail out if that fails. */
	errno = 0;
	endptr = NULL;
	num = strtoul(str, &endptr, base);
	if (!endptr || errno) {
		if (!errno)
			errno = EINVAL;
		return SR_ERR;
	}
	*ret = num;

	/* Advance to optional non-space trailing suffix. */
	while (endptr && isspace(*endptr))
		endptr++;
	if (end)
		*end = endptr;

	return SR_OK;
}

/**
 * Convert a string representation of a numeric value (base 10) to an integer. The
 * conversion is strict and will fail if the complete string does not represent
 * a valid integer. The function sets errno according to the details of the
 * failure.
 *
 * @param str The string representation to convert.
 * @param ret Pointer to int where the result of the conversion will be stored.
 *
 * @retval SR_OK Conversion successful.
 * @retval SR_ERR Failure.
 *
 * @private
 */
SR_PRIV int sr_atoi(const char *str, int *ret)
{
	long tmp;

	if (sr_atol(str, &tmp) != SR_OK)
		return SR_ERR;

	if ((int) tmp != tmp) {
		errno = ERANGE;
		return SR_ERR;
	}

	*ret = (int) tmp;
	return SR_OK;
}

/**
 * Convert a string representation of a numeric value to a double. The
 * conversion is strict and will fail if the complete string does not represent
 * a valid double. The function sets errno according to the details of the
 * failure.
 *
 * @param str The string representation to convert.
 * @param ret Pointer to double where the result of the conversion will be stored.
 *
 * @retval SR_OK Conversion successful.
 * @retval SR_ERR Failure.
 *
 * @private
 */
SR_PRIV int sr_atod(const char *str, double *ret)
{
	double tmp;
	char *endptr = NULL;

	errno = 0;
	tmp = strtof(str, &endptr);

	while (endptr && isspace(*endptr))
		endptr++;

	if (!endptr || *endptr || errno) {
		if (!errno)
			errno = EINVAL;
		return SR_ERR;
	}

	*ret = tmp;
	return SR_OK;
}

/**
 * Convert a string representation of a numeric value to a float. The
 * conversion is strict and will fail if the complete string does not represent
 * a valid float. The function sets errno according to the details of the
 * failure.
 *
 * @param str The string representation to convert.
 * @param ret Pointer to float where the result of the conversion will be stored.
 *
 * @retval SR_OK Conversion successful.
 * @retval SR_ERR Failure.
 *
 * @private
 */
SR_PRIV int sr_atof(const char *str, float *ret)
{
	double tmp;

	if (sr_atod(str, &tmp) != SR_OK)
		return SR_ERR;

	if ((float) tmp != tmp) {
		errno = ERANGE;
		return SR_ERR;
	}

	*ret = (float) tmp;
	return SR_OK;
}

/**
 * Convert a string representation of a numeric value to a double. The
 * conversion is strict and will fail if the complete string does not represent
 * a valid double. The function sets errno according to the details of the
 * failure. This version ignores the locale.
 *
 * @param str The string representation to convert.
 * @param ret Pointer to double where the result of the conversion will be stored.
 *
 * @retval SR_OK Conversion successful.
 * @retval SR_ERR Failure.
 *
 * @private
 */
SR_PRIV int sr_atod_ascii(const char *str, double *ret)
{
	double tmp;
	char *endptr = NULL;

	errno = 0;
	tmp = g_ascii_strtod(str, &endptr);

	if (!endptr || *endptr || errno) {
		if (!errno)
			errno = EINVAL;
		return SR_ERR;
	}

	*ret = tmp;
	return SR_OK;
}

/**
 * Convert text to a floating point value, and get its precision.
 *
 * @param[in] str The input text to convert.
 * @param[out] ret The conversion result, a double precision float number.
 * @param[out] digits The number of significant decimals.
 *
 * @returns SR_OK in case of successful text to number conversion.
 * @returns SR_ERR when conversion fails.
 *
 * @since 0.6.0
 */
SR_PRIV int sr_atod_ascii_digits(const char *str, double *ret, int *digits)
{
	const char *p;
	int *dig_ref, m_dig, exp;
	char c;
	double f;

	/*
	 * Convert floating point text to the number value, _and_ get
	 * the value's precision in the process. Steps taken to do it:
	 * - Skip leading whitespace.
	 * - Count the number of decimals after the mantissa's period.
	 * - Get the exponent's signed value.
	 *
	 * This implementation still uses common code for the actual
	 * conversion, but "violates API layers" by duplicating the
	 * text scan, to get the number of significant digits.
	 */
	p = str;
	while (*p && isspace(*p))
		p++;
	if (*p == '-' || *p == '+')
		p++;
	m_dig = 0;
	exp = 0;
	dig_ref = NULL;
	while (*p) {
		c = *p++;
		if (toupper(c) == 'E') {
			exp = strtol(p, NULL, 10);
			break;
		}
		if (c == '.') {
			m_dig = 0;
			dig_ref = &m_dig;
			continue;
		}
		if (isdigit(c)) {
			if (dig_ref)
				(*dig_ref)++;
			continue;
		}
		/* Need not warn, conversion will fail. */
		break;
	}
	sr_spew("atod digits: txt \"%s\" -> m %d, e %d -> digits %d",
		str, m_dig, exp, m_dig + -exp);
	m_dig += -exp;

	if (sr_atod_ascii(str, &f) != SR_OK)
		return SR_ERR;
	if (ret)
		*ret = f;
	if (digits)
		*digits = m_dig;

	return SR_OK;
}

/**
 * Convert a string representation of a numeric value to a float. The
 * conversion is strict and will fail if the complete string does not represent
 * a valid float. The function sets errno according to the details of the
 * failure. This version ignores the locale.
 *
 * @param str The string representation to convert.
 * @param ret Pointer to float where the result of the conversion will be stored.
 *
 * @retval SR_OK Conversion successful.
 * @retval SR_ERR Failure.
 *
 * @private
 */
SR_PRIV int sr_atof_ascii(const char *str, float *ret)
{
	double tmp;
	char *endptr = NULL;

	errno = 0;
	tmp = g_ascii_strtod(str, &endptr);

	if (!endptr || *endptr || errno) {
		if (!errno)
			errno = EINVAL;
		return SR_ERR;
	}

	/* FIXME This fails unexpectedly. Some other method to safel downcast
	 * needs to be found. Checking against FLT_MAX doesn't work as well. */
	/*
	if ((float) tmp != tmp) {
		errno = ERANGE;
		sr_dbg("ERANGEEEE %e != %e", (float) tmp, tmp);
		return SR_ERR;
	}
	*/

	*ret = (float) tmp;
	return SR_OK;
}

/**
 * Compose a string with a format string in the buffer pointed to by buf.
 *
 * It is up to the caller to ensure that the allocated buffer is large enough
 * to hold the formatted result.
 *
 * A terminating NUL character is automatically appended after the content
 * written.
 *
 * After the format parameter, the function expects at least as many additional
 * arguments as needed for format.
 *
 * This version ignores the current locale and uses the locale "C" for Linux,
 * FreeBSD, OSX and Android.
 *
 * @param buf Pointer to a buffer where the resulting C string is stored.
 * @param format C string that contains a format string (see printf).
 * @param ... A sequence of additional arguments, each containing a value to be
 *        used to replace a format specifier in the format string.
 *
 * @return On success, the number of characters that would have been written,
 *         not counting the terminating NUL character.
 *
 * @since 0.6.0
 */
SR_API int sr_sprintf_ascii(char *buf, const char *format, ...)
{
	int ret;
	va_list args;

	va_start(args, format);
	ret = sr_vsprintf_ascii(buf, format, args);
	va_end(args);

	return ret;
}

/**
 * Compose a string with a format string in the buffer pointed to by buf.
 *
 * It is up to the caller to ensure that the allocated buffer is large enough
 * to hold the formatted result.
 *
 * Internally, the function retrieves arguments from the list identified by
 * args as if va_arg was used on it, and thus the state of args is likely to
 * be altered by the call.
 *
 * In any case, args should have been initialized by va_start at some point
 * before the call, and it is expected to be released by va_end at some point
 * after the call.
 *
 * This version ignores the current locale and uses the locale "C" for Linux,
 * FreeBSD, OSX and Android.
 *
 * @param buf Pointer to a buffer where the resulting C string is stored.
 * @param format C string that contains a format string (see printf).
 * @param args A value identifying a variable arguments list initialized with
 *        va_start.
 *
 * @return On success, the number of characters that would have been written,
 *         not counting the terminating NUL character.
 *
 * @since 0.6.0
 */
SR_API int sr_vsprintf_ascii(char *buf, const char *format, va_list args)
{
#if defined(_WIN32)
	int ret;

#if 0
	/*
	 * TODO: This part compiles with mingw-w64 but doesn't run with Win7.
	 *       Doesn't start because of "Procedure entry point _create_locale
	 *       not found in msvcrt.dll".
	 *       mingw-w64 should link to msvcr100.dll not msvcrt.dll!
	 * See: https://msdn.microsoft.com/en-us/en-en/library/1kt27hek.aspx
	 */
	_locale_t locale;

	locale = _create_locale(LC_NUMERIC, "C");
	ret = _vsprintf_l(buf, format, locale, args);
	_free_locale(locale);
#endif

	/* vsprintf() uses the current locale, may not work correctly for floats. */
	ret = vsprintf(buf, format, args);

	return ret;
#elif defined(__APPLE__)
	/*
	 * See:
	 * https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man3/printf_l.3.html
	 * https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man3/xlocale.3.html
	 */
	int ret;
	locale_t locale;

	locale = newlocale(LC_NUMERIC_MASK, "C", NULL);
	ret = vsprintf_l(buf, locale, format, args);
	freelocale(locale);

	return ret;
#elif defined(__FreeBSD__) && __FreeBSD_version >= 901000
	/*
	 * See:
	 * https://www.freebsd.org/cgi/man.cgi?query=printf_l&apropos=0&sektion=3&manpath=FreeBSD+9.1-RELEASE
	 * https://www.freebsd.org/cgi/man.cgi?query=xlocale&apropos=0&sektion=3&manpath=FreeBSD+9.1-RELEASE
	 */
	int ret;
	locale_t locale;

	locale = newlocale(LC_NUMERIC_MASK, "C", NULL);
	ret = vsprintf_l(buf, locale, format, args);
	freelocale(locale);

	return ret;
#elif defined(__ANDROID__)
	/*
	 * The Bionic libc only has two locales ("C" aka "POSIX" and "C.UTF-8"
	 * aka "en_US.UTF-8"). The decimal point is hard coded as "."
	 * See: https://android.googlesource.com/platform/bionic/+/master/libc/bionic/locale.cpp
	 */
	int ret;

	ret = vsprintf(buf, format, args);

	return ret;
#elif defined(__linux__)
	int ret;
	locale_t old_locale, temp_locale;

	/* Switch to C locale for proper float/double conversion. */
	temp_locale = newlocale(LC_NUMERIC, "C", NULL);
	old_locale = uselocale(temp_locale);

	ret = vsprintf(buf, format, args);

	/* Switch back to original locale. */
	uselocale(old_locale);
	freelocale(temp_locale);

	return ret;
#elif defined(__unix__) || defined(__unix)
	/*
	 * This is a fallback for all other BSDs, *nix and FreeBSD <= 9.0, by
	 * using the current locale for snprintf(). This may not work correctly
	 * for floats!
	 */
	int ret;

	ret = vsprintf(buf, format, args);

	return ret;
#else
	/* No implementation for unknown systems! */
	return -1;
#endif
}

/**
 * Composes a string with a format string (like printf) in the buffer pointed
 * by buf (taking buf_size as the maximum buffer capacity to fill).
 * If the resulting string would be longer than n - 1 characters, the remaining
 * characters are discarded and not stored, but counted for the value returned
 * by the function.
 * A terminating NUL character is automatically appended after the content
 * written.
 * After the format parameter, the function expects at least as many additional
 * arguments as needed for format.
 *
 * This version ignores the current locale and uses the locale "C" for Linux,
 * FreeBSD, OSX and Android.
 *
 * @param buf Pointer to a buffer where the resulting C string is stored.
 * @param buf_size Maximum number of bytes to be used in the buffer. The
 *        generated string has a length of at most buf_size - 1, leaving space
 *        for the additional terminating NUL character.
 * @param format C string that contains a format string (see printf).
 * @param ... A sequence of additional arguments, each containing a value to be
 *        used to replace a format specifier in the format string.
 *
 * @return On success, the number of characters that would have been written if
 *         buf_size had been sufficiently large, not counting the terminating
 *         NUL character. On failure, a negative number is returned.
 *         Notice that only when this returned value is non-negative and less
 *         than buf_size, the string has been completely written.
 *
 * @since 0.6.0
 */
SR_API int sr_snprintf_ascii(char *buf, size_t buf_size,
	const char *format, ...)
{
	int ret;
	va_list args;

	va_start(args, format);
	ret = sr_vsnprintf_ascii(buf, buf_size, format, args);
	va_end(args);

	return ret;
}

/**
 * Composes a string with a format string (like printf) in the buffer pointed
 * by buf (taking buf_size as the maximum buffer capacity to fill).
 * If the resulting string would be longer than n - 1 characters, the remaining
 * characters are discarded and not stored, but counted for the value returned
 * by the function.
 * A terminating NUL character is automatically appended after the content
 * written.
 * Internally, the function retrieves arguments from the list identified by
 * args as if va_arg was used on it, and thus the state of args is likely to
 * be altered by the call.
 * In any case, arg should have been initialized by va_start at some point
 * before the call, and it is expected to be released by va_end at some point
 * after the call.
 *
 * This version ignores the current locale and uses the locale "C" for Linux,
 * FreeBSD, OSX and Android.
 *
 * @param buf Pointer to a buffer where the resulting C string is stored.
 * @param buf_size Maximum number of bytes to be used in the buffer. The
 *        generated string has a length of at most buf_size - 1, leaving space
 *        for the additional terminating NUL character.
 * @param format C string that contains a format string (see printf).
 * @param args A value identifying a variable arguments list initialized with
 *        va_start.
 *
 * @return On success, the number of characters that would have been written if
 *         buf_size had been sufficiently large, not counting the terminating
 *         NUL character. On failure, a negative number is returned.
 *         Notice that only when this returned value is non-negative and less
 *         than buf_size, the string has been completely written.
 *
 * @since 0.6.0
 */
SR_API int sr_vsnprintf_ascii(char *buf, size_t buf_size,
	const char *format, va_list args)
{
#if defined(_WIN32)
	int ret;

#if 0
	/*
	 * TODO: This part compiles with mingw-w64 but doesn't run with Win7.
	 *       Doesn't start because of "Procedure entry point _create_locale
	 *       not found in msvcrt.dll".
	 *       mingw-w64 should link to msvcr100.dll not msvcrt.dll!.
	 * See: https://msdn.microsoft.com/en-us/en-en/library/1kt27hek.aspx
	 */
	_locale_t locale;

	locale = _create_locale(LC_NUMERIC, "C");
	ret = _vsnprintf_l(buf, buf_size, format, locale, args);
	_free_locale(locale);
#endif

	/* vsprintf uses the current locale, may cause issues for floats. */
	ret = vsnprintf(buf, buf_size, format, args);

	return ret;
#elif defined(__APPLE__)
	/*
	 * See:
	 * https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man3/printf_l.3.html
	 * https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man3/xlocale.3.html
	 */
	int ret;
	locale_t locale;

	locale = newlocale(LC_NUMERIC_MASK, "C", NULL);
	ret = vsnprintf_l(buf, buf_size, locale, format, args);
	freelocale(locale);

	return ret;
#elif defined(__FreeBSD__) && __FreeBSD_version >= 901000
	/*
	 * See:
	 * https://www.freebsd.org/cgi/man.cgi?query=printf_l&apropos=0&sektion=3&manpath=FreeBSD+9.1-RELEASE
	 * https://www.freebsd.org/cgi/man.cgi?query=xlocale&apropos=0&sektion=3&manpath=FreeBSD+9.1-RELEASE
	 */
	int ret;
	locale_t locale;

	locale = newlocale(LC_NUMERIC_MASK, "C", NULL);
	ret = vsnprintf_l(buf, buf_size, locale, format, args);
	freelocale(locale);

	return ret;
#elif defined(__ANDROID__)
	/*
	 * The Bionic libc only has two locales ("C" aka "POSIX" and "C.UTF-8"
	 * aka "en_US.UTF-8"). The decimal point is hard coded as ".".
	 * See: https://android.googlesource.com/platform/bionic/+/master/libc/bionic/locale.cpp
	 */
	int ret;

	ret = vsnprintf(buf, buf_size, format, args);

	return ret;
#elif defined(__linux__)
	int ret;
	locale_t old_locale, temp_locale;

	/* Switch to C locale for proper float/double conversion. */
	temp_locale = newlocale(LC_NUMERIC, "C", NULL);
	old_locale = uselocale(temp_locale);

	ret = vsnprintf(buf, buf_size, format, args);

	/* Switch back to original locale. */
	uselocale(old_locale);
	freelocale(temp_locale);

	return ret;
#elif defined(__unix__) || defined(__unix)
	/*
	 * This is a fallback for all other BSDs, *nix and FreeBSD <= 9.0, by
	 * using the current locale for snprintf(). This may not work correctly
	 * for floats!
	 */
	int ret;

	ret = vsnprintf(buf, buf_size, format, args);

	return ret;
#else
	/* No implementation for unknown systems! */
	return -1;
#endif
}

/**
 * Convert a sequence of bytes to its textual representation ("hex dump").
 *
 * Callers should free the allocated GString. See sr_hexdump_free().
 *
 * @param[in] data Pointer to the byte sequence to print.
 * @param[in] len Number of bytes to print.
 *
 * @return NULL upon error, newly allocated GString pointer otherwise.
 *
 * @private
 */
SR_PRIV GString *sr_hexdump_new(const uint8_t *data, const size_t len)
{
	GString *s;
	size_t i;

	s = g_string_sized_new(3 * len);
	for (i = 0; i < len; i++) {
		if (i)
			g_string_append_c(s, ' ');
		g_string_append_printf(s, "%02x", data[i]);
	}

	return s;
}

/**
 * Free a hex dump text that was created by sr_hexdump_new().
 *
 * @param[in] s Pointer to the GString to release.
 *
 * @private
 */
SR_PRIV void sr_hexdump_free(GString *s)
{
	if (s)
		g_string_free(s, TRUE);
}

/**
 * Convert a string representation of a numeric value to a sr_rational.
 *
 * The conversion is strict and will fail if the complete string does not
 * represent a valid number. The function sets errno according to the details
 * of the failure. This version ignores the locale.
 *
 * @param str The string representation to convert.
 * @param ret Pointer to sr_rational where the result of the conversion will be stored.
 *
 * @retval SR_OK Conversion successful.
 * @retval SR_ERR Failure.
 *
 * @since 0.5.0
 */
SR_API int sr_parse_rational(const char *str, struct sr_rational *ret)
{
	char *endptr = NULL;
	int64_t integral;
	int64_t fractional = 0;
	int64_t denominator = 1;
	int32_t fractional_len = 0;
	int32_t exponent = 0;
	gboolean is_negative = FALSE;
	gboolean no_integer, no_fractional;

	while (isspace(*str))
		str++;

	errno = 0;
	integral = g_ascii_strtoll(str, &endptr, 10);

	if (str == endptr && (str[0] == '-' || str[0] == '+') && str[1] == '.') {
		endptr += 1;
		no_integer = TRUE;
	} else if (str == endptr && str[0] == '.') {
		no_integer = TRUE;
	} else if (errno) {
		return SR_ERR;
	} else {
		no_integer = FALSE;
	}

	if (integral < 0 || str[0] == '-')
		is_negative = TRUE;

	errno = 0;
	if (*endptr == '.') {
		gboolean is_exp, is_eos;
		const char *start = endptr + 1;
		fractional = g_ascii_strtoll(start, &endptr, 10);
		is_exp = *endptr == 'E' || *endptr == 'e';
		is_eos = *endptr == '\0';
		if (endptr == start && (is_exp || is_eos)) {
			fractional = 0;
			errno = 0;
		}
		if (errno)
			return SR_ERR;
		no_fractional = endptr == start;
		if (no_integer && no_fractional)
			return SR_ERR;
		fractional_len = endptr - start;
	}

	errno = 0;
	if ((*endptr == 'E') || (*endptr == 'e')) {
		exponent = g_ascii_strtoll(endptr + 1, &endptr, 10);
		if (errno)
			return SR_ERR;
	}

	if (*endptr != '\0')
		return SR_ERR;

	for (int i = 0; i < fractional_len; i++)
		integral *= 10;
	exponent -= fractional_len;

	if (!is_negative)
		integral += fractional;
	else
		integral -= fractional;

	while (exponent > 0) {
		integral *= 10;
		exponent--;
	}

	while (exponent < 0) {
		denominator *= 10;
		exponent++;
	}

	ret->p = integral;
	ret->q = denominator;

	return SR_OK;
}

/**
 * Convert a numeric value value to its "natural" string representation
 * in SI units.
 *
 * E.g. a value of 3000000, with units set to "W", would be converted
 * to "3 MW", 20000 to "20 kW", 31500 would become "31.5 kW".
 *
 * @param x The value to convert.
 * @param unit The unit to append to the string, or NULL if the string
 *             has no units.
 *
 * @return A newly allocated string representation of the samplerate value,
 *         or NULL upon errors. The caller is responsible to g_free() the
 *         memory.
 *
 * @since 0.2.0
 */
SR_API char *sr_si_string_u64(uint64_t x, const char *unit)
{
	uint8_t i;
	uint64_t quot, divisor[] = {
		SR_HZ(1), SR_KHZ(1), SR_MHZ(1), SR_GHZ(1),
		SR_GHZ(1000), SR_GHZ(1000 * 1000), SR_GHZ(1000 * 1000 * 1000),
	};
	const char *p, prefix[] = "\0kMGTPE";
	char fmt[16], fract[20] = "", *f;

	if (!unit)
		unit = "";

	for (i = 0; (quot = x / divisor[i]) >= 1000; i++);

	if (i) {
		sprintf(fmt, ".%%0%d"PRIu64, i * 3);
		f = fract + sprintf(fract, fmt, x % divisor[i]) - 1;

		while (f >= fract && strchr("0.", *f))
			*f-- = 0;
	}

	p = prefix + i;

	return g_strdup_printf("%" PRIu64 "%s %.1s%s", quot, fract, p, unit);
}

/**
 * Convert a numeric samplerate value to its "natural" string representation.
 *
 * E.g. a value of 3000000 would be converted to "3 MHz", 20000 to "20 kHz",
 * 31500 would become "31.5 kHz".
 *
 * @param samplerate The samplerate in Hz.
 *
 * @return A newly allocated string representation of the samplerate value,
 *         or NULL upon errors. The caller is responsible to g_free() the
 *         memory.
 *
 * @since 0.1.0
 */
SR_API char *sr_samplerate_string(uint64_t samplerate)
{
	return sr_si_string_u64(samplerate, "Hz");
}

/**
 * Convert a numeric period value to the "natural" string representation
 * of its period value.
 *
 * The period is specified as a rational number's numerator and denominator.
 *
 * E.g. a pair of (1, 5) would be converted to "200 ms", (10, 100) to "100 ms".
 *
 * @param v_p The period numerator.
 * @param v_q The period denominator.
 *
 * @return A newly allocated string representation of the period value,
 *         or NULL upon errors. The caller is responsible to g_free() the
 *         memory.
 *
 * @since 0.5.0
 */
SR_API char *sr_period_string(uint64_t v_p, uint64_t v_q)
{
	double freq, v;
	int prec;

	freq = 1 / ((double)v_p / v_q);

	if (freq > SR_GHZ(1)) {
		v = (double)v_p / v_q * 1000000000000.0;
		prec = ((v - (uint64_t)v) < FLT_MIN) ? 0 : 3;
		return g_strdup_printf("%.*f ps", prec, v);
	} else if (freq > SR_MHZ(1)) {
		v = (double)v_p / v_q * 1000000000.0;
		prec = ((v - (uint64_t)v) < FLT_MIN) ? 0 : 3;
		return g_strdup_printf("%.*f ns", prec, v);
	} else if (freq > SR_KHZ(1)) {
		v = (double)v_p / v_q * 1000000.0;
		prec = ((v - (uint64_t)v) < FLT_MIN) ? 0 : 3;
		return g_strdup_printf("%.*f us", prec, v);
	} else if (freq > 1) {
		v = (double)v_p / v_q * 1000.0;
		prec = ((v - (uint64_t)v) < FLT_MIN) ? 0 : 3;
		return g_strdup_printf("%.*f ms", prec, v);
	} else {
		v = (double)v_p / v_q;
		prec = ((v - (uint64_t)v) < FLT_MIN) ? 0 : 3;
		return g_strdup_printf("%.*f s", prec, v);
	}
}

/**
 * Convert a numeric voltage value to the "natural" string representation
 * of its voltage value. The voltage is specified as a rational number's
 * numerator and denominator.
 *
 * E.g. a value of 300000 would be converted to "300mV", 2 to "2V".
 *
 * @param v_p The voltage numerator.
 * @param v_q The voltage denominator.
 *
 * @return A newly allocated string representation of the voltage value,
 *         or NULL upon errors. The caller is responsible to g_free() the
 *         memory.
 *
 * @since 0.2.0
 */
SR_API char *sr_voltage_string(uint64_t v_p, uint64_t v_q)
{
	if (v_q == 1000)
		return g_strdup_printf("%" PRIu64 " mV", v_p);
	else if (v_q == 1)
		return g_strdup_printf("%" PRIu64 " V", v_p);
	else
		return g_strdup_printf("%g V", (float)v_p / (float)v_q);
}

/**
 * Convert a "natural" string representation of a size value to uint64_t.
 *
 * E.g. a value of "3k" or "3 K" would be converted to 3000, a value
 * of "15M" would be converted to 15000000.
 *
 * Value representations other than decimal (such as hex or octal) are not
 * supported. Only 'k' (kilo), 'm' (mega), 'g' (giga) suffixes are supported.
 * Spaces (but not other whitespace) between value and suffix are allowed.
 *
 * @param sizestring A string containing a (decimal) size value.
 * @param size Pointer to uint64_t which will contain the string's size value.
 *
 * @return SR_OK upon success, SR_ERR upon errors.
 *
 * @since 0.1.0
 */
SR_API int sr_parse_sizestring(const char *sizestring, uint64_t *size)
{
	uint64_t multiplier;
	int done;
	double frac_part;
	char *s;

	*size = strtoull(sizestring, &s, 10);
	multiplier = 0;
	frac_part = 0;
	done = FALSE;
	while (s && *s && multiplier == 0 && !done) {
		switch (*s) {
		case ' ':
			break;
		case '.':
			frac_part = g_ascii_strtod(s, &s);
			break;
		case 'k':
		case 'K':
			multiplier = SR_KHZ(1);
			break;
		case 'm':
		case 'M':
			multiplier = SR_MHZ(1);
			break;
		case 'g':
		case 'G':
			multiplier = SR_GHZ(1);
			break;
		case 't':
		case 'T':
			multiplier = SR_GHZ(1000);
			break;
		case 'p':
		case 'P':
			multiplier = SR_GHZ(1000 * 1000);
			break;
		case 'e':
		case 'E':
			multiplier = SR_GHZ(1000 * 1000 * 1000);
			break;
		default:
			done = TRUE;
			s--;
		}
		s++;
	}
	if (multiplier > 0) {
		*size *= multiplier;
		*size += frac_part * multiplier;
	} else {
		*size += frac_part;
	}

	if (s && *s && g_ascii_strcasecmp(s, "Hz"))
		return SR_ERR;

	return SR_OK;
}

/**
 * Convert a "natural" string representation of a time value to an
 * uint64_t value in milliseconds.
 *
 * E.g. a value of "3s" or "3 s" would be converted to 3000, a value
 * of "15ms" would be converted to 15.
 *
 * Value representations other than decimal (such as hex or octal) are not
 * supported. Only lower-case "s" and "ms" time suffixes are supported.
 * Spaces (but not other whitespace) between value and suffix are allowed.
 *
 * @param timestring A string containing a (decimal) time value.
 * @return The string's time value as uint64_t, in milliseconds.
 *
 * @todo Add support for "m" (minutes) and others.
 * @todo Add support for picoseconds?
 * @todo Allow both lower-case and upper-case? If no, document it.
 *
 * @since 0.1.0
 */
SR_API uint64_t sr_parse_timestring(const char *timestring)
{
	uint64_t time_msec;
	char *s;

	/* TODO: Error handling, logging. */

	time_msec = strtoull(timestring, &s, 10);
	if (time_msec == 0 && s == timestring)
		return 0;

	if (s && *s) {
		while (*s == ' ')
			s++;
		if (!strcmp(s, "s"))
			time_msec *= 1000;
		else if (!strcmp(s, "ms"))
			; /* redundant */
		else
			return 0;
	}

	return time_msec;
}

/** @since 0.1.0 */
SR_API gboolean sr_parse_boolstring(const char *boolstr)
{
	/*
	 * Complete absence of an input spec is assumed to mean TRUE,
	 * as in command line option strings like this:
	 *   ...:samplerate=100k:header:numchannels=4:...
	 */
	if (!boolstr || !*boolstr)
		return TRUE;

	if (!g_ascii_strncasecmp(boolstr, "true", 4) ||
	    !g_ascii_strncasecmp(boolstr, "yes", 3) ||
	    !g_ascii_strncasecmp(boolstr, "on", 2) ||
	    !g_ascii_strncasecmp(boolstr, "1", 1))
		return TRUE;

	return FALSE;
}

/** @since 0.2.0 */
SR_API int sr_parse_period(const char *periodstr, uint64_t *p, uint64_t *q)
{
	char *s;

	*p = strtoull(periodstr, &s, 10);
	if (*p == 0 && s == periodstr)
		/* No digits found. */
		return SR_ERR_ARG;

	if (s && *s) {
		while (*s == ' ')
			s++;
		if (!strcmp(s, "fs"))
			*q = UINT64_C(1000000000000000);
		else if (!strcmp(s, "ps"))
			*q = UINT64_C(1000000000000);
		else if (!strcmp(s, "ns"))
			*q = UINT64_C(1000000000);
		else if (!strcmp(s, "us"))
			*q = 1000000;
		else if (!strcmp(s, "ms"))
			*q = 1000;
		else if (!strcmp(s, "s"))
			*q = 1;
		else
			/* Must have a time suffix. */
			return SR_ERR_ARG;
	}

	return SR_OK;
}

/** @since 0.2.0 */
SR_API int sr_parse_voltage(const char *voltstr, uint64_t *p, uint64_t *q)
{
	char *s;

	*p = strtoull(voltstr, &s, 10);
	if (*p == 0 && s == voltstr)
		/* No digits found. */
		return SR_ERR_ARG;

	if (s && *s) {
		while (*s == ' ')
			s++;
		if (!g_ascii_strcasecmp(s, "mv"))
			*q = 1000L;
		else if (!g_ascii_strcasecmp(s, "v"))
			*q = 1;
		else
			/* Must have a base suffix. */
			return SR_ERR_ARG;
	}

	return SR_OK;
}

/** @} */
