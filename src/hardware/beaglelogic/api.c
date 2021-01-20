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
#include "protocol.h"
#include "beaglelogic.h"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_NUM_LOGIC_CHANNELS,
};

static const uint32_t drvopts[] = {
	SR_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_MATCH | SR_CONF_LIST,
	SR_CONF_CAPTURE_RATIO | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_NUM_LOGIC_CHANNELS | SR_CONF_GET,
};

static const int32_t trigger_matches[] = {
	SR_TRIGGER_ZERO,
	SR_TRIGGER_ONE,
	SR_TRIGGER_RISING,
	SR_TRIGGER_FALLING,
	SR_TRIGGER_EDGE,
};

SR_PRIV const char *channel_names[] = {
	"P8_45", "P8_46", "P8_43", "P8_44", "P8_41", "P8_42", "P8_39",
	"P8_40", "P8_27", "P8_29", "P8_28", "P8_30", "P8_21", "P8_20",
};

/* Possible sample rates : 10 Hz to 100 MHz = (100 / x) MHz */
static const uint64_t samplerates[] = {
	SR_HZ(10),
	SR_MHZ(100),
	SR_HZ(1),
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	GSList *l;
	struct sr_config *src;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	const char *conn;
	gchar **params;
	int i, maxch;

	maxch = NUM_CHANNELS;
	conn = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		if (src->key == SR_CONF_NUM_LOGIC_CHANNELS)
			maxch = g_variant_get_int32(src->data);
		if (src->key == SR_CONF_CONN)
			conn = g_variant_get_string(src->data, NULL);
	}

	/* Probe for /dev/beaglelogic if not connecting via TCP */
	if (!conn) {
		params = NULL;
		if (!g_file_test(BEAGLELOGIC_DEV_NODE, G_FILE_TEST_EXISTS))
			return NULL;
	} else {
		params = g_strsplit(conn, "/", 0);
		if (!params || !params[1] || !params[2]) {
			sr_err("Invalid Parameters.");
			g_strfreev(params);
			return NULL;
		}
		if (g_ascii_strncasecmp(params[0], "tcp", 3)) {
			sr_err("Only TCP (tcp-raw) protocol is currently supported.");
			g_strfreev(params);
			return NULL;
		}
	}

	maxch = (maxch > 8) ? NUM_CHANNELS : 8;

	sdi = g_new0(struct sr_dev_inst, 1);
	sdi->status = SR_ST_INACTIVE;
	sdi->model = g_strdup("BeagleLogic");
	sdi->version = g_strdup("1.0");

	devc = g_malloc0(sizeof(struct dev_context));

	/* Default non-zero values (if any) */
	devc->fd = -1;
	devc->limit_samples = 10000000;
	devc->tcp_buffer = 0;

	if (!conn) {
		devc->beaglelogic = &beaglelogic_native_ops;
		sr_info("BeagleLogic device found at "BEAGLELOGIC_DEV_NODE);
	} else {
		devc->read_timeout = 1000 * 1000;
		devc->beaglelogic = &beaglelogic_tcp_ops;
		devc->address = g_strdup(params[1]);
		devc->port = g_strdup(params[2]);
		g_strfreev(params);

		if (devc->beaglelogic->open(devc) != SR_OK)
			goto err_free;
		if (beaglelogic_tcp_detect(devc) != SR_OK)
			goto err_free;
		if (devc->beaglelogic->close(devc) != SR_OK)
			goto err_free;
		sr_info("BeagleLogic device found at %s : %s",
			devc->address, devc->port);
	}

	/* Fill the channels */
	for (i = 0; i < maxch; i++)
		sr_channel_new(sdi, i, SR_CHANNEL_LOGIC, TRUE,
				channel_names[i]);

	sdi->priv = devc;

	return std_scan_complete(di, g_slist_append(NULL, sdi));

err_free:
	g_free(sdi->model);
	g_free(sdi->version);
	g_free(devc->address);
	g_free(devc->port);
	g_free(devc);
	g_free(sdi);

	return NULL;
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;

	/* Open BeagleLogic */
	if (devc->beaglelogic->open(devc))
		return SR_ERR;

	/* Set fd and local attributes */
	if (devc->beaglelogic == &beaglelogic_tcp_ops)
		devc->pollfd.fd = devc->socket;
	else
		devc->pollfd.fd = devc->fd;
	devc->pollfd.events = G_IO_IN;
	devc->pollfd.revents = 0;

	/* Get the default attributes */
	devc->beaglelogic->get_samplerate(devc);
	devc->beaglelogic->get_sampleunit(devc);
	devc->beaglelogic->get_buffersize(devc);
	devc->beaglelogic->get_bufunitsize(devc);

	/* Set the triggerflags to default for continuous capture unless we
	 * explicitly limit samples using SR_CONF_LIMIT_SAMPLES */
	devc->triggerflags = BL_TRIGGERFLAGS_CONTINUOUS;
	devc->beaglelogic->set_triggerflags(devc);

	/* Map the kernel capture FIFO for reads, saves 1 level of memcpy */
	if (devc->beaglelogic == &beaglelogic_native_ops) {
		if (devc->beaglelogic->mmap(devc) != SR_OK) {
			sr_err("Unable to map capture buffer");
			devc->beaglelogic->close(devc);
			return SR_ERR;
		}
	} else {
		devc->tcp_buffer = g_malloc(TCP_BUFFER_SIZE);
	}

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;

	/* Close the memory mapping and the file */
	if (devc->beaglelogic == &beaglelogic_native_ops)
		devc->beaglelogic->munmap(devc);
	devc->beaglelogic->close(devc);

	return SR_OK;
}

