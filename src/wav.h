#ifndef WAV_H
#define WAV_H

#include "sample.h"

int wav_read_header(const char *, SampleInfo *);
int wav_read_sample(const char *, unsigned char *, int, unsigned long);

#endif /* WAV_H */
