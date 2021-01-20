/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2015 Stefan Brüns <stefan.bruens@rwth-aachen.de>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "input/raw_analog"

/* How many bytes at a time to process and send to the session bus. */
#define CHUNK_SIZE		(4 * 1024 * 1024)
#define DEFAULT_NUM_CHANNELS	1
#define DEFAULT_SAMPLERATE	0

struct context {
	gboolean started;
	int fmt_index;
	uint64_t samplerate;
	int samplesize;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;
};

struct sample_format {
	const char *fmt_name;
	struct sr_analog_encoding encoding;
};

static const struct sample_format sample_formats[] =
{
	                                // bytes, signed, floating, bigendian, digits, digits decimal, scale, offset
	{ "S8 (-1..1)",                 { 1, TRUE,  FALSE, FALSE,  7, FALSE, { 1,                     128}, { 0, 1}}},
	{ "S8 (-128..127)",             { 1, TRUE,  FALSE, FALSE,  7, FALSE, { 1,                       1}, { 0, 1}}},
	{ "U8 (0..1)",                  { 1, FALSE, FALSE, FALSE,  8, FALSE, { 1,                     255}, {-1, 2}}},
	{ "U8 (0..255)",                { 1, FALSE, FALSE, FALSE,  8, FALSE, { 1,                       1}, { 0, 1}}},
	{ "S16_LE (-1..1)",             { 2, TRUE,  FALSE, FALSE, 15, FALSE, { 1,           INT16_MAX + 1}, { 0, 1}}},
	{ "S16_LE (-32768..32767)",     { 2, TRUE,  FALSE, FALSE, 15, FALSE, { 1,                       1}, { 0, 1}}},
	{ "U16_LE (0..1)",              { 2, FALSE, FALSE, FALSE, 16, FALSE, { 1,              UINT16_MAX}, {-1, 2}}},
	{ "U16_LE (0..65535)",          { 2, FALSE, FALSE, FALSE, 16, FALSE, { 1,                       1}, { 0, 1}}},
	{ "S16_BE (-1..1)",             { 2, TRUE,  FALSE, TRUE,  15, FALSE, { 1,           INT16_MAX + 1}, { 0, 1}}},
	{ "S16_BE (-32768..32767)",     { 2, TRUE,  FALSE, TRUE,  15, FALSE, { 1,                       1}, { 0, 1}}},
	{ "U16_BE (0..1)",              { 2, FALSE, FALSE, TRUE,  16, FALSE, { 1,              UINT16_MAX}, {-1, 2}}},
	{ "U16_BE (0..65535)",          { 2, FALSE, FALSE, TRUE,  16, FALSE, { 1,                       1}, { 0, 1}}},
	{ "S32_LE (-1..1)",             { 4, TRUE,  FALSE, FALSE, 31, FALSE, { 1, (uint64_t)INT32_MAX + 1}, { 0, 1}}},
	{ "S32_LE (-2147483648..2147483647)", { 4, TRUE,  FALSE, FALSE, 31, FALSE, { 1,                 1}, { 0, 1}}},
	{ "U32_LE (0..1)",              { 4, FALSE, FALSE, FALSE, 32, FALSE, { 1,              UINT32_MAX}, {-1, 2}}},
	{ "U32_LE (0..4294967295)",     { 4, FALSE, FALSE, FALSE, 32, FALSE, { 1,                       1}, { 0, 1}}},
	{ "S32_BE (-1..1)",             { 4, TRUE,  FALSE, TRUE,  31, FALSE, { 1, (uint64_t)INT32_MAX + 1}, { 0, 1}}},
	{ "S32_BE (-2147483648..2147483647)", { 4, TRUE,  FALSE, TRUE,  31, FALSE, { 1,                 1}, { 0, 1}}},
	{ "U32_BE (0..1)",              { 4, FALSE, FALSE, TRUE,  32, FALSE, { 1,              UINT32_MAX}, {-1, 2}}},
	{ "U32_BE (0..4294967295)",     { 4, FALSE, FALSE, TRUE,  32, FALSE, { 1,                       1}, { 0, 1}}},
	{ "FLOAT_LE",                   { 4, TRUE,  TRUE,  FALSE,   6, TRUE, { 1,                       1}, { 0, 1}}},
	{ "FLOAT_BE",                   { 4, TRUE,  TRUE,  TRUE,    6, TRUE, { 1,                       1}, { 0, 1}}},
	{ "FLOAT64_LE",                 { 8, TRUE,  TRUE,  FALSE,  15, TRUE, { 1,                       1}, { 0, 1}}},
	{ "FLOAT64_BE",                 { 8, TRUE,  TRUE,  TRUE,   15, TRUE, { 1,                       1}, { 0, 1}}},
};

static int parse_format_string(const char *format)
{
	for (unsigned int i = 0; i < ARRAY_SIZE(sample_formats); i++) {
		if (!strcmp(format, sample_formats[i].fmt_name))
			return i;
	}

	return -1;
}

static void init_context(struct context *inc, const struct sample_format *fmt, GSList *channels)
{
	inc->packet.type = SR_DF_ANALOG;
	inc->packet.payload = &inc->analog;

	inc->analog.data = NULL;
	inc->analog.num_samples = 0;
	inc->analog.encoding = &inc->encoding;
	inc->analog.meaning = &inc->meaning;
	inc->analog.spec = &inc->spec;

	memcpy(&inc->encoding, &fmt->encoding, sizeof(inc->encoding));

	inc->meaning.mq = 0;
	inc->meaning.unit = 0;
	inc->meaning.mqflags = 0;
	inc->meaning.channels = channels;

	inc->spec.spec_digits = 0;
}

