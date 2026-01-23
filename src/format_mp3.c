/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2006 Timothy Robinson
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

#include "format_mp3.h"

#if defined(HAVE_MPG123)

//#define WAVBREAKER_MP3_DEBUG

#include <stdint.h>
#include <mpg123.h>

typedef struct OpenedMP3File_ OpenedMP3File;
struct OpenedMP3File_ {
    OpenedAudioFile hdr;

    mpg123_handle *mpg123;
    size_t mpg123_offset;
};

static long
mp3_read_samples(OpenedAudioFile *self, unsigned char *buf, size_t buf_size, unsigned long start_pos)
{
    OpenedMP3File *mp3 = (OpenedMP3File *)self;

    size_t result;
    gboolean retried = FALSE;

    if (mp3->mpg123_offset != start_pos) {
retry:
        mpg123_seek(mp3->mpg123, start_pos / mp3->hdr.sample_info.blockAlign, SEEK_SET);
        mp3->mpg123_offset = start_pos;
    }

    if (mpg123_read(mp3->mpg123, buf, buf_size, &result) == MPG123_OK) {
        mp3->mpg123_offset += result;
        return result;
    } else {
        g_warning("MP3 decoding failed: %s", mpg123_strerror(mp3->mpg123));
        if (!retried) {
            g_message("Retrying read at %zu...", mp3->mpg123_offset);
            retried = TRUE;
            goto retry;
        }
    }

    return -1;
}

static gboolean
mp3_parse_header(uint32_t header, uint32_t *bitrate, uint32_t *frequency, uint32_t *samples, uint32_t *framesize)
{
    // http://www.datavoyage.com/mpgscript/mpeghdr.htm
    int a = ((header >> 21) & 0x07ff);
    int b = ((header >> 19) & 0x0003);
    int c = ((header >> 17) & 0x0003);
    int e = ((header >> 12) & 0x000f);
    int f = ((header >> 10) & 0x0003);
    int g = ((header >> 9) & 0x0001);

    static const int BITRATES_V1_L2[] = { -1, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, -1 };
    static const int BITRATES_V1_L3[] = { -1, 32, 40, 48, 56, 64, 80,  96, 112, 128, 160, 192, 224, 256, 320, -1 };
    static const int FREQUENCIES[] = { 44100, 48000, 32000, 0 };

    static const int LAYER_II = 0x2;  /* 0b10 */
    static const int LAYER_III = 0x1; /* 0b01 */

    if (a != 0x7ff /* sync */ ||
            b != 0x3 /* MPEG1 */ ||
            (c != LAYER_II && c != LAYER_III) ||
            e == 0x0 /* freeform bitrate */ || e == 0xf /* invalid bitrate */ ||
            f == 0x3 /* invalid frequency */) {
        return FALSE;
    }

    *bitrate = (c == LAYER_III) ? BITRATES_V1_L3[e] : BITRATES_V1_L2[e];
    *frequency = FREQUENCIES[f];
    *samples = 1152;
    *framesize = (int)((*samples) / 8 * 1000 * (*bitrate) / (*frequency)) + g /* padding */;

#if defined(WAVBREAKER_MP3_DEBUG)
    static const char *VERSIONS[] = { "MPEG 2.5", NULL, "MPEG 2", "MPEG 1" };
    static const char *LAYERS[] = { NULL, "III", "II", "I" };

    const char *mpeg_version = VERSIONS[b];
    const char *layer = LAYERS[c];

    g_debug("MP3 frame: %s %s, %dHz, %dkbps, %d samples (%d bytes)", mpeg_version, layer,
            *frequency, *bitrate, *samples, *framesize);
#endif /* WAVBREAKER_MP3_DEBUG */

    return TRUE;
}

