#ifndef WAVBREAKER_H
#define WAVBREAKER_H

typedef struct {
        int min, max;
} Points;

typedef struct {
	unsigned long numSamples;
	unsigned long maxSampleValue;
	Points *data;
} GraphData;

#endif /* WAVBREAKER_H */
