#include <stdio.h>
#include <string.h>
#include "wav.h"

unsigned long wavDataPtr;
unsigned long wavDataSize;
long x;

int wav_read_header(FILE *fp, SampleInfo *sampleInfo)
{
	WaveHeader wavHdr;
	ChunkHeader chunkHdr;
	FormatChunk fmtChunk;
	char str[128];

	/* DEBUG CODE START */
	printf("WaveHeader Size:\t%u\n", sizeof(WaveHeader));
	printf("ChunkHeader Size:\t%u\n", sizeof(ChunkHeader));
	printf("FormatChunk Size:\t%u\n", sizeof(FormatChunk));
	/* DEBUG CODE END */

	/* read in file header */

	if (fread(&wavHdr, sizeof(WaveHeader), 1, fp) < 1) {
		printf("error reading wave header\n");
		fclose(fp);
		return 1;
	}

	if (memcmp(wavHdr.riffID, RiffID, 4) &&
		memcmp(wavHdr.wavID, WaveID, 4)) {

		printf("?? is not a wave file\n");
		fclose(fp);
		return 1;
	}

	/* DEBUG CODE START */
	memcpy(str, wavHdr.riffID, 4);
	memcpy(str+4, "\0", 1);
	printf("RIFF ID:\t%s\n", str);

	printf("Total Size:\t%lu\n", wavHdr.totSize);

	memcpy(str, wavHdr.wavID, 4);
	memcpy(str+4, "\0", 1);
	printf("Wave ID:\t%s\n", str);
	/* DEBUG CODE END */

	/* read in format chunk header */

	if (fread(&chunkHdr, sizeof(ChunkHeader), 1, fp) < 1) {
		printf("error reading chunk\n");
		return 1;
	}

	while (memcmp(chunkHdr.chunkID, FormatID, 4)) {
		memcpy(str, chunkHdr.chunkID, 4);
		memcpy(str+4, "\0", 1);
		printf("Chunk %s is not a Format Chunk\n", str);

		if (fseek(fp, chunkHdr.chunkSize, SEEK_CUR)) {
			printf("error seeking to %lu in file",
				chunkHdr.chunkSize);
			return 1;
		}

		if (fread(&chunkHdr, sizeof(ChunkHeader), 1, fp) < 1) {
			printf("error reading chunk\n");
			return 1;
		}
	}

	/* read in format chunk data */

	if (fread(&fmtChunk, sizeof(FormatChunk), 1, fp) < 1) {
		printf("error reading format chunk\n");
		return 1;
	}

	if (fmtChunk.wFormatTag != 1) {
		printf("Compressed wave data not supported\n");
		return 1;
	}

	/* DEBUG CODE START */
	memcpy(str, chunkHdr.chunkID, 4);
	memcpy(str+4, "\0", 1);

	printf("Chunk ID:\t%s\n", str);
	printf("Chunk Size:\t%lu\n", chunkHdr.chunkSize);

	printf("Channels:\t%d\n", fmtChunk.wChannels);
	printf("Sample Rate:\t%lu\n", fmtChunk.dwSamplesPerSec);
	printf("Bytes / Sec:\t%lu\n", fmtChunk.dwAvgBytesPerSec);
	printf("wBlockAlign:\t%u\n", fmtChunk.wBlockAlign);
	printf("Bits Per Sample Point:\t%u\n", fmtChunk.wBitsPerSample);
	/* DEBUG CODE END */

	sampleInfo->channels		= fmtChunk.wChannels;
	sampleInfo->samplesPerSec	= fmtChunk.dwSamplesPerSec;
	sampleInfo->avgBytesPerSec	= fmtChunk.dwAvgBytesPerSec;
	sampleInfo->blockAlign		= fmtChunk.wBlockAlign;
	sampleInfo->bitsPerSample	= fmtChunk.wBitsPerSample;

	/* read in wav data header */

	if (fread(&chunkHdr, sizeof(ChunkHeader), 1, fp) < 1) {
		printf("error reading chunk\n");
		return 1;
	}

	while (memcmp(chunkHdr.chunkID, WaveDataID, 4)) {
		memcpy(str, chunkHdr.chunkID, 4);
		memcpy(str + 4, "\0", 1);
		printf("Chunk %s is not a Format Chunk\n", str);

		if (fseek(fp, chunkHdr.chunkSize, SEEK_CUR)) {
			printf("error seeking to %lu in file",
				chunkHdr.chunkSize);
			return 1;
		}

		if (fread(&chunkHdr, sizeof(ChunkHeader), 1, fp) < 1) {
			printf("error reading chunk\n");
			return 1;
		}
	}

	if ((x = ftell(fp)) >= 0) {
		wavDataPtr = x;
	}

	wavDataSize = chunkHdr.chunkSize;
	sampleInfo->numBytes = chunkHdr.chunkSize;

	/* DEBUG CODE START */
	printf("wavDataPtr: %lu\n", wavDataPtr);
	printf("wavDataSize: %lu\n", wavDataSize);
	/* DEBUG CODE END */

	return 0;
}

