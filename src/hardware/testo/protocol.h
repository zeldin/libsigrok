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

#ifndef LIBSIGROK_HARDWARE_TESTO_PROTOCOL_H
#define LIBSIGROK_HARDWARE_TESTO_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "testo"

#define MAX_REPLY_SIZE       128
#define MAX_CHANNELS         16

/* FTDI commands */
#define FTDI_SET_MODEMCTRL   0x01
#define FTDI_SET_FLOWCTRL    0x02
#define FTDI_SET_BAUDRATE    0x03
#define FTDI_SET_PARAMS      0x04
/* FTDI command values */
#define FTDI_BAUDRATE_115200 0x001a
#define FTDI_PARAMS_8N1      0x0008
#define FTDI_FLOW_NONE       0x0008
#define FTDI_MODEM_ALLHIGH   0x0303
#define FTDI_INDEX           0x0000
/* FTDI USB stuff */
#define EP_IN                1 | LIBUSB_ENDPOINT_IN
#define EP_OUT               2 | LIBUSB_ENDPOINT_OUT

struct testo_model {
	const char *name;
	int request_size;
	const uint8_t *request;
};

struct dev_context {
	const struct testo_model *model;
	struct sr_sw_limits sw_limits;

	uint8_t channel_units[MAX_CHANNELS];
	int num_channels;

	struct libusb_transfer *out_transfer;
	uint8_t reply[MAX_REPLY_SIZE];
	int reply_size;
};

SR_PRIV int testo_set_serial_params(struct sr_usb_dev_inst *usb);
SR_PRIV int testo_probe_channels(struct sr_dev_inst *sdi);
SR_PRIV void LIBUSB_CALL receive_transfer(struct libusb_transfer *transfer);
SR_PRIV int testo_request_packet(const struct sr_dev_inst *sdi);
SR_PRIV gboolean testo_check_packet_prefix(uint8_t *buf, int len);
SR_PRIV uint16_t crc16_mcrf4xx(uint16_t crc, uint8_t *data, size_t len);
SR_PRIV void testo_receive_packet(const struct sr_dev_inst *sdi);

#endif
