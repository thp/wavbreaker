#include <stdio.h>
#include <sys/stat.h>
#include "cdda.h"

unsigned long file_size;

int
cdda_read_header(const char *filename,
		 SampleInfo *sampleInfo)
{
	struct stat statBuf;

	if (stat(filename, &statBuf)) {
		printf("error stat'ing %s\n", filename);
		return -1;
	}

	file_size = statBuf.st_size;

	sampleInfo->numBytes = statBuf.st_size;
	sampleInfo->channels = 2; 
	sampleInfo->samplesPerSec = 44100;
	sampleInfo->bitsPerSample = 16;

	return 0;
}

int
cdda_read_sample(const char	*filename,
		 unsigned char	*buf,
		 int		buf_size,
		 unsigned long	start_pos)
{
	FILE *fp;
	int i = 0;
	int ret;

	if ((fp = fopen(filename, "r")) == NULL) {
		printf("error opening %s\n", filename);
		return -1;
	}

	if (fseek(fp, start_pos, SEEK_SET)) {
		return -1;
	}

	if (start_pos > file_size) {
		return -1;
	}

	ret = fread(buf, 1, buf_size, fp);

	for (i = 0; i < ret / 4; i++) {
		unsigned char tmp;

		/* left channel */
		tmp = buf[i*4+0];
		buf[i*4+0] = buf[i*4+1];
		buf[i*4+1] = tmp;

		/* right channel */
		tmp = buf[i*4+2];
		buf[i*4+2] = buf[i*4+3];
		buf[i*4+3] = tmp;
	}

	fclose(fp);
	return ret;
}
