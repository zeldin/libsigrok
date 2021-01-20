/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012-2013 Uwe Hermann <uwe@hermann-uwe.de>
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
#include <string.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"
#include "protocol.h"

/*
 * Driver for various UNI-T multimeters (and rebranded ones).
 *
 * Most UNI-T DMMs can be used with two (three) different PC interface cables:
 *  - The UT-D04 USB/HID cable, old version with Hoitek HE2325U chip.
 *  - The UT-D04 USB/HID cable, new version with WCH CH9325 chip.
 *  - The UT-D02 RS232 cable.
 *
 * This driver is meant to support all USB/HID cables, and various DMMs that
 * can be attached to a PC via these cables. Currently only the UT-D04 cable
 * (new version) is supported/tested.
 * The UT-D02 RS232 cable is handled by the 'serial-dmm' driver.
 *
 * The data for one DMM packet (e.g. 14 bytes if the respective DMM uses a
 * Fortune Semiconductor FS9922-DMM4 chip) is spread across multiple
 * 8-byte chunks.
 *
 * An 8-byte chunk looks like this:
 *  - Byte 0: 0xfz, where z is the number of actual data bytes in this chunk.
 *  - Bytes 1-7: z data bytes, the rest of the bytes should be ignored.
 *
 * Example:
 *  f0 00 00 00 00 00 00 00 (no data bytes)
 *  f2 55 77 00 00 00 00 00 (2 data bytes, 0x55 and 0x77)
 *  f1 d1 00 00 00 00 00 00 (1 data byte, 0xd1)
 */

static void decode_packet(struct sr_dev_inst *sdi, const uint8_t *buf)
{
	struct dev_context *devc;
	struct dmm_info *dmm;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;
	float floatval;
	void *info;
	int ret;

	devc = sdi->priv;
	dmm = (struct dmm_info *)sdi->driver;
	/* Note: digits/spec_digits will be overridden by the DMM parsers. */
	sr_analog_init(&analog, &encoding, &meaning, &spec, 0);
	info = g_malloc(dmm->info_size);

	/* Parse the protocol packet. */
	ret = dmm->packet_parse(buf, &floatval, &analog, info);
	if (ret != SR_OK) {
		sr_dbg("Invalid DMM packet, ignoring.");
		g_free(info);
		return;
	}

	/* If this DMM needs additional handling, call the resp. function. */
	if (dmm->dmm_details)
		dmm->dmm_details(&analog, info);

	g_free(info);

	/* Send a sample packet with one analog value. */
	analog.meaning->channels = sdi->channels;
	analog.num_samples = 1;
	analog.data = &floatval;
	packet.type = SR_DF_ANALOG;
	packet.payload = &analog;
	sr_session_send(sdi, &packet);

	sr_sw_limits_update_samples_read(&devc->limits, 1);
}

static int hid_chip_init(struct sr_dev_inst *sdi, uint16_t baudrate)
{
	int ret;
	uint8_t buf[5];
	struct sr_usb_dev_inst *usb;

	usb = sdi->conn;

	if (libusb_kernel_driver_active(usb->devhdl, 0) == 1) {
		ret = libusb_detach_kernel_driver(usb->devhdl, 0);
		if (ret < 0) {
			sr_err("Failed to detach kernel driver: %s.",
			       libusb_error_name(ret));
			return SR_ERR;
		}
	}

	if ((ret = libusb_claim_interface(usb->devhdl, 0)) < 0) {
		sr_err("Failed to claim interface 0: %s.",
		       libusb_error_name(ret));
		return SR_ERR;
	}

	/* Set data for the HID feature report (e.g. baudrate). */
	buf[0] = baudrate & 0xff;        /* Baudrate, LSB */
	buf[1] = (baudrate >> 8) & 0xff; /* Baudrate, MSB */
	buf[2] = 0x00;                   /* Unknown/unused (?) */
	buf[3] = 0x00;                   /* Unknown/unused (?) */
	buf[4] = 0x03;                   /* Unknown, always 0x03. */

	/* Send HID feature report to setup the baudrate/chip. */
	sr_dbg("Sending initial HID feature report.");
	sr_spew("HID init = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x (%d baud)",
		buf[0], buf[1], buf[2], buf[3], buf[4], baudrate);
	ret = libusb_control_transfer(
		usb->devhdl, /* libusb device handle */
		LIBUSB_REQUEST_TYPE_CLASS |
		LIBUSB_RECIPIENT_INTERFACE |
		LIBUSB_ENDPOINT_OUT,
		9, /* bRequest: HID set_report */
		0x300, /* wValue: HID feature, report number 0 */
		0, /* wIndex: interface 0 */
		(unsigned char *)&buf, /* payload buffer */
		5, /* wLength: 5 bytes payload */
		1000 /* timeout (ms) */);

	if (ret < 0) {
		sr_err("HID feature report error: %s.", libusb_error_name(ret));
		return SR_ERR;
	}

	if (ret != 5) {
		/* TODO: Handle better by also sending the remaining bytes. */
		sr_err("Short packet: sent %d/5 bytes.", ret);
		return SR_ERR;
	}

	return SR_OK;
}

