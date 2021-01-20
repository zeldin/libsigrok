/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
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
#include <math.h>
#include <string.h>
#include <glib.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"
#include "protocol.h"

static void handle_qm_18x(const struct sr_dev_inst *sdi, char **tokens)
{
	struct dev_context *devc;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;
	float fvalue;
	char *e, *u;
	gboolean is_oor;

	devc = sdi->priv;

	if (strcmp(tokens[0], "QM") || !tokens[1])
		return;

	if ((e = strstr(tokens[1], "Out of range"))) {
		is_oor = TRUE;
		fvalue = -1;
		while (*e && *e != '.')
			e++;
	} else {
		is_oor = FALSE;
		/* Delimit the float, since sr_atof_ascii() wants only
		 * a valid float here. */
		e = tokens[1];
		while (*e && *e != ' ')
			e++;
		*e++ = '\0';
		if (sr_atof_ascii(tokens[1], &fvalue) != SR_OK || fvalue == 0.0) {
			/* Happens all the time, when switching modes. */
			sr_dbg("Invalid float.");
			return;
		}
	}
	while (*e && *e == ' ')
		e++;

	/* TODO: Use proper 'digits' value for this device (and its modes). */
	sr_analog_init(&analog, &encoding, &meaning, &spec, 2);
	analog.data = &fvalue;
	analog.meaning->channels = sdi->channels;
	analog.num_samples = 1;
	if (is_oor)
		fvalue = NAN;
	analog.meaning->mq = 0;

	if ((u = strstr(e, "V DC")) || (u = strstr(e, "V AC"))) {
		analog.meaning->mq = SR_MQ_VOLTAGE;
		analog.meaning->unit = SR_UNIT_VOLT;
		if (!is_oor && e[0] == 'm')
			fvalue /= 1000;
		/* This catches "V AC", "V DC" and "V AC+DC". */
		if (strstr(u, "AC"))
			analog.meaning->mqflags |= SR_MQFLAG_AC | SR_MQFLAG_RMS;
		if (strstr(u, "DC"))
			analog.meaning->mqflags |= SR_MQFLAG_DC;
	} else if ((u = strstr(e, "dBV")) || (u = strstr(e, "dBm"))) {
		analog.meaning->mq = SR_MQ_VOLTAGE;
		if (u[2] == 'm')
			analog.meaning->unit = SR_UNIT_DECIBEL_MW;
		else
			analog.meaning->unit = SR_UNIT_DECIBEL_VOLT;
		analog.meaning->mqflags |= SR_MQFLAG_AC | SR_MQFLAG_RMS;
	} else if ((u = strstr(e, "Ohms"))) {
		analog.meaning->mq = SR_MQ_RESISTANCE;
		analog.meaning->unit = SR_UNIT_OHM;
		if (is_oor)
			fvalue = INFINITY;
		else if (e[0] == 'k')
			fvalue *= 1000;
		else if (e[0] == 'M')
			fvalue *= 1000000;
	} else if (!strcmp(e, "nS")) {
		analog.meaning->mq = SR_MQ_CONDUCTANCE;
		analog.meaning->unit = SR_UNIT_SIEMENS;
		*((float *)analog.data) /= 1e+9;
	} else if ((u = strstr(e, "Farads"))) {
		analog.meaning->mq = SR_MQ_CAPACITANCE;
		analog.meaning->unit = SR_UNIT_FARAD;
		if (!is_oor) {
			if (e[0] == 'm')
				fvalue /= 1e+3;
			else if (e[0] == 'u')
				fvalue /= 1e+6;
			else if (e[0] == 'n')
				fvalue /= 1e+9;
		}
	} else if ((u = strstr(e, "Deg C")) || (u = strstr(e, "Deg F"))) {
		analog.meaning->mq = SR_MQ_TEMPERATURE;
		if (u[4] == 'C')
			analog.meaning->unit = SR_UNIT_CELSIUS;
		else
			analog.meaning->unit = SR_UNIT_FAHRENHEIT;
	} else if ((u = strstr(e, "A AC")) || (u = strstr(e, "A DC"))) {
		analog.meaning->mq = SR_MQ_CURRENT;
		analog.meaning->unit = SR_UNIT_AMPERE;
		/* This catches "A AC", "A DC" and "A AC+DC". */
		if (strstr(u, "AC"))
			analog.meaning->mqflags |= SR_MQFLAG_AC | SR_MQFLAG_RMS;
		if (strstr(u, "DC"))
			analog.meaning->mqflags |= SR_MQFLAG_DC;
		if (!is_oor) {
			if (e[0] == 'm')
				fvalue /= 1e+3;
			else if (e[0] == 'u')
				fvalue /= 1e+6;
		}
	} else if ((u = strstr(e, "Hz"))) {
		analog.meaning->mq = SR_MQ_FREQUENCY;
		analog.meaning->unit = SR_UNIT_HERTZ;
		if (e[0] == 'k')
			fvalue *= 1e+3;
	} else if (!strcmp(e, "%")) {
		analog.meaning->mq = SR_MQ_DUTY_CYCLE;
		analog.meaning->unit = SR_UNIT_PERCENTAGE;
	} else if ((u = strstr(e, "ms"))) {
		analog.meaning->mq = SR_MQ_PULSE_WIDTH;
		analog.meaning->unit = SR_UNIT_SECOND;
		fvalue /= 1e+3;
	}

	if (analog.meaning->mq != 0) {
		/* Got a measurement. */
		packet.type = SR_DF_ANALOG;
		packet.payload = &analog;
		sr_session_send(sdi, &packet);
		sr_sw_limits_update_samples_read(&devc->limits, 1);
	}
}

