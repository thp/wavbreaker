#ifndef LINUXAUDIO_H
#define LINUXAUDIO_H

#include "sample.h"

void close_device(int);
int open_device(const char *, const SampleInfo *);

#endif /* LINUXAUDIO_H */
