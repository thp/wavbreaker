/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2006 Timothy Robinson
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <gtk/gtk.h>
#include <stdint.h>
 
#include "wavbreaker.h"

#include "aoaudio.h"

#include "sample_info.h"
#include "format.h"
#include "overwritedialog.h"
#include "gettext.h"

static SampleInfo sampleInfo;
static GraphData g_graphData;
static unsigned long sample_start = 0;
static int playing = 0;
static int writing = 0;
static gboolean kill_play_thread = FALSE;

static OpenedAudioFile *
opened_audio_file = NULL;

static GThread *thread;
static GMutex mutex;

/* typedef and struct stuff for new thread open junk */

typedef struct WriteThreadData_ WriteThreadData;
struct WriteThreadData_ {
    GList *tbl;
    WriteInfo *write_info;
    char *outputdir;
};
WriteThreadData wtd;

typedef struct OpenThreadData_ OpenThreadData;
struct OpenThreadData_ {
    GraphData *graphData;
    double *pct;
};
OpenThreadData open_thread_data;

static void sample_max_min(GraphData *graphData, double *pct);

static long
read_sample(unsigned char *buf, int buf_size, unsigned long start_pos)
{
    if (opened_audio_file != NULL) {
        return format_read_samples(opened_audio_file, buf, buf_size, start_pos);
    }

    return -1;
}

void sample_init()
{
    g_mutex_init(&mutex);

    format_init();
}

static gpointer play_thread(gpointer thread_data)
{
    int read_ret = 0;
    int i;
    guint *play_marker = (guint *)thread_data;
    unsigned char *devbuf;

    /*
    printf("play_thread: calling open_audio_device\n");
    */
    if (ao_audio_open_device(&sampleInfo) != 0) {
        g_mutex_lock(&mutex);
        playing = 0;
        ao_audio_close_device();
        g_mutex_unlock(&mutex);
        //printf("play_thread: return from open_audio_device != 0\n");
        return NULL;
    }
    //printf("play_thread: return from open_audio_device\n");

    i = 0;

    devbuf = malloc(DEFAULT_BUF_SIZE);
    if (devbuf == NULL) {
        g_mutex_lock(&mutex);
        playing = 0;
        ao_audio_close_device();
        g_mutex_unlock(&mutex);
        printf("play_thread: out of memory\n");
        return NULL;
    }

    read_ret = read_sample(devbuf, DEFAULT_BUF_SIZE, sample_start + (DEFAULT_BUF_SIZE * i++));

    while (read_ret > 0 && read_ret <= DEFAULT_BUF_SIZE) {
        /*
        if (read_ret < 0) {
            printf("read_ret: %d\n", read_ret);
        }
        */

        ao_audio_write(devbuf, read_ret);

        if (g_mutex_trylock(&mutex)) {
            if (kill_play_thread == TRUE) {
                ao_audio_close_device();
                playing = 0;
                kill_play_thread = FALSE;
                g_mutex_unlock(&mutex);
                return NULL;
            }
            g_mutex_unlock(&mutex);
        }

        read_ret = read_sample(devbuf, DEFAULT_BUF_SIZE, sample_start + (DEFAULT_BUF_SIZE * i++));

        *play_marker = ((DEFAULT_BUF_SIZE * i) + sample_start) /
	    sampleInfo.blockSize;
    }

    g_mutex_lock(&mutex);

    ao_audio_close_device();
    playing = 0;

    g_mutex_unlock(&mutex);

    return NULL;
}

int sample_is_playing()
{
    return playing;
}

int sample_is_writing()
{
    return writing;
}

int play_sample(gulong startpos, gulong *play_marker)
{       
    g_mutex_lock(&mutex);
    if (playing) {
        g_mutex_unlock(&mutex);
        return 2;
    }

    if (opened_audio_file == NULL) {
        g_mutex_unlock(&mutex);
        return 3;
    }

    playing = 1;
    sample_start = startpos * sampleInfo.blockSize;

    /* setup thread */

    //printf("creating the thread\n");
    fflush(stdout);
    thread = g_thread_new("play_sample", play_thread, play_marker);

    g_mutex_unlock(&mutex);
    //printf("finished creating the thread\n");
    return 0;
}               

