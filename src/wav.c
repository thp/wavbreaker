#include <stdio.h>
#include "wav.h"


#define RiffID "RIFF"
#define WaveID "WAVE"
#define FormatID "fmt "
#define WaveDataID "data"

typedef char ID[4];

typedef struct {
	ID		riffID;
	unsigned long	totSize;
	ID		wavID;
} WaveHeader;

typedef struct {
	ID		chunkID;
	long		chunkSize;
} ChunkHeader;

typedef struct {
	short		wFormatTag;
	unsigned short	wChannels;
	unsigned long	dwSamplesPerSec;
	unsigned long	dwAvgBytesPerSec;
	unsigned short	wBlockAlign;
	unsigned short	wBitsPerSample;
} FormatChunk;

unsigned long wavDataPtr;
unsigned long wavDataSize;
long x;

int wav_read_header(FILE *fp, SampleInfo *sampleInfo)
{
	WaveHeader wavHdr;
	ChunkHeader chunkHdr;
	FormatChunk fmtChunk;
	char str[128];

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

	memcpy(str, chunkHdr.chunkID, 4);
	memcpy(str+4, "\0", 1);

/*
	printf("Chunk ID:\t%s\n", str);
	printf("Chunk Size:\t%lu\n", chunkHdr.chunkSize);

	printf("Channels:\t%d\n", fmtChunk.wChannels);
	printf("Sample Rate:\t%lu\n", fmtChunk.dwSamplesPerSec);
	printf("Bytes / Sec:\t%lu\n", fmtChunk.dwAvgBytesPerSec);
	printf("Bytes / Sec calculated:\t%lu\n", fmtChunk.dwSamplesPerSec *
		fmtChunk.wBlockAlign);
	printf("wBlockAlign:\t%u\n", fmtChunk.wBlockAlign);
	printf("wBlockAlign calculated:\t%u\n", fmtChunk.wChannels *
		fmtChunk.wBitsPerSample / 8);
	printf("Bits Per Sample Point:\t%u\n", fmtChunk.wBitsPerSample);
*/

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

/*
	printf("wavDataPtr: %lu\n", wavDataPtr);
	printf("wavDataSize: %lu\n", wavDataSize);
*/

	return 0;
}

int
wav_read_sample(FILE		*fp,
		unsigned char	*buf,
		int		buf_size,
		unsigned long	start_pos)
{
	int ret;

	if (fseek(fp, start_pos + wavDataPtr, SEEK_SET)) {
		return -1;
	}

	if (start_pos > wavDataSize) {
		return -1;
	} else if (start_pos + wavDataPtr > wavDataSize) {
		buf_size = wavDataSize - start_pos + wavDataPtr;
		ret = fread(buf, 1, buf_size, fp);
	} else {
		ret = fread(buf, 1, buf_size, fp);
	}

	return ret;
}
