/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2002 Timothy Robinson
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
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <glib.h>

#include "format_wav.h"
#include "gettext.h"

#define RiffID "RIFF"
#define WaveID "WAVE"
#define FormatID "fmt "
#define WaveDataID "data"

typedef char ID[4];

typedef struct {
	ID riffID;
	unsigned int totSize;
	ID wavID;
} WaveHeader;

typedef struct {
	ID chunkID;
	int chunkSize;
} ChunkHeader;

typedef struct {
	short wFormatTag;
	unsigned short  wChannels;
	unsigned int   dwSamplesPerSec;
	unsigned int   dwAvgBytesPerSec;
	unsigned short  wBlockAlign;
	unsigned short  wBitsPerSample;
//	unsigned short  extraNonPcm;
} FormatChunk;


typedef struct OpenedWavFile_ OpenedWavFile;
struct OpenedWavFile_ {
    OpenedAudioFile hdr;

    unsigned long wavDataPtr;
    unsigned long wavDataSize;
};

static void
wav_close_file(const FormatModule *self, OpenedAudioFile *file)
{
    OpenedWavFile *wav = (OpenedWavFile *)file;

    opened_audio_file_close(&wav->hdr);
    g_free(wav);
}

static OpenedAudioFile *
wav_open_file(const FormatModule *self, const char *filename, char **error_message)
{
    const char *CHUNK_ERROR_MESSAGE = _("Error reading chunk. Maybe the wave file you are trying to load is truncated?");

    WaveHeader wavHdr;
    ChunkHeader chunkHdr;
    FormatChunk fmtChunk;
    char str[128];

    OpenedWavFile *wav = g_new0(OpenedWavFile, 1);

    if (!format_module_open_file(self, &wav->hdr, filename, error_message)) {
        g_free(wav);
        return NULL;
    }

    /**
     * This is needed for RAW audio
     * and also lets us check if the file size in the header
     * is correct (or the wave file is truncated, in which
     * case we are going to use the real file size instead).
     **/
    wav->wavDataPtr = 0;
    wav->wavDataSize = wav->hdr.file_size;

    /* read in file header */

    if (fread(&wavHdr, sizeof(WaveHeader), 1, wav->hdr.fp) < 1) {
        format_module_set_error_message(error_message, "%s", _("Cannot read wave header."));
        goto error;
    }

    if (memcmp(wavHdr.riffID, RiffID, 4) && memcmp(wavHdr.wavID, WaveID, 4)) {
        format_module_set_error_message(error_message, _("%s is not a wave file."), wav->hdr.filename);
        goto error;
    }

    /* read in format chunk header */

    if (fread(&chunkHdr, sizeof(ChunkHeader), 1, wav->hdr.fp) < 1) {
        format_module_set_error_message(error_message, "%s", CHUNK_ERROR_MESSAGE);
        goto error;
    }

    while (memcmp(chunkHdr.chunkID, FormatID, 4)) {
        memcpy(str, chunkHdr.chunkID, 4);
        str[4] = '\0';
        g_warning("Chunk %s is not a Format Chunk", str);

        if (fseek(wav->hdr.fp, chunkHdr.chunkSize, SEEK_CUR)) {
            format_module_set_error_message(error_message, _("Error seeking to %u in %s: %s"), chunkHdr.chunkSize, wav->hdr.filename, strerror(errno));
            goto error;
        }

        if (fread(&chunkHdr, sizeof(ChunkHeader), 1, wav->hdr.fp) < 1) {
            format_module_set_error_message(error_message, "%s", CHUNK_ERROR_MESSAGE);
            goto error;
        }
    }

    /* read in format chunk data */

    if (fread(&fmtChunk, sizeof(FormatChunk), 1, wav->hdr.fp) < 1) {
        format_module_set_error_message(error_message, _("Error reading format chunk: %s"), strerror(errno));
        goto error;
    }

    if (fmtChunk.wFormatTag != 1) {
        format_module_set_error_message(error_message, "%s", _("Loading compressed wave data is not supported."));
        goto error;
    }

    wav->hdr.sample_info.channels       = fmtChunk.wChannels;
    wav->hdr.sample_info.samplesPerSec  = fmtChunk.dwSamplesPerSec;
    wav->hdr.sample_info.avgBytesPerSec = fmtChunk.dwAvgBytesPerSec;
    wav->hdr.sample_info.blockAlign     = fmtChunk.wBlockAlign;
    wav->hdr.sample_info.bitsPerSample  = fmtChunk.wBitsPerSample;
    wav->hdr.sample_info.blockSize      = wav->hdr.sample_info.avgBytesPerSec / CD_BLOCKS_PER_SEC;

    // if we have a FormatChunk that is larger than standard size, skip over extra data
    if (chunkHdr.chunkSize > sizeof(FormatChunk)) {
        if (fseek(wav->hdr.fp, chunkHdr.chunkSize - sizeof(FormatChunk), SEEK_CUR)) {
            format_module_set_error_message(error_message, _("Error seeking to %u in %s: %s"), chunkHdr.chunkSize, wav->hdr.filename, strerror(errno));
            goto error;
        }
    }

    /* read in wav data header */

    if (fread(&chunkHdr, sizeof(ChunkHeader), 1, wav->hdr.fp) < 1) {
        format_module_set_error_message(error_message, "%s", CHUNK_ERROR_MESSAGE);
        goto error;
    }

    while (memcmp(chunkHdr.chunkID, WaveDataID, 4)) {
        memcpy(str, chunkHdr.chunkID, 4);
        str[4] = '\0';
        g_warning("Chunk %s is not a Format Chunk", str);

        if (fseek(wav->hdr.fp, chunkHdr.chunkSize, SEEK_CUR)) {
            format_module_set_error_message(error_message, _("Error seeking to %u in %s: %s"), chunkHdr.chunkSize, wav->hdr.filename, strerror(errno));
            goto error;
        }

        if (fread(&chunkHdr, sizeof(ChunkHeader), 1, wav->hdr.fp) < 1) {
            format_module_set_error_message(error_message, "%s", CHUNK_ERROR_MESSAGE);
            goto error;
        }
    }

    long x;
    if ((x = ftell(wav->hdr.fp)) >= 0) {
        wav->wavDataPtr = x;
    }

    /**
     * If we got a truncated wave file, we will not 
     * use the header's size info here, but use the 
     * real file size, minus the header's size.
     ***/
    if (wav->wavDataSize != 0 && chunkHdr.chunkSize > wav->wavDataSize) {
        g_warning("Real file size is %lu, but wave header says it should be %d. Using real file size instead.",
                wav->wavDataSize, chunkHdr.chunkSize);
        wav->wavDataSize = wav->wavDataSize - wav->wavDataPtr;
    } else {
        wav->wavDataSize = chunkHdr.chunkSize;
    }

    wav->hdr.sample_info.numBytes = wav->wavDataSize;

    return &wav->hdr;

error:
    wav_close_file(self, &wav->hdr);

    return NULL;
}

