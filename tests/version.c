/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Uwe Hermann <uwe@hermann-uwe.de>
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

#include <config.h>
#include <stdlib.h>
#include <check.h>
#include <libsigrok/libsigrok.h>
#include "lib.h"

/*
 * Check the version number API calls and macros.
 *
 * The numbers returned by the sr_*_version*get() calls must match the
 * respective SR_*_VERSION* macro values, must be >= 0, and must not be
 * unreasonably high (> 20), otherwise something is probably wrong.
 */
START_TEST(test_version_numbers)
{
	int ver;

	ver = sr_package_version_major_get();
	fail_unless(ver == SR_PACKAGE_VERSION_MAJOR);
	fail_unless(ver >= 0 && ver <= 20);
	ver = sr_package_version_minor_get();
	fail_unless(ver == SR_PACKAGE_VERSION_MINOR);
	fail_unless(ver >= 0 && ver <= 20);
	ver = sr_package_version_micro_get();
	fail_unless(ver == SR_PACKAGE_VERSION_MICRO);
	fail_unless(ver >= 0 && ver <= 20);

	ver = sr_lib_version_current_get();
	fail_unless(ver == SR_LIB_VERSION_CURRENT);
	fail_unless(ver >= 0 && ver <= 20);
	ver = sr_lib_version_revision_get();
	fail_unless(ver == SR_LIB_VERSION_REVISION);
	fail_unless(ver >= 0 && ver <= 20);
	ver = sr_lib_version_age_get();
	fail_unless(ver == SR_LIB_VERSION_AGE);
	fail_unless(ver >= 0 && ver <= 20);
}
END_TEST

/*
 * Check the version number API calls and macros.
 *
 * The string representations of the package/lib version must neither be
 * NULL nor empty, and the length shall be within an expected range.
 *
 * The lower limit assumes:
 * - A version text consists of three parts (major, minor, micro),
 *   like "0.1.0".
 * - Three numbers with at least one digit, and their separators,
 *   result in a minimum length of 5.
 *
 * The upper limit assumes:
 * - The major, minor, and micro parts won't contain more than two
 *   digits each (this is an arbitrary choice).
 * - An optional "-git-<hash>" suffix might follow. While git(1)
 *   defaults to 7 hex digits for abbreviated hashes, projects of
 *   larger scale might recommend to use more digits to avoid
 *   potential ambiguity (e.g. Linux recommends core.abbrev=12).
 *   Again, this is an arbitrary choice.
 */
START_TEST(test_version_strings)
{
	const char *str;
	const size_t len_min = 5;
	const size_t len_max = 2 + 1 + 2 + 1 + 2 + 5 + 12;

	str = sr_package_version_string_get();
	fail_unless(str != NULL);
	fail_unless(strlen(str) >= len_min);
	fail_unless(strlen(str) <= len_max);
	str = sr_lib_version_string_get();
	fail_unless(str != NULL);
	fail_unless(strlen(str) >= len_min);
	fail_unless(strlen(str) <= len_max);
}
END_TEST

Suite *suite_version(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("version");

	tc = tcase_create("version");
	tcase_add_test(tc, test_version_numbers);
	tcase_add_test(tc, test_version_strings);
	suite_add_tcase(s, tc);

	return s;
}
