#ifndef WAV_H
#define WAV_H

#include "sample.h"

#define RiffID "RIFF"
#define WaveID "WAVE"
#define FormatID "fmt "
#define WaveDataID "data"

typedef char ID[4];

typedef struct {
	ID riffID;
	unsigned long totSize;
	ID wavID;
} WaveHeader;

typedef struct {
	ID chunkID;
	long chunkSize;
} ChunkHeader;

typedef struct {
	short wFormatTag;
	unsigned short  wChannels;
	unsigned long   dwSamplesPerSec;
	unsigned long   dwAvgBytesPerSec;
	unsigned short  wBlockAlign;
	unsigned short  wBitsPerSample;
} FormatChunk;

int wav_read_header(FILE *, SampleInfo *);
int wav_read_sample(FILE *, unsigned char *, int, unsigned long);

int
wav_write_file(FILE *fp,
               char *filename,
               int buf_size,
               SampleInfo *sample_info,
               unsigned long start_pos,
               unsigned long end_pos);

#endif /* WAV_H */
