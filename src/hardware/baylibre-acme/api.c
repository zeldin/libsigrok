/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015 Bartosz Golaszewski <bgolaszewski@baylibre.com>
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
#include <time.h>
#include <sys/timerfd.h>

static const uint32_t drvopts[] = {
	SR_CONF_THERMOMETER,
	SR_CONF_POWERMETER,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

/*
 * Currently there are two channel-group/probe options for ACME:
 *   - SR_CONF_PROBE_FACTOR - allows to modify current shunt resistance
 *     calibration
 *   - SR_CONF_POWER_OFF - allows to remotely cut-off/restore power to
 *     measured devices
 *
 * They are not static - we have to check each probe's capabilities in
 * config_list().
 */
#define MAX_DEVOPTS_CG		2
#define HAS_PROBE_FACTOR	(SR_CONF_PROBE_FACTOR | SR_CONF_GET | SR_CONF_SET)
#define HAS_POWER_OFF		(SR_CONF_POWER_OFF | SR_CONF_GET | SR_CONF_SET)

#define MAX_SAMPLE_RATE 500 /* In Hz */

static const uint64_t samplerates[] = {
	SR_HZ(1),
	SR_HZ(MAX_SAMPLE_RATE),
	SR_HZ(1),
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	gboolean status;
	int i;

	(void)options;

	devc = g_malloc0(sizeof(struct dev_context));
	devc->samplerate = SR_HZ(10);

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("BayLibre");
	sdi->model = g_strdup("ACME");
	sdi->priv = devc;

	status = bl_acme_is_sane();
	if (!status)
		goto err_out;

	/*
	 * Iterate over all ACME connectors and check if any probes
	 * are present.
	 */
	for (i = 0; i < MAX_PROBES; i++) {
		/*
		 * First check if there's an energy probe on this connector. If
		 * not, and we're already at the fifth probe - see if we can
		 * detect a temperature probe.
		 */
		status = bl_acme_detect_probe(bl_acme_get_enrg_addr(i),
					      PROBE_NUM(i), ENRG_PROBE_NAME);
		if (status) {
			/* Energy probe detected. */
			status = bl_acme_register_probe(sdi, PROBE_ENRG,
					bl_acme_get_enrg_addr(i), PROBE_NUM(i));
			if (!status) {
				sr_err("Error registering power probe %d",
				       PROBE_NUM(i));
				continue;
			}
		} else if (i >= TEMP_PRB_START_INDEX) {
			status = bl_acme_detect_probe(bl_acme_get_temp_addr(i),
					      PROBE_NUM(i), TEMP_PROBE_NAME);
			if (status) {
				/* Temperature probe detected. */
				status = bl_acme_register_probe(sdi,PROBE_TEMP,
					bl_acme_get_temp_addr(i), PROBE_NUM(i));
				if (!status) {
					sr_err("Error registering temp "
					       "probe %d", PROBE_NUM(i));
					continue;
				}
			}
		}
	}

	/*
	 * Let's assume there's no ACME device present if no probe
	 * has been registered.
	 */
	if (!sdi->channel_groups)
		goto err_out;

	return std_scan_complete(di, g_slist_append(NULL, sdi));

err_out:
	g_free(devc);
	sr_dev_inst_free(sdi);

	return NULL;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	int ret;
	uint64_t shunt;
	gboolean power_off;

	devc = sdi->priv;

	ret = SR_OK;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_get(&devc->limits, key, data);
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->samplerate);
		break;
	case SR_CONF_PROBE_FACTOR:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		ret = bl_acme_get_shunt(cg, &shunt);
		if (ret == SR_OK)
			*data = g_variant_new_uint64(shunt);
		break;
	case SR_CONF_POWER_OFF:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		ret = bl_acme_read_power_state(cg, &power_off);
		if (ret == SR_OK)
			*data = g_variant_new_boolean(power_off);
		break;
	default:
		return SR_ERR_NA;
	}

	return ret;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	uint64_t samplerate;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
	case SR_CONF_LIMIT_MSEC:
		return sr_sw_limits_config_set(&devc->limits, key, data);
	case SR_CONF_SAMPLERATE:
		samplerate = g_variant_get_uint64(data);
		if (samplerate > MAX_SAMPLE_RATE) {
			sr_err("Maximum sample rate is %d", MAX_SAMPLE_RATE);
			return SR_ERR_SAMPLERATE;
		}
		devc->samplerate = samplerate;
		bl_acme_maybe_set_update_interval(sdi, samplerate);
		break;
	case SR_CONF_PROBE_FACTOR:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		return bl_acme_set_shunt(cg, g_variant_get_uint64(data));
	case SR_CONF_POWER_OFF:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		return bl_acme_set_power_off(cg, g_variant_get_boolean(data));
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	uint32_t devopts_cg[MAX_DEVOPTS_CG];
	int num_devopts_cg = 0;

	if (!cg) {
		switch (key) {
		case SR_CONF_DEVICE_OPTIONS:
			return STD_CONFIG_LIST(key, data, sdi, cg, NO_OPTS, drvopts, devopts);
		case SR_CONF_SAMPLERATE:
			*data = std_gvar_samplerates_steps(ARRAY_AND_SIZE(samplerates));
			break;
		default:
			return SR_ERR_NA;
		}
	} else {
		switch (key) {
		case SR_CONF_DEVICE_OPTIONS:
			if (bl_acme_get_probe_type(cg) == PROBE_ENRG)
				devopts_cg[num_devopts_cg++] = HAS_PROBE_FACTOR;
			if (bl_acme_probe_has_pws(cg))
				devopts_cg[num_devopts_cg++] = HAS_POWER_OFF;

			*data = std_gvar_array_u32(devopts_cg, num_devopts_cg);
			break;
		default:
			return SR_ERR_NA;
		}
	}

	return SR_OK;
}

