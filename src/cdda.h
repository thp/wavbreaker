#ifndef CDDA_H
#define CDDA_H

#include "sample.h"

int cdda_read_header(const char *, SampleInfo *);
int cdda_read_sample(FILE *, unsigned char *, int, unsigned long);
int
cdda_write_file(FILE *fp,
                char *filename,
                int buf_size,
                unsigned long start_pos,
                unsigned long end_pos);

#endif /* CDDA_H */
