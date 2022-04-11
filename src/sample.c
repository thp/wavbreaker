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

#include "sample.h"

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>

#include "aoaudio.h"

#include "sample_info.h"
#include "track_break.h"

#include "format.h"
#include "gettext.h"

typedef struct WriteThreadData_ WriteThreadData;
struct WriteThreadData_ {
    Sample *sample;

    TrackBreakList *list;
    WriteStatusCallbacks *callbacks;
    const char *outputdir;
};

struct Sample_ {
    OpenedAudioFile *opened_audio_file;

    gchar *filename_dirname;
    gchar *filename_basename;
    gchar *basename_without_extension;

    GMutex load_mutex;
    gboolean loaded;
    GraphData graph_data;
    double load_percentage;

    GThread *play_thread;
    GMutex play_mutex;
    gboolean playing;
    gboolean kill_play_thread;
    gulong play_position;
    unsigned long play_start_position;

    GMutex write_mutex;
    gboolean writing;

    WriteThreadData write_thread_data;
};


static void
sample_max_min(Sample *sample);

static long
read_sample(OpenedAudioFile *oaf, unsigned char *buf, int buf_size, unsigned long start_pos)
{
    if (oaf != NULL) {
        return format_read_samples(oaf, buf, buf_size, start_pos);
    }

    return -1;
}

void sample_init()
{
    format_init();
}

static gpointer
play_thread(gpointer thread_data)
{
    Sample *sample = thread_data;

    int read_ret = 0;
    int i;

    unsigned char *devbuf;

    /*
    printf("play_thread: calling open_audio_device\n");
    */
    if (ao_audio_open_device(&sample->opened_audio_file->sample_info) != 0) {
        g_mutex_lock(&sample->play_mutex);
        sample->playing = FALSE;
        ao_audio_close_device();
        g_mutex_unlock(&sample->play_mutex);
        //printf("play_thread: return from open_audio_device != 0\n");
        return NULL;
    }
    //printf("play_thread: return from open_audio_device\n");

    i = 0;

    devbuf = malloc(DEFAULT_BUF_SIZE);
    if (devbuf == NULL) {
        g_mutex_lock(&sample->play_mutex);
        sample->playing = FALSE;
        ao_audio_close_device();
        g_mutex_unlock(&sample->play_mutex);
        printf("play_thread: out of memory\n");
        return NULL;
    }

    read_ret = read_sample(sample->opened_audio_file, devbuf, DEFAULT_BUF_SIZE, sample->play_start_position + (DEFAULT_BUF_SIZE * i++));

    while (read_ret > 0 && read_ret <= DEFAULT_BUF_SIZE) {
        /*
        if (read_ret < 0) {
            printf("read_ret: %d\n", read_ret);
        }
        */

        ao_audio_write(devbuf, read_ret);

        if (g_mutex_trylock(&sample->play_mutex)) {
            if (sample->kill_play_thread) {
                ao_audio_close_device();
                sample->playing = FALSE;
                sample->kill_play_thread = FALSE;
                g_mutex_unlock(&sample->play_mutex);
                return NULL;
            }
            g_mutex_unlock(&sample->play_mutex);
        }

        read_ret = read_sample(sample->opened_audio_file, devbuf, DEFAULT_BUF_SIZE, sample->play_start_position + (DEFAULT_BUF_SIZE * i++));

        g_mutex_lock(&sample->play_mutex);

        sample->play_position = ((DEFAULT_BUF_SIZE * i) + sample->play_start_position) / sample->opened_audio_file->sample_info.blockSize;

        g_mutex_unlock(&sample->play_mutex);
    }

    g_mutex_lock(&sample->play_mutex);

    ao_audio_close_device();
    sample->playing = FALSE;

    g_mutex_unlock(&sample->play_mutex);

    return NULL;
}

gboolean
sample_is_playing(Sample *sample)
{
    gboolean result;

    g_mutex_lock(&sample->play_mutex);
    result = sample->playing;
    g_mutex_unlock(&sample->play_mutex);

    return result;
}

gulong
sample_get_play_marker(Sample *sample)
{
    gulong result;

    g_mutex_lock(&sample->play_mutex);
    result = sample->play_position;
    g_mutex_unlock(&sample->play_mutex);

    return result;
}

