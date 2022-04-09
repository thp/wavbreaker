/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2002 Timothy Robinson
 * Copyright (C) 2022 Thomas Perl
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <sys/stat.h>

#include "format_cdda_raw.h"


/**
 * Example files can be created from an Audio CD using (for example):
 *
 *     cdparanoia -l -R -B
 *
 * The filename extension here must match (".cdda.raw") since the raw
 * file doesn't have any header for auto-detection / probing).
 */

typedef struct OpenedCDDAFile_ OpenedCDDAFile;
struct OpenedCDDAFile_ {
    OpenedAudioFile hdr;

    unsigned long file_size;
};

static void
cdda_raw_close_file(const FormatModule *self, OpenedAudioFile *file)
{
    OpenedCDDAFile *cdda = (OpenedCDDAFile *)file;

    opened_audio_file_close(&cdda->hdr);
    g_free(cdda);
}

static OpenedAudioFile *
cdda_raw_open_file(const FormatModule *self, const char *filename, char **error_message)
{
    if (!format_module_filename_extension_check(self, filename, NULL)) {
        format_module_set_error_message(error_message, "Extension is not %s", self->default_file_extension);
        return NULL;
    }

    OpenedCDDAFile *cdda = g_new0(OpenedCDDAFile, 1);

    if (!format_module_open_file(self, &cdda->hdr, filename, error_message)) {
        g_free(cdda);
        return NULL;
    }

    struct stat statBuf;

    if (stat(cdda->hdr.filename, &statBuf)) {
        format_module_set_error_message(error_message, "Error stat()ing %s", cdda->hdr.filename);
        goto error;
    }

    cdda->file_size = statBuf.st_size;

    SampleInfo *si = &cdda->hdr.sample_info;

    si->numBytes = statBuf.st_size;
    si->channels = 2;
    si->samplesPerSec = 44100;
    si->bitsPerSample = 16;
    si->avgBytesPerSec = si->bitsPerSample/8 * si->samplesPerSec * si->channels;
    si->blockAlign = 4;
    si->blockSize = si->avgBytesPerSec / CD_BLOCKS_PER_SEC;

    return &cdda->hdr;

error:
    cdda_raw_close_file(self, &cdda->hdr);

    return NULL;
}

static long
cdda_raw_read_samples(OpenedAudioFile *self, unsigned char *buf, size_t buf_size, unsigned long start_pos)
{
    OpenedCDDAFile *cdda = (OpenedCDDAFile *)self;

    size_t i = 0;
    size_t ret;

    if (fseek(cdda->hdr.fp, start_pos, SEEK_SET)) {
        return -1;
    }

    if (start_pos > cdda->file_size) {
        return -1;
    }

    ret = fread(buf, 1, buf_size, cdda->hdr.fp);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    for (i = 0; i < ret / 4; i++) {
        unsigned char tmp;

        /* left channel */
        tmp = buf[i*4+0];
        buf[i*4+0] = buf[i*4+1];
        buf[i*4+1] = tmp;

        /* right channel */
        tmp = buf[i*4+2];
        buf[i*4+2] = buf[i*4+3];
        buf[i*4+3] = tmp;
    }
#endif /* G_LITTLE_ENDIAN */

    return ret;
}

int
cdda_raw_write_file(OpenedAudioFile *self, const char *output_filename, unsigned long start_pos, unsigned long end_pos, double *progress)
{
    OpenedCDDAFile *cdda = (OpenedCDDAFile *)self;

    int buf_size = cdda->hdr.sample_info.blockSize;

    size_t ret;
    FILE *new_fp;
    unsigned long cur_pos;
    unsigned char buf[buf_size];

    if ((new_fp = fopen(output_filename, "wb")) == NULL) {
        g_warning("Error opening %s for writing", output_filename);
        return -1;
    }

    cur_pos = start_pos;

    if (fseek(cdda->hdr.fp, cur_pos, SEEK_SET)) {
        fclose(new_fp);
        return -1;
    }

    if (cur_pos > cdda->file_size) {
        fclose(new_fp);
        return -1;
    }

    if (cur_pos + buf_size > end_pos && end_pos != 0) {
        buf_size = end_pos - cur_pos;
    }

    while ((ret = fread(buf, 1, buf_size, cdda->hdr.fp)) > 0 &&
            (cur_pos < end_pos || end_pos == 0)) {

        if ((fwrite(buf, 1, ret, new_fp)) < ret) {
            g_warning("Error writing to file %s", output_filename);
            fclose(new_fp);
            return -1;
        }
        cur_pos += ret;

        if (cur_pos + buf_size > end_pos && end_pos != 0) {
            buf_size = end_pos - cur_pos;
        }
    }

    fclose(new_fp);
    return ret;
}

static const FormatModule
CDDA_RAW_FORMAT_MODULE = {
    .name = "CD Digital Audio (Big-Endian)",
    .default_file_extension = ".cdda.raw",

    .open_file = cdda_raw_open_file,
    .close_file = cdda_raw_close_file,

    .read_samples = cdda_raw_read_samples,
    .write_file = cdda_raw_write_file,
};

const FormatModule *
format_module_cdda_raw()
{
    return &CDDA_RAW_FORMAT_MODULE;
}
