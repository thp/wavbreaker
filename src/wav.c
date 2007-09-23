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
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include "wav.h"
#include "gettext.h"

unsigned long wavDataPtr;
unsigned long wavDataSize;
long x;

#define ERROR_MESSAGE_SIZE 1024
static char error_message[ERROR_MESSAGE_SIZE];

#define CHUNK_ERROR_MESSAGE (_("Error reading chunk. Maybe the wave file you are trying to load is truncated?"))

char *wav_get_error_message()
{
    return error_message;
}

void wav_set_error_message(const char *val)
{
    strncpy(error_message, val, ERROR_MESSAGE_SIZE);
}

int wav_read_header(char *sample_file, SampleInfo *sampleInfo, int debug)
{
    WaveHeader wavHdr;
    ChunkHeader chunkHdr;
    FormatChunk fmtChunk;
    char str[128];
    FILE *fp;
    struct stat statBuf;

    /**
     * This is needed for RAW audio
     * and also lets us check if the file size in the header
     * is correct (or the wave file is truncated, in which 
     * case we are going to use the real file size instead).
     **/
    wavDataPtr = 0;
    if (stat(sample_file, &statBuf)) {
	printf("error stat'ing %s\n", sample_file);
    } else {
        wavDataSize = statBuf.st_size;
    }

    /* DEBUG CODE START */
    if (debug) {
        printf("WaveHeader Size:\t%u\n", sizeof(WaveHeader));
        printf("ChunkHeader Size:\t%u\n", sizeof(ChunkHeader));
        printf("FormatChunk Size:\t%u\n", sizeof(FormatChunk));
    }
    /* DEBUG CODE END */

    if ((fp = fopen(sample_file, "rb")) == NULL) {
        snprintf(error_message, ERROR_MESSAGE_SIZE, _("Cannot open %s: %s"), sample_file, strerror( errno));
        return 1;
    }

    /* read in file header */

    if (fread(&wavHdr, sizeof(WaveHeader), 1, fp) < 1) {
        snprintf(error_message, ERROR_MESSAGE_SIZE, _("Cannot read wave header."));
        fclose(fp);
        return 1;
    }

    if (memcmp(wavHdr.riffID, RiffID, 4) && memcmp(wavHdr.wavID, WaveID, 4)) {
        snprintf(error_message, ERROR_MESSAGE_SIZE, _("%s is not a wave file."), sample_file);
        fclose(fp);
        return 1;
    }

    /* DEBUG CODE START */
    if (debug) {
        memcpy(str, wavHdr.riffID, 4);
        memcpy(str+4, "\0", 1);
        printf("RIFF ID:\t%s\n", str);
    
        printf("Total Size:\t%u\n", wavHdr.totSize);
    
        memcpy(str, wavHdr.wavID, 4);
        memcpy(str+4, "\0", 1);
        printf("Wave ID:\t%s\n", str);
    }
    /* DEBUG CODE END */

    /* read in format chunk header */

    if (fread(&chunkHdr, sizeof(ChunkHeader), 1, fp) < 1) {
        snprintf(error_message, ERROR_MESSAGE_SIZE, CHUNK_ERROR_MESSAGE);
        fclose(fp);
        return 1;
    }

    while (memcmp(chunkHdr.chunkID, FormatID, 4)) {
        if (debug) {
            memcpy(str, chunkHdr.chunkID, 4);
            memcpy(str+4, "\0", 1);

            printf("Chunk ID:\t%s\n", str);
            printf("Chunk Size:\t%u\n", chunkHdr.chunkSize);
        }

        memcpy(str, chunkHdr.chunkID, 4);
        memcpy(str+4, "\0", 1);
        printf("Chunk %s is not a Format Chunk\n", str);

        if (fseek(fp, chunkHdr.chunkSize, SEEK_CUR)) {
            snprintf(error_message, ERROR_MESSAGE_SIZE, _("Error seeking to %u in %s: %s"), chunkHdr.chunkSize, sample_file, strerror( errno));
            fclose(fp);
            return 1;
        }

        if (fread(&chunkHdr, sizeof(ChunkHeader), 1, fp) < 1) {
            snprintf(error_message, ERROR_MESSAGE_SIZE, CHUNK_ERROR_MESSAGE);
            fclose(fp);
            return 1;
        }
    }

    if (debug) {
        memcpy(str, chunkHdr.chunkID, 4);
        memcpy(str+4, "\0", 1);

        printf("Chunk ID:\t%s\n", str);
        printf("Chunk Size:\t%u\n", chunkHdr.chunkSize);
    }

    /* read in format chunk data */

    if (fread(&fmtChunk, sizeof(FormatChunk), 1, fp) < 1) {
        snprintf(error_message, ERROR_MESSAGE_SIZE, "Error reading format chunk: %s", strerror( errno));
        fclose(fp);
        return 1;
    }

    if (debug) {
        printf("Compression format is of type: %d\n", fmtChunk.wFormatTag);
        printf("Channels:\t%d\n", fmtChunk.wChannels);
        printf("Sample Rate:\t%u\n", fmtChunk.dwSamplesPerSec);
        printf("Bytes / Sec:\t%u\n", fmtChunk.dwAvgBytesPerSec);
        printf("wBlockAlign:\t%u\n", fmtChunk.wBlockAlign);
        printf("Bits Per Sample Point:\t%u\n", fmtChunk.wBitsPerSample);
    }
    /* DEBUG CODE END */

    if (fmtChunk.wFormatTag != 1) {
        snprintf(error_message, ERROR_MESSAGE_SIZE, _("Loading compressed wave data is not supported."));
        fclose(fp);
        return 1;
    }
    sampleInfo->channels        = fmtChunk.wChannels;
    sampleInfo->samplesPerSec    = fmtChunk.dwSamplesPerSec;
    sampleInfo->avgBytesPerSec    = fmtChunk.dwAvgBytesPerSec;
    sampleInfo->blockAlign        = fmtChunk.wBlockAlign;
    sampleInfo->bitsPerSample    = fmtChunk.wBitsPerSample;

    // if we have a FormatChunk that is larger than standard size, skip over extra data
    if (chunkHdr.chunkSize > sizeof(FormatChunk)) {
printf("in size compare\n");
        if (fseek(fp, chunkHdr.chunkSize - sizeof(FormatChunk), SEEK_CUR)) {
            snprintf(error_message, ERROR_MESSAGE_SIZE, _("Error seeking to %u in %s: %s"), chunkHdr.chunkSize, sample_file, strerror( errno));
            fclose(fp);
            return 1;
        }
    }

    /* DEBUG CODE START */
    /* read in wav data header */

    if (fread(&chunkHdr, sizeof(ChunkHeader), 1, fp) < 1) {
        snprintf(error_message, ERROR_MESSAGE_SIZE, CHUNK_ERROR_MESSAGE);
        fclose(fp);
        return 1;
    }

    while (memcmp(chunkHdr.chunkID, WaveDataID, 4)) {
        memcpy(str, chunkHdr.chunkID, 4);
        memcpy(str + 4, "\0", 1);
        printf("Chunk %s is not a Format Chunk\n", str);

        if (fseek(fp, chunkHdr.chunkSize, SEEK_CUR)) {
            snprintf(error_message, ERROR_MESSAGE_SIZE, _("Error seeking to %u in %s: %s"), chunkHdr.chunkSize, sample_file, strerror( errno));
            fclose(fp);
            return 1;
        }

        if (fread(&chunkHdr, sizeof(ChunkHeader), 1, fp) < 1) {
            snprintf(error_message, ERROR_MESSAGE_SIZE, CHUNK_ERROR_MESSAGE);
            fclose(fp);
            return 1;
        }
    }

    if ((x = ftell(fp)) >= 0) {
        wavDataPtr = x;
    }

    /**
     * If we got a truncated wave file, we will not 
     * use the header's size info here, but use the 
     * real file size, minus the header's size.
     ***/
    if( chunkHdr.chunkSize > wavDataSize) {
        printf( "Warning: Real file size is %lu, but wave header "
                "says it should be %d. Using real file size instead.\n",
                wavDataSize, chunkHdr.chunkSize);
        wavDataSize = wavDataSize - wavDataPtr;
    } else {
        wavDataSize = chunkHdr.chunkSize;
    }
    sampleInfo->numBytes = wavDataSize;

    /* DEBUG CODE START */
    if (debug) {
        printf("wavDataPtr: %lu\n", wavDataPtr);
        printf("wavDataSize: %lu\n", wavDataSize);
    }
    /* DEBUG CODE END */

    fclose(fp);
    return 0;
}