static void handle_qm_28x(const struct sr_dev_inst *sdi, char **tokens)
{
	struct dev_context *devc;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;
	float fvalue;

	devc = sdi->priv;

	if (!tokens[1])
		return;

	if (sr_atof_ascii(tokens[0], &fvalue) != SR_OK || fvalue == 0.0) {
		sr_err("Invalid float '%s'.", tokens[0]);
		return;
	}

	/* TODO: Use proper 'digits' value for this device (and its modes). */
	sr_analog_init(&analog, &encoding, &meaning, &spec, 2);
	analog.data = &fvalue;
	analog.meaning->channels = sdi->channels;
	analog.num_samples = 1;
	analog.meaning->mq = 0;

	if (!strcmp(tokens[1], "VAC") || !strcmp(tokens[1], "VDC")) {
		analog.meaning->mq = SR_MQ_VOLTAGE;
		analog.meaning->unit = SR_UNIT_VOLT;
		if (!strcmp(tokens[2], "NORMAL")) {
			if (tokens[1][1] == 'A') {
				analog.meaning->mqflags |= SR_MQFLAG_AC;
				analog.meaning->mqflags |= SR_MQFLAG_RMS;
			} else
				analog.meaning->mqflags |= SR_MQFLAG_DC;
		} else if (!strcmp(tokens[2], "OL") || !strcmp(tokens[2], "OL_MINUS")) {
			fvalue = NAN;
		} else
			analog.meaning->mq = 0;
	} else if (!strcmp(tokens[1], "dBV") || !strcmp(tokens[1], "dBm")) {
		analog.meaning->mq = SR_MQ_VOLTAGE;
		if (tokens[1][2] == 'm')
			analog.meaning->unit = SR_UNIT_DECIBEL_MW;
		else
			analog.meaning->unit = SR_UNIT_DECIBEL_VOLT;
		analog.meaning->mqflags |= SR_MQFLAG_AC | SR_MQFLAG_RMS;
	} else if (!strcmp(tokens[1], "CEL") || !strcmp(tokens[1], "FAR")) {
		if (!strcmp(tokens[2], "NORMAL")) {
			analog.meaning->mq = SR_MQ_TEMPERATURE;
			if (tokens[1][0] == 'C')
				analog.meaning->unit = SR_UNIT_CELSIUS;
			else
				analog.meaning->unit = SR_UNIT_FAHRENHEIT;
		}
	} else if (!strcmp(tokens[1], "OHM")) {
		if (!strcmp(tokens[3], "NONE")) {
			analog.meaning->mq = SR_MQ_RESISTANCE;
			analog.meaning->unit = SR_UNIT_OHM;
			if (!strcmp(tokens[2], "OL") || !strcmp(tokens[2], "OL_MINUS")) {
				fvalue = INFINITY;
			} else if (strcmp(tokens[2], "NORMAL"))
				analog.meaning->mq = 0;
		} else if (!strcmp(tokens[3], "OPEN_CIRCUIT")) {
			analog.meaning->mq = SR_MQ_CONTINUITY;
			analog.meaning->unit = SR_UNIT_BOOLEAN;
			fvalue = 0.0;
		} else if (!strcmp(tokens[3], "SHORT_CIRCUIT")) {
			analog.meaning->mq = SR_MQ_CONTINUITY;
			analog.meaning->unit = SR_UNIT_BOOLEAN;
			fvalue = 1.0;
		}
	} else if (!strcmp(tokens[1], "F")
			&& !strcmp(tokens[2], "NORMAL")
			&& !strcmp(tokens[3], "NONE")) {
		analog.meaning->mq = SR_MQ_CAPACITANCE;
		analog.meaning->unit = SR_UNIT_FARAD;
	} else if (!strcmp(tokens[1], "AAC") || !strcmp(tokens[1], "ADC")) {
		analog.meaning->mq = SR_MQ_CURRENT;
		analog.meaning->unit = SR_UNIT_AMPERE;
		if (!strcmp(tokens[2], "NORMAL")) {
			if (tokens[1][1] == 'A') {
				analog.meaning->mqflags |= SR_MQFLAG_AC;
				analog.meaning->mqflags |= SR_MQFLAG_RMS;
			} else
				analog.meaning->mqflags |= SR_MQFLAG_DC;
		} else if (!strcmp(tokens[2], "OL") || !strcmp(tokens[2], "OL_MINUS")) {
			fvalue = NAN;
		} else
			analog.meaning->mq = 0;
	} else if (!strcmp(tokens[1], "Hz") && !strcmp(tokens[2], "NORMAL")) {
		analog.meaning->mq = SR_MQ_FREQUENCY;
		analog.meaning->unit = SR_UNIT_HERTZ;
	} else if (!strcmp(tokens[1], "PCT") && !strcmp(tokens[2], "NORMAL")) {
		analog.meaning->mq = SR_MQ_DUTY_CYCLE;
		analog.meaning->unit = SR_UNIT_PERCENTAGE;
	} else if (!strcmp(tokens[1], "S") && !strcmp(tokens[2], "NORMAL")) {
		analog.meaning->mq = SR_MQ_PULSE_WIDTH;
		analog.meaning->unit = SR_UNIT_SECOND;
	} else if (!strcmp(tokens[1], "SIE") && !strcmp(tokens[2], "NORMAL")) {
		analog.meaning->mq = SR_MQ_CONDUCTANCE;
		analog.meaning->unit = SR_UNIT_SIEMENS;
	}

	if (analog.meaning->mq != 0) {
		/* Got a measurement. */
		packet.type = SR_DF_ANALOG;
		packet.payload = &analog;
		sr_session_send(sdi, &packet);
		sr_sw_limits_update_samples_read(&devc->limits, 1);
	}
}