static void log_8byte_chunk(const uint8_t *buf)
{
	sr_spew("8-byte chunk: %02x %02x %02x %02x %02x %02x %02x %02x "
		"(%d data bytes)", buf[0], buf[1], buf[2], buf[3],
		buf[4], buf[5], buf[6], buf[7], (buf[0] & 0x0f));
}

static void log_dmm_packet(const uint8_t *buf)
{
	GString *text;

	text = sr_hexdump_new(buf, 14);
	sr_dbg("DMM packet:   %s", text->str);
	sr_hexdump_free(text);
}

static int get_and_handle_data(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct dmm_info *dmm;
	uint8_t buf[CHUNK_SIZE], *pbuf;
	int i, ret, len, num_databytes_in_chunk;
	struct sr_usb_dev_inst *usb;

	devc = sdi->priv;
	dmm = (struct dmm_info *)sdi->driver;
	usb = sdi->conn;
	pbuf = devc->protocol_buf;

	/* On the first run, we need to init the HID chip. */
	if (devc->first_run) {
		if ((ret = hid_chip_init(sdi, dmm->baudrate)) != SR_OK) {
			sr_err("HID chip init failed: %d.", ret);
			return SR_ERR;
		}
		memset(pbuf, 0x00, DMM_BUFSIZE);
		devc->first_run = FALSE;
	}

	memset(&buf, 0x00, CHUNK_SIZE);

	/* Get data from EP2 using an interrupt transfer. */
	ret = libusb_interrupt_transfer(
		usb->devhdl, /* libusb device handle */
		LIBUSB_ENDPOINT_IN | 2, /* EP2, IN */
		(unsigned char *)&buf, /* receive buffer */
		CHUNK_SIZE, /* wLength */
		&len, /* actually received byte count */
		1000 /* timeout (ms) */);

	if (ret < 0) {
		sr_err("USB receive error: %s.", libusb_error_name(ret));
		return SR_ERR;
	}

	if (len != CHUNK_SIZE) {
		sr_err("Short packet: received %d/%d bytes.", len, CHUNK_SIZE);
		/* TODO: Print the bytes? */
		return SR_ERR;
	}

	log_8byte_chunk((const uint8_t *)&buf);

	/* If there are no data bytes just return (without error). */
	if (buf[0] == 0xf0)
		return SR_OK;

	devc->bufoffset = 0;

	/*
	 * Append the 1-7 data bytes of this chunk to pbuf.
	 *
	 * Special case:
	 * DMMs with Cyrustek ES51922 chip and UT71x DMMs need serial settings
	 * of 7o1. The WCH CH9325 UART to USB/HID chip used in (some
	 * versions of) the UNI-T UT-D04 cable however, will also send
	 * the parity bit to the host in the 8-byte data chunks. This bit
	 * is encoded in bit 7 of each of the 1-7 data bytes and must thus
	 * be removed in order for the actual protocol parser to work properly.
	 */
	num_databytes_in_chunk = buf[0] & 0x0f;
	for (i = 0; i < num_databytes_in_chunk; i++, devc->buflen++) {
		pbuf[devc->buflen] = buf[1 + i];
		if ((dmm->packet_parse == sr_es519xx_19200_14b_parse) ||
		    (dmm->packet_parse == sr_es519xx_19200_11b_parse) ||
		    (dmm->packet_parse == sr_es519xx_2400_11b_parse) ||
		    (dmm->packet_parse == sr_ut71x_parse)) {
			/* Mask off the parity bit. */
			pbuf[devc->buflen] &= ~(1 << 7);
		}
	}

	/* Now look for packets in that data. */
	while ((devc->buflen - devc->bufoffset) >= dmm->packet_size) {
		if (dmm->packet_valid(pbuf + devc->bufoffset)) {
			log_dmm_packet(pbuf + devc->bufoffset);
			decode_packet(sdi, pbuf + devc->bufoffset);
			devc->bufoffset += dmm->packet_size;
		} else {
			devc->bufoffset++;
		}
	}

	/* Move remaining bytes to beginning of buffer. */
	if (devc->bufoffset < devc->buflen)
		memmove(pbuf, pbuf + devc->bufoffset, devc->buflen - devc->bufoffset);
	devc->buflen -= devc->bufoffset;

	return SR_OK;
}

SR_PRIV int uni_t_dmm_receive_data(int fd, int revents, void *cb_data)
{
	int ret;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;

	(void)fd;
	(void)revents;

	sdi = cb_data;
	devc = sdi->priv;

	if ((ret = get_and_handle_data(sdi)) != SR_OK)
		return FALSE;

	/* Abort acquisition if we acquired enough samples. */
	if (sr_sw_limits_check(&devc->limits))
		sr_dev_acquisition_stop(sdi);

	return TRUE;
}
