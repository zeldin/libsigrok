/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2011 Uwe Hermann <uwe@hermann-uwe.de>
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
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "input/chronovu-la8"

#define DEFAULT_NUM_CHANNELS    8
#define DEFAULT_SAMPLERATE      SR_MHZ(100)
#define CHUNK_SIZE              (4 * 1024 * 1024)

/*
 * File layout:
 * - Fixed size 8MiB data part at offset 0.
 *   - Either one byte per sample for LA8.
 *   - Or two bytes per sample for LA16, in little endian format.
 * - Five byte "header" at offset 8MiB.
 *   - One "clock divider" byte. The byte value is the divider factor
 *     minus 1. Value 0xff is invalid. Base clock is 100MHz for LA8, or
 *     200MHz for LA16.
 *   - Four bytes for the trigger position. This 32bit value is the
 *     sample number in little endian format, or 0 when unused.
 */
#define CHRONOVU_LA8_DATASIZE   (8 * 1024 * 1024)
#define CHRONOVU_LA8_HDRSIZE    (sizeof(uint8_t) + sizeof(uint32_t))
#define CHRONOVU_LA8_FILESIZE   (CHRONOVU_LA8_DATASIZE + CHRONOVU_LA8_HDRSIZE)

/*
 * Implementation note:
 *
 * The .format_match() routine only checks the file size, but none of
 * the header fields. Only little would be gained (only clock divider
 * 0xff could get tested), but complexity would increase dramatically.
 * Also the .format_match() routine is unlikely to receive large enough
 * a buffer to include the header. Neither is the filename available to
 * the .format_match() routine.
 *
 * There is no way to programmatically tell whether the file was created
 * by LA8 or LA16 software, i.e. with 8 or 16 logic channels. If the
 * filename was available, one might guess based on the file extension,
 * but still would require user specs if neither of the known extensions
 * were used or the input is fed from a pipe.
 *
 * The current input module implementation assumes that users specify
 * the (channel count and) sample rate. Input data gets processed and
 * passed along to the session bus, before the file "header" is seen.
 * A future implementation could move channel creation from init() to
 * receive() or end() (actually: a common routine called from those two
 * routines), and could defer sample processing and feeding the session
 * until the header was seen, including deferred samplerate calculation
 * after having seen the header. But again this improvement depends on
 * the availability of either the filename or the device type. Also note
 * that applications then had to keep sending data to the input module's
 * receive() routine until sufficient amounts of input data were seen
 * including the header (see bug #1017).
 */

struct context {
	gboolean started;
	uint64_t samplerate;
	uint64_t samples_remain;
};

static int format_match(GHashTable *metadata, unsigned int *confidence)
{
	uint64_t size;

	/*
	 * In the absence of a reliable condition like magic strings,
	 * we can only guess based on the file size. Since this is
	 * rather weak a condition, signal "little confidence" and
	 * optionally give precedence to better matches.
	 */
	size = GPOINTER_TO_SIZE(g_hash_table_lookup(metadata,
			GINT_TO_POINTER(SR_INPUT_META_FILESIZE)));
	if (size != CHRONOVU_LA8_FILESIZE)
		return SR_ERR;
	*confidence = 100;

	return SR_OK;
}

static int init(struct sr_input *in, GHashTable *options)
{
	struct context *inc;
	int num_channels, i;
	char name[16];

	num_channels = g_variant_get_int32(g_hash_table_lookup(options, "numchannels"));
	if (num_channels < 1) {
		sr_err("Invalid value for numchannels: must be at least 1.");
		return SR_ERR_ARG;
	}

	in->sdi = g_malloc0(sizeof(struct sr_dev_inst));
	in->priv = inc = g_malloc0(sizeof(struct context));

	inc->samplerate = g_variant_get_uint64(g_hash_table_lookup(options, "samplerate"));

	for (i = 0; i < num_channels; i++) {
		snprintf(name, sizeof(name), "%d", i);
		sr_channel_new(in->sdi, i, SR_CHANNEL_LOGIC, TRUE, name);
	}

	return SR_OK;
}

static int process_buffer(struct sr_input *in)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;
	struct context *inc;
	gsize chunk_size, i;
	gsize chunk;
	uint16_t unitsize;

	inc = in->priv;
	unitsize = (g_slist_length(in->sdi->channels) + 7) / 8;

	if (!inc->started) {
		std_session_send_df_header(in->sdi);

		if (inc->samplerate) {
			(void)sr_session_send_meta(in->sdi, SR_CONF_SAMPLERATE,
				g_variant_new_uint64(inc->samplerate));
		}

		inc->samples_remain = CHRONOVU_LA8_DATASIZE;
		inc->samples_remain /= unitsize;

		inc->started = TRUE;
	}

	packet.type = SR_DF_LOGIC;
	packet.payload = &logic;
	logic.unitsize = unitsize;

	/* Cut off at multiple of unitsize. Avoid sending the "header". */
	chunk_size = in->buf->len / logic.unitsize * logic.unitsize;
	chunk_size = MIN(chunk_size, inc->samples_remain * unitsize);

	for (i = 0; i < chunk_size; i += chunk) {
		logic.data = in->buf->str + i;
		chunk = MIN(CHUNK_SIZE, chunk_size - i);
		if (chunk) {
			logic.length = chunk;
			sr_session_send(in->sdi, &packet);
			inc->samples_remain -= chunk / unitsize;
		}
	}
	g_string_erase(in->buf, 0, chunk_size);

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

static int reset(struct sr_input *in)
{
	struct context *inc = in->priv;

	inc->started = FALSE;
	g_string_truncate(in->buf, 0);

	return SR_OK;
}

static struct sr_option options[] = {
	{ "numchannels", "Number of logic channels", "The number of (logic) channels in the data", NULL, NULL },
	{ "samplerate", "Sample rate (Hz)", "The sample rate of the (logic) data in Hz", NULL, NULL },
	ALL_ZERO
};

static const struct sr_option *get_options(void)
{
	if (!options[0].def) {
		options[0].def = g_variant_ref_sink(g_variant_new_int32(DEFAULT_NUM_CHANNELS));
		options[1].def = g_variant_ref_sink(g_variant_new_uint64(DEFAULT_SAMPLERATE));
	}

	return options;
}

SR_PRIV struct sr_input_module input_chronovu_la8 = {
	.id = "chronovu-la8",
	.name = "ChronoVu LA8/LA16",
	.desc = "ChronoVu LA8/LA16 native file format data",
	.exts = (const char*[]){"kdt", "kd1", NULL},
	.metadata = { SR_INPUT_META_FILESIZE | SR_INPUT_META_REQUIRED },
	.options = get_options,
	.format_match = format_match,
	.init = init,
	.receive = receive,
	.end = end,
	.reset = reset,
};
