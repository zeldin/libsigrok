/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Daniel Elstner <daniel.kitta@gmail.com>
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
#include <glib.h>
#include <libusb.h>
#include <stdlib.h>
#include <string.h>
#include <libsigrok/libsigrok.h>
#include <libsigrok-internal.h>
#include "protocol.h"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
};

static const uint32_t drvopts[] = {
	SR_CONF_LOGIC_ANALYZER,
};

static const int32_t trigger_matches[] = {
	SR_TRIGGER_ZERO,
	SR_TRIGGER_ONE,
	SR_TRIGGER_RISING,
	SR_TRIGGER_FALLING,
};

static const char *trigger_sources[] = {
	[TRIGGER_CHANNELS] = "CH",
	[TRIGGER_EXT_TRG] = "TRG",
};

static const char *signal_edges[] = {
	[EDGE_POSITIVE] = "r",
	[EDGE_NEGATIVE] = "f",
};

static struct sr_dev_inst *dev_inst_new(const struct model_info *model)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	int i;
	char name[8];

	devc = g_malloc0(sizeof(struct dev_context));
	devc->model = model;
	devc->active_fpga_config = FPGA_NOCONF;
	devc->cfg_rle = TRUE;
	devc->samplerate = model->samplerates[0];
	devc->channel_mask = (UINT64_C(1) << model->num_channels) - 1;

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("Sysclk");
	sdi->model = g_strdup(model->name);
	sdi->priv = devc;

	for (i = 0; i < model->num_channels; i++) {
		g_snprintf(name, sizeof(name), "CH%d", i + 1);
		sr_channel_new(sdi, i, SR_CHANNEL_LOGIC, TRUE, name);
	}

	return sdi;
}

/* Create a new device instance for a libusb device if it is a Sysclk LWLA
 * device and also matches the connection specification.
 */