void stop_sample()
{       
    g_mutex_lock(&mutex);

    if (!playing) {
        g_mutex_unlock(&mutex);
        return;
    }

    kill_play_thread = TRUE;
    g_mutex_unlock(&mutex);

    // Wait for play thread to actually quit
    g_thread_join(thread);
    thread = NULL;
}

static gpointer open_thread(gpointer data)
{
    OpenThreadData *thread_data = data;

    sample_max_min(thread_data->graphData,
                   thread_data->pct);

    return NULL;
}

int sample_open_file(const char *filename, double *pct, char **error_message)
{
    sample_close_file();

    GraphData *graphData = &g_graphData;

    opened_audio_file = format_open_file(filename, error_message);
    if (opened_audio_file == NULL) {
        g_message("Could not open %s with format_open_file(): %s", filename, *error_message);
        return 1;
    }

    // TODO: Remove those global variables
    sampleInfo = opened_audio_file->sample_info;

    open_thread_data.graphData = graphData;
    open_thread_data.pct = pct;

    g_thread_unref(g_thread_new("open file", open_thread, &open_thread_data));

    return 0;
}

GraphData *
sample_get_graph_data(void)
{
    return &g_graphData;
}

unsigned long
sample_get_num_samples(void)
{
    return g_graphData.numSamples;
}

void sample_close_file()
{
    if (opened_audio_file != NULL) {
        format_close_file(g_steal_pointer(&opened_audio_file));
    }
} 

