/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002 Timothy Robinson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <gtk/gtk.h>
 
#include "wavbreaker.h"

#ifdef _WIN32
#include "winaudio.h"
#else
#include "linuxaudio.h"
#endif

#include "sample.h"
#include "wav.h"
#include "cdda.h"
#include "appconfig.h"

#define CDDA 1
#define WAV  2 

static SampleInfo sampleInfo;
static unsigned long sample_start = 0;
static int playing = 0;
static gboolean kill_play_thread = FALSE;
static int audio_type;
static int audio_fd;

static char *audio_dev = "/dev/dsp";
static char *sample_file = NULL;
static FILE *sample_fp = NULL;

static GThread *thread;
static GMutex *mutex = NULL;

/* typedef and struct stuff for new thread open junk */

typedef struct WriteThreadData_ WriteThreadData;
struct WriteThreadData_ {
	GList *tbl;
	WriteInfo *write_info;
};
WriteThreadData wtd;

typedef struct OpenThreadData_ OpenThreadData;
struct OpenThreadData_ {
	GraphData *graphData;
	double *pct;
};
OpenThreadData open_thread_data;

static void sample_max_min(GraphData *graphData, double *pct);

#define ERROR_MESSAGE_SIZE 1024
static char error_message[ERROR_MESSAGE_SIZE];

char *sample_get_error_message()
{
	return error_message;
}

void sample_set_error_message(const char *val)
{
	strncpy(error_message, val, ERROR_MESSAGE_SIZE);
}

gint sample_get_playing()
{
	return playing;
}

void sample_init()
{
	mutex = g_mutex_new();
	if (mutex == NULL) {
		perror("Return from g_mutex_init was NULL");
		exit(1);
	}
}

static gpointer
play_thread(gpointer thread_data)
{
	int ret, i;
	guint *play_marker = (guint *)thread_data;
	unsigned char devbuf[BUF_SIZE];

	if (audio_open_device(get_outputdev(), &sampleInfo) != 0) {
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
		if (audio_write(devbuf, ret) < 0) {
			audio_close_device();
			playing = 0;
			break;
		}

		if (g_mutex_trylock(mutex)) {
			if (kill_play_thread == TRUE) {
				audio_close_device();
				playing = 0;
				kill_play_thread = FALSE;
				g_mutex_unlock(mutex);
				return NULL;
			}
			g_mutex_unlock(mutex);
		}

		if (audio_type == CDDA) {
			ret = cdda_read_sample(sample_fp, devbuf, BUF_SIZE,
				sample_start + (BUF_SIZE * i++));
		} else if (audio_type == WAV) {
			ret = wav_read_sample(sample_fp, devbuf, BUF_SIZE,
				sample_start + (BUF_SIZE * i++));
		}

		*play_marker = ((BUF_SIZE * i) + sample_start) / BLOCK_SIZE;
	}

	g_mutex_lock(mutex);

	audio_close_device();
	playing = 0;

	g_mutex_unlock(mutex);

	return NULL;
}

int play_sample(gulong startpos, gulong *play_marker)
{       
	int ret;

	g_mutex_lock(mutex);
	if (playing) {
		g_mutex_unlock(mutex);
		return 2;
	}

	if (sample_file == NULL) {
		g_mutex_unlock(mutex);
		return 3;
	}

	playing = 1;
	sample_start = startpos * BLOCK_SIZE;

	/* setup thread */

	thread = g_thread_create(play_thread, play_marker, FALSE, NULL);
	if (thread == NULL) {
		perror("Return from g_thread_create was NULL");
		g_mutex_unlock(mutex);
		return 1;
	}

	g_mutex_unlock(mutex);
	return 0;
}               

void stop_sample()
{       
	g_mutex_lock(mutex);

	if (!playing) {
		g_mutex_unlock(mutex);
		return;
	}

	kill_play_thread = TRUE;
	g_mutex_unlock(mutex);
}

static gpointer
open_thread(gpointer data)
{
	OpenThreadData *thread_data = data;

	sample_max_min(thread_data->graphData,
	               thread_data->pct);

	return NULL;
}

int sample_open_file(const char *filename, GraphData *graphData, double *pct)
{
	if (sample_file != NULL) {
		free(sample_file);
	}
	sample_file = strdup(filename);

	if (strstr(sample_file, ".wav")) {
		if (wav_read_header(sample_file, &sampleInfo, 0) != 0) {
			sample_set_error_message(wav_get_error_message());
			return 1;
		}
		audio_type = WAV;
	} else {
		cdda_read_header(sample_file, &sampleInfo);
		audio_type = CDDA;
	}

	if ((sample_fp = fopen(sample_file, "rb")) == NULL) {
		snprintf(error_message, ERROR_MESSAGE_SIZE, "error opening %s\n", sample_file);
		return 2;
	}

	open_thread_data.graphData = graphData;
	open_thread_data.pct = pct;

	/* start new thread stuff */
	thread = g_thread_create(open_thread, &open_thread_data, FALSE, NULL);
	if (thread == NULL) {
		perror("Return from g_thread_create was NULL");
		return 3;
	}
	/* end new thread stuff */

	return 0;
}