static void clear_helper(struct dev_context *devc)
{
	g_free(devc->tcp_buffer);
	g_free(devc->address);
	g_free(devc->port);
}

static int dev_clear(const struct sr_dev_driver *di)
{
	return std_dev_clear_with_callback(di, (std_dev_clear_callback)clear_helper);
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc = sdi->priv;

	(void)cg;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->cur_samplerate);
		break;
	case SR_CONF_CAPTURE_RATIO:
		*data = g_variant_new_uint64(devc->capture_ratio);
		break;
	case SR_CONF_NUM_LOGIC_CHANNELS:
		*data = g_variant_new_uint32(g_slist_length(sdi->channels));
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc = sdi->priv;
	uint64_t tmp_u64;

	(void)cg;

	switch (key) {
	case SR_CONF_SAMPLERATE:
		devc->cur_samplerate = g_variant_get_uint64(data);
		return devc->beaglelogic->set_samplerate(devc);
	case SR_CONF_LIMIT_SAMPLES:
		tmp_u64 = g_variant_get_uint64(data);
		devc->limit_samples = tmp_u64;
		devc->triggerflags = BL_TRIGGERFLAGS_ONESHOT;

		/* Check if we have sufficient buffer size */
		tmp_u64 *= SAMPLEUNIT_TO_BYTES(devc->sampleunit);
		if (tmp_u64 > devc->buffersize) {
			sr_warn("Insufficient buffer space has been allocated.");
			sr_warn("Please use \'echo <size in bytes> > "\
				BEAGLELOGIC_SYSFS_ATTR(memalloc) \
				"\' to increase the buffer size, this"\
				" capture is now truncated to %d Msamples",
				devc->buffersize /
				(SAMPLEUNIT_TO_BYTES(devc->sampleunit) * 1000000));
		}
		return devc->beaglelogic->set_triggerflags(devc);
	case SR_CONF_CAPTURE_RATIO:
		devc->capture_ratio = g_variant_get_uint64(data);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case SR_CONF_SAMPLERATE:
		*data = std_gvar_samplerates_steps(ARRAY_AND_SIZE(samplerates));
		break;
	case SR_CONF_TRIGGER_MATCH:
		*data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

/* get a sane timeout for poll() */
#define BUFUNIT_TIMEOUT_MS(devc)	(100 + ((devc->bufunitsize * 1000) / \
				(uint32_t)(devc->cur_samplerate)))

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;
	GSList *l;
	struct sr_trigger *trigger;
	struct sr_channel *channel;

	/* Clear capture state */
	devc->bytes_read = 0;
	devc->offset = 0;

	/* Configure channels */
	devc->sampleunit = BL_SAMPLEUNIT_8_BITS;

	for (l = sdi->channels; l; l = l->next) {
		channel = l->data;
		if (channel->index >= 8 && channel->enabled)
			devc->sampleunit = BL_SAMPLEUNIT_16_BITS;
	}
	devc->beaglelogic->set_sampleunit(devc);

	/* If continuous sampling, set the limit_samples to max possible value */
	if (devc->triggerflags == BL_TRIGGERFLAGS_CONTINUOUS)
		devc->limit_samples = UINT64_MAX;

	/* Configure triggers & send header packet */
	if ((trigger = sr_session_trigger_get(sdi->session))) {
		int pre_trigger_samples = 0;
		if (devc->limit_samples > 0)
			pre_trigger_samples = (devc->capture_ratio * devc->limit_samples) / 100;
		devc->stl = soft_trigger_logic_new(sdi, trigger, pre_trigger_samples);
		if (!devc->stl)
			return SR_ERR_MALLOC;
		devc->trigger_fired = FALSE;
	} else
		devc->trigger_fired = TRUE;
	std_session_send_df_header(sdi);

	/* Trigger and add poll on file */
	devc->beaglelogic->start(devc);
	if (devc->beaglelogic == &beaglelogic_native_ops)
		sr_session_source_add_pollfd(sdi->session, &devc->pollfd,
			BUFUNIT_TIMEOUT_MS(devc), beaglelogic_native_receive_data,
			(void *)sdi);
	else
		sr_session_source_add_pollfd(sdi->session, &devc->pollfd,
			BUFUNIT_TIMEOUT_MS(devc), beaglelogic_tcp_receive_data,
			(void *)sdi);

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;

	/* Execute a stop on BeagleLogic */
	devc->beaglelogic->stop(devc);

	/* Flush the cache */
	if (devc->beaglelogic == &beaglelogic_native_ops)
		lseek(devc->fd, 0, SEEK_SET);
	else
		beaglelogic_tcp_drain(devc);

	/* Remove session source and send EOT packet */
	sr_session_source_remove_pollfd(sdi->session, &devc->pollfd);
	std_session_send_df_end(sdi);

	return SR_OK;
}

static struct sr_dev_driver beaglelogic_driver_info = {
	.name = "beaglelogic",
	.longname = "BeagleLogic",
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
SR_REGISTER_DEV_DRIVER(beaglelogic_driver_info);
