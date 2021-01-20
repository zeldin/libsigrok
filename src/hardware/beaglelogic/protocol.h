/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014-2017 Kumar Abhishek <abhishek@theembeddedkitchen.net>
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

#ifndef LIBSIGROK_HARDWARE_BEAGLELOGIC_PROTOCOL_H
#define LIBSIGROK_HARDWARE_BEAGLELOGIC_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "beaglelogic"

/* Maximum possible input channels */
#define NUM_CHANNELS            14

#define SAMPLEUNIT_TO_BYTES(x)	((x) == 1 ? 1 : 2)

#define TCP_BUFFER_SIZE         (128 * 1024)

/** Private, per-device-instance driver context. */
struct dev_context {
	int max_channels;
	uint32_t fw_ver;

	/* Operations */
	const struct beaglelogic_ops *beaglelogic;

	/* TCP Settings */
	char *address;
	char *port;
	int socket;
	unsigned int read_timeout;
	unsigned char *tcp_buffer;

	/* Acquisition settings: see beaglelogic.h */
	uint64_t cur_samplerate;
	uint64_t limit_samples;
	uint32_t sampleunit;
	uint32_t triggerflags;
	uint64_t capture_ratio;

	/* Buffers: size of each buffer block and the total buffer area */
	uint32_t bufunitsize;
	uint32_t buffersize;

	int fd;
	GPollFD pollfd;
	int last_error;

	uint64_t bytes_read;
	uint64_t sent_samples;
	uint32_t offset;
	uint8_t *sample_buf;	/* mmap'd kernel buffer here */

	/* Trigger logic */
	struct soft_trigger_logic *stl;
	gboolean trigger_fired;
};

SR_PRIV int beaglelogic_native_receive_data(int fd, int revents, void *cb_data);
SR_PRIV int beaglelogic_tcp_receive_data(int fd, int revents, void *cb_data);

#endif
