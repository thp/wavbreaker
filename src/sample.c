#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <limits.h>
#include <gtk/gtk.h>

#include "wavbreaker.h"
#include "linuxaudio.h"
#include "sample.h"
#include "wav.h"
#include "cdda.h"

#define CDDA 1
#define WAV  2 

static SampleInfo sampleInfo;
static unsigned long sample_start = 0;
static int playing = 0;
static int audio_type;
static int audio_fd;

static char *audio_dev = "/dev/dsp";
static char *sample_file = NULL;
static FILE *sample_fp = NULL;

static pthread_t thread;
static pthread_attr_t thread_attr;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* typedef and struct stuff for new thread open junk */

typedef struct {
	GraphData *graphData;
	double *pct;
} OpenThreadType;

OpenThreadType o;
OpenThreadType *open_thread_data = &o;

static void sample_max_min(GraphData *graphData, double *pct);

void sample_set_audio_dev(char *str)
{
	audio_dev = strdup(str);
}

char * sample_get_audio_dev()
{
	return strdup(audio_dev);
}

char * sample_get_sample_file()
{
	return strdup(sample_file);
}

static void *
play_thread(void *thread_data)
{
	int ret, i;
	unsigned char devbuf[BUF_SIZE];

	if ((audio_fd = open_device(audio_dev, &sampleInfo)) < 0) {
		playing = 0;
		return NULL;
	}

	i = 0;

	if (audio_type == CDDA) {
		ret = cdda_read_sample(sample_fp, devbuf, BUF_SIZE,
			sample_start + (BUF_SIZE * i++));
	} else if (audio_type == WAV) {
		ret = wav_read_sample(sample_fp, devbuf, BUF_SIZE,
			sample_start + (BUF_SIZE * i++));
	}

	while (ret > 0 && ret <= BUF_SIZE) {
	        write(audio_fd, devbuf, ret);

		if (audio_type == CDDA) {
			ret = cdda_read_sample(sample_fp, devbuf, BUF_SIZE,
				sample_start + (BUF_SIZE * i++));
		} else if (audio_type == WAV) {
			ret = wav_read_sample(sample_fp, devbuf, BUF_SIZE,
				sample_start + (BUF_SIZE * i++));
		}
	}

	pthread_mutex_lock(&mutex);

	close_device(audio_fd);
	playing = 0;

	pthread_mutex_unlock(&mutex);

printf("done playing\n");
	return NULL;
}

int play_sample(unsigned long startpos)
{       
	int ret;

	if (playing) {
		return 2;
	}

	if (sample_file == NULL) {
		return 3;
	}

	playing = 1;
	sample_start = startpos * BLOCK_SIZE;

	/* setup thread */

	if ((ret = pthread_mutex_init(&mutex, NULL)) != 0) {
		perror("Return from pthread_mutex_init");
		printf("Error #%d\n", ret);
		return 1;
	}

	if ((ret = pthread_attr_init(&thread_attr)) != 0) {
		perror("Return from pthread_attr_init");
		printf("Error #%d\n", ret);
		return 1;
	}

	if ((ret = pthread_attr_setdetachstate(&thread_attr,
		PTHREAD_CREATE_DETACHED)) != 0) {

		perror("Return from pthread_attr_setdetachstate");
		printf("Error #%d\n", ret);
		return 1;
	}

	if ((ret = pthread_create(&thread, &thread_attr, play_thread,
			NULL)) != 0) {

		perror("Return from pthread_create");
		printf("Error #%d\n", ret);
		return 1;
	}

printf("playing\n");

	return 0;
}               

void stop_sample()
{       
	if (pthread_mutex_trylock(&mutex)) {
		return;
	}

	if (!playing) {
		pthread_mutex_unlock(&mutex);
		return;
	}

	if (pthread_cancel(thread)) {
		perror("Return from pthread_cancel");
	        printf("trouble cancelling the thread\n");
		pthread_mutex_unlock(&mutex);
		return;
	}

	close_device(audio_fd);
	playing = 0;

	pthread_mutex_unlock(&mutex);
printf("stopping\n");
}

static void *
open_thread(void *data)
{
	OpenThreadType *thread_data = data;

	sample_max_min(thread_data->graphData,
	               thread_data->pct);

	return NULL;
}

void sample_open_file(const char *filename, GraphData *graphData, double *pct)
{
	sample_file = strdup(filename);

	if ((sample_fp = fopen(sample_file, "r")) == NULL) {
		printf("error opening %s\n", sample_file);
		return;
	}

	if (strstr(sample_file, ".wav")) {
		wav_read_header(sample_fp, &sampleInfo);
		audio_type = WAV;
	} else {
		cdda_read_header(sample_file, &sampleInfo);
		audio_type = CDDA;
	}

	open_thread_data->graphData = graphData;
	open_thread_data->pct = pct;

/* start new thread stuff */
	if (pthread_attr_init(&thread_attr) != 0) {
		perror("return from pthread_attr_init");
	}

	if (pthread_attr_setdetachstate(&thread_attr,
			PTHREAD_CREATE_DETACHED) != 0) {
		perror("return from pthread_attr_setdetachstate");
	}

	if (pthread_create(&thread, &thread_attr, open_thread, 
			   open_thread_data) != 0) {
		perror("Return from pthread_create");
	}
/* end new thread stuff */
}