gboolean
sample_is_writing(Sample *sample)
{
    gboolean result;

    g_mutex_lock(&sample->write_mutex);
    result = sample->writing;
    g_mutex_unlock(&sample->write_mutex);

    return result;
}

int
sample_play(Sample *sample, gulong startpos)
{
    g_mutex_lock(&sample->play_mutex);
    if (sample->playing) {
        g_mutex_unlock(&sample->play_mutex);
        return 2;
    }

    if (sample->opened_audio_file == NULL) {
        g_mutex_unlock(&sample->play_mutex);
        return 3;
    }

    sample->playing = TRUE;
    sample->play_start_position = startpos * sample->opened_audio_file->sample_info.blockSize;

    /* setup thread */

    sample->play_position = startpos;

    sample->play_thread = g_thread_new("play_sample", play_thread, sample);

    g_mutex_unlock(&sample->play_mutex);
    return 0;
}

void
sample_stop(Sample *sample)
{
    g_mutex_lock(&sample->play_mutex);

    if (!sample->playing) {
        g_mutex_unlock(&sample->play_mutex);
        return;
    }

    sample->kill_play_thread = TRUE;
    g_mutex_unlock(&sample->play_mutex);

    // Wait for play thread to actually quit
    g_thread_join(g_steal_pointer(&sample->play_thread));
}

static gpointer
open_thread(gpointer data)
{
    Sample *sample = data;

    sample_max_min(sample);

    return NULL;
}

Sample *
sample_open(const char *filename, char **error_message)
{
    Sample *sample = g_new0(Sample, 1);

    sample->opened_audio_file = format_open_file(filename, error_message);
    if (sample->opened_audio_file == NULL) {
        g_free(sample);
        g_message("Could not open %s with format_open_file(): %s", filename, *error_message);
        return NULL;
    }

    sample->filename_dirname = g_path_get_dirname(filename);
    sample->filename_basename = g_path_get_basename(filename);

    gchar *tmp = g_strdup(sample->filename_basename);
    if (format_module_filename_extension_check(sample->opened_audio_file->mod, tmp, NULL)) {
        tmp[strlen(tmp)-strlen(sample->opened_audio_file->mod->default_file_extension)] = '\0';
    } else {
        gchar *end = strrchr(tmp, '.');
        if (end) {
            *end = '\0';
        }
    }
    sample->basename_without_extension = tmp;

    g_mutex_init(&sample->load_mutex);
    g_mutex_init(&sample->play_mutex);
    g_mutex_init(&sample->write_mutex);

    // TODO: Capture thread and properly tear it down - if needed - in sample_close()
    g_thread_unref(g_thread_new("open file", open_thread, sample));

    return sample;
}

void
sample_print_file_info(Sample *sample)
{
    format_print_file_info(sample->opened_audio_file);
}

GraphData *
sample_get_graph_data(Sample *sample)
{
    GraphData *result = NULL;

    g_mutex_lock(&sample->load_mutex);
    if (sample->loaded) {
        result = &sample->graph_data;
    }
    g_mutex_unlock(&sample->load_mutex);

    return result;
}

unsigned long
sample_get_num_sample_blocks(Sample *sample)
{
    return sample->graph_data.numSamples;
}

void
sample_close(Sample *sample)
{
    g_free(sample->basename_without_extension);
    g_free(sample->filename_basename);
    g_free(sample->filename_dirname);

    if (sample->opened_audio_file != NULL) {
        format_close_file(g_steal_pointer(&sample->opened_audio_file));
    }

    g_free(sample);
}

gboolean
sample_is_loaded(Sample *sample)
{
    gboolean result;

    g_mutex_lock(&sample->load_mutex);
    result = sample->loaded;
    g_mutex_unlock(&sample->load_mutex);

    return result;
}

double
sample_get_load_percentage(Sample *sample)
{
    double result;

    g_mutex_lock(&sample->load_mutex);
    result = sample->load_percentage;
    g_mutex_unlock(&sample->load_mutex);

    return result;
}

uint64_t
sample_get_file_size(Sample *sample)
{
    return sample->opened_audio_file->file_size;
}

const char *
sample_get_filename(Sample *sample)
{
    return sample->opened_audio_file->filename;
}