static void handle_qm_19x_meta(const struct sr_dev_inst *sdi, char **tokens)
{
	struct dev_context *devc;
	int meas_type, meas_unit, meas_char, i;

	/* Make sure we have 7 valid tokens. */
	for (i = 0; tokens[i] && i < 7; i++);
	if (i != 7)
		return;

	if (strcmp(tokens[1], "1"))
		/* Invalid measurement. */
		return;

	if (strcmp(tokens[2], "3"))
		/* Only interested in input from the meter mode source. */
		return;

	devc = sdi->priv;

	/* Measurement type 11 == absolute, 19 = relative */
	meas_type = strtol(tokens[0], NULL, 10);
	if (meas_type != 11 && meas_type != 19)
		/* Device is in some mode we don't support. */
		return;

	/* We might get metadata for absolute and relative mode (if the device
	 * is in relative mode). In that case, relative takes precedence. */
	if (meas_type == 11 && devc->meas_type == 19)
		return;

	meas_unit = strtol(tokens[3], NULL, 10);
	if (meas_unit == 0)
		/* Device is turned off. Really. */
		return;
	meas_char = strtol(tokens[4], NULL, 10);

	devc->mq = 0;
	devc->unit = 0;
	devc->mqflags = 0;

	switch (meas_unit) {
	case 1:
		devc->mq = SR_MQ_VOLTAGE;
		devc->unit = SR_UNIT_VOLT;
		if (meas_char == 1)
			devc->mqflags |= SR_MQFLAG_DC;
		else if (meas_char == 2)
			devc->mqflags |= SR_MQFLAG_AC;
		else if (meas_char == 3)
			devc->mqflags |= SR_MQFLAG_DC | SR_MQFLAG_AC;
		else if (meas_char == 15)
			devc->mqflags |= SR_MQFLAG_DIODE | SR_MQFLAG_DC;
		break;
	case 2:
		devc->mq = SR_MQ_CURRENT;
		devc->unit = SR_UNIT_AMPERE;
		if (meas_char == 1)
			devc->mqflags |= SR_MQFLAG_DC;
		else if (meas_char == 2)
			devc->mqflags |= SR_MQFLAG_AC;
		else if (meas_char == 3)
			devc->mqflags |= SR_MQFLAG_DC | SR_MQFLAG_AC;
		break;
	case 3:
		if (meas_char == 1) {
			devc->mq = SR_MQ_RESISTANCE;
			devc->unit = SR_UNIT_OHM;
		} else if (meas_char == 16) {
			devc->mq = SR_MQ_CONTINUITY;
			devc->unit = SR_UNIT_BOOLEAN;
		}
		break;
	case 12:
		devc->mq = SR_MQ_TEMPERATURE;
		devc->unit = SR_UNIT_CELSIUS;
		break;
	case 13:
		devc->mq = SR_MQ_TEMPERATURE;
		devc->unit = SR_UNIT_FAHRENHEIT;
		break;
	default:
		sr_dbg("unknown unit: %d", meas_unit);
	}
	if (devc->mq == 0 && devc->unit == 0)
		return;

	/* If we got here, we know how to interpret the measurement. */
	devc->meas_type = meas_type;
	if (meas_type == 11)
		/* Absolute meter reading. */
		devc->is_relative = FALSE;
	else if (!strcmp(tokens[0], "19"))
		/* Relative meter reading. */
		devc->is_relative = TRUE;

}

