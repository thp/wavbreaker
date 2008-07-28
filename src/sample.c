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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <gtk/gtk.h>
 
#include "wavbreaker.h"

#include "sample.h"
#include "wav.h"
#include "cdda.h"
#include "appconfig.h"
#include "fileutils.h"
#include "overwritedialog.h"
#include "gettext.h"

#define CDDA 1
#define WAV  2 

SampleInfo sampleInfo;
static AudioFunctionPointers *audio_function_pointers;
static unsigned long sample_start = 0;
static int playing = 0;
static int writing = 0;
static gboolean kill_play_thread = FALSE;
static int audio_type;

static char *sample_file = NULL;
static FILE *read_sample_fp = NULL;
static FILE *write_sample_fp = NULL;

static GThread *thread;
static GMutex *mutex = NULL;

/* typedef and struct stuff for new thread open junk */

typedef struct WriteThreadData_ WriteThreadData;
struct WriteThreadData_ {
    GList *tbl;
    WriteInfo *write_info;
    char *outputdir;
};
WriteThreadData wtd;

typedef struct MergeThreadData_ MergeThreadData;
struct MergeThreadData_ {
    char *merge_filename;
    int num_files;
    GList *filenames;
    WriteInfo *write_info;
};
MergeThreadData mtd;

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
    return g_strdup(error_message);
}

void sample_set_error_message(const char *val)
{
    strncpy(error_message, val, ERROR_MESSAGE_SIZE);
}

void sample_init()
{
    mutex = g_mutex_new();
    if (mutex == NULL) {
        perror("Return from g_mutex_init was NULL");
        exit(1);
    }
}

