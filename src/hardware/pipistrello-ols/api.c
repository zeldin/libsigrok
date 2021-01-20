/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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
#include "protocol.h"

static const uint32_t drvopts[] = {
	SR_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_MATCH | SR_CONF_LIST,
	SR_CONF_CAPTURE_RATIO | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_PATTERN_MODE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_EXTERNAL_CLOCK | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_SWAP | SR_CONF_SET,
	SR_CONF_RLE | SR_CONF_GET | SR_CONF_SET,
};

static const int32_t trigger_matches[] = {
	SR_TRIGGER_ZERO,
	SR_TRIGGER_ONE,
	SR_TRIGGER_RISING,
	SR_TRIGGER_FALLING,
};

#define STR_PATTERN_NONE     "None"
#define STR_PATTERN_EXTERNAL "External"
#define STR_PATTERN_INTERNAL "Internal"

/* Supported methods of test pattern outputs */
enum {
	/**
	 * Capture pins 31:16 (unbuffered wing) output a test pattern
	 * that can captured on pins 0:15.
	 */
	PATTERN_EXTERNAL,

	/** Route test pattern internally to capture buffer. */
	PATTERN_INTERNAL,
};

static const char *patterns[] = {
	STR_PATTERN_NONE,
	STR_PATTERN_EXTERNAL,
	STR_PATTERN_INTERNAL,
};

/* Channels are numbered 0-31 (on the PCB silkscreen). */
SR_PRIV const char *p_ols_channel_names[] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
	"13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23",
	"24", "25", "26", "27", "28", "29", "30", "31",
};

/* Default supported samplerates, can be overridden by device metadata. */
static const uint64_t samplerates[] = {
	SR_HZ(10),
	SR_MHZ(200),
	SR_HZ(1),
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	GSList *devices;
	int ret, i;
	char buf[70];
	int bytes_read;

	(void)options;

	devices = NULL;

	devc = g_malloc0(sizeof(struct dev_context));

	devc->max_samplebytes = devc->max_samplerate = devc->protocol_version = 0;

	devc->limit_samples = devc->capture_ratio = 0;
	devc->trigger_at = -1;
	devc->channel_mask = 0xffffffff;
	devc->flag_reg = 0;

	devc->ftdi_buf = g_malloc0(FTDI_BUF_SIZE);

	if (!(devc->ftdic = ftdi_new())) {
		sr_err("Failed to initialize libftdi.");
		goto err_free_ftdi_buf;;
	}

	if (p_ols_open(devc) != SR_OK)
		goto err_free_ftdic;

	/* The discovery procedure is like this: first send the Reset
	 * command (0x00) 5 times, since the device could be anywhere
	 * in a 5-byte command. Then send the ID command (0x02).
	 * If the device responds with 4 bytes ("OLS1" or "SLA1"), we
	 * have a match.
	 */

	ret = SR_OK;
	for (i = 0; i < 5; i++) {
		if ((ret = write_shortcommand(devc, CMD_RESET)) != SR_OK) {
			break;
		}
	}
	if (ret != SR_OK) {
		sr_err("Could not reset device. Quitting.");
		goto err_close_ftdic;
	}
	write_shortcommand(devc, CMD_ID);

	/* Read the response data. */
	bytes_read = ftdi_read_data(devc->ftdic, (uint8_t *)buf, 4);
	if (bytes_read < 0) {
		sr_err("Failed to read FTDI data (%d): %s.",
		       bytes_read, ftdi_get_error_string(devc->ftdic));
		goto err_close_ftdic;
	}
	if (bytes_read == 0) {
		goto err_close_ftdic;
	}

	if (strncmp(buf, "1SLO", 4) && strncmp(buf, "1ALS", 4))
		goto err_close_ftdic;

	/* Definitely using the OLS protocol, check if it supports
	 * the metadata command.
	 */
	write_shortcommand(devc, CMD_METADATA);

        /* Read the metadata. */
	bytes_read = ftdi_read_data(devc->ftdic, (uint8_t *)buf, 64);
	if (bytes_read < 0) {
		sr_err("Failed to read FTDI data (%d): %s.",
		       bytes_read, ftdi_get_error_string(devc->ftdic));
		goto err_close_ftdic;
	}
	if (bytes_read == 0) {
		goto err_close_ftdic;
	}

	p_ols_close(devc);

	/* Parse the metadata. */
	sdi = p_ols_get_metadata((uint8_t *)buf, bytes_read, devc);

	/* Configure samplerate and divider. */
	if (p_ols_set_samplerate(sdi, DEFAULT_SAMPLERATE) != SR_OK)
		sr_dbg("Failed to set default samplerate (%"PRIu64").",
				DEFAULT_SAMPLERATE);

	devices = g_slist_append(devices, sdi);

	return std_scan_complete(di, devices);

err_close_ftdic:
	p_ols_close(devc);
err_free_ftdic:
	ftdi_free(devc->ftdic);
err_free_ftdi_buf:
	g_free(devc->ftdi_buf);
	g_free(devc);

	return NULL;
}