static void handle_qm_19x_data(const struct sr_dev_inst *sdi, char **tokens)
{
	struct dev_context *devc;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;
	float fvalue;

	if (!strcmp(tokens[0], "9.9E+37")) {
		/* An invalid measurement shows up on the display as "OL", but
		 * comes through like this. Since comparing 38-digit floats
		 * is rather problematic, we'll cut through this here. */
		fvalue = NAN;
	} else {
		if (sr_atof_ascii(tokens[0], &fvalue) != SR_OK || fvalue == 0.0) {
			sr_err("Invalid float '%s'.", tokens[0]);
			return;
		}
	}

	devc = sdi->priv;
	if (devc->mq == 0 || devc->unit == 0)
		/* Don't have valid metadata yet. */
		return;

	if (devc->mq == SR_MQ_RESISTANCE && isnan(fvalue))
		fvalue = INFINITY;
	else if (devc->mq == SR_MQ_CONTINUITY) {
		if (isnan(fvalue))
			fvalue = 0.0;
		else
			fvalue = 1.0;
	}

	/* TODO: Use proper 'digits' value for this device (and its modes). */
	sr_analog_init(&analog, &encoding, &meaning, &spec, 2);
	analog.meaning->channels = sdi->channels;
	analog.num_samples = 1;
	analog.data = &fvalue;
	analog.meaning->mq = devc->mq;
	analog.meaning->unit = devc->unit;
	analog.meaning->mqflags = 0;
	packet.type = SR_DF_ANALOG;
	packet.payload = &analog;
	sr_session_send(sdi, &packet);

	sr_sw_limits_update_samples_read(&devc->limits, 1);
}

