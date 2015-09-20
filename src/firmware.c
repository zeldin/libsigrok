/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015 Marcus Comstedt <marcus@mc.pp.se>
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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <zip.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

/** @cond PRIVATE */
#define LOG_PREFIX "firmware"
/** @endcond */

/**
 * @file
 *
 * Firmware handling.
 */

/**
 * @defgroup grp_firmware Firmware handling
 *
 * Firmware handling functions.
 *
 * @{
 */

static int firmware_open_zip(struct sr_firmware_inst *fw, const char *zipname, const char *entryname)
{
	struct zip *archive;
	struct zip_file *zf;
	struct zip_stat zs;
	int ret;
#ifdef G_OS_WIN32
	int flags = ZIP_FL_NOCASE;
#else
	int flags = 0;
#endif

	sr_dbg("Trying to open %s in %s", entryname, zipname);

	if (!(archive = zip_open(zipname, 0, &ret)))
		return SR_ERR;

	if (zip_stat(archive, entryname, flags, &zs) == -1) {
		zip_close(archive);
		return SR_ERR;
	}

	if (!(zf = zip_fopen(archive, entryname, flags))) {
		sr_err("Unable to open firmware file %s in archive %s for reading: %s",
		       entryname, zipname, zip_strerror(archive));
		zip_close(archive);
		return SR_ERR;
	}

	fw->type = SR_FIRMWARE_TYPE_ZIPFILE;
	fw->size = zs.size;
	fw->context.zipfile.archive = archive;
	fw->context.zipfile.zf = zf;

	return SR_OK;
}

static int firmware_open_at(struct sr_firmware_inst *fw, char *filename)
{
	struct stat statbuf;
	FILE *file;
	const char *rp = g_path_skip_root(filename);
	char *p = filename + strlen(filename);
	if (rp == NULL)
		rp = filename;

	/* Check for zipfile */
	while (p != rp) {
		char c = *p;
		if (G_IS_DIR_SEPARATOR(c)) {
			*p = 0;
			if (stat(filename, &statbuf) >= 0) {
				if (S_ISREG(statbuf.st_mode)) {
					if (firmware_open_zip(fw, filename, p+1) == SR_OK) {
						*p = c;
						fw->filename = filename;
						return SR_OK;
					}
				}
				*p = c;
				/* Higher levels can't be a file */
				break;
			}
			*p = c;
		}
		--p;
	}

	if (stat(filename, &statbuf) < 0) {
		return SR_ERR_NA;
	}

	if (!S_ISREG(statbuf.st_mode)) {
		sr_err("%s is not a regular file.", filename);
		return SR_ERR;
	}

	if (!(file = g_fopen(filename, "rb"))) {
		sr_err("Unable to open firmware file %s for reading: %s",
		       filename, g_strerror(errno));
		return SR_ERR;
	}

	fw->type = SR_FIRMWARE_TYPE_FILE;
	fw->filename = filename;
	fw->size = statbuf.st_size;
	fw->context.file.file = file;

	return SR_OK;
}

/**
 * Open the specified firmware.
 *
 * @param fw Uninitialized firmware structure.
 * @param[in] filename Filename to use when opening the firmware.
 *
 * If successful, the firmware structure will become initialized.
 *
 * @retval SR_OK Success.
 * @retval SR_ERR Failure.
 *
 * @private
 */
SR_PRIV int firmware_open(struct sr_firmware_inst *fw, const char *filename)
{
	char *fn;
	char *env_path;
	int err;

	if (!fw) {
		sr_dbg("Invalid firmware.");
		return SR_ERR_ARG;
	}

	/* First, check environment variable */
	if ((env_path = getenv("SIGROK_FIRMWARE_DIR"))) {
		fn = g_build_filename(env_path, filename, NULL);
		if (firmware_open_at(fw, fn) == SR_OK)
			return SR_OK;
		g_free(fn);
	}

	/* Last, try default FIRMWARE_DIR */
	fn = g_build_filename(FIRMWARE_DIR, filename, NULL);
	err = firmware_open_at(fw, fn);
	if (err == SR_OK) {
		return SR_OK;
	}
	if (err == SR_ERR_NA) {
		sr_err("Failed to access firmware file %s: %s.",
		       fn, g_strerror(errno));
	}
	g_free(fn);
	return SR_ERR;
}

/**
 * Close the specified firmware.
 *
 * @param firmware Previously initialized firmware structure.
 *
 * @retval SR_OK Success.
 * @retval SR_ERR Failure.
 *
 * @private
 */
SR_PRIV int firmware_close(struct sr_firmware_inst *fw)
{
	if (!fw) {
		sr_dbg("Invalid firmware.");
		return SR_ERR_ARG;
	}
	if (fw->type == SR_FIRMWARE_TYPE_FILE) {
		fclose(fw->context.file.file);
	} else if (fw->type == SR_FIRMWARE_TYPE_ZIPFILE) {
		zip_fclose(fw->context.zipfile.zf);
		zip_close(fw->context.zipfile.archive);
	} else {
		sr_dbg("Invalid firmware type.");
	}
	g_free(fw->filename);
	return SR_OK;
}

/**
 * Read a number of bytes from the specified firmware, block until finished.
 *
 * @param firmware Previously initialized firmware structure.
 * @param buf Buffer where to store the bytes that are read.
 * @param[in] count The number of bytes to read.
 *
 * @retval other      The number of bytes read. If this is less than the number
 * requested, the end of file was reached.
 *
 * @private
 */
SR_PRIV size_t firmware_read(struct sr_firmware_inst *fw, void *buf, size_t count)
{
	if (!fw) {
		sr_dbg("Invalid firmware.");
		return 0;
	}
	if (fw->type == SR_FIRMWARE_TYPE_FILE) {
		return fread(buf, 1, count, fw->context.file.file);
	} else if (fw->type == SR_FIRMWARE_TYPE_ZIPFILE) {
		zip_int64_t cnt = zip_fread(fw->context.zipfile.zf, buf, count);
		return (cnt < 0? 0 : cnt);
	} else {
		sr_dbg("Invalid firmware type.");
		return 0;
	}
}

/** @} */
