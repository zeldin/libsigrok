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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef _WIN32
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif
#include <errno.h>
#include "protocol.h"
#include "beaglelogic.h"

/* Define data packet size independent of packet (bufunitsize bytes) size
 * from the BeagleLogic kernel module */
#define PACKET_SIZE	(512 * 1024)

/* This implementation is zero copy from the libsigrok side.
 * It does not copy any data, just passes a pointer from the mmap'ed
 * kernel buffers appropriately. It is up to the application which is
 * using libsigrok to decide how to deal with the data.
 */
SR_PRIV int beaglelogic_native_receive_data(int fd, int revents, void *cb_data)
{
	const struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;

	int trigger_offset;
	int pre_trigger_samples;
	uint32_t packetsize;
	uint64_t bytes_remaining;

	if (!(sdi = cb_data) || !(devc = sdi->priv))
		return TRUE;

	packetsize = PACKET_SIZE;
	logic.unitsize = SAMPLEUNIT_TO_BYTES(devc->sampleunit);

	if (revents == G_IO_IN) {
		sr_info("In callback G_IO_IN, offset=%d", devc->offset);

		bytes_remaining = (devc->limit_samples * logic.unitsize) -
				devc->bytes_read;

		/* Configure data packet */
		packet.type = SR_DF_LOGIC;
		packet.payload = &logic;
		logic.data = devc->sample_buf + devc->offset;
		logic.length = MIN(packetsize, bytes_remaining);

		if (devc->trigger_fired) {
			/* Send the incoming transfer to the session bus. */
			sr_session_send(sdi, &packet);
		} else {
			/* Check for trigger */
			trigger_offset = soft_trigger_logic_check(devc->stl,
					logic.data, packetsize, &pre_trigger_samples);
			if (trigger_offset > -1) {
				devc->bytes_read += pre_trigger_samples * logic.unitsize;
				trigger_offset *= logic.unitsize;
				logic.length = MIN(packetsize - trigger_offset,
						bytes_remaining);
				logic.data += trigger_offset;

				sr_session_send(sdi, &packet);

				devc->trigger_fired = TRUE;
			}
		}

		/* Move the read pointer forward */
		lseek(fd, packetsize, SEEK_CUR);

		/* Update byte count and offset (roll over if needed) */
		devc->bytes_read += logic.length;
		if ((devc->offset += packetsize) >= devc->buffersize) {
			/* One shot capture, we abort and settle with less than
			 * the required number of samples */
			if (devc->triggerflags == BL_TRIGGERFLAGS_CONTINUOUS)
				devc->offset = 0;
			else
				packetsize = 0;
		}
	}

	/* EOF Received or we have reached the limit */
	if (devc->bytes_read >= devc->limit_samples * logic.unitsize ||
			packetsize == 0) {
		/* Send EOA Packet, stop polling */
		std_session_send_df_end(sdi);
		sr_session_source_remove_pollfd(sdi->session, &devc->pollfd);
	}

	return TRUE;
}

SR_PRIV int beaglelogic_tcp_receive_data(int fd, int revents, void *cb_data)
{
	const struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;

	int len;
	int pre_trigger_samples;
	int trigger_offset;
	uint32_t packetsize;
	uint64_t bytes_remaining;

	if (!(sdi = cb_data) || !(devc = sdi->priv))
		return TRUE;

	packetsize = TCP_BUFFER_SIZE;
	logic.unitsize = SAMPLEUNIT_TO_BYTES(devc->sampleunit);

	if (revents == G_IO_IN) {
		sr_info("In callback G_IO_IN");

		len = recv(fd, devc->tcp_buffer, TCP_BUFFER_SIZE, 0);
		if (len < 0) {
			sr_err("Receive error: %s", g_strerror(errno));
			return SR_ERR;
		}

		packetsize = len;

		bytes_remaining = (devc->limit_samples * logic.unitsize) -
				devc->bytes_read;

		/* Configure data packet */
		packet.type = SR_DF_LOGIC;
		packet.payload = &logic;
		logic.data = devc->tcp_buffer;
		logic.length = MIN(packetsize, bytes_remaining);

		if (devc->trigger_fired) {
			/* Send the incoming transfer to the session bus. */
			sr_session_send(sdi, &packet);
		} else {
			/* Check for trigger */
			trigger_offset = soft_trigger_logic_check(devc->stl,
					logic.data, packetsize, &pre_trigger_samples);
			if (trigger_offset > -1) {
				devc->bytes_read += pre_trigger_samples * logic.unitsize;
				trigger_offset *= logic.unitsize;
				logic.length = MIN(packetsize - trigger_offset,
						bytes_remaining);
				logic.data += trigger_offset;

				sr_session_send(sdi, &packet);

				devc->trigger_fired = TRUE;
			}
		}

		/* Update byte count and offset (roll over if needed) */
		devc->bytes_read += logic.length;
		if ((devc->offset += packetsize) >= devc->buffersize) {
			/* One shot capture, we abort and settle with less than
			 * the required number of samples */
			if (devc->triggerflags == BL_TRIGGERFLAGS_CONTINUOUS)
				devc->offset = 0;
			else
				packetsize = 0;
		}
	}

	/* EOF Received or we have reached the limit */
	if (devc->bytes_read >= devc->limit_samples * logic.unitsize ||
			packetsize == 0) {
		/* Send EOA Packet, stop polling */
		std_session_send_df_end(sdi);
		devc->beaglelogic->stop(devc);

		/* Drain the receive buffer */
		beaglelogic_tcp_drain(devc);

		sr_session_source_remove_pollfd(sdi->session, &devc->pollfd);
	}

	return TRUE;
}