static void clear_helper(struct dev_context *devc)
{
	ftdi_free(devc->ftdic);
	g_free(devc->ftdi_buf);
}

static int dev_clear(const struct sr_dev_driver *di)
{
	return std_dev_clear_with_callback(di, (std_dev_clear_callback)clear_helper);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->cur_samplerate);
		break;
	case SR_CONF_CAPTURE_RATIO:
		*data = g_variant_new_uint64(devc->capture_ratio);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	case SR_CONF_PATTERN_MODE:
		if (devc->flag_reg & FLAG_EXTERNAL_TEST_MODE)
			*data = g_variant_new_string(STR_PATTERN_EXTERNAL);
		else if (devc->flag_reg & FLAG_INTERNAL_TEST_MODE)
			*data = g_variant_new_string(STR_PATTERN_INTERNAL);
		else
			*data = g_variant_new_string(STR_PATTERN_NONE);
		break;
	case SR_CONF_RLE:
		*data = g_variant_new_boolean(devc->flag_reg & FLAG_RLE ? TRUE : FALSE);
		break;
	case SR_CONF_EXTERNAL_CLOCK:
		*data = g_variant_new_boolean(devc->flag_reg & FLAG_CLOCK_EXTERNAL ? TRUE : FALSE);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	uint16_t flag;
	uint64_t tmp_u64;
	const char *stropt;

	(void)cg;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_SAMPLERATE:
		tmp_u64 = g_variant_get_uint64(data);
		if (tmp_u64 < samplerates[0] || tmp_u64 > samplerates[1])
			return SR_ERR_SAMPLERATE;
		return p_ols_set_samplerate(sdi, g_variant_get_uint64(data));
	case SR_CONF_LIMIT_SAMPLES:
		tmp_u64 = g_variant_get_uint64(data);
		if (tmp_u64 < MIN_NUM_SAMPLES)
			return SR_ERR;
		devc->limit_samples = tmp_u64;
		break;
	case SR_CONF_CAPTURE_RATIO:
		devc->capture_ratio = g_variant_get_uint64(data);
		break;
	case SR_CONF_EXTERNAL_CLOCK:
		if (g_variant_get_boolean(data)) {
			sr_info("Enabling external clock.");
			devc->flag_reg |= FLAG_CLOCK_EXTERNAL;
		} else {
			sr_info("Disabled external clock.");
			devc->flag_reg &= ~FLAG_CLOCK_EXTERNAL;
		}
		break;
	case SR_CONF_PATTERN_MODE:
		stropt = g_variant_get_string(data, NULL);
		if (!strcmp(stropt, STR_PATTERN_NONE)) {
			sr_info("Disabling test modes.");
			flag = 0x0000;
		} else if (!strcmp(stropt, STR_PATTERN_INTERNAL)) {
			sr_info("Enabling internal test mode.");
			flag = FLAG_INTERNAL_TEST_MODE;
		} else if (!strcmp(stropt, STR_PATTERN_EXTERNAL)) {
			sr_info("Enabling external test mode.");
			flag = FLAG_EXTERNAL_TEST_MODE;
		} else {
			return SR_ERR;
		}
		devc->flag_reg &= ~FLAG_INTERNAL_TEST_MODE;
		devc->flag_reg &= ~FLAG_EXTERNAL_TEST_MODE;
		devc->flag_reg |= flag;
		break;
	case SR_CONF_SWAP:
		if (g_variant_get_boolean(data)) {
			sr_info("Enabling channel swapping.");
			devc->flag_reg |= FLAG_SWAP_CHANNELS;
		} else {
			sr_info("Disabling channel swapping.");
			devc->flag_reg &= ~FLAG_SWAP_CHANNELS;
		}
		break;
	case SR_CONF_RLE:
		if (g_variant_get_boolean(data)) {
			sr_info("Enabling RLE.");
			devc->flag_reg |= FLAG_RLE;
		} else {
			sr_info("Disabling RLE.");
			devc->flag_reg &= ~FLAG_RLE;
		}
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	int num_pols_changrp, i;

	switch (key) {
	case SR_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, NO_OPTS, drvopts, devopts);
	case SR_CONF_SAMPLERATE:
		*data = std_gvar_samplerates_steps(ARRAY_AND_SIZE(samplerates));
		break;
	case SR_CONF_TRIGGER_MATCH:
		*data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
		break;
	case SR_CONF_PATTERN_MODE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(patterns));
		break;
	case SR_CONF_LIMIT_SAMPLES:
		if (!sdi)
			return SR_ERR_ARG;
		devc = sdi->priv;
		if (devc->flag_reg & FLAG_RLE)
			return SR_ERR_NA;
		if (devc->max_samplebytes == 0)
			/* Device didn't specify sample memory size in metadata. */
			return SR_ERR_NA;
		/*
		 * Channel groups are turned off if no channels in that group are
		 * enabled, making more room for samples for the enabled group.
		*/
		pols_channel_mask(sdi);
		num_pols_changrp = 0;
		for (i = 0; i < 4; i++) {
			if (devc->channel_mask & (0xff << (i * 8)))
				num_pols_changrp++;
		}
		/* 3 channel groups takes as many bytes as 4 channel groups */
		if (num_pols_changrp == 3)
			num_pols_changrp = 4;

		*data = std_gvar_tuple_u64(MIN_NUM_SAMPLES,
			(num_pols_changrp) ? devc->max_samplebytes / num_pols_changrp : MIN_NUM_SAMPLES);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;

	return p_ols_open(devc);
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;

	return p_ols_close(devc);
}

