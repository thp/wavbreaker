#ifndef SAMPLE_H
#define SAMPLE_H

#include "wavbreaker.h"

#define BUF_SIZE 4096
#define BLOCK_SIZE 2352

typedef struct SampleInfo_ SampleInfo;

struct SampleInfo_ {
		unsigned short	channels;
		unsigned long	samplesPerSec;
		unsigned long	avgBytesPerSec;
		unsigned short	blockAlign;
		unsigned short	bitsPerSample;
		unsigned long	numBytes;
};

void sample_set_audio_dev(char *str);
char * sample_get_audio_dev();

char * sample_get_sample_file();

int play_sample(unsigned long);
void stop_sample();
void sample_open_file(const char *, GraphData *, double *);
void sample_write_files(const char *, GList *);

#endif /* SAMPLE_H*/