long
wav_read_samples(OpenedAudioFile *self, unsigned char *buf, size_t buf_size, unsigned long start_pos)
{
    OpenedWavFile *wav = (OpenedWavFile *)self;

    if (fseek(wav->hdr.fp, start_pos + wav->wavDataPtr, SEEK_SET)) {
        return -1;
    }

    if (start_pos > wav->wavDataSize) {
        return -1;
    }

    if (start_pos + buf_size > wav->wavDataSize) {
        buf_size = wav->wavDataSize - start_pos;
    }

    return fread(buf, 1, buf_size, wav->hdr.fp);
}

int
wav_write_file(OpenedAudioFile *self, const char *output_filename, unsigned long start_pos, unsigned long end_pos, double *progress)
{
    OpenedWavFile *wav = (OpenedWavFile *)self;

    int buf_size = wav->hdr.sample_info.blockSize;

    int ret;
    FILE *new_fp = NULL;
    unsigned long cur_pos, num_bytes;
    unsigned char *buf = malloc(buf_size);

    if (start_pos > wav->wavDataSize) {
        goto error;
    }

    if ((new_fp = fopen(output_filename, "wb")) == NULL) {
        g_warning("Error opening %s for writing", output_filename);
        goto error;
    }

    start_pos += wav->wavDataPtr;
    if (end_pos != 0) {
        end_pos += wav->wavDataPtr;
        num_bytes = end_pos - start_pos;
    } else {
        num_bytes = wav->wavDataSize + wav->wavDataPtr - start_pos;
    }
    cur_pos = start_pos;

    if ((wav_write_file_header(new_fp, &wav->hdr.sample_info, num_bytes)) != 0) {
        g_message("Could not write WAV header to %s", output_filename);
        goto error;
    }

    if (fseek(wav->hdr.fp, cur_pos, SEEK_SET)) {
        g_message("Could not seek to read position in %s", wav->hdr.filename);
        goto error;
    }

    while ((ret = fread(buf, 1, buf_size, wav->hdr.fp)) > 0 &&
                (cur_pos < end_pos || end_pos == 0)) {
        if ((fwrite(buf, 1, ret, new_fp)) < ret) {
            g_message("Error writing to file %s", output_filename);
            goto error;
        }

        cur_pos += ret;
        *progress = (double) (cur_pos - start_pos) / num_bytes;
    }

    free(buf);
    fclose(new_fp);

    *progress = 1.0;

    return ret;

error:
    if (new_fp != NULL) {
        fclose(new_fp);
    }

    free(buf);
    return -1;
}