static int init(struct sr_input *in, GHashTable *options)
{
	struct context *inc;
	int num_channels;
	char channelname[16];
	const char *format;
	int fmt_index;

	num_channels = g_variant_get_int32(g_hash_table_lookup(options, "numchannels"));
	if (num_channels < 1) {
		sr_err("Invalid value for numchannels: must be at least 1.");
		return SR_ERR_ARG;
	}

	format = g_variant_get_string(g_hash_table_lookup(options, "format"), NULL);
	if ((fmt_index = parse_format_string(format)) == -1) {
		GString *formats = g_string_sized_new(200);
		for (unsigned int i = 0; i < ARRAY_SIZE(sample_formats); i++)
			g_string_append_printf(formats, "%s ", sample_formats[i].fmt_name);
		sr_err("Invalid format '%s': must be one of: %s.",
		       format, formats->str);
		g_string_free(formats, TRUE);
		return SR_ERR_ARG;
	}

	in->sdi = g_malloc0(sizeof(struct sr_dev_inst));
	in->priv = inc = g_malloc0(sizeof(struct context));

	for (int i = 0; i < num_channels; i++) {
		snprintf(channelname, sizeof(channelname) - 1, "CH%d", i + 1);
		sr_channel_new(in->sdi, i, SR_CHANNEL_ANALOG, TRUE, channelname);
	}

	inc->samplerate = g_variant_get_uint64(g_hash_table_lookup(options, "samplerate"));
	inc->samplesize = sample_formats[fmt_index].encoding.unitsize * num_channels;
	init_context(inc, &sample_formats[fmt_index], in->sdi->channels);

	return SR_OK;
}

static int process_buffer(struct sr_input *in)
{
	struct context *inc;
	unsigned int offset, chunk_size;

	inc = in->priv;
	if (!inc->started) {
		std_session_send_df_header(in->sdi);

		if (inc->samplerate) {
			(void)sr_session_send_meta(in->sdi, SR_CONF_SAMPLERATE,
				g_variant_new_uint64(inc->samplerate));
		}

		inc->started = TRUE;
	}

	/* Round down to the last channels * unitsize boundary. */
	inc->analog.num_samples = CHUNK_SIZE / inc->samplesize;
	chunk_size = inc->analog.num_samples * inc->samplesize;
	offset = 0;

	while ((offset + chunk_size) < in->buf->len) {
		inc->analog.data = in->buf->str + offset;
		sr_session_send(in->sdi, &inc->packet);
		offset += chunk_size;
	}

	inc->analog.num_samples = (in->buf->len - offset) / inc->samplesize;
	chunk_size = inc->analog.num_samples * inc->samplesize;
	if (chunk_size > 0) {
		inc->analog.data = in->buf->str + offset;
		sr_session_send(in->sdi, &inc->packet);
		offset += chunk_size;
	}

	if ((unsigned int)offset < in->buf->len) {
		/*
		 * The incoming buffer wasn't processed completely. Stash
		 * the leftover data for next time.
		 */
		g_string_erase(in->buf, 0, offset);
	} else {
		g_string_truncate(in->buf, 0);
	}

	return SR_OK;
}

static int receive(struct sr_input *in, GString *buf)
{
	int ret;

	g_string_append_len(in->buf, buf->str, buf->len);

	if (!in->sdi_ready) {
		/* sdi is ready, notify frontend. */
		in->sdi_ready = TRUE;
		return SR_OK;
	}

	ret = process_buffer(in);

	return ret;
}

static int end(struct sr_input *in)
{
	struct context *inc;
	int ret;

	if (in->sdi_ready)
		ret = process_buffer(in);
	else
		ret = SR_OK;

	inc = in->priv;
	if (inc->started)
		std_session_send_df_end(in->sdi);

	return ret;
}

static struct sr_option options[] = {
	{ "numchannels", "Number of analog channels", "The number of (analog) channels in the data", NULL, NULL },
	{ "samplerate", "Sample rate (Hz)", "The sample rate of the (analog) data in Hz", NULL, NULL },
	{ "format", "Data format", "The format of the data (data type, signedness, endianness)", NULL, NULL },
	ALL_ZERO
};

static const struct sr_option *get_options(void)
{
	if (!options[0].def) {
		options[0].def = g_variant_ref_sink(g_variant_new_int32(DEFAULT_NUM_CHANNELS));
		options[1].def = g_variant_ref_sink(g_variant_new_uint64(DEFAULT_SAMPLERATE));
		options[2].def = g_variant_ref_sink(g_variant_new_string(sample_formats[0].fmt_name));
		for (unsigned int i = 0; i < ARRAY_SIZE(sample_formats); i++) {
			options[2].values = g_slist_append(options[2].values,
				g_variant_ref_sink(g_variant_new_string(sample_formats[i].fmt_name)));
		}
	}

	return options;
}

static void cleanup(struct sr_input *in)
{
	g_free(in->priv);
	in->priv = NULL;

	g_variant_unref(options[0].def);
	g_variant_unref(options[1].def);
	g_variant_unref(options[2].def);
	g_slist_free_full(options[2].values, (GDestroyNotify)g_variant_unref);
}

static int reset(struct sr_input *in)
{
	struct context *inc = in->priv;

	inc->started = FALSE;

	g_string_truncate(in->buf, 0);

	return SR_OK;
}

SR_PRIV struct sr_input_module input_raw_analog = {
	.id = "raw_analog",
	.name = "RAW analog",
	.desc = "Raw analog data without header",
	.exts = (const char*[]){"raw", "bin", NULL},
	.options = get_options,
	.init = init,
	.receive = receive,
	.end = end,
	.cleanup = cleanup,
	.reset = reset,
};
