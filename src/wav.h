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

#ifndef WAV_H
#define WAV_H

#include "sample.h"
#include "wavbreaker.h"

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

char *wav_get_error_message();

int wav_read_header(char *, SampleInfo *, int);
int wav_read_sample(FILE *, unsigned char *, int, unsigned long);

int
wav_write_file(FILE *fp,
               char *filename,
               int buf_size,
               SampleInfo *sample_info,
               unsigned long start_pos,
               unsigned long end_pos,
			   double *pct_done);

int
wav_merge_files(char *filename,
                int num_files,
                char *filenames[],
                int buf_size,
                WriteInfo *write_info);

#endif /* WAV_H */
