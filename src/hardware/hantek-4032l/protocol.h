/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2016 Andreas Zschunke <andreas.zschunke@gmx.net>
 * Copyright (C) 2017 Andrej Valek <andy@skyrain.eu>
 * Copyright (C) 2017 Uwe Hermann <uwe@hermann-uwe.de>
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

#ifndef LIBSIGROK_HARDWARE_HANTEK_4032L_PROTOCOL_H
#define LIBSIGROK_HARDWARE_HANTEK_4032L_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <string.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "hantek-4032l"

#define H4032L_USB_VENDOR 0x04b5
#define H4032L_USB_PRODUCT 0x4032

#define H4032L_DATA_BUFFER_SIZE (2 * 1024)
#define H4032L_DATA_TRANSFER_MAX_NUM 32

#define H4043L_NUM_SAMPLES_MIN (2 * 1024)
#define H4032L_NUM_SAMPLES_MAX (64 * 1024 * 1024)

#define H4032L_THR_VOLTAGE_MIN -6.0
#define H4032L_THR_VOLTAGE_MAX 6.0
#define H4032L_THR_VOLTAGE_STEP 0.1
/*
 * Array index of the default voltage threshold value (2.5V):
 * (|min| / step) + (default / step) = (|-6.0| / 0.1) + (2.5 / 0.1) = 85
 */
#define H4032L_THR_VOLTAGE_DEFAULT 85

#define H4032L_CMD_PKT_MAGIC 0x017f
#define H4032L_STATUS_PACKET_MAGIC 0x2B1A037F
#define H4032L_START_PACKET_MAGIC 0x2B1A027F
#define H4032L_END_PACKET_MAGIC 0x4D3C037F

enum h4032l_clock_edge_type {
	H4032L_CLOCK_EDGE_TYPE_RISE,
	H4032L_CLOCK_EDGE_TYPE_FALL,
	H4032L_CLOCK_EDGE_TYPE_BOTH
};

enum h4032l_ext_clock_source {
	H4032L_EXT_CLOCK_SOURCE_CHANNEL_A,
	H4032L_EXT_CLOCK_SOURCE_CHANNEL_B
};

enum h4032l_clock_edge_type_channel {
	H4032L_CLOCK_EDGE_TYPE_RISE_A = 0x24,
	H4032L_CLOCK_EDGE_TYPE_RISE_B,
	H4032L_CLOCK_EDGE_TYPE_BOTH_A,
	H4032L_CLOCK_EDGE_TYPE_BOTH_B,
	H4032L_CLOCK_EDGE_TYPE_FALL_A,
	H4032L_CLOCK_EDGE_TYPE_FALL_B
};

enum h4032l_trigger_edge_type {
	H4032L_TRIGGER_EDGE_TYPE_RISE,
	H4032L_TRIGGER_EDGE_TYPE_FALL,
	H4032L_TRIGGER_EDGE_TYPE_TOGGLE,
	H4032L_TRIGGER_EDGE_TYPE_DISABLED
};

enum h4032l_trigger_data_range_type {
	H4032L_TRIGGER_DATA_RANGE_TYPE_MAX,
	H4032L_TRIGGER_DATA_RANGE_TYPE_MIN_OR_MAX,
	H4032L_TRIGGER_DATA_RANGE_TYPE_OUT_OF_RANGE,
	H4032L_TRIGGER_DATA_RANGE_TYPE_WITHIN_RANGE
};

enum h4032l_trigger_time_range_type {
	H4032L_TRIGGER_TIME_RANGE_TYPE_MAX,
	H4032L_TRIGGER_TIME_RANGE_TYPE_MIN_OR_MAX,
	H4032L_TRIGGER_TIME_RANGE_TYPE_OUT_OF_RANGE,
	H4032L_TRIGGER_TIME_RANGE_TYPE_WITHIN_RANGE
};

enum h4032l_trigger_data_selection {
	H4032L_TRIGGER_DATA_SELECTION_NEXT,
	H4032L_TRIGGER_DATA_SELECTION_CURRENT,
	H4032L_TRIGGER_DATA_SELECTION_PREV
};

enum h4032l_status {
	H4032L_STATUS_IDLE,
	H4032L_STATUS_CMD_CONFIGURE,
	H4032L_STATUS_CMD_STATUS,
	H4032L_STATUS_RESPONSE_STATUS,
	H4032L_STATUS_RESPONSE_STATUS_RETRY,
	H4032L_STATUS_RESPONSE_STATUS_CONTINUE,
	H4032L_STATUS_CMD_GET,
	H4032L_STATUS_FIRST_TRANSFER,
	H4032L_STATUS_TRANSFER
};

#pragma pack(push,2)
struct h4032l_trigger {
	struct {
		uint32_t edge_signal:5;
		uint32_t edge_type:2;
		uint32_t :1;
		uint32_t data_range_type:2;
		uint32_t time_range_type:2;
		uint32_t data_range_enabled:1;
		uint32_t time_range_enabled:1;
		uint32_t :2;
		uint32_t data_sel:2;
		uint32_t combined_enabled:1;
	} flags;
	uint32_t data_range_min;
	uint32_t data_range_max;
	uint32_t time_range_min;
	uint32_t time_range_max;
	uint32_t data_range_mask;
	uint32_t combine_mask;
	uint32_t combine_data;
};

struct h4032l_cmd_pkt {
	uint16_t magic; /* 0x017f */
	uint8_t sample_rate;
	struct {
		uint8_t enable_trigger1:1;
		uint8_t enable_trigger2:1;
		uint8_t trigger_and_logic:1;
	} trig_flags;
	uint16_t pwm_a;
	uint16_t pwm_b;
	uint16_t reserved;
	uint32_t sample_size; /* Sample depth in bits per channel, 2k-64M, must be multiple of 512. */
	uint32_t pre_trigger_size; /* Pretrigger buffer depth in bits, must be < sample_size. */
	struct h4032l_trigger trigger[2];
	uint16_t cmd;
};
#pragma pack(pop)

struct dev_context {
	enum h4032l_status status;
	uint64_t sample_rate;
	unsigned int sent_samples;
	int submitted_transfers;
	uint32_t remaining_samples;
	gboolean acq_aborted;
	struct h4032l_cmd_pkt cmd_pkt;
	unsigned int num_transfers;
	struct libusb_transfer **transfers;
	uint8_t buf[512];
	uint64_t capture_ratio;
	uint32_t trigger_pos;
	gboolean external_clock;
	enum h4032l_ext_clock_source external_clock_source;
	enum h4032l_clock_edge_type clock_edge;
	double cur_threshold[2];
	uint32_t fpga_version;
};

SR_PRIV int h4032l_receive_data(int fd, int revents, void *cb_data);
SR_PRIV uint16_t h4032l_voltage2pwm(double voltage);
SR_PRIV void LIBUSB_CALL h4032l_usb_callback(struct libusb_transfer *transfer);
SR_PRIV void LIBUSB_CALL h4032l_data_transfer_callback(struct libusb_transfer *transfer);
SR_PRIV int h4032l_start_data_transfers(const struct sr_dev_inst *sdi);
SR_PRIV int h4032l_start(const struct sr_dev_inst *sdi);
SR_PRIV int h4032l_stop(struct sr_dev_inst *sdi);
SR_PRIV int h4032l_dev_open(struct sr_dev_inst *sdi);
SR_PRIV int h4032l_get_fpga_version(const struct sr_dev_inst *sdi);

#endif
