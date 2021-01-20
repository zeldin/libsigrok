/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Alexandru Gagniuc <mr.nuke.me@gmail.com>
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

#ifndef LIBSIGROK_HARDWARE_SERIAL_DMM_PROTOCOL_H
#define LIBSIGROK_HARDWARE_SERIAL_DMM_PROTOCOL_H

#define LOG_PREFIX "serial-dmm"

struct dmm_info {
	/** libsigrok driver info struct. */
	struct sr_dev_driver di;
	/** Manufacturer/brand. */
	const char *vendor;
	/** Model. */
	const char *device;
	/** conn string. */
	const char *conn;
	/** serialcomm string. */
	const char *serialcomm;
	/** Packet size in bytes. */
	size_t packet_size;
	/**
	 * Request timeout [ms] before request is considered lost and a new
	 * one is sent. Used only if device needs polling.
	 */
	uint64_t req_timeout_ms;
	/**
	 * Delay between reception of packet and next request. Some DMMs
	 * need this. Used only if device needs polling.
	 */
	uint64_t req_delay_ms;
	/** Packet request function. */
	int (*packet_request)(struct sr_serial_dev_inst *);
	/** Number of channels / displays. */
	size_t channel_count;
	/** (Optional) printf formats for channel names. */
	const char **channel_formats;
	/** Packet validation function. */
	gboolean (*packet_valid)(const uint8_t *);
	/** Packet parsing function. */
	int (*packet_parse)(const uint8_t *, float *,
			    struct sr_datafeed_analog *, void *);
	/** */
	void (*dmm_details)(struct sr_datafeed_analog *, void *);
	/** Size of chipset info struct. */
	gsize info_size;
	/* Serial-dmm items "with state" and variable length packets. */
	void *dmm_state;
	void *(*dmm_state_init)(void);
	void (*dmm_state_free)(void *state);
	int (*after_open)(struct sr_serial_dev_inst *serial);
	int (*packet_valid_len)(void *state, const uint8_t *data, size_t dlen,
		size_t *pkt_len);
	int (*packet_parse_len)(void *state, const uint8_t *data, size_t dlen,
		double *val, struct sr_datafeed_analog *analog, void *info);
	int (*config_get)(void *state, uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);
	int (*config_set)(void *state, uint32_t key, GVariant *data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);
	int (*config_list)(void *state, uint32_t key, GVariant **data,
		const struct sr_dev_inst *sdi, const struct sr_channel_group *cg);
	/** Hook at acquisition start. Can re-route the receive routine. */
	int (*acquire_start)(void *state, const struct sr_dev_inst *sdi,
		sr_receive_data_callback *cb, void **cb_data);
};

#define DMM_BUFSIZE 256

struct dev_context {
	struct sr_sw_limits limits;

	uint8_t buf[DMM_BUFSIZE];
	size_t buflen;

	/**
	 * The timestamp [µs] to send the next request.
	 * Used only if device needs polling.
	 */
	uint64_t req_next_at;
};

SR_PRIV int req_packet(struct sr_dev_inst *sdi);
SR_PRIV int receive_data(int fd, int revents, void *cb_data);

#endif