static const FormatModule
WAV_FORMAT_MODULE = {
    .name = "RIFF WAVE",
    .library_name = "built-in",
    .default_file_extension = ".wav",

    .open_file = wav_open_file,
    .close_file = wav_close_file,

    .read_samples = wav_read_samples,
    .write_file = wav_write_file,
};

const FormatModule *
format_module_wav()
{
    return &WAV_FORMAT_MODULE;
}


int
wav_write_file_header(FILE *fp,
                      SampleInfo *sample_info,
                      unsigned long num_bytes)
{
    WaveHeader wavHdr;
    ChunkHeader chunkHdr;
    FormatChunk fmtChunk;

    /* Write wave header */
    memcpy(wavHdr.riffID, RiffID, 4);
    wavHdr.totSize = num_bytes + sizeof(ChunkHeader) + sizeof(FormatChunk)
                               + sizeof(ChunkHeader) + 4;
    memcpy(wavHdr.wavID, WaveID, 4);

    if ((fwrite(&wavHdr, sizeof(WaveHeader), 1, fp)) < 1) {
        printf("error writing wave header\n");
        return 1;
    }

    /* Write format chunk header */
    memcpy(chunkHdr.chunkID, FormatID, 4);
    chunkHdr.chunkSize = sizeof(FormatChunk);

    if ((fwrite(&chunkHdr, sizeof(ChunkHeader), 1, fp)) < 1) {
        printf("error writing fmt chunk header\n");
        return 1; 
    }

    /* Write format chunk data */
    fmtChunk.wFormatTag            = 1;
    fmtChunk.wChannels            = sample_info->channels;
    fmtChunk.dwSamplesPerSec    = sample_info->samplesPerSec;
    fmtChunk.dwAvgBytesPerSec    = sample_info->avgBytesPerSec;
    fmtChunk.wBlockAlign        = sample_info->blockAlign;
    fmtChunk.wBitsPerSample        = sample_info->bitsPerSample;

    if (fwrite(&fmtChunk, sizeof(FormatChunk), 1, fp) < 1) {
        printf("error writing format chunk\n");
        return 1;
    }

    /* Write data chunk header */
    memcpy(chunkHdr.chunkID, WaveDataID, 4);
    chunkHdr.chunkSize = num_bytes;

    if ((fwrite(&chunkHdr, sizeof(ChunkHeader), 1, fp)) < 1) {
        printf("error writing data chunk header\n");
        return 1; 
    }

    return 0;
}