int
mp3_write_file(OpenedAudioFile *self, const char *output_filename, unsigned long start_pos, unsigned long end_pos, report_progress_func report_progress, void *report_progress_user_data)
{
    OpenedMP3File *mp3 = (OpenedMP3File *)self;

    start_pos /= mp3->hdr.sample_info.blockSize;
    end_pos /= mp3->hdr.sample_info.blockSize;

    uint32_t start_samples = start_pos * mp3->hdr.sample_info.samplesPerSec / CD_BLOCKS_PER_SEC;
    uint32_t end_samples = end_pos * mp3->hdr.sample_info.samplesPerSec / CD_BLOCKS_PER_SEC;

    if (end_samples == 0) {
        end_samples = mp3->hdr.sample_info.numBytes / mp3->hdr.sample_info.blockAlign;
    }

    FILE *output_file = fopen(output_filename, "wb");

    if (!output_file) {
        g_warning("Could not open '%s' for writing", output_filename);
        return -1;
    }

    fseek(mp3->hdr.fp, 0, SEEK_SET);

    report_progress(0.0, report_progress_user_data);

    uint32_t header = 0x00000000;
    uint32_t sample_position = 0;
    uint32_t file_offset = 0;
    uint32_t last_frame_end = 0;
    uint32_t frames_written = 0;

    while (!feof(mp3->hdr.fp)) {
        fseek(mp3->hdr.fp, file_offset, SEEK_SET);

        int a = fgetc(mp3->hdr.fp);
        if (a == EOF) {
            break;
        }

        header = ((header & 0xffffff) << 8) | (a & 0xff);

        uint32_t bitrate = 0;
        uint32_t frequency = 0;
        uint32_t samples = 0;
        uint32_t framesize = 0;

        if (mp3_parse_header(header, &bitrate, &frequency, &samples, &framesize)) {
            uint32_t frame_start = file_offset - 3;

            if (last_frame_end < frame_start) {
                g_warning("Skipped non-frame data in MP3 @ 0x%08x (%d bytes)",
                        last_frame_end, frame_start - last_frame_end);
            }

            if (start_samples <= sample_position) {
                // Write this frame to the output file
                char *buf = malloc(framesize);
                fseek(mp3->hdr.fp, frame_start, SEEK_SET);
                if (fread(buf, 1, framesize, mp3->hdr.fp) != framesize) {
                    g_warning("Tried to read over the end of the input file");
                    break;
                }
                if (fwrite(buf, 1, framesize, output_file) != framesize) {
                    g_warning("Failed to write %d bytes to output file", framesize);
                    break;
                }
                free(buf);

                frames_written++;

                report_progress((double)(sample_position - start_samples) / (double)(end_samples - start_samples), report_progress_user_data);

                if (end_samples <= sample_position + samples) {
                    // Done writing this part
                    break;
                }
            }

            sample_position += samples;

            file_offset = frame_start + framesize;
            last_frame_end = file_offset;

            header = 0x00000000;
        } else {
            file_offset++;
        }
    }

    report_progress(1.0, report_progress_user_data);

#if defined(WAVBREAKER_MP3_DEBUG)
    g_debug("Wrote %d MP3 frames from '%s' to '%s'", frames_written, self->filename, output_filename);
#endif /* WAVBREAKER_MP3_DEBUG */

    fclose(output_file);

    return 0;
}

static void
mp3_close_file(const FormatModule *self, OpenedAudioFile *file)
{
    OpenedMP3File *mp3 = (OpenedMP3File *)file;

    opened_audio_file_close(&mp3->hdr);
    mpg123_close(g_steal_pointer(&mp3->mpg123));
    g_free(mp3);
}

