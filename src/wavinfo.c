#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include "wav.h"
#include "sample.h"

int main(int argc, char *argv[])
{
	WaveHeader wavHdr;
	ChunkHeader chunkHdr;
	FormatChunk fmtChunk;
	char str[128];
	FILE *fp;
	int i;

	if (argc < 2) {
		printf("must pass filename of wave file.\n");
		return 1;
	}

	for (i = 1; i < argc; i++) {
		printf("Header info for: %s\n", argv[i]);

		SampleInfo sampleInfo;
		wav_read_header(argv[i], &sampleInfo, 1);
		printf("\n");
	}

	return 0;
}