static gpointer play_thread(gpointer thread_data)
{
    int read_ret = 0;
    int write_ret, i;
    guint *play_marker = (guint *)thread_data;
    unsigned char *devbuf;

    audio_function_pointers = get_audio_function_pointers();

    /*
    printf("play_thread: calling open_audio_device\n");
    printf("devault outputdev: %s\n", audio_function_pointers->get_outputdev());
    */
    if (audio_function_pointers->audio_open_device(
        audio_function_pointers->get_outputdev(), &sampleInfo) != 0) {

        g_mutex_lock(mutex);
        playing = 0;
        audio_function_pointers->audio_close_device();
        g_mutex_unlock(mutex);
        //printf("play_thread: return from open_audio_device != 0\n");
        return NULL;
    }
    //printf("play_thread: return from open_audio_device\n");

    i = 0;

    devbuf = malloc(sampleInfo.bufferSize);
    if (devbuf == NULL) {
        g_mutex_lock(mutex);
        playing = 0;
        audio_function_pointers->audio_close_device();
        g_mutex_unlock(mutex);
        printf("play_thread: out of memory\n");
        return NULL;
    }

    if (audio_type == CDDA) {
        read_ret = cdda_read_sample(read_sample_fp, devbuf, sampleInfo.bufferSize,
            sample_start + (sampleInfo.bufferSize * i++));
    } else if (audio_type == WAV) {
        read_ret = wav_read_sample(read_sample_fp, devbuf, sampleInfo.bufferSize,
            sample_start + (sampleInfo.bufferSize * i++));
    }

    while (read_ret > 0 && read_ret <= sampleInfo.bufferSize) {
        /*
        if (read_ret < 0) {
            printf("read_ret: %d\n", read_ret);
        }
        */

        write_ret = audio_function_pointers->audio_write(devbuf, read_ret);

        if (g_mutex_trylock(mutex)) {
            if (kill_play_thread == TRUE) {
                audio_function_pointers->audio_close_device();
                playing = 0;
                kill_play_thread = FALSE;
                g_mutex_unlock(mutex);
                return NULL;
            }
            g_mutex_unlock(mutex);
        }

        if (audio_type == CDDA) {
            read_ret = cdda_read_sample(read_sample_fp, devbuf,
                sampleInfo.bufferSize,
                sample_start + (sampleInfo.bufferSize * i++));
        } else if (audio_type == WAV) {
            read_ret = wav_read_sample(read_sample_fp, devbuf,
                sampleInfo.bufferSize,
                sample_start + (sampleInfo.bufferSize * i++));
        }

        *play_marker = ((sampleInfo.bufferSize * i) + sample_start) /
	    sampleInfo.blockSize;
    }

    g_mutex_lock(mutex);

    audio_function_pointers->audio_close_device();
    playing = 0;

    g_mutex_unlock(mutex);

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
    sample_start = startpos * sampleInfo.blockSize;

    /* setup thread */

    //printf("creating the thread\n");
    fflush(stdout);
    thread = g_thread_create(play_thread, play_marker, FALSE, NULL);
    if (thread == NULL) {
        perror("Return from g_thread_create was NULL");
        g_mutex_unlock(mutex);
        return 1;
    }

    g_mutex_unlock(mutex);
    g_thread_yield();
    //printf("finished creating the thread\n");
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

static gpointer open_thread(gpointer data)
{
    OpenThreadData *thread_data = data;

    sample_max_min(thread_data->graphData,
                   thread_data->pct);

    return NULL;
}

int ask_open_as_raw()
{
    GtkMessageDialog *dialog;
    gint result;
    const gchar *message = _("Open as RAW audio");
    const gchar *info_text = _("The file you selected does not contain a wave header. wavbreaker can interpret the file as \"Signed 16 bit, 44100 Hz, Stereo\" audio. Choose the byte order for the RAW audio or cancel to abort.");

    dialog = (GtkMessageDialog*)gtk_message_dialog_new( GTK_WINDOW(wavbreaker_get_main_window()),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_CANCEL,
                                     message);
    
    gtk_dialog_add_button( GTK_DIALOG(dialog), _("Big endian"), WB_RESPONSE_BIG_ENDIAN);
    gtk_dialog_add_button( GTK_DIALOG(dialog), _("Little endian"), WB_RESPONSE_LITTLE_ENDIAN);

    gtk_message_dialog_format_secondary_text( dialog, info_text);
    gtk_window_set_title( GTK_WINDOW(dialog), message);

    result = gtk_dialog_run( GTK_DIALOG(dialog));
    gtk_widget_destroy( GTK_WIDGET( dialog));

    return result;
}

int sample_open_file(const char *filename, GraphData *graphData, double *pct)
{
    int ask_result = 0;
    sample_close_file();

    sample_file = strdup(filename);

    audio_type = 0;
    if( wav_read_header(sample_file, &sampleInfo, 0) == 0) {
        audio_type = WAV;
    }
    
    if( audio_type == 0) {
        ask_result = ask_open_as_raw();
        if( ask_result == GTK_RESPONSE_CANCEL) {
            sample_set_error_message( wav_get_error_message());
            return 1;
        }
        cdda_read_header(sample_file, &sampleInfo);
        if( ask_result == WB_RESPONSE_BIG_ENDIAN) {
            audio_type = CDDA;
        } else {
            audio_type = WAV;
        }
    }

    sampleInfo.blockSize = (((sampleInfo.bitsPerSample / 8) *
			     sampleInfo.channels * sampleInfo.samplesPerSec) /
			    CD_BLOCKS_PER_SEC);

    if ((read_sample_fp = fopen(sample_file, "rb")) == NULL) {
        snprintf(error_message, ERROR_MESSAGE_SIZE, _("Error opening %s: %s"), sample_file, strerror( errno));
        return 2;
    }

    open_thread_data.graphData = graphData;
    open_thread_data.pct = pct;

    /* start new thread stuff */
    fflush(stdout);
    thread = g_thread_create(open_thread, &open_thread_data, FALSE, NULL);
    if (thread == NULL) {
        perror("Return from g_thread_create was NULL");
        return 3;
    }
    /* end new thread stuff */

    g_thread_yield();
    return 0;
}

void sample_close_file()
{
    if( read_sample_fp != NULL) {
        fclose( read_sample_fp);
        read_sample_fp = NULL;
    }

    if( sample_file != NULL) {
        free( sample_file);
        sample_file = NULL;
    }
} 

static void sample_max_min(GraphData *graphData, double *pct)
{
    int tmp = 0;
    long int ret = 0;
    int min, max, xtmp;
    int min_sample, max_sample;
    long int i, k;
    int numSampleBlocks;
    long int tmp_sample_calc;
    unsigned char devbuf[sampleInfo.blockSize];
    Points *graph_data;

    tmp_sample_calc = sampleInfo.numBytes;
    tmp_sample_calc = tmp_sample_calc / sampleInfo.blockSize;
    numSampleBlocks = (int) (tmp_sample_calc + 1);

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

    if (audio_type == CDDA) {
        ret = cdda_read_sample(read_sample_fp, devbuf, sampleInfo.blockSize,
			       sampleInfo.blockSize * i);
    } else if (audio_type == WAV) {
        ret = wav_read_sample(read_sample_fp, devbuf, sampleInfo.blockSize,
			      sampleInfo.blockSize * i);
    }

    min_sample = SHRT_MAX; /* highest value for 16-bit samples */
    max_sample = 0;

    while (ret == sampleInfo.blockSize) {
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

        if (audio_type == CDDA) {
            ret = cdda_read_sample(read_sample_fp, devbuf,
				   sampleInfo.blockSize,
				   sampleInfo.blockSize * i);
        } else if (audio_type == WAV) {
            ret = wav_read_sample(read_sample_fp, devbuf,
				  sampleInfo.blockSize,
				  sampleInfo.blockSize * i);
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
    int ret;
    int index;
    unsigned long start_pos, end_pos;
    char filename[1024];

    write_info->num_files = 0;
    write_info->cur_file = 0;
    write_info->sync = 0;
    write_info->sync_check_file_overwrite_to_write_progress = 0;
    write_info->check_file_exists = 0;
    write_info->skip_file = -1;

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

            /* add file extension to filename */
            if ((audio_type == WAV) && (!strstr(filename, ".wav"))) {
                strcat(filename, ".wav");
            } else if ((audio_type == CDDA) && (!strstr(filename, ".dat"))) {
                strcat(filename, ".dat");
            }
            write_info->pct_done = 0.0;
            write_info->cur_file++;
            if (write_info->cur_filename != NULL) {
                free(write_info->cur_filename);
            }
            write_info->cur_filename = strdup(filename);

            if (write_info->skip_file < 2) {
                if (check_file_exists(filename)) {
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
                if (audio_type == CDDA) {
                    ret = cdda_write_file(write_sample_fp, filename,
                        sampleInfo.bufferSize, start_pos, end_pos);
                } else if (audio_type == WAV) {
                    ret = wav_write_file(write_sample_fp, filename,
					 sampleInfo.blockSize,
					 &sampleInfo, start_pos, end_pos,
					 &write_info->pct_done);
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
        free(write_info->cur_filename);
    }
    write_info->cur_filename = NULL;

    fclose(write_sample_fp);

    writing = 0;
    return NULL;
}

void sample_write_files(GList *tbl, WriteInfo *write_info, char *outputdir)
{
    wtd.tbl = tbl;
    wtd.write_info = write_info;
    wtd.outputdir = outputdir;

    writing = 1;

    if (sample_file == NULL) {
        perror("Must open file first\n");
        writing = 0;
        return;
    }

    if ((write_sample_fp = fopen(sample_file, "rb")) == NULL) {
        printf("error opening %s\n", sample_file);
        writing = 0;
        return;
    }

    /* start new thread stuff */
    thread = g_thread_create(write_thread, &wtd, FALSE, NULL);
    if (thread == NULL) {
        perror("Return from g_thread_create was NULL");
        writing = 0;
        return;
    }
    /* end new thread stuff */
}

static gpointer
merge_thread(gpointer data)
{
    MergeThreadData *thread_data = data;
    char *filenames[g_list_length(thread_data->filenames)];
    GList *cur, *head;
    int index, i;
    char *list_data;

    head = thread_data->filenames;
    cur = head;
    i = 0;
    while (cur != NULL) {
        index = g_list_position(head, cur);
        list_data = (char *)g_list_nth_data(head, index);

        filenames[i++] = list_data;

        cur = g_list_next(cur);
    }

    wav_merge_files(thread_data->merge_filename,
                        g_list_length(thread_data->filenames),
                        filenames,
                        DEFAULT_BUF_SIZE,
                        thread_data->write_info);

    head = thread_data->filenames;
    cur = head;
    while (cur != NULL) {
        index = g_list_position(head, cur);
        list_data = (char *)g_list_nth_data(head, index);

        free(list_data);

        cur = g_list_next(cur);
    }
    
    g_list_free(thread_data->filenames);

    return NULL;
}

void sample_merge_files(char *merge_filename, GList *filenames, WriteInfo *write_info)
{
    mtd.merge_filename = strdup(merge_filename);
    mtd.filenames = filenames;
    mtd.write_info = write_info;

    if (write_info->merge_filename != NULL) {
        free(write_info->merge_filename);
    }
    write_info->merge_filename = mtd.merge_filename;

    /* start new thread stuff */
    thread = g_thread_create(merge_thread, &mtd, FALSE, NULL);
    if (thread == NULL) {
        perror("Return from g_thread_create was NULL");
        return;
    }
    /* end new thread stuff */
}