int
wav_merge_files(char *filename,
                int num_files,
                char *filenames[],
                WriteInfo *write_info)
{
    int i;
    int ret = 0;
    SampleInfo sample_info[num_files];
    unsigned long data_ptr[num_files];
    FILE *new_fp, *read_fp;
    unsigned long cur_pos, end_pos, num_bytes;
    unsigned char buf[DEFAULT_BUF_SIZE];

    if( write_info != NULL) {
        write_info->num_files = num_files;
        write_info->cur_file = 0;
        write_info->sync = 0;
        write_info->sync_check_file_overwrite_to_write_progress = 0;
        write_info->check_file_exists = 0;
        write_info->skip_file = -1;
    }

    const FormatModule *mod = format_module_wav();

    for (i = 0; i < num_files; i++) {
        char *error_message = NULL;

        OpenedAudioFile *oaf = mod->open_file(mod, filenames[i], &error_message);
        OpenedWavFile *wav = (OpenedWavFile *)oaf;
        if (oaf == NULL) {
            g_warning("Could not read WAV header of %s: %s", filenames[i], error_message);
            g_free(error_message);
            return -1;
        } else {
            sample_info[i] = oaf->sample_info;
            data_ptr[i] = wav->wavDataPtr;
            mod->close_file(mod, oaf);
        }
    }

    num_bytes = sample_info[0].numBytes;

    for (i = 1; i < num_files; i++) {
        if (sample_info[0].channels != sample_info[i].channels) {
            return 1;
        } else if (sample_info[0].samplesPerSec != 
                            sample_info[i].samplesPerSec) {
            return 1;
        } else if (sample_info[0].avgBytesPerSec != 
                            sample_info[i].avgBytesPerSec) {
            return 1;
        } else if (sample_info[0].blockAlign != sample_info[i].blockAlign) {
            return 1;
        } else if (sample_info[0].bitsPerSample != 
                            sample_info[i].bitsPerSample) {
            return 1;
        }

        num_bytes += sample_info[i].numBytes;
    }

    if ((new_fp = fopen(filename, "wb")) == NULL) {
        printf("error opening %s for writing\n", filename);
        return -1;
    }

    if ((wav_write_file_header(new_fp, &sample_info[0], num_bytes)) != 0) {
        fclose(new_fp);
        return -1;
    }

    for (i = 0; i < num_files; i++) {
        if( write_info != NULL) {
            write_info->pct_done = 0.0;
            write_info->cur_file++;
            if (write_info->cur_filename != NULL) {
                free(write_info->cur_filename);
            }
            write_info->cur_filename = g_strdup(filenames[i]);
        }

        if ((read_fp = fopen(filenames[i], "rb")) == NULL) {
            printf("error opening %s for reading\n", filenames[i]);
            fclose(new_fp);
            fclose(read_fp);
            return -1;
        }

        cur_pos = data_ptr[i];
        num_bytes = sample_info[i].numBytes;
        end_pos = cur_pos + num_bytes;

        if (fseek(read_fp, cur_pos, SEEK_SET)) {
            fclose(new_fp);
            fclose(read_fp);
            return -1;
        }

        while ((ret = fread(buf, 1, sizeof(buf), read_fp)) > 0 &&
                           (cur_pos < end_pos)) {

            if ((fwrite(buf, 1, ret, new_fp)) < ret) {
                printf("error writing to file %s\n", filename);
                fclose(new_fp);
                fclose(read_fp);
                return -1;
            }

            if( write_info != NULL) {
                write_info->pct_done = (double) cur_pos / num_bytes;
            }

            cur_pos += ret;
        }

        fclose(read_fp);
    }

    if( write_info != NULL) {
        write_info->sync = 1;
        if (write_info->cur_filename != NULL) {
            g_free(write_info->cur_filename);
        }
        write_info->cur_filename = NULL;
    }

    fclose(new_fp);

    if( write_info != NULL) {
        write_info->pct_done = 1.0;
    }

    return ret;
}


int wav_read_header(char *filename, SampleInfo *sample_info_out, int debug)
{
    const FormatModule *mod = format_module_wav();

    char *error_message = NULL;

    OpenedAudioFile *oaf = mod->open_file(mod, filename, &error_message);
    if (oaf == NULL) {
        g_warning("Could not read WAV header of %s: %s", filename, error_message);
        g_free(error_message);
        return 1;
    }

    *sample_info_out = oaf->sample_info;

    mod->close_file(mod, oaf);

    return 0;
}