static void sample_max_min(GraphData *graphData, double *pct)
{
	int tmp, min, max;
	int i, k, ret;
	int numSampleBlocks;
	double tmp_sample_calc;
	unsigned char devbuf[BLOCK_SIZE];
	Points *graph_data;

	tmp_sample_calc = sampleInfo.numBytes / (sampleInfo.bitsPerSample / 8);
	tmp_sample_calc = tmp_sample_calc / BLOCK_SIZE;
	tmp_sample_calc = tmp_sample_calc * sampleInfo.channels;
	numSampleBlocks = (int) (tmp_sample_calc +  1);

	/* DEBUG CODE START */
	/*
	printf("\nsampleInfo.numBytes: %lu\n", sampleInfo.numBytes);
	printf("sampleInfo.bitsPerSample: %d\n", sampleInfo.bitsPerSample);
	printf("BLOCK_SIZE: %d\n", BLOCK_SIZE);
	printf("sampleInfo.channels: %d\n\n", sampleInfo.channels);
	*/
	/* DEBUG CODE END */

	graph_data = (Points *)malloc(numSampleBlocks * sizeof(Points));

	if (graph_data == NULL) {
		printf("NULL returned from malloc of graph_data\n");
		return;
	}

	i = 0;

	if (audio_type == CDDA) {
		ret = cdda_read_sample(sample_fp, devbuf, BLOCK_SIZE,
			BLOCK_SIZE * i);
	} else if (audio_type == WAV) {
		ret = wav_read_sample(sample_fp, devbuf, BLOCK_SIZE,
			BLOCK_SIZE * i);
	}

	while (ret == BLOCK_SIZE) {
		min = max = 0;
		for (k = 0; k < ret; k++) {
			if (sampleInfo.bitsPerSample == 8) {
				tmp = devbuf[k];
				tmp -= 128;
			} else if (sampleInfo.bitsPerSample == 16) {
				tmp = (char)devbuf[k+1] << 8 | (char)devbuf[k];
				k++;
			}

			if (tmp > max) {
				max = tmp;
			} else if (tmp < min) {
				min = tmp;
			}
		}

		graph_data[i].min = min;
		graph_data[i].max = max;

		if (audio_type == CDDA) {
			ret = cdda_read_sample(sample_fp, devbuf, BLOCK_SIZE,
					BLOCK_SIZE * i);
		} else if (audio_type == WAV) {
			ret = wav_read_sample(sample_fp, devbuf, BLOCK_SIZE,
					BLOCK_SIZE * i);
		}

		*pct = (double) i / numSampleBlocks;
		i++;
	}

	*pct = 1.0;

	graphData->numSamples = numSampleBlocks;

	if (graphData->data != NULL) {
		free(graphData->data);
	}
	graphData->data = graph_data;

	if (sampleInfo.bitsPerSample == 8) {
		graphData->maxSampleValue = UCHAR_MAX;
	} else if (sampleInfo.bitsPerSample == 16) {
		graphData->maxSampleValue = SHRT_MAX;
	}

	printf("\ni: %d\n", i);
	printf("graphData->numSamples: %ld\n", graphData->numSamples);
	printf("graphData->maxSampleValue: %ld\n\n", graphData->maxSampleValue);
}

static void *
write_thread(void *data)
{
	GList *tbl_head = (GList *)data;
	GList *tbl_cur, *tbl_next;
	TrackBreak *tb_cur, *tb_next;
	int i;
	int ret;
	int index;
	unsigned long start_pos, end_pos;
	char *filename;
	char str_tmp[1024];

	i = 0;

	tbl_cur = tbl_head;
	tbl_next = g_list_next(tbl_cur);

	while (tbl_cur != NULL) {
		index = g_list_position(tbl_head, tbl_cur);
		tb_cur = (TrackBreak *)g_list_nth_data(tbl_head, index);
		if (tb_cur->write == TRUE) {
			start_pos = tb_cur->offset * BLOCK_SIZE;
			filename = strdup(tb_cur->filename);

			if (tbl_next == NULL) {
				end_pos = 0;
				tb_next = NULL;
			} else {
				index = g_list_position(tbl_head, tbl_next);
				tb_next = (TrackBreak *)g_list_nth_data(tbl_head, index);
				end_pos = tb_next->offset * BLOCK_SIZE;
			}

			strcpy(str_tmp, "tmp/");
			filename = strcat(str_tmp, filename);
			if ((audio_type == WAV) && (!strstr(filename, ".wav"))) {
				filename = strcat(str_tmp, ".wav");
			}

			if (audio_type == CDDA) {
				ret = cdda_write_file(sample_fp, filename, BUF_SIZE,
							start_pos, end_pos);
			} else if (audio_type == WAV) {
				ret = wav_write_file(sample_fp, filename, BLOCK_SIZE,
							&sampleInfo, start_pos, end_pos);
			}
		}

		tbl_cur = g_list_next(tbl_cur);
		tbl_next = g_list_next(tbl_next);
	}

	return NULL;
}

void sample_write_files(const char *filename, GList *tbl)
{
	sample_file = strdup(filename);

	if ((sample_fp = fopen(sample_file, "r")) == NULL) {
		printf("error opening %s\n", sample_file);
		return;
	}

	/*
	if (strstr(sample_file, ".wav")) {
		wav_read_header(sample_fp, &sampleInfo);
		audio_type = WAV;
	} else {
		cdda_read_header(sample_file, &sampleInfo);
		audio_type = CDDA;
	}
	*/

/* start new thread stuff */
	if (pthread_attr_init(&thread_attr) != 0) {
		perror("return from pthread_attr_init");
	}

	if (pthread_attr_setdetachstate(&thread_attr,
			PTHREAD_CREATE_DETACHED) != 0) {
		perror("return from pthread_attr_setdetachstate");
	}

	if (pthread_create(&thread, &thread_attr, write_thread, tbl) != 0) {
		perror("Return from pthread_create");
	}
/* end new thread stuff */
}