int
wav_read_sample(FILE *fp,
                unsigned char *buf,
                int buf_size,
                unsigned long start_pos)
{
    int ret;

    if (fseek(fp, start_pos + wavDataPtr, SEEK_SET)) {
        return -1;
    }

    /* DEBUG CODE START */
    /*
    printf("start_pos: %lu\t", start_pos);
    */
    /* DEBUG CODE END */

    if (start_pos > wavDataSize) {
        return -1;
    } else if (start_pos + buf_size > wavDataSize) {
        buf_size = wavDataSize - start_pos;
        ret = fread(buf, 1, buf_size, fp);
    } else {
        ret = fread(buf, 1, buf_size, fp);
    }

    /* DEBUG CODE START */
    /*
    printf("start_pos: %lu\t", start_pos);
    printf("buf_size: %d\n", buf_size);
    */
    /* DEBUG CODE END */

    return ret;
}

static int
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
wav_write_file(FILE *fp,
               char *filename,
               int buf_size,
               SampleInfo *sample_info,
               unsigned long start_pos,
               unsigned long end_pos,
               double *pct_done)
{
    int ret;
    FILE *new_fp;
    unsigned long cur_pos, num_bytes;
    unsigned char buf[buf_size];

    if ((new_fp = fopen(filename, "wb")) == NULL) {
        printf("error opening %s for writing\n", filename);
        return -1;
    }

    if (start_pos > wavDataSize) {
        fclose(new_fp);
        return -1;
    }

    start_pos += wavDataPtr;
    if (end_pos != 0) {
        end_pos += wavDataPtr;
        num_bytes = end_pos - start_pos;
    } else {
        num_bytes = wavDataSize + wavDataPtr - start_pos;
    }
    cur_pos = start_pos;

    if ((wav_write_file_header(new_fp, sample_info, num_bytes)) != 0) {
        fclose(new_fp);
        return -1;
    }

    if (fseek(fp, cur_pos, SEEK_SET)) {
        fclose(new_fp);
        return -1;
    }

    /* DEBUG CODE START */
    /*
    printf("\nstart_pos: %lu\n", start_pos);
    printf("end_pos: %lu\n", end_pos);
    printf("cur_pos: %lu\n", cur_pos);
    */
    /* DEBUG CODE END */

    /*
    if (cur_pos + buf_size > end_pos && end_pos != 0) {
        buf_size = end_pos - cur_pos;
    }
    */

    while ((ret = fread(buf, 1, buf_size, fp)) > 0 &&
                (cur_pos < end_pos || end_pos == 0)) {

        if ((fwrite(buf, 1, ret, new_fp)) < ret) {
            printf("error writing to file %s\n", filename);
            fclose(new_fp);
            return -1;
        }
        cur_pos += ret;
        *pct_done = (double) (cur_pos - start_pos) / num_bytes;

        /*
        if (cur_pos + buf_size > end_pos && end_pos != 0) {
            buf_size = end_pos - cur_pos;
        }
        */
    }

    /* DEBUG CODE START */
    /*
    printf("cur_pos: %lu\n", cur_pos);
    printf("buf_size: %d\n", buf_size);
    printf("ret: %d\n", ret);
    printf("num_bytes: %lu\n", num_bytes);
    printf("done writing - %s\n", filename);
    */
    /* DEBUG CODE END */

    fclose(new_fp);

    *pct_done = 1.0;

    return ret;
}

int
wav_merge_files(char *filename,
                int num_files,
                char *filenames[],
                int buf_size,
                WriteInfo *write_info)
{
    int i;
    int ret = 0;
    SampleInfo sample_info[num_files];
    unsigned long data_ptr[num_files];
    FILE *new_fp, *read_fp;
    unsigned long cur_pos, end_pos, num_bytes;
    unsigned char buf[buf_size];

    if( write_info != NULL) {
        write_info->num_files = num_files;
        write_info->cur_file = 0;
        write_info->sync = 0;
        write_info->sync_check_file_overwrite_to_write_progress = 0;
        write_info->check_file_exists = 0;
        write_info->skip_file = -1;
    }

    for (i = 0; i < num_files; i++) {
        wav_read_header(filenames[i], &sample_info[i], 0);
        data_ptr[i] = wavDataPtr;
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
            write_info->cur_filename = strdup(filenames[i]);
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

        while ((ret = fread(buf, 1, buf_size, read_fp)) > 0 &&
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
            free(write_info->cur_filename);
        }
        write_info->cur_filename = NULL;
    }

    fclose(new_fp);

    if( write_info != NULL) {
        write_info->pct_done = 1.0;
    }

    return ret;
}