static void sample_max_min(GraphData *graphData, double *pct)
{
    int tmp = 0;
    long int ret = 0;
    int min, max, xtmp;
    int min_sample, max_sample;
    long int i, k;
    long int numSampleBlocks;
    long int tmp_sample_calc;
    unsigned char devbuf[sampleInfo.blockSize];
    Points *graph_data;

    tmp_sample_calc = sampleInfo.numBytes;
    tmp_sample_calc = tmp_sample_calc / sampleInfo.blockSize;
    numSampleBlocks = (tmp_sample_calc + 1);

    /* DEBUG CODE START */
    /*
    printf("\nsampleInfo.numBytes: %lu\n", sampleInfo.numBytes);
    printf("sampleInfo.bitsPerSample: %d\n", sampleInfo.bitsPerSample);
    printf("sampleInfo.blockSize: %d\n", sampleInfo.blockSize);
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

    ret = read_sample(devbuf, sampleInfo.blockSize, sampleInfo.blockSize * i);

    min_sample = SHRT_MAX; /* highest value for 16-bit samples */
    max_sample = 0;

    while (ret == sampleInfo.blockSize && i < numSampleBlocks) {
        min = max = 0;
        for (k = 0; k < ret; k++) {
            if (sampleInfo.bitsPerSample == 8) {
                tmp = devbuf[k];
                tmp -= 128;
            } else if (sampleInfo.bitsPerSample == 16) {
                tmp = (char)devbuf[k+1] << 8 | (char)devbuf[k];
                k++;
	    } else if (sampleInfo.bitsPerSample == 24) {
		tmp   = ((char)devbuf[k]) | ((char)devbuf[k+1] << 8);
		tmp  &= 0x0000ffff;
		xtmp  =  (char)devbuf[k+2] << 16;
		tmp  |= xtmp;
		k += 2;
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

        if( min_sample > (max-min)) {
            min_sample = (max-min);
        }
        if( max_sample < (max-min)) {
            max_sample = (max-min);
        }

        ret = read_sample(devbuf, sampleInfo.blockSize, sampleInfo.blockSize * i);

        *pct = (double) i / numSampleBlocks;
        i++;
    }

    *pct = 1.0;

    graphData->numSamples = numSampleBlocks;

    if (graphData->data != NULL) {
        free(graphData->data);
    }
    graphData->data = graph_data;

    graphData->minSampleAmp = min_sample;
    graphData->maxSampleAmp = max_sample;

    if (sampleInfo.bitsPerSample == 8) {
        graphData->maxSampleValue = UCHAR_MAX;
    } else if (sampleInfo.bitsPerSample == 16) {
        graphData->maxSampleValue = SHRT_MAX;
    } else if (sampleInfo.bitsPerSample == 24) {
	graphData->maxSampleValue = 0x7fffff;
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
    char *outputdir = thread_data->outputdir;
    TrackBreak *tb_cur, *tb_next;
    WriteInfo *write_info = thread_data->write_info;

    int i;
    int index;
    unsigned long start_pos, end_pos;
    char filename[1024];

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
            start_pos = tb_cur->offset * sampleInfo.blockSize;

            if (tbl_next == NULL) {
                end_pos = 0;
                tb_next = NULL;
            } else {
                index = g_list_position(tbl_head, tbl_next);
                tb_next = (TrackBreak *)g_list_nth_data(tbl_head, index);
                end_pos = tb_next->offset * sampleInfo.blockSize;
            }

            /* add output directory to filename */
            strcpy(filename, outputdir);
            strcat(filename, "/");

            strcat(filename, tb_cur->filename);

            // TODO: CDDA needs .cdda.raw file extension, not .raw
            const char *source_file_extension = opened_audio_file->filename ? strrchr(opened_audio_file->filename, '.') : NULL;
            if (source_file_extension == NULL) {
                /* Fallback extensions if not in source filename */
                if (opened_audio_file != NULL) {
                    source_file_extension = opened_audio_file->mod->default_file_extension;
                }
            }

            /* add file extension to filename */
            if (source_file_extension != NULL && strstr(filename, source_file_extension) == NULL) {
                strcat(filename, source_file_extension);
            }

            write_info->pct_done = 0.0;
            write_info->cur_file++;
            if (write_info->cur_filename != NULL) {
                g_free(write_info->cur_filename);
            }
            write_info->cur_filename = g_strdup(filename);

            if (write_info->skip_file < 2) {
                if (g_file_test(filename, G_FILE_TEST_EXISTS)) {
                    write_info->skip_file = -1;
                    write_info->check_file_exists = 1;
                    // sync the threads to wait on overwrite question
                    while (write_info->skip_file < 0) {
                        sleep(1);
                    }
                } else {
                    write_info->skip_file = 1;
                }
            }

            if (write_info->skip_file > 0) {
                if (format_write_file(opened_audio_file, filename, start_pos, end_pos, &write_info->pct_done) == -1) {
                    g_warning("Could not write file %s", filename);
                    write_info->errors = g_list_append(write_info->errors, g_strdup(filename));
                }

                i++;
            }

            if (write_info->skip_file < 2) {
                write_info->skip_file = -1;
            }
        }

        tbl_cur = g_list_next(tbl_cur);
        tbl_next = g_list_next(tbl_next);
    }
    write_info->sync = 1;
    if (write_info->cur_filename != NULL) {
        g_free(write_info->cur_filename);
    }
    write_info->cur_filename = NULL;

    writing = 0;
    return NULL;
}

void sample_write_files(GList *tbl, WriteInfo *write_info, char *outputdir)
{
    wtd.tbl = tbl;
    wtd.write_info = write_info;
    wtd.outputdir = outputdir;

    writing = 1;

    if (opened_audio_file == NULL) {
        perror("Must open file first\n");
        writing = 0;
        return;
    }

    write_info->num_files = 0;
    write_info->cur_file = 0;
    write_info->sync = 0;
    write_info->sync_check_file_overwrite_to_write_progress = 0;
    write_info->check_file_exists = 0;
    write_info->skip_file = -1;
    write_info->errors = NULL;

    g_thread_unref(g_thread_new("write data", write_thread, &wtd));
}

