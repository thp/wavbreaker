#ifndef WAV_H
#define WAV_H

#include "sample.h"

int wav_read_header(FILE *, SampleInfo *);
int wav_read_sample(FILE *, unsigned char *, int, unsigned long);

#endif /* WAV_H */
