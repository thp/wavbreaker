#ifndef CDDA_H
#define CDDA_H

#include "sample.h"

int cdda_read_header(const char *filename, SampleInfo *sampleInfo);
int cdda_read_sample(const char *, unsigned char *, int, unsigned long);

#endif /* CDDA_H */
