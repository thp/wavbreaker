#ifndef SAMPLE_H
#define SAMPLE_H

#include "wavbreaker.h"

typedef struct {
        unsigned short	channels;
        unsigned long	samplesPerSec;
        unsigned long	avgBytesPerSec;
        unsigned short	blockAlign;
        unsigned short	bitsPerSample;
	unsigned long	numBytes;
} SampleInfo;

void sample_set_audio_dev(char *str);
char * sample_get_audio_dev();

char * sample_get_sample_file();

int play_sample(unsigned long);
void stop_sample();
void sample_open_file(const char *, GraphData *, double *);

#endif /* SAMPLE_H*/