static int set_trigger(const struct sr_dev_inst *sdi, int stage)
{
	struct dev_context *devc;
	uint8_t cmd, arg[4];

	devc = sdi->priv;

	cmd = CMD_SET_TRIGGER_MASK + stage * 4;
	arg[0] = devc->trigger_mask[stage] & 0xff;
	arg[1] = (devc->trigger_mask[stage] >> 8) & 0xff;
	arg[2] = (devc->trigger_mask[stage] >> 16) & 0xff;
	arg[3] = (devc->trigger_mask[stage] >> 24) & 0xff;
	if (write_longcommand(devc, cmd, arg) != SR_OK)
		return SR_ERR;

	cmd = CMD_SET_TRIGGER_VALUE + stage * 4;
	arg[0] = devc->trigger_value[stage] & 0xff;
	arg[1] = (devc->trigger_value[stage] >> 8) & 0xff;
	arg[2] = (devc->trigger_value[stage] >> 16) & 0xff;
	arg[3] = (devc->trigger_value[stage] >> 24) & 0xff;
	if (write_longcommand(devc, cmd, arg) != SR_OK)
		return SR_ERR;

	cmd = CMD_SET_TRIGGER_CONFIG + stage * 4;
	arg[0] = arg[1] = arg[3] = 0x00;
	arg[2] = stage;
	if (stage == devc->num_stages)
		/* Last stage, fire when this one matches. */
		arg[3] |= TRIGGER_START;
	if (write_longcommand(devc, cmd, arg) != SR_OK)
		return SR_ERR;

	cmd = CMD_SET_TRIGGER_EDGE + stage * 4;
	arg[0] = devc->trigger_edge[stage] & 0xff;
	arg[1] = (devc->trigger_edge[stage] >> 8) & 0xff;
	arg[2] = (devc->trigger_edge[stage] >> 16) & 0xff;
	arg[3] = (devc->trigger_edge[stage] >> 24) & 0xff;
	if (write_longcommand(devc, cmd, arg) != SR_OK)
		return SR_ERR;

	return SR_OK;
}