static void handle_line(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	int num_tokens, n, i;
	char cmd[16], **tokens;

	devc = sdi->priv;
	serial = sdi->conn;
	sr_spew("Received line '%s' (%d).", devc->buf, devc->buflen);

	if (devc->buflen == 1) {
		if (devc->buf[0] != '0') {
			/* Not just a CMD_ACK from the query command. */
			sr_dbg("Got CMD_ACK '%c'.", devc->buf[0]);
			devc->expect_response = FALSE;
		}
		devc->buflen = 0;
		return;
	}

	tokens = g_strsplit(devc->buf, ",", 0);
	if (tokens[0]) {
		if (devc->profile->model == FLUKE_187 || devc->profile->model == FLUKE_189) {
			devc->expect_response = FALSE;
			handle_qm_18x(sdi, tokens);
		} else if (devc->profile->model == FLUKE_287 || devc->profile->model == FLUKE_289) {
			devc->expect_response = FALSE;
			handle_qm_28x(sdi, tokens);
		} else if (devc->profile->model == FLUKE_190) {
			devc->expect_response = FALSE;
			for (num_tokens = 0; tokens[num_tokens]; num_tokens++);
			if (num_tokens >= 7) {
				/* Response to QM: this is a comma-separated list of
				 * fields with metadata about the measurement. This
				 * format can return multiple sets of metadata,
				 * split into sets of 7 tokens each. */
				devc->meas_type = 0;
				for (i = 0; i < num_tokens; i += 7)
					handle_qm_19x_meta(sdi, tokens + i);
				if (devc->meas_type) {
					/* Slip the request in now, before the main
					 * timer loop asks for metadata again. */
					n = sprintf(cmd, "QM %d\r", devc->meas_type);
					if (serial_write_blocking(serial, cmd, n, SERIAL_WRITE_TIMEOUT_MS) < 0)
						sr_err("Unable to send QM (measurement).");
				}
			} else {
				/* Response to QM <n> measurement request. */
				handle_qm_19x_data(sdi, tokens);
			}
		}
	}
	g_strfreev(tokens);
	devc->buflen = 0;
}

SR_PRIV int fluke_receive_data(int fd, int revents, void *cb_data)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	int len;
	int64_t now, elapsed;

	(void)fd;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	serial = sdi->conn;
	if (revents == G_IO_IN) {
		/* Serial data arrived. */
		while (FLUKEDMM_BUFSIZE - devc->buflen - 1 > 0) {
			len = serial_read_nonblocking(serial, devc->buf + devc->buflen, 1);
			if (len < 1)
				break;
			devc->buflen++;
			*(devc->buf + devc->buflen) = '\0';
			if (*(devc->buf + devc->buflen - 1) == '\r') {
				*(devc->buf + --devc->buflen) = '\0';
				handle_line(sdi);
				break;
			}
		}
	}

	if (sr_sw_limits_check(&devc->limits)) {
		sr_dev_acquisition_stop(sdi);
		return TRUE;
	}

	now = g_get_monotonic_time() / 1000;
	elapsed = now - devc->cmd_sent_at;
	/* Send query command at poll_period interval, or after 1 second
	 * has elapsed. This will make it easier to recover from any
	 * out-of-sync or temporary disconnect issues. */
	if ((devc->expect_response == FALSE && elapsed > devc->profile->poll_period)
			|| elapsed > devc->profile->timeout) {
		if (serial_write_blocking(serial, "QM\r", 3, SERIAL_WRITE_TIMEOUT_MS) < 0)
			sr_err("Unable to send QM.");
		devc->cmd_sent_at = now;
		devc->expect_response = TRUE;
	}

	return TRUE;
}
