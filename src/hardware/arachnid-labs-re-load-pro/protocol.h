/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015-2016 Uwe Hermann <uwe@hermann-uwe.de>
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

#ifndef LIBSIGROK_HARDWARE_ARACHNID_LABS_RE_LOAD_PRO_PROTOCOL_H
#define LIBSIGROK_HARDWARE_ARACHNID_LABS_RE_LOAD_PRO_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "arachnid-labs-re-load-pro"

#define RELOADPRO_BUFSIZE 100

struct dev_context {
	struct sr_sw_limits limits;

	char buf[RELOADPRO_BUFSIZE];
	int buflen;

	float current_limit;
	float voltage;
	float current;
	gboolean otp_active;
	gboolean uvc_active;
	float uvc_threshold;

	gboolean acquisition_running;
	GMutex acquisition_mutex;

	GCond current_limit_cond;
	GCond voltage_cond;
	GCond uvc_threshold_cond;
};

SR_PRIV int reloadpro_set_current_limit(const struct sr_dev_inst *sdi,
		float current);
SR_PRIV int reloadpro_set_on_off(const struct sr_dev_inst *sdi, gboolean on);
SR_PRIV int reloadpro_set_under_voltage_threshold(const struct sr_dev_inst *sdi,
		float uvc_threshold);
SR_PRIV int reloadpro_get_current_limit(const struct sr_dev_inst *sdi,
		float *current_limit);
SR_PRIV int reloadpro_get_under_voltage_threshold(const struct sr_dev_inst *sdi,
		float *uvc_threshold);
SR_PRIV int reloadpro_get_voltage_current(const struct sr_dev_inst *sdi,
		float *voltage, float *current);
SR_PRIV int reloadpro_receive_data(int fd, int revents, void *cb_data);

#endif