static int disable_trigger(const struct sr_dev_inst *sdi, int stage)
{
	struct dev_context *devc;
	uint8_t cmd, arg[4];

	devc = sdi->priv;

	cmd = CMD_SET_TRIGGER_MASK + stage * 4;
	arg[0] = arg[1] = arg[2] = arg[3] = 0x00;
	if (write_longcommand(devc, cmd, arg) != SR_OK)
		return SR_ERR;

	cmd = CMD_SET_TRIGGER_VALUE + stage * 4;
	if (write_longcommand(devc, cmd, arg) != SR_OK)
		return SR_ERR;

	cmd = CMD_SET_TRIGGER_CONFIG + stage * 4;
	arg[2] = 0x03;
	if (write_longcommand(devc, cmd, arg) != SR_OK)
		return SR_ERR;

	cmd = CMD_SET_TRIGGER_EDGE + stage * 4;
	arg[2] = 0x00;
	if (write_longcommand(devc, cmd, arg) != SR_OK)
		return SR_ERR;

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	uint32_t samplecount, readcount, delaycount;
	uint8_t pols_changrp_mask, arg[4];
	uint16_t flag_tmp;
	int num_pols_changrp, samplespercount;
	int ret, i;

	devc = sdi->priv;

	pols_channel_mask(sdi);

	/*
	 * Enable/disable channel groups in the flag register according to the
	 * channel mask. Calculate this here, because num_pols_changrp is
	 * needed to limit readcount.
	 */
	pols_changrp_mask = 0;
	num_pols_changrp = 0;
	for (i = 0; i < 4; i++) {
		if (devc->channel_mask & (0xff << (i * 8))) {
			pols_changrp_mask |= (1 << i);
			num_pols_changrp++;
		}
	}
	/* 3 channel groups takes as many bytes as 4 channel groups */
	if (num_pols_changrp == 3)
		num_pols_changrp = 4;
	/* maximum number of samples (or RLE counts) the buffer memory can hold */
	devc->max_samples = devc->max_samplebytes / num_pols_changrp;

	/*
	 * Limit readcount to prevent reading past the end of the hardware
	 * buffer.
	 */
	sr_dbg("max_samples = %d", devc->max_samples);
	sr_dbg("limit_samples = %" PRIu64, devc->limit_samples);
	samplecount = MIN(devc->max_samples, devc->limit_samples);
	sr_dbg("Samplecount = %d", samplecount);

	/* In demux mode the OLS is processing two samples per clock */
	if (devc->flag_reg & FLAG_DEMUX) {
		samplespercount = 8;
	}
	else {
		samplespercount = 4;
	}

	readcount = samplecount / samplespercount;

	/* Rather read too many samples than too few. */
	if (samplecount % samplespercount != 0)
		readcount++;

	/* Basic triggers. */
	if (pols_convert_trigger(sdi) != SR_OK) {
		sr_err("Failed to configure channels.");
		return SR_ERR;
	}

	if (devc->num_stages > 0) {
		delaycount = readcount * (1 - devc->capture_ratio / 100.0);
		devc->trigger_at = (readcount - delaycount) * samplespercount - devc->num_stages;
		for (i = 0; i < NUM_TRIGGER_STAGES; i++) {
			if (i <= devc->num_stages) {
				sr_dbg("Setting p-ols stage %d trigger.", i);
				if ((ret = set_trigger(sdi, i)) != SR_OK)
					return ret;
			}
			else {
				sr_dbg("Disabling p-ols stage %d trigger.", i);
				if ((ret = disable_trigger(sdi, i)) != SR_OK)
					return ret;
			}
		}
	} else {
		/* No triggers configured, force trigger on first stage. */
		sr_dbg("Forcing trigger at stage 0.");
		if ((ret = set_trigger(sdi, 0)) != SR_OK)
			return ret;
		delaycount = readcount;
	}

	/* Samplerate. */
	sr_dbg("Setting samplerate to %" PRIu64 "Hz (divider %u)",
			devc->cur_samplerate, devc->cur_samplerate_divider);
	arg[0] = devc->cur_samplerate_divider & 0xff;
	arg[1] = (devc->cur_samplerate_divider & 0xff00) >> 8;
	arg[2] = (devc->cur_samplerate_divider & 0xff0000) >> 16;
	arg[3] = 0x00;
	if (write_longcommand(devc, CMD_SET_DIVIDER, arg) != SR_OK)
		return SR_ERR;

	/* Send extended sample limit and pre/post-trigger capture ratio. */
	arg[0] = ((readcount - 1) & 0xff);
	arg[1] = ((readcount - 1) & 0xff00) >> 8;
	arg[2] = ((readcount - 1) & 0xff0000) >> 16;
	arg[3] = ((readcount - 1) & 0xff000000) >> 24;
	if (write_longcommand(devc, CMD_CAPTURE_READCOUNT, arg) != SR_OK)
		return SR_ERR;
	arg[0] = ((delaycount - 1) & 0xff);
	arg[1] = ((delaycount - 1) & 0xff00) >> 8;
	arg[2] = ((delaycount - 1) & 0xff0000) >> 16;
	arg[3] = ((delaycount - 1) & 0xff000000) >> 24;
	if (write_longcommand(devc, CMD_CAPTURE_DELAYCOUNT, arg) != SR_OK)
		return SR_ERR;

	/* Flag register. */
	sr_dbg("Setting intpat %s, extpat %s, RLE %s, noise_filter %s, demux %s",
			devc->flag_reg & FLAG_INTERNAL_TEST_MODE ? "on": "off",
			devc->flag_reg & FLAG_EXTERNAL_TEST_MODE ? "on": "off",
			devc->flag_reg & FLAG_RLE ? "on" : "off",
			devc->flag_reg & FLAG_FILTER ? "on": "off",
			devc->flag_reg & FLAG_DEMUX ? "on" : "off");

	/*
	* Enable/disable OLS channel groups in the flag register according
	* to the channel mask. 1 means "disable channel".
	*/
	devc->flag_reg &= ~0x3c;
	devc->flag_reg |= ~(pols_changrp_mask << 2) & 0x3c;
	sr_dbg("flag_reg = %x", devc->flag_reg);

	/*
	* In demux mode the OLS is processing two 8-bit or 16-bit samples
	* in parallel and for this to work the lower two bits of the four
	* "channel_disable" bits must be replicated to the upper two bits.
	*/
	flag_tmp = devc->flag_reg;
	if (devc->flag_reg & FLAG_DEMUX) {
		flag_tmp &= ~0x30;
		flag_tmp |= ~(pols_changrp_mask << 4) & 0x30;
	}
	arg[0] = flag_tmp & 0xff;
	arg[1] = flag_tmp >> 8;
	arg[2] = arg[3] = 0x00;
	if (write_longcommand(devc, CMD_SET_FLAGS, arg) != SR_OK)
		return SR_ERR;

	/* Start acquisition on the device. */
	if (write_shortcommand(devc, CMD_RUN) != SR_OK)
		return SR_ERR;

	/* Reset all operational states. */
	devc->rle_count = devc->num_transfers = 0;
	devc->num_samples = devc->num_bytes = 0;
	devc->cnt_bytes = devc->cnt_samples = devc->cnt_samples_rle = 0;
	memset(devc->sample, 0, 4);

	std_session_send_df_header(sdi);

	/* Hook up a dummy handler to receive data from the device. */
	sr_session_source_add(sdi->session, -1, 0, 10, p_ols_receive_data,
				(struct sr_dev_inst *)sdi);

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;

	write_shortcommand(devc, CMD_RESET);
	write_shortcommand(devc, CMD_RESET);
	write_shortcommand(devc, CMD_RESET);
	write_shortcommand(devc, CMD_RESET);
	write_shortcommand(devc, CMD_RESET);

	sr_session_source_remove(sdi->session, -1);

	std_session_send_df_end(sdi);

	return SR_OK;
}

static struct sr_dev_driver p_ols_driver_info = {
	.name = "p-ols",
	.longname = "Pipistrello OLS",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
SR_REGISTER_DEV_DRIVER(p_ols_driver_info);
