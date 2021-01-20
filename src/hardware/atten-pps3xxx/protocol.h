/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBSIGROK_HARDWARE_ATTEN_PPS3XXX_PROTOCOL_H
#define LIBSIGROK_HARDWARE_ATTEN_PPS3XXX_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "atten-pps3xxx"

/* Packets to/from the device. */
#define PACKET_SIZE 24

enum {
	PPS_3203T_3S,
	PPS_3203T_2S,
	PPS_3205T_3S,
	PPS_3205T_2S,
	PPS_3003S,
	PPS_3005S,
};

/* Maximum number of output channels handled by this driver. */
#define MAX_CHANNELS 3

#define CHANMODE_INDEPENDENT 1 << 0
#define CHANMODE_SERIES      1 << 1
#define CHANMODE_PARALLEL    1 << 2

struct channel_spec {
	/* Min, max, step. */
	gdouble voltage[3];
	gdouble current[3];
};

struct pps_model {
	int modelid;
	const char *name;
	int channel_modes;
	int num_channels;
	struct channel_spec channels[MAX_CHANNELS];
};

struct per_channel_config {
	/* Received from device. */
	gdouble output_voltage_last;
	gdouble output_current_last;
	gboolean output_enabled;
	/* Set by frontend. */
	gdouble output_voltage_max;
	gdouble output_current_max;
	gboolean output_enabled_set;
};

struct dev_context {
	const struct pps_model *model;

	gboolean acquisition_running;

	gboolean config_dirty;
	struct per_channel_config *config;
	/* Blocking write timeout for packet. */
	int delay_ms;
	/* Received from device. */
	int channel_mode;
	gboolean over_current_protection;
	/* Set by frontend. */
	int channel_mode_set;
	gboolean over_current_protection_set;

	uint8_t packet[PACKET_SIZE];
	int packet_size;

};

SR_PRIV int atten_pps3xxx_receive_data(int fd, int revents, void *cb_data);
SR_PRIV void send_packet(const struct sr_dev_inst *sdi, uint8_t *packet);
SR_PRIV void send_config(const struct sr_dev_inst *sdi);

#endif
