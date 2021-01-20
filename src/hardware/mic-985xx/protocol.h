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

#ifndef LIBSIGROK_HARDWARE_MIC_985XX_PROTOCOL_H
#define LIBSIGROK_HARDWARE_MIC_985XX_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "mic-985xx"

enum {
	MIC_98581,
	MIC_98583,
};

struct mic_dev_info {
	const char *vendor;
	const char *device;
	const char *conn;
	uint32_t max_sample_points;
	gboolean has_temperature;
	gboolean has_humidity;
	uint8_t packet_size;
	gboolean (*packet_valid)(const uint8_t *);
	struct sr_dev_driver *di;
	int (*receive_data)(int, int, void *);
};

extern SR_PRIV const struct mic_dev_info mic_devs[];

#define SERIAL_BUFSIZE 256

struct dev_context {
	struct sr_sw_limits limits;

	uint8_t buf[SERIAL_BUFSIZE];
	int bufoffset;
	int buflen;
};

SR_PRIV gboolean packet_valid_temp(const uint8_t *buf);
SR_PRIV gboolean packet_valid_temp_hum(const uint8_t *buf);

SR_PRIV int receive_data_MIC_98581(int fd, int revents, void *cb_data);
SR_PRIV int receive_data_MIC_98583(int fd, int revents, void *cb_data);

SR_PRIV int mic_cmd_get_device_info(struct sr_serial_dev_inst *serial);

#endif
