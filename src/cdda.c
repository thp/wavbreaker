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
cdda_read_sample(FILE *fp,
                 unsigned char *buf,
                 int buf_size,
                 unsigned long start_pos)
{
	int i = 0;
	int ret;

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

	return ret;
}

int
cdda_write_file(FILE *fp,
                char *filename,
                int buf_size,
                unsigned long start_pos,
                unsigned long end_pos)
{
	int ret;
	FILE *new_fp;
	unsigned long cur_pos;
	unsigned char buf[buf_size];

	if ((new_fp = fopen(filename, "w")) == NULL) {
		printf("error opening %s for writing\n", filename);
		return -1;
	}

	cur_pos = start_pos;
	
	if (fseek(fp, cur_pos, SEEK_SET)) {
		return -1;
	}

	if (cur_pos > file_size) {
		return -1;
	}

printf("start_pos: %lu\n", start_pos);
printf("end_pos: %lu\n", end_pos);
printf("cur_pos: %lu\n", cur_pos);

	while ((ret = fread(buf, 1, buf_size, fp)) == buf_size &&
				(cur_pos < end_pos || end_pos == 0)) {

		if (cur_pos + buf_size > end_pos && end_pos != 0) {
			buf_size = end_pos - cur_pos;
		}
		if ((fwrite(buf, 1, buf_size, new_fp)) < buf_size) {
			printf("error writing to file %s\n", filename);
			return -1;
		}
		cur_pos += buf_size;
	}

printf("start_pos: %lu\n", start_pos);
printf("end_pos: %lu\n", end_pos);
printf("cur_pos: %lu\n", cur_pos);
printf("done writing - %s\n", filename);
	return ret;
}