static void sample_max_min(GraphData *graphData, double *pct)
{
	int tmp, min, max;
	int i, k, ret;
	int numSampleBlocks;
	double tmp_sample_calc;
	unsigned char devbuf[BLOCK_SIZE];
	Points *graph_data;

	tmp_sample_calc = sampleInfo.numBytes;
	tmp_sample_calc = tmp_sample_calc / BLOCK_SIZE;
	numSampleBlocks = (int) (tmp_sample_calc + 1);

	/* DEBUG CODE START */
	/*
	printf("\nsampleInfo.numBytes: %lu\n", sampleInfo.numBytes);
	printf("sampleInfo.bitsPerSample: %d\n", sampleInfo.bitsPerSample);
	printf("BLOCK_SIZE: %d\n", BLOCK_SIZE);
	printf("sampleInfo.channels: %d\n", sampleInfo.channels);
	printf("numSampleBlocks: %d\n\n", numSampleBlocks);
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

			// skip over any extra channels
			k += (sampleInfo.channels - 1) * (sampleInfo.bitsPerSample / 8);
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

	/* DEBUG CODE START */
	/*
	printf("\ni: %d\n", i);
	printf("graphData->numSamples: %ld\n", graphData->numSamples);
	printf("graphData->maxSampleValue: %ld\n\n", graphData->maxSampleValue);
	*/
	/* DEBUG CODE END */
}

static gpointer
write_thread(gpointer data)
{
	WriteThreadData *thread_data = data;

	GList *tbl_head = thread_data->tbl;
	GList *tbl_cur, *tbl_next;
	TrackBreak *tb_cur, *tb_next;
	WriteInfo *write_info = thread_data->write_info;

	int i;
	int ret;
	int index;
	unsigned long start_pos, end_pos;
	char filename[1024];
	char str_tmp[1024];

	write_info->num_files = 0;
	write_info->cur_file = 0;
	write_info->sync = 0;

	i = 1;
	tbl_cur = tbl_head;
	while (tbl_cur != NULL) {
		index = g_list_position(tbl_head, tbl_cur);
		tb_cur = (TrackBreak *)g_list_nth_data(tbl_head, index);

		if (tb_cur->write == TRUE) {
			write_info->num_files++;
		}

		tbl_cur = g_list_next(tbl_cur);
	}

	i = 1;
	tbl_cur = tbl_head;
	tbl_next = g_list_next(tbl_cur);

	while (tbl_cur != NULL) {
		index = g_list_position(tbl_head, tbl_cur);
		tb_cur = (TrackBreak *)g_list_nth_data(tbl_head, index);
		if (tb_cur->write == TRUE) {
			start_pos = tb_cur->offset * BLOCK_SIZE;

			if (tbl_next == NULL) {
				end_pos = 0;
				tb_next = NULL;
			} else {
				index = g_list_position(tbl_head, tbl_next);
				tb_next = (TrackBreak *)g_list_nth_data(tbl_head, index);
				end_pos = tb_next->offset * BLOCK_SIZE;
			}

			/* add output directory to filename */
			strcpy(filename, get_outputdir());
			strcat(filename, "/");
			strcat(filename, tb_cur->filename);

			/* add file number to filename */
			/* !!now doing this in the track break list!!
			if (i < 10) {
				sprintf(str_tmp, "0%d", i);
			} else {
				sprintf(str_tmp, "%d", i);
			}
			strcat(filename, str_tmp);
			*/

			/* add file extension to filename */
			if ((audio_type == WAV) && (!strstr(filename, ".wav"))) {
				strcat(filename, ".wav");
			} else if ((audio_type == CDDA) && (!strstr(filename, ".dat"))) {
				strcat(filename, ".dat");
			}
			write_info->cur_file++;
			if (write_info->cur_filename != NULL) {
				free(write_info->cur_filename);
			}
			write_info->cur_filename = strdup(filename);

			if (audio_type == CDDA) {
				ret = cdda_write_file(sample_fp, filename, BUF_SIZE,
							start_pos, end_pos);
			} else if (audio_type == WAV) {
				ret = wav_write_file(sample_fp, filename, BLOCK_SIZE,
							&sampleInfo, start_pos, end_pos,
							&write_info->pct_done);
			}
			i++;
		}

		tbl_cur = g_list_next(tbl_cur);
		tbl_next = g_list_next(tbl_next);
	}
	write_info->sync = 1;

	return NULL;
}

void sample_write_files(const char *filename, GList *tbl, WriteInfo *write_info)
{
//	WriteThreadData wtd;

	wtd.tbl = tbl;
	wtd.write_info = write_info;

	if (sample_file != NULL) {
		free(sample_file);
	}
	sample_file = strdup(filename);

	if ((sample_fp = fopen(sample_file, "rb")) == NULL) {
		printf("error opening %s\n", sample_file);
		return;
	}

	/* start new thread stuff */
	thread = g_thread_create(write_thread, &wtd, FALSE, NULL);
	if (thread == NULL) {
		perror("Return from g_thread_create was NULL");
		return;
	}
	/* end new thread stuff */
}