static struct sr_dev_inst *dev_inst_new_matching(GSList *conn_matches,
						 libusb_device *dev)
{
	GSList *node;
	struct sr_usb_dev_inst *usb;
	const struct model_info *model;
	struct sr_dev_inst *sdi;
	struct libusb_device_descriptor des;
	int bus, address, ret;
	unsigned int vid, pid;

	bus = libusb_get_bus_number(dev);
	address = libusb_get_device_address(dev);

	for (node = conn_matches; node != NULL; node = node->next) {
		usb = node->data;
		if (usb && usb->bus == bus && usb->address == address)
			break; /* found */
	}
	if (conn_matches && !node)
		return NULL; /* no match */

	ret = libusb_get_device_descriptor(dev, &des);
	if (ret != 0) {
		sr_err("Failed to get USB device descriptor: %s.",
			libusb_error_name(ret));
		return NULL;
	}
	vid = des.idVendor;
	pid = des.idProduct;

	/* Create sigrok device instance. */
	if (vid == USB_VID_SYSCLK && pid == USB_PID_LWLA1016) {
		model = &lwla1016_info;
	} else if (vid == USB_VID_SYSCLK && pid == USB_PID_LWLA1034) {
		model = &lwla1034_info;
	} else {
		if (conn_matches)
			sr_warn("USB device %d.%d (%04x:%04x) is not a"
				" Sysclk LWLA.", bus, address, vid, pid);
		return NULL;
	}
	sdi = dev_inst_new(model);

	sdi->inst_type = SR_INST_USB;
	sdi->conn = sr_usb_dev_inst_new(bus, address, NULL);

	return sdi;
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	GSList *conn_devices, *devices, *node;
	struct drv_context *drvc;
	struct sr_dev_inst *sdi;
	struct sr_config *src;
	const char *conn;
	libusb_device **devlist;
	ssize_t num_devs, i;

	drvc = di->context;
	conn = NULL;
	conn_devices = NULL;
	devices = NULL;

	for (node = options; node != NULL; node = node->next) {
		src = node->data;
		if (src->key == SR_CONF_CONN) {
			conn = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (conn) {
		/* Find devices matching the connection specification. */
		conn_devices = sr_usb_find(drvc->sr_ctx->libusb_ctx, conn);
	}

	/* List all libusb devices. */
	num_devs = libusb_get_device_list(drvc->sr_ctx->libusb_ctx, &devlist);
	if (num_devs < 0) {
		sr_err("Failed to list USB devices: %s.",
			libusb_error_name(num_devs));
		g_slist_free_full(conn_devices,
			(GDestroyNotify)&sr_usb_dev_inst_free);
		return NULL;
	}

	/* Scan the USB device list for matching LWLA devices. */
	for (i = 0; i < num_devs; i++) {
		sdi = dev_inst_new_matching(conn_devices, devlist[i]);
		if (!sdi)
			continue; /* no match */

		/* Register device instance with driver. */
		devices = g_slist_append(devices, sdi);
	}

	libusb_free_device_list(devlist, 1);
	g_slist_free_full(conn_devices, (GDestroyNotify)&sr_usb_dev_inst_free);

	return std_scan_complete(di, devices);
}

/* Drain any pending data from the USB transfer buffers on the device.
 * This may be necessary e.g. after a crash or generally to clean up after
 * an abnormal condition.
 */
static int drain_usb(struct sr_usb_dev_inst *usb, unsigned int endpoint)
{
	int drained, xfer_len, ret;
	unsigned char buf[512];
	const unsigned int drain_timeout_ms = 10;

	drained = 0;
	do {
		xfer_len = 0;
		ret = libusb_bulk_transfer(usb->devhdl, endpoint,
					   buf, sizeof(buf), &xfer_len,
					   drain_timeout_ms);
		drained += xfer_len;
	} while (ret == LIBUSB_SUCCESS && xfer_len != 0);

	if (ret != LIBUSB_SUCCESS && ret != LIBUSB_ERROR_TIMEOUT) {
		sr_err("Failed to drain USB endpoint %u: %s.",
		       endpoint & (LIBUSB_ENDPOINT_IN - 1),
		       libusb_error_name(ret));
		return SR_ERR;
	}
	if (drained > 0) {
		sr_warn("Drained %d bytes from USB endpoint %u.",
			drained, endpoint & (LIBUSB_ENDPOINT_IN - 1));
	}

	return SR_OK;
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct drv_context *drvc;
	struct dev_context *devc;
	struct sr_usb_dev_inst *usb;
	int i, ret;

	drvc = sdi->driver->context;
	devc = sdi->priv;
	usb = sdi->conn;

	/* Try the whole shebang three times, fingers crossed. */
	for (i = 0; i < 3; i++) {
		ret = sr_usb_open(drvc->sr_ctx->libusb_ctx, usb);
		if (ret != SR_OK)
			return ret;

		ret = libusb_set_configuration(usb->devhdl, USB_CONFIG);
		if (ret != LIBUSB_SUCCESS) {
			sr_err("Failed to set USB configuration: %s.",
				libusb_error_name(ret));
			sr_usb_close(usb);
			return SR_ERR;
		}

		ret = libusb_claim_interface(usb->devhdl, USB_INTERFACE);
		if (ret != LIBUSB_SUCCESS) {
			sr_err("Failed to claim interface: %s.",
				libusb_error_name(ret));
			sr_usb_close(usb);
			return SR_ERR;
		}

		ret = drain_usb(usb, EP_REPLY);
		if (ret != SR_OK) {
			sr_usb_close(usb);
			return ret;
		}
		/* This delay appears to be necessary for reliable operation. */
		g_usleep(30 * 1000);

		sdi->status = SR_ST_ACTIVE;

		devc->active_fpga_config = FPGA_NOCONF;
		devc->short_transfer_quirk = FALSE;
		devc->state = STATE_IDLE;

		ret = (*devc->model->apply_fpga_config)(sdi);

		if (ret == SR_OK)
			ret = (*devc->model->device_init_check)(sdi);
		if (ret == SR_OK)
			break;

		/* Rinse and repeat. */
		sdi->status = SR_ST_INACTIVE;
		sr_usb_close(usb);
	}

	if (ret == SR_OK && devc->short_transfer_quirk)
		sr_warn("Short transfer quirk detected! "
			"Memory reads will be slow.");
	return ret;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_usb_dev_inst *usb;
	int ret;

	devc = sdi->priv;
	usb = sdi->conn;

	if (devc->acquisition) {
		sr_err("Cannot close device during acquisition!");
		/* Request stop, leak handle, and prepare for the worst. */
		devc->cancel_requested = TRUE;
		return SR_ERR_BUG;
	}

	/* Download of the shutdown bitstream, if any. */
	ret = (*devc->model->apply_fpga_config)(sdi);
	if (ret != SR_OK)
		sr_warn("Unable to shut down device.");

	if (usb->devhdl)
		libusb_release_interface(usb->devhdl, USB_INTERFACE);

	sr_usb_close(usb);

	return ret;
}

/* Check whether the device options contain a specific key.
 * Also match against get/set/list bits if specified.
 */
static int has_devopt(const struct model_info *model, uint32_t key)
{
	unsigned int i;

	for (i = 0; i < model->num_devopts; i++) {
		if ((model->devopts[i] & (SR_CONF_MASK | key)) == key)
			return TRUE;
	}

	return FALSE;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	unsigned int idx;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	if (!has_devopt(devc->model, key | SR_CONF_GET))
		return SR_ERR_NA;

	switch (key) {
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->samplerate);
		break;
	case SR_CONF_LIMIT_MSEC:
		*data = g_variant_new_uint64(devc->limit_msec);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	case SR_CONF_RLE:
		*data = g_variant_new_boolean(devc->cfg_rle);
		break;
	case SR_CONF_EXTERNAL_CLOCK:
		*data = g_variant_new_boolean(devc->cfg_clock_source
						== CLOCK_EXT_CLK);
		break;
	case SR_CONF_CLOCK_EDGE:
		idx = devc->cfg_clock_edge;
		if (idx >= ARRAY_SIZE(signal_edges))
			return SR_ERR_BUG;
		*data = g_variant_new_string(signal_edges[idx]);
		break;
	case SR_CONF_TRIGGER_SOURCE:
		idx = devc->cfg_trigger_source;
		if (idx >= ARRAY_SIZE(trigger_sources))
			return SR_ERR_BUG;
		*data = g_variant_new_string(trigger_sources[idx]);
		break;
	case SR_CONF_TRIGGER_SLOPE:
		idx = devc->cfg_trigger_slope;
		if (idx >= ARRAY_SIZE(signal_edges))
			return SR_ERR_BUG;
		*data = g_variant_new_string(signal_edges[idx]);
		break;
	default:
		/* Must not happen for a key listed in devopts. */
		return SR_ERR_BUG;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	uint64_t value;
	struct dev_context *devc;
	int idx;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	if (!has_devopt(devc->model, key | SR_CONF_SET))
		return SR_ERR_NA;

	switch (key) {
	case SR_CONF_SAMPLERATE:
		value = g_variant_get_uint64(data);
		if (value < devc->model->samplerates[devc->model->num_samplerates - 1]
				|| value > devc->model->samplerates[0])
			return SR_ERR_SAMPLERATE;
		devc->samplerate = value;
		break;
	case SR_CONF_LIMIT_MSEC:
		value = g_variant_get_uint64(data);
		if (value > MAX_LIMIT_MSEC)
			return SR_ERR_ARG;
		devc->limit_msec = value;
		break;
	case SR_CONF_LIMIT_SAMPLES:
		value = g_variant_get_uint64(data);
		if (value > MAX_LIMIT_SAMPLES)
			return SR_ERR_ARG;
		devc->limit_samples = value;
		break;
	case SR_CONF_RLE:
		devc->cfg_rle = g_variant_get_boolean(data);
		break;
	case SR_CONF_EXTERNAL_CLOCK:
		devc->cfg_clock_source = (g_variant_get_boolean(data))
			? CLOCK_EXT_CLK : CLOCK_INTERNAL;
		break;
	case SR_CONF_CLOCK_EDGE:
		if ((idx = std_str_idx(data, ARRAY_AND_SIZE(signal_edges))) < 0)
			return SR_ERR_ARG;
		devc->cfg_clock_edge = idx;
		break;
	case SR_CONF_TRIGGER_SOURCE:
		if ((idx = std_str_idx(data, ARRAY_AND_SIZE(trigger_sources))) < 0)
			return SR_ERR_ARG;
		devc->cfg_trigger_source = idx;
		break;
	case SR_CONF_TRIGGER_SLOPE:
		if ((idx = std_str_idx(data, ARRAY_AND_SIZE(signal_edges))) < 0)
			return SR_ERR_ARG;
		devc->cfg_trigger_slope = idx;
		break;
	default:
		/* Must not happen for a key listed in devopts. */
		return SR_ERR_BUG;
	}

	return SR_OK;
}

static int config_channel_set(const struct sr_dev_inst *sdi,
	struct sr_channel *ch, unsigned int changes)
{
	uint64_t channel_bit;
	struct dev_context *devc;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	if (ch->index < 0 || ch->index >= devc->model->num_channels) {
		sr_err("Channel index %d out of range.", ch->index);
		return SR_ERR_BUG;
	}

	if ((changes & SR_CHANNEL_SET_ENABLED) != 0) {
		channel_bit = UINT64_C(1) << ch->index;

		/* Enable or disable logic input for this channel. */
		if (ch->enabled)
			devc->channel_mask |= channel_bit;
		else
			devc->channel_mask &= ~channel_bit;
	}

	return SR_OK;
}

/* Derive trigger masks from the session's trigger configuration. */
static int prepare_trigger_masks(const struct sr_dev_inst *sdi)
{
	uint64_t trigger_mask, trigger_values, trigger_edge_mask;
	uint64_t level_bit, type_bit;
	struct dev_context *devc;
	struct sr_trigger *trigger;
	struct sr_trigger_stage *stage;
	struct sr_trigger_match *match;
	const GSList *node;
	int idx;
	enum sr_trigger_matches trg;

	devc = sdi->priv;

	trigger = sr_session_trigger_get(sdi->session);
	if (!trigger || !trigger->stages)
		return SR_OK;

	if (trigger->stages->next) {
		sr_err("This device only supports 1 trigger stage.");
		return SR_ERR_ARG;
	}
	stage = trigger->stages->data;

	trigger_mask = 0;
	trigger_values = 0;
	trigger_edge_mask = 0;

	for (node = stage->matches; node; node = node->next) {
		match = node->data;

		if (!match->channel->enabled)
			continue; /* Ignore disabled channel. */

		idx = match->channel->index;
		trg = match->match;

		if (idx < 0 || idx >= devc->model->num_channels) {
			sr_err("Channel index %d out of range.", idx);
			return SR_ERR_BUG; /* Should not happen. */
		}
		if (trg != SR_TRIGGER_ZERO
				&& trg != SR_TRIGGER_ONE
				&& trg != SR_TRIGGER_RISING
				&& trg != SR_TRIGGER_FALLING) {
			sr_err("Unsupported trigger match for CH%d.", idx + 1);
			return SR_ERR_ARG;
		}
		level_bit = (trg == SR_TRIGGER_ONE
			|| trg == SR_TRIGGER_RISING) ? 1 : 0;
		type_bit = (trg == SR_TRIGGER_RISING
			|| trg == SR_TRIGGER_FALLING) ? 1 : 0;

		trigger_mask |= UINT64_C(1) << idx;
		trigger_values |= level_bit << idx;
		trigger_edge_mask |= type_bit << idx;
	}
	devc->trigger_mask = trigger_mask;
	devc->trigger_values = trigger_values;
	devc->trigger_edge_mask = trigger_edge_mask;

	return SR_OK;
}

static int config_commit(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;

	devc = sdi->priv;

	if (devc->acquisition) {
		sr_err("Acquisition still in progress?");
		return SR_ERR;
	}

	ret = prepare_trigger_masks(sdi);
	if (ret != SR_OK)
		return ret;

	ret = (*devc->model->apply_fpga_config)(sdi);
	if (ret != SR_OK) {
		sr_err("Failed to apply FPGA configuration.");
		return ret;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	devc = (sdi) ? sdi->priv : NULL;

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		return std_opts_config_list(key, data, sdi, cg,
			ARRAY_AND_SIZE(scanopts), ARRAY_AND_SIZE(drvopts),
			(devc) ? devc->model->devopts : NULL,
			(devc) ? devc->model->num_devopts : 0);
	}

	if (!devc)
		return SR_ERR_ARG;
	if (!has_devopt(devc->model, key | SR_CONF_LIST))
		return SR_ERR_NA;

	switch (key) {
	case SR_CONF_SAMPLERATE:
		*data = std_gvar_samplerates(devc->model->samplerates, devc->model->num_samplerates);
		break;
	case SR_CONF_TRIGGER_MATCH:
		*data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
		break;
	case SR_CONF_TRIGGER_SOURCE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(trigger_sources));
		break;
	case SR_CONF_TRIGGER_SLOPE:
	case SR_CONF_CLOCK_EDGE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(signal_edges));
		break;
	default:
		/* Must not happen for a key listed in devopts. */
		return SR_ERR_BUG;
	}

	return SR_OK;
}

/* Set up the device hardware to begin capturing samples as soon as the
 * configured trigger conditions are met, or immediately if no triggers
 * are configured.
 */
static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	return lwla_start_acquisition(sdi);
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;

	if (devc->state != STATE_IDLE && !devc->cancel_requested) {
		devc->cancel_requested = TRUE;
		sr_dbg("Requesting cancel.");
	}

	return SR_OK;
}

static struct sr_dev_driver sysclk_lwla_driver_info = {
	.name = "sysclk-lwla",
	.longname = "Sysclk LWLA series",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_channel_set = config_channel_set,
	.config_commit = config_commit,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
SR_REGISTER_DEV_DRIVER(sysclk_lwla_driver_info);