int
wav_read_sample(FILE *fp,
                unsigned char *buf,
                int buf_size,
                unsigned long start_pos)
{
	int ret;

	if (fseek(fp, start_pos + wavDataPtr, SEEK_SET)) {
		return -1;
	}

	/* DEBUG CODE START */
	/*
	printf("start_pos: %lu\t", start_pos);
	*/
	/* DEBUG CODE END */

	if (start_pos > wavDataSize) {
		return -1;
	} else if (start_pos + buf_size > wavDataSize) {
		buf_size = wavDataSize - start_pos;
		ret = fread(buf, 1, buf_size, fp);
	} else {
		ret = fread(buf, 1, buf_size, fp);
	}

	/* DEBUG CODE START */
	/*
	printf("start_pos: %lu\t", start_pos);
	printf("buf_size: %d\n", buf_size);
	*/
	/* DEBUG CODE END */

	return ret;
}

static int
wav_write_file_header(FILE *fp,
                      SampleInfo *sample_info,
                      unsigned long num_bytes)
{
	WaveHeader wavHdr;
	ChunkHeader chunkHdr;
	FormatChunk fmtChunk;

	/* Write wave header */
	memcpy(wavHdr.riffID, RiffID, 4);
	wavHdr.totSize = num_bytes + sizeof(ChunkHeader) + sizeof(FormatChunk)
							   + sizeof(ChunkHeader) + 4;
	memcpy(wavHdr.wavID, WaveID, 4);

	if ((fwrite(&wavHdr, sizeof(WaveHeader), 1, fp)) < 1) {
		printf("error writing wave header\n");
		return 1;
	}

	/* Write format chunk header */
	memcpy(chunkHdr.chunkID, FormatID, 4);
	chunkHdr.chunkSize = sizeof(FormatChunk);

	if ((fwrite(&chunkHdr, sizeof(ChunkHeader), 1, fp)) < 1) {
		printf("error writing fmt chunk header\n");
		return 1; 
	}

	/* Write format chunk data */
	fmtChunk.wFormatTag			= 1;
	fmtChunk.wChannels			= sample_info->channels;
	fmtChunk.dwSamplesPerSec	= sample_info->samplesPerSec;
	fmtChunk.dwAvgBytesPerSec	= sample_info->avgBytesPerSec;
	fmtChunk.wBlockAlign		= sample_info->blockAlign;
	fmtChunk.wBitsPerSample		= sample_info->bitsPerSample;

	if (fwrite(&fmtChunk, sizeof(FormatChunk), 1, fp) < 1) {
		printf("error writing format chunk\n");
		return 1;
	}

	/* Write data chunk header */
	memcpy(chunkHdr.chunkID, WaveDataID, 4);
	chunkHdr.chunkSize = num_bytes;

	if ((fwrite(&chunkHdr, sizeof(ChunkHeader), 1, fp)) < 1) {
		printf("error writing data chunk header\n");
		return 1; 
	}

	return 0;
}

int
wav_write_file(FILE *fp,
               char *filename,
               int buf_size,
               SampleInfo *sample_info,
               unsigned long start_pos,
               unsigned long end_pos)
{
	int ret;
	FILE *new_fp;
	unsigned long cur_pos, num_bytes;
	unsigned char buf[buf_size];

	if ((new_fp = fopen(filename, "w")) == NULL) {
		printf("error opening %s for writing\n", filename);
		return -1;
	}

	if (start_pos > wavDataSize) {
		fclose(new_fp);
		return -1;
	}

	start_pos += wavDataPtr;
	if (end_pos != 0) {
		end_pos += wavDataPtr;
		num_bytes = end_pos - start_pos;
	} else {
		num_bytes = wavDataSize + wavDataPtr - start_pos;
	}
	cur_pos = start_pos;

	if ((wav_write_file_header(new_fp, sample_info, num_bytes)) != 0) {
		fclose(new_fp);
		return -1;
	}

	if (fseek(fp, cur_pos, SEEK_SET)) {
		fclose(new_fp);
		return -1;
	}

	/* DEBUG CODE START */
	printf("\nstart_pos: %lu\n", start_pos);
	printf("end_pos: %lu\n", end_pos);
	printf("cur_pos: %lu\n", cur_pos);
	/* DEBUG CODE END */

	/*
	if (cur_pos + buf_size > end_pos && end_pos != 0) {
		buf_size = end_pos - cur_pos;
	}
	*/

	while ((ret = fread(buf, 1, buf_size, fp)) > 0 &&
				(cur_pos < end_pos || end_pos == 0)) {

		if ((fwrite(buf, 1, ret, new_fp)) < ret) {
			printf("error writing to file %s\n", filename);
			return -1;
		}
		cur_pos += ret;
		/*
		if (cur_pos + buf_size > end_pos && end_pos != 0) {
			buf_size = end_pos - cur_pos;
		}
		*/
	}

	/* DEBUG CODE START */
	printf("cur_pos: %lu\n", cur_pos);
	printf("buf_size: %d\n", buf_size);
	printf("ret: %d\n", ret);
	printf("num_bytes: %lu\n", num_bytes);
	printf("done writing - %s\n", filename);
	/* DEBUG CODE END */

	fclose(new_fp);

	return ret;
}
