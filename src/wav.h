#ifndef WAV_H
#define WAV_H

#include "sample.h"

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
