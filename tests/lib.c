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
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <check.h>
#include <libsigrok/libsigrok.h>
#include "lib.h"

struct sr_context *srtest_ctx;

void srtest_setup(void)
{
	int ret;

	ret = sr_init(&srtest_ctx);
	fail_unless(ret == SR_OK, "sr_init() failed: %d.", ret);
}

void srtest_teardown(void)
{
	int ret;

	ret = sr_exit(srtest_ctx);
	fail_unless(ret == SR_OK, "sr_exit() failed: %d.", ret);
}

/* Get a libsigrok driver by name. */
struct sr_dev_driver *srtest_driver_get(const char *drivername)
{
	struct sr_dev_driver **drivers, *driver = NULL;
	int i;

	drivers = sr_driver_list(srtest_ctx);
	fail_unless(drivers != NULL, "No drivers found.");

	for (i = 0; drivers[i]; i++) {
		if (strcmp(drivers[i]->name, drivername))
			continue;
		driver = drivers[i];
	}
	fail_unless(driver != NULL, "Driver '%s' not found.", drivername);

	return driver;
}

/* Initialize a libsigrok driver. */
void srtest_driver_init(struct sr_context *sr_ctx, struct sr_dev_driver *driver)
{
	int ret;

	ret = sr_driver_init(sr_ctx, driver);
	fail_unless(ret == SR_OK, "Failed to init '%s' driver: %d.",
		    driver->name, ret);
}

/* Initialize all libsigrok drivers. */
void srtest_driver_init_all(struct sr_context *sr_ctx)
{
	struct sr_dev_driver **drivers, *driver;
	int i, ret;

	drivers = sr_driver_list(srtest_ctx);
	fail_unless(drivers != NULL, "No drivers found.");

	for (i = 0; drivers[i]; i++) {
		driver = drivers[i];
		ret = sr_driver_init(sr_ctx, driver);
		fail_unless(ret == SR_OK, "Failed to init '%s' driver: %d.",
			    driver->name, ret);
	}
}

/* Set the samplerate for the respective driver to the specified value. */
void srtest_set_samplerate(struct sr_dev_driver *driver, uint64_t samplerate)
{
	int ret;
	struct sr_dev_inst *sdi;
	GVariant *gvar;

	sdi = g_slist_nth_data(driver->context, 0);

	gvar = g_variant_new_uint64(samplerate);
	ret = driver->config_set(SR_CONF_SAMPLERATE, gvar, sdi, NULL);
	g_variant_unref(gvar);

	fail_unless(ret == SR_OK, "%s: Failed to set SR_CONF_SAMPLERATE: %d.",
		    driver->name, ret);
}

/* Get the respective driver's current samplerate. */
uint64_t srtest_get_samplerate(struct sr_dev_driver *driver)
{
	int ret;
	uint64_t samplerate;
	struct sr_dev_inst *sdi;
	GVariant *gvar;

	sdi = g_slist_nth_data(driver->context, 0);

	ret = driver->config_get(SR_CONF_SAMPLERATE, &gvar, sdi, NULL);
	samplerate = g_variant_get_uint64(gvar);
	g_variant_unref(gvar);

	fail_unless(ret == SR_OK, "%s: Failed to get SR_CONF_SAMPLERATE: %d.",
		    driver->name, ret);

	return samplerate;
}

/* Check whether the respective driver can set/get the correct samplerate. */
void srtest_check_samplerate(struct sr_context *sr_ctx, const char *drivername,
			     uint64_t samplerate)
{
	struct sr_dev_driver *driver;
	uint64_t s;

	driver = srtest_driver_get(drivername);
	srtest_driver_init(sr_ctx, driver);;
	srtest_set_samplerate(driver, samplerate);
	s = srtest_get_samplerate(driver);
	fail_unless(s == samplerate, "%s: Incorrect samplerate: %" PRIu64 ".",
		    drivername, s);
}

GArray *srtest_get_enabled_logic_channels(const struct sr_dev_inst *sdi)
{
	struct sr_channel *ch;
	GArray *channels;
	GSList *l;

	channels = g_array_new(FALSE, FALSE, sizeof(int));
	for (l = sr_dev_inst_channels_get(sdi); l; l = l->next) {
		ch = l->data;
		if (ch->type != SR_CHANNEL_LOGIC)
			continue;
		if (ch->enabled != TRUE)
			continue;
		g_array_append_val(channels, ch->index);
	}

	return channels;
}
