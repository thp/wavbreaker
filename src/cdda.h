#ifndef CDDA_H
#define CDDA_H

#include "sample.h"

int cdda_read_header(const char *, SampleInfo *);
int cdda_read_sample(FILE *, unsigned char *, int, unsigned long);

#endif /* CDDA_H */