static void dev_acquisition_close(const struct sr_dev_inst *sdi)
{
	GSList *chl;
	struct sr_channel *ch;

	for (chl = sdi->channels; chl; chl = chl->next) {
		ch = chl->data;
		bl_acme_close_channel(ch);
	}
}

static int dev_acquisition_open(const struct sr_dev_inst *sdi)
{
	GSList *chl;
	struct sr_channel *ch;

	for (chl = sdi->channels; chl; chl = chl->next) {
		ch = chl->data;
		if (bl_acme_open_channel(ch)) {
			sr_err("Error opening channel %s", ch->name);
			dev_acquisition_close(sdi);
			return SR_ERR;
		}
	}

	return 0;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct itimerspec tspec = {
		.it_interval = { 0, 0 },
		.it_value = { 0, 0 }
	};

	if (dev_acquisition_open(sdi))
		return SR_ERR;

	devc = sdi->priv;
	devc->samples_missed = 0;
	devc->timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (devc->timer_fd < 0) {
		sr_err("Error creating timer fd");
		return SR_ERR;
	}

	tspec.it_interval.tv_sec = 0;
	tspec.it_interval.tv_nsec = SR_HZ_TO_NS(devc->samplerate);
	tspec.it_value = tspec.it_interval;

	if (timerfd_settime(devc->timer_fd, 0, &tspec, NULL)) {
		sr_err("Failed to set timer");
		close(devc->timer_fd);
		return SR_ERR;
	}

	devc->channel = g_io_channel_unix_new(devc->timer_fd);
	g_io_channel_set_flags(devc->channel, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_encoding(devc->channel, NULL, NULL);
	g_io_channel_set_buffered(devc->channel, FALSE);

	sr_session_source_add_channel(sdi->session, devc->channel,
		G_IO_IN | G_IO_ERR, 1000, bl_acme_receive_data, (void *)sdi);

	std_session_send_df_header(sdi);
	sr_sw_limits_acquisition_start(&devc->limits);

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;

	dev_acquisition_close(sdi);
	sr_session_source_remove_channel(sdi->session, devc->channel);
	g_io_channel_shutdown(devc->channel, FALSE, NULL);
	g_io_channel_unref(devc->channel);
	devc->channel = NULL;

	std_session_send_df_end(sdi);

	if (devc->samples_missed > 0)
		sr_warn("%" PRIu64 " samples missed", devc->samples_missed);

	return SR_OK;
}

static struct sr_dev_driver baylibre_acme_driver_info = {
	.name = "baylibre-acme",
	.longname = "BayLibre ACME (Another Cute Measurement Equipment)",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = std_dummy_dev_open,
	.dev_close = std_dummy_dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
SR_REGISTER_DEV_DRIVER(baylibre_acme_driver_info);