static OpenedAudioFile *
mp3_open_file(const FormatModule *self, const char *filename, char **error_message)
{
    /* This format module supports MP3 files (default extension) and MP2 file */
    if (!format_module_filename_extension_check(self, filename, NULL) &&
            !format_module_filename_extension_check(self, filename, ".mp2")) {
        format_module_set_error_message(error_message, "File extension is not .mp3/.mp2");
        return NULL;
    }

    OpenedMP3File *mp3 = g_new0(OpenedMP3File, 1);

    if (!format_module_open_file(self, &mp3->hdr, filename, error_message)) {
        g_free(mp3);
        return NULL;
    }

    SampleInfo *si = &mp3->hdr.sample_info;

    mp3->mpg123_offset = 0;

    if ((mp3->mpg123 = mpg123_new(NULL, NULL)) == NULL) {
        format_module_set_error_message(error_message, "Failed to create MP3 decoder");
        goto error;
    }

    if (mpg123_open(mp3->mpg123, mp3->hdr.filename) == MPG123_OK) {
        g_debug("Detected MP3 format");

        long rate;
        int channels;
        int encoding;
        if (mpg123_getformat(mp3->mpg123, &rate, &channels, &encoding) != MPG123_OK ) {
            format_module_set_error_message(error_message, "Could not get MP3 file format");
            goto error;
        }

        g_debug("Scanning MP3 file...");
        if (mpg123_scan(mp3->mpg123) != MPG123_OK) {
            format_module_set_error_message(error_message, "Failed to scan MP3");
            goto error;
        }

        struct mpg123_frameinfo fi;
        memset(&fi, 0, sizeof(fi));

        if (mpg123_info(mp3->mpg123, &fi) == MPG123_OK) {
            si->channels = (fi.mode == MPG123_M_MONO) ? 1 : 2;
            si->samplesPerSec = fi.rate;
            si->bitsPerSample = 16;

            si->blockAlign = si->channels * (si->bitsPerSample / 8);
            si->avgBytesPerSec = si->blockAlign * si->samplesPerSec;
            si->blockSize = si->avgBytesPerSec / CD_BLOCKS_PER_SEC;
            si->numBytes = mpg123_length(mp3->mpg123) * si->blockAlign;
            g_debug("Channels: %d, rate: %d, bits: %d, decoded size: %lu",
                    si->channels, si->samplesPerSec,
                    si->bitsPerSample, si->numBytes);

            const char *mpeg_version = "?";
            switch (fi.version) {
                case MPG123_1_0: mpeg_version = "1"; break;
                case MPG123_2_0: mpeg_version = "2"; break;
                case MPG123_2_5: mpeg_version = "2.5"; break;
                default: break;
            }

            const char *mode = "?";
            switch (fi.mode) {
                case MPG123_M_STEREO: mode = "Stereo"; break;
                case MPG123_M_JOINT: mode = "Joint Stereo"; break;
                case MPG123_M_DUAL: mode = "Dual Channel"; break;
                case MPG123_M_MONO: mode = "Mono"; break;
            }

            const char *layer = "?";
            switch (fi.layer) {
                case 1: layer = "I"; break;
                case 2: layer = "II"; break;
                case 3: layer = "III"; break;
            }

            mp3->hdr.details = g_strdup_printf("MPEG-%s Layer %s, %s, %d kbps", mpeg_version, layer, mode, fi.bitrate);

            mpg123_format_none(mp3->mpg123);
            if (mpg123_format(mp3->mpg123, si->samplesPerSec,
                              (si->channels == 1) ? MPG123_MONO : MPG123_STEREO,
                              MPG123_ENC_SIGNED_16) != MPG123_OK) {
                format_module_set_error_message(error_message, "Failed to set mpg123 format");
                goto error;
            }
        }
    } else {
        format_module_set_error_message(error_message, "mpg123_open() failed");
        goto error;
    }

    return &mp3->hdr;

error:
    mp3_close_file(self, &mp3->hdr);

    return NULL;
}

static const FormatModule
MP3_FORMAT_MODULE = {
    .name = "MPEG Audio Layer I/II/III",
    .library_name = "libmpg123",
    .default_file_extension = ".mp3",

    .open_file = mp3_open_file,
    .close_file = mp3_close_file,

    .read_samples = mp3_read_samples,
    .write_file = mp3_write_file,
};

const FormatModule *
format_module_mp3(void)
{
    static gboolean mpg123_inited = FALSE;
    if (!mpg123_inited) {
        if (mpg123_init() != MPG123_OK) {
            g_warning("Failed to initialize libmpg123");
            return NULL;
        }

        mpg123_inited = TRUE;
    }

    return &MP3_FORMAT_MODULE;
}

#else

const FormatModule *
format_module_mp3(void)
{
    return NULL;
}

#endif /* HAVE_MPG123 */
