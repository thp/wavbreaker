/* wavbreaker - A tool to split a wave file up into multiple waves.
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

#include <config.h>

#include "format_ogg_vorbis.h"

#if defined(HAVE_VORBISFILE)

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <stdint.h>
#include <inttypes.h>


typedef struct OpenedOGGVorbisFile_ OpenedOGGVorbisFile;
struct OpenedOGGVorbisFile_ {
    OpenedAudioFile hdr;

    OggVorbis_File ogg_vorbis_file;
    size_t ogg_vorbis_offset;
};

static long
ogg_vorbis_read_samples(OpenedAudioFile *self, unsigned char *buf, size_t buf_size, unsigned long start_pos)
{
    OpenedOGGVorbisFile *ogg = (OpenedOGGVorbisFile *)self;

    if (ogg->ogg_vorbis_offset != start_pos) {
        ov_pcm_seek(&ogg->ogg_vorbis_file, start_pos / ogg->hdr.sample_info.blockAlign);
        ogg->ogg_vorbis_offset = start_pos;
    }

    long result = 0;

    while (buf_size > 0) {
        long res = ov_read(&ogg->ogg_vorbis_file, (char *)buf, buf_size, 0, 2, 1, NULL);
        if (res < 0) {
            g_warning("Error in ov_read(): %ld", res);
            return -1;
        }

        result += res;
        ogg->ogg_vorbis_offset += res;
        buf += res;
        buf_size -= res;

        if (res == 0) {
            break;
        }
    }

    return result;
}

int
ogg_vorbis_write_file(OpenedAudioFile *self, const char *output_filename, unsigned long start_pos, unsigned long end_pos, report_progress_func report_progress, void *report_progress_user_data)
{
    OpenedOGGVorbisFile *ogg = (OpenedOGGVorbisFile *)self;

    g_warning("TODO: Write file '%s', start=%lu, end=%lu", output_filename, start_pos, end_pos);

    report_progress(0.0, report_progress_user_data);

    uint32_t start_samples = start_pos / ogg->hdr.sample_info.blockSize * ogg->hdr.sample_info.samplesPerSec / CD_BLOCKS_PER_SEC;
    uint32_t end_samples = end_pos / ogg->hdr.sample_info.blockSize * ogg->hdr.sample_info.samplesPerSec / CD_BLOCKS_PER_SEC;

    if (end_samples == 0) {
        end_samples = ogg->hdr.sample_info.numBytes / ogg->hdr.sample_info.blockAlign;
    }

    // TODO: Copy Ogg Vorbis samples from source to output file
    (void)start_samples;
    (void)end_samples;

    return -1;
}

static void
ogg_vorbis_close_file(const FormatModule *self, OpenedAudioFile *file)
{
    OpenedOGGVorbisFile *ogg = (OpenedOGGVorbisFile *)file;

    opened_audio_file_close(&ogg->hdr);

    ov_clear(&ogg->ogg_vorbis_file);

    g_free(ogg);
}

static OpenedAudioFile *
ogg_vorbis_open_file(const FormatModule *self, const char *filename, char **error_message)
{
    OpenedOGGVorbisFile *ogg = g_new0(OpenedOGGVorbisFile, 1);

    if (!format_module_open_file(self, &ogg->hdr, filename, error_message)) {
        g_free(ogg);
        return NULL;
    }

    SampleInfo *si = &ogg->hdr.sample_info;

    ogg->ogg_vorbis_offset = 0;

    g_debug("Trying as Ogg Vorbis...");
    int ogg_res = ov_fopen(ogg->hdr.filename, &ogg->ogg_vorbis_file);

    if (ogg_res == 0) {
        g_debug("Detected Ogg Vorbis");

        vorbis_info *info = ov_info(&ogg->ogg_vorbis_file, -1);

        g_debug("Ogg Vorbis info: version=%d, channels=%d, rate=%ld, samples=%ld",
                info->version, info->channels, info->rate, (long int)ov_pcm_total(&ogg->ogg_vorbis_file, -1));

        vorbis_comment *comment = ov_comment(&ogg->ogg_vorbis_file, -1);

        long bitrate = ov_bitrate(&ogg->ogg_vorbis_file, -1);

        if (bitrate == OV_EINVAL || bitrate == OV_FALSE) {
            bitrate = -1;
        } else {
            bitrate /= 1000;
        }

        uint32_t serial = ov_serialnumber(&ogg->ogg_vorbis_file, -1) & 0xFFFFFFFF;

        ogg->hdr.details = g_strdup_printf("%s, serial %08"PRIx32", %ld kbps", comment->vendor, serial, bitrate);

        si->channels = info->channels;
        si->samplesPerSec = info->rate;
        si->bitsPerSample = 16;

        si->blockAlign = si->channels * (si->bitsPerSample / 8);
        si->avgBytesPerSec = si->blockAlign * si->samplesPerSec;
        si->blockSize = si->avgBytesPerSec / CD_BLOCKS_PER_SEC;
        si->numBytes = ov_pcm_total(&ogg->ogg_vorbis_file, -1) * si->blockAlign;
    } else {
        format_module_set_error_message(error_message, "ov_fopen() returned %d, probably not an Ogg file", ogg_res);
        goto error;
    }

    return &ogg->hdr;

error:
    ogg_vorbis_close_file(self, &ogg->hdr);

    return NULL;
}

static const FormatModule
OGG_VORBIS_FORMAT_MODULE = {
    .name = "Ogg Vorbis",
    .library_name = "libvorbisfile",
    .default_file_extension = ".ogg",

    .open_file = ogg_vorbis_open_file,
    .close_file = ogg_vorbis_close_file,

    .read_samples = ogg_vorbis_read_samples,
    .write_file = ogg_vorbis_write_file,
};

const FormatModule *
format_module_ogg_vorbis(void)
{
    return &OGG_VORBIS_FORMAT_MODULE;
}

#else

const FormatModule *
format_module_ogg_vorbis(void)
{
    return NULL;
}

#endif /* HAVE_VORBISFILE */