const char *
sample_get_dirname(Sample *sample)
{
    return sample->filename_dirname;
}

const char *
sample_get_basename(Sample *sample)
{
    return sample->filename_basename;
}

const char *
sample_get_basename_without_extension(Sample *sample)
{
    return sample->basename_without_extension;
}

static void
sample_max_min(Sample *sample)
{
    GraphData *graphData = &sample->graph_data;

    SampleInfo *sample_info = &sample->opened_audio_file->sample_info;
    int tmp = 0;
    long int ret = 0;
    int min, max, xtmp;
    int min_sample, max_sample;
    long int i, k;
    long int numSampleBlocks;
    long int tmp_sample_calc;
    unsigned char devbuf[sample->opened_audio_file->sample_info.blockSize];
    Points *graph_data;

    tmp_sample_calc = sample_info->numBytes;
    tmp_sample_calc = tmp_sample_calc / sample_info->blockSize;
    numSampleBlocks = (tmp_sample_calc + 1);

    /* DEBUG CODE START */
    /*
    printf("\nsample_info->numBytes: %lu\n", sample_info->numBytes);
    printf("sample_info->bitsPerSample: %d\n", sample_info->bitsPerSample);
    printf("sample_info->blockSize: %d\n", sample_info->blockSize);
    printf("sample_info->channels: %d\n", sample_info->channels);
    printf("numSampleBlocks: %d\n\n", numSampleBlocks);
    */
    /* DEBUG CODE END */

    graph_data = (Points *)malloc(numSampleBlocks * sizeof(Points));

    if (graph_data == NULL) {
        printf("NULL returned from malloc of graph_data\n");
        return;
    }

    i = 0;

    ret = read_sample(sample->opened_audio_file, devbuf, sample_info->blockSize, sample_info->blockSize * i);

    min_sample = SHRT_MAX; /* highest value for 16-bit samples */
    max_sample = 0;

    while (ret == sample_info->blockSize && i < numSampleBlocks) {
        min = max = 0;
        for (k = 0; k < ret; k++) {
            if (sample_info->bitsPerSample == 8) {
                tmp = devbuf[k];
                tmp -= 128;
            } else if (sample_info->bitsPerSample == 16) {
                tmp = (char)devbuf[k+1] << 8 | (char)devbuf[k];
                k++;
	    } else if (sample_info->bitsPerSample == 24) {
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
            k += (sample_info->channels - 1) * (sample_info->bitsPerSample / 8);
        }

        graph_data[i].min = min;
        graph_data[i].max = max;

        if( min_sample > (max-min)) {
            min_sample = (max-min);
        }
        if( max_sample < (max-min)) {
            max_sample = (max-min);
        }

        ret = read_sample(sample->opened_audio_file, devbuf, sample_info->blockSize, sample_info->blockSize * i);

        g_mutex_lock(&sample->load_mutex);
        sample->load_percentage = (double) i / numSampleBlocks;
        g_mutex_unlock(&sample->load_mutex);
        i++;
    }

    graphData->numSamples = numSampleBlocks;

    if (graphData->data != NULL) {
        free(graphData->data);
    }
    graphData->data = graph_data;

    graphData->minSampleAmp = min_sample;
    graphData->maxSampleAmp = max_sample;

    if (sample_info->bitsPerSample == 8) {
        graphData->maxSampleValue = UCHAR_MAX;
    } else if (sample_info->bitsPerSample == 16) {
        graphData->maxSampleValue = SHRT_MAX;
    } else if (sample_info->bitsPerSample == 24) {
	graphData->maxSampleValue = 0x7fffff;
    }

    g_mutex_lock(&sample->load_mutex);
    sample->load_percentage = 1.0;
    sample->loaded = TRUE;
    g_mutex_unlock(&sample->load_mutex);
}

static void
trampoline_file_progress_changed(double progress, void *user_data)
{
    WriteStatusCallbacks *callbacks = user_data;
    callbacks->on_file_progress_changed(progress, callbacks->user_data);
}

static gpointer
write_thread(gpointer data)
{
    WriteThreadData *thread_data = data;

    TrackBreakList *list = thread_data->list;

    GList *tbl_head = list->breaks;
    GList *tbl_cur, *tbl_next;
    const char *outputdir = thread_data->outputdir;
    TrackBreak *tb_cur, *tb_next;

    WriteStatusCallbacks *callbacks = thread_data->callbacks;

    Sample *sample = thread_data->sample;

    unsigned long start_pos, end_pos;
    char filename[1024];

    gulong num_files = 0;
    enum OverwriteDecision overwrite_decision = OVERWRITE_DECISION_ASK;

    tbl_cur = tbl_head;
    while (tbl_cur != NULL) {
        tb_cur = tbl_cur->data;

        if (tb_cur->write == TRUE) {
            ++num_files;
        }

        tbl_cur = g_list_next(tbl_cur);
    }

    int i = 1;
    tbl_cur = tbl_head;
    tbl_next = g_list_next(tbl_cur);

    while (tbl_cur != NULL && !callbacks->is_cancelled(callbacks->user_data)) {
        tb_cur = tbl_cur->data;

        if (tb_cur->write) {
            start_pos = tb_cur->offset * sample->opened_audio_file->sample_info.blockSize;

            if (tbl_next == NULL) {
                end_pos = 0;
                tb_next = NULL;
            } else {
                tb_next = tbl_next->data;
                end_pos = tb_next->offset * sample->opened_audio_file->sample_info.blockSize;
            }

            /* add output directory to filename */
            strcpy(filename, outputdir);
            strcat(filename, "/");

            gchar *tmp = track_break_get_filename(tb_cur, list);
            strcat(filename, tmp);
            g_free(tmp);

            // TODO: CDDA needs .cdda.raw file extension, not .raw
            const char *source_file_extension = sample->opened_audio_file->filename ? strrchr(sample->opened_audio_file->filename, '.') : NULL;
            if (source_file_extension == NULL) {
                /* Fallback extensions if not in source filename */
                if (sample->opened_audio_file != NULL) {
                    source_file_extension = sample->opened_audio_file->mod->default_file_extension;
                }
            }

            /* add file extension to filename */
            if (source_file_extension != NULL && strstr(filename, source_file_extension) == NULL) {
                strcat(filename, source_file_extension);
            }

            callbacks->on_file_changed(i, num_files, filename, callbacks->user_data);
            callbacks->on_file_progress_changed(0.0, callbacks->user_data);

            gboolean file_exists = g_file_test(filename, G_FILE_TEST_EXISTS);

            if (file_exists && overwrite_decision == OVERWRITE_DECISION_ASK) {
                overwrite_decision = callbacks->ask_overwrite(filename, callbacks->user_data);
            }

            if (!file_exists || overwrite_decision == OVERWRITE_DECISION_OVERWRITE || overwrite_decision == OVERWRITE_DECISION_OVERWRITE_ALL) {
                if (format_write_file(sample->opened_audio_file, filename, start_pos, end_pos, trampoline_file_progress_changed, callbacks) == -1) {
                    g_warning("Could not write file %s", filename);
                    callbacks->on_error(filename, callbacks->user_data);
                }
            }

            callbacks->on_file_progress_changed(1.0, callbacks->user_data);

            if (overwrite_decision != OVERWRITE_DECISION_SKIP_ALL && overwrite_decision != OVERWRITE_DECISION_OVERWRITE_ALL) {
                overwrite_decision = OVERWRITE_DECISION_ASK;
            }

            i++;
        }

        tbl_cur = g_list_next(tbl_cur);
        tbl_next = g_list_next(tbl_next);
    }

    g_mutex_lock(&sample->write_mutex);
    sample->writing = FALSE;
    g_mutex_unlock(&sample->write_mutex);

    callbacks->on_finished(callbacks->user_data);

    return NULL;
}

void
sample_write_files(Sample *sample, TrackBreakList *list, WriteStatusCallbacks *callbacks, const char *output_dir)
{
    sample->write_thread_data = (WriteThreadData) {
        .sample = sample,
        .list = list,
        .callbacks = callbacks,
        .outputdir = output_dir,
    };

    g_mutex_lock(&sample->write_mutex);
    sample->writing = TRUE;
    g_mutex_unlock(&sample->write_mutex);

    // TODO: Capture thread and properly tear it down - if needed - in sample_close()
    g_thread_unref(g_thread_new("write data", write_thread, &sample->write_thread_data));
}
