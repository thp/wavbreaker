#include <stdio.h>
#include <string.h>
#include "sample.h"
#include "wav.h"
#include "wavmerge.h"

int main(int argc, char *argv[])
{
	int ret;

	if (argc < 3) {
		printf("Must pass filenames of wave files to merge.\n");
		printf("Usage: wavmerge [-o outfile] mergefiles...");
		return 1;
	}

	if (strcmp(argv[1], "-o") == 0) {
		ret = wav_merge_files(argv[2], argc - 3, &argv[3], BUF_SIZE);
	} else {
		ret = wav_merge_files("merged.wav", argc - 1, &argv[1], BUF_SIZE);
	}

	if (ret != 0) {
		return 1;
	}

	return 0;
}
