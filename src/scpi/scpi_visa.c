/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Martin Ling <martin-sigrok@earth.li>
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
#include <visa.h>
#include <string.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"
#include "scpi.h"

#define LOG_PREFIX "scpi_visa"

struct scpi_visa {
	char *resource;
	ViSession rmgr;
	ViSession vi;
};

static int scpi_visa_dev_inst_new(void *priv, struct drv_context *drvc,
		const char *resource, char **params, const char *serialcomm)
{
	struct scpi_visa *vscpi = priv;

	(void)drvc;
	(void)resource;
	(void)serialcomm;

	if (!params || !params[1]) {
		sr_err("Invalid parameters.");
		return SR_ERR_BUG;
	}

	vscpi->resource = g_strdup(params[1]);

	return SR_OK;
}

static int scpi_visa_open(struct sr_scpi_dev_inst *scpi)
{
	struct scpi_visa *vscpi = scpi->priv;

	if (viOpenDefaultRM(&vscpi->rmgr) != VI_SUCCESS) {
		sr_err("Cannot open default resource manager.");
		return SR_ERR;
	}

	if (viOpen(vscpi->rmgr, vscpi->resource, VI_NO_LOCK, 0, &vscpi->vi) != VI_SUCCESS) {
		sr_err("Cannot open resource.");
		return SR_ERR;
	}

	return SR_OK;
}

static int scpi_visa_connection_id(struct sr_scpi_dev_inst *scpi,
		char **connection_id)
{
	struct scpi_visa *vscpi = scpi->priv;

	*connection_id = g_strdup_printf("%s/%s", scpi->prefix, vscpi->resource);

	return SR_OK;
}

static int scpi_visa_source_add(struct sr_session *session, void *priv,
		int events, int timeout, sr_receive_data_callback cb, void *cb_data)
{
	(void) priv;

	/* Hook up a dummy handler to receive data from the device. */
	return sr_session_source_add(session, -1, events, timeout, cb, cb_data);
}

static int scpi_visa_source_remove(struct sr_session *session, void *priv)
{
	(void) priv;

	return sr_session_source_remove(session, -1);
}

static int scpi_visa_send(void *priv, const char *command)
{
	struct scpi_visa *vscpi = priv;
	ViUInt32 written = 0;
	int len;

	len = strlen(command);
	if (viWrite(vscpi->vi, (ViBuf) (command + written), len,
			&written) != VI_SUCCESS) {
		sr_err("Error while sending SCPI command: '%s'.", command);
		return SR_ERR;
	}

	sr_spew("Successfully sent SCPI command: '%s'.", command);

	return SR_OK;
}

static int scpi_visa_read_begin(void *priv)
{
	(void) priv;

	return SR_OK;
}

static int scpi_visa_read_data(void *priv, char *buf, int maxlen)
{
	struct scpi_visa *vscpi = priv;
	ViUInt32 count;

	if (viRead(vscpi->vi, (ViBuf) buf, maxlen, &count) != VI_SUCCESS) {
		sr_err("Read failed.");
		return SR_ERR;
	}

	return count;
}

static int scpi_visa_read_complete(void *priv)
{
	struct scpi_visa *vscpi = priv;
	ViUInt16 status;

	if (viReadSTB(vscpi->vi, &status) != VI_SUCCESS) {
		sr_err("Failed to read status.");
		return SR_ERR;
	}

	return !(status & 16);
}

static int scpi_visa_close(struct sr_scpi_dev_inst *scpi)
{
	struct scpi_visa *vscpi = scpi->priv;

	viClose(vscpi->vi);
	viClose(vscpi->rmgr);

	return SR_OK;
}

static void scpi_visa_free(void *priv)
{
	struct scpi_visa *vscpi = priv;

	g_free(vscpi->resource);
	g_free(vscpi);
}

SR_PRIV const struct sr_scpi_dev_inst scpi_visa_dev = {
	.name          = "VISA",
	.prefix        = "visa",
	.transport     = SCPI_TRANSPORT_VISA,
	.priv_size     = sizeof(struct scpi_visa),
	.dev_inst_new  = scpi_visa_dev_inst_new,
	.open          = scpi_visa_open,
	.connection_id = scpi_visa_connection_id,
	.source_add    = scpi_visa_source_add,
	.source_remove = scpi_visa_source_remove,
	.send          = scpi_visa_send,
	.read_begin    = scpi_visa_read_begin,
	.read_data     = scpi_visa_read_data,
	.read_complete = scpi_visa_read_complete,
	.close         = scpi_visa_close,
	.free          = scpi_visa_free,
};
