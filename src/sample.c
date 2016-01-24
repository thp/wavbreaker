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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

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

#if defined(HAVE_MPG123)
#include <mpg123.h>
#endif

enum AudioType {
    UNKNOWN = 0,
    CDDA = 1,
    WAV = 2,
#if defined(HAVE_MPG123)
    MP3 = 3,
#endif
};

SampleInfo sampleInfo;
static AudioFunctionPointers *audio_function_pointers;
static unsigned long sample_start = 0;
static int playing = 0;
static int writing = 0;
static gboolean kill_play_thread = FALSE;
static enum AudioType audio_type;

static char *sample_file = NULL;
static FILE *read_sample_fp = NULL;
static FILE *write_sample_fp = NULL;

#if defined(HAVE_MPG123)
static mpg123_handle *mpg123 = NULL;
static size_t mpg123_offset = 0;
#endif

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

#if defined(HAVE_MPG123)
int
mp3_read_sample(mpg123_handle *handle,
                unsigned char *buf,
                int buf_size,
                unsigned long start_pos)
{
    unsigned long result = -1;

    if (mpg123_offset != start_pos) {
        mpg123_seek(handle, start_pos / sampleInfo.blockAlign, SEEK_SET);
        mpg123_offset = start_pos;
    }

    if (mpg123_read(handle, buf, buf_size, &result) == MPG123_OK) {
        mpg123_offset += result;
    } else {
        fprintf(stderr, "MP3 decoding failed: %s\n", mpg123_strerror(mpg123));
    }

    return result;
}

//#define WAVBREAKER_MP3_DEBUG

static gboolean
mp3_parse_header(uint32_t header, uint32_t *bitrate, uint32_t *frequency, uint32_t *samples, uint32_t *framesize)
{
    // http://www.datavoyage.com/mpgscript/mpeghdr.htm
    int a = ((header >> 21) & 0x07ff);
    int b = ((header >> 19) & 0x0003);
    int c = ((header >> 17) & 0x0003);
    int e = ((header >> 12) & 0x000f);
    int f = ((header >> 10) & 0x0003);
    int g = ((header >> 9) & 0x0001);

    static const int BITRATES_V1_L3[] = { -1, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1 };
    static const int FREQUENCIES[] = { 44100, 48000, 32000, 0 };

    if (a != 0x7ff /* sync */ ||
            b != 0x3 /* MPEG1 */ ||
            c != 0x1 /* Layer III */ ||
            e == 0x0 /* freeform bitrate */ || e == 0xf /* invalid bitrate */ ||
            f == 0x3 /* invalid frequency */) {
        return FALSE;
    }

    *bitrate = BITRATES_V1_L3[e];
    *frequency = FREQUENCIES[f];
    *samples = 1152;
    *framesize = (int)((*samples) / 8 * 1000 * (*bitrate) / (*frequency)) + g /* padding */;

#if defined(WAVBREAKER_MP3_DEBUG)
    static const char *VERSIONS[] = { "MPEG 2.5", NULL, "MPEG 2", "MPEG 1" };
    static const char *LAYERS[] = { NULL, "III", "II", "I" };

    const char *mpeg_version = VERSIONS[b];
    const char *layer = LAYERS[c];

    fprintf(stderr, "MP3 frame: %s %s, %dHz, %dkbps, %d samples (%d bytes)\n", mpeg_version, layer,
            *frequency, *bitrate, *samples, *framesize);
#endif /* WAVBREAKER_MP3_DEBUG */

    return TRUE;
}

void
mp3_write_file(FILE *input_file, const char *filename, SampleInfo *sampleInfo, unsigned long start_pos, unsigned long end_pos)
{
    start_pos /= sampleInfo->blockSize;
    end_pos /= sampleInfo->blockSize;

    FILE *output_file = fopen(filename, "wb");

    if (!output_file) {
        fprintf(stderr, "Could not open '%s' for writing\n", filename);
        return;
    }

    fseek(input_file, 0, SEEK_SET);

    uint32_t header = 0x00000000;
    uint32_t sample_position = 0;
    uint32_t file_offset = 0;
    uint32_t last_frame_end = 0;
    uint32_t frames_written = 0;

    while (!feof(input_file)) {
        fseek(input_file, file_offset, SEEK_SET);

        int a = fgetc(input_file);
        if (a == EOF) {
            break;
        }

        header = ((header & 0xffffff) << 8) | (a & 0xff);

        uint32_t bitrate = 0;
        uint32_t frequency = 0;
        uint32_t samples = 0;
        uint32_t framesize = 0;

        if (mp3_parse_header(header, &bitrate, &frequency, &samples, &framesize)) {
            uint32_t frame_start = file_offset - 3;

            if (last_frame_end < frame_start) {
                fprintf(stderr, "Skipped non-frame data in MP3 @ 0x%08x (%d bytes)\n",
                        last_frame_end, frame_start - last_frame_end);
            }

            uint32_t start_samples = start_pos * frequency / CD_BLOCKS_PER_SEC;
            if (start_samples <= sample_position) {
                // Write this frame to the output file
                char *buf = malloc(framesize);
                fseek(input_file, frame_start, SEEK_SET);
                if (fread(buf, 1, framesize, input_file) != framesize) {
                    fprintf(stderr, "Tried to read over the end of the input file\n");
                    break;
                }
                if (fwrite(buf, 1, framesize, output_file) != framesize) {
                    fprintf(stderr, "Failed to write %d bytes to output file\n", framesize);
                    break;
                }
                free(buf);

                frames_written++;

                uint32_t end_samples = end_pos * frequency / CD_BLOCKS_PER_SEC;
                if (end_samples > 0 && end_samples <= sample_position + samples) {
                    // Done writing this part
                    break;
                }
            }

            sample_position += samples;

            file_offset = frame_start + framesize;
            last_frame_end = file_offset;

            header = 0x00000000;
        } else {
            file_offset++;
        }
    }

#if defined(WAVBREAKER_MP3_DEBUG)
    fprintf(stderr, "Wrote %d MP3 frames from '%s' to '%s'\n", frames_written, sample_file, filename);
#endif /* WAVBREAKER_MP3_DEBUG */

    fclose(output_file);
}
#endif

void sample_init()
{
    g_mutex_init(&mutex);

#if defined(HAVE_MPG123)
    if (mpg123_init() != MPG123_OK) {
        fprintf(stderr, "Failed to initialize libmpg123\n");
    }
#endif
}

static gpointer play_thread(gpointer thread_data)
{
    int read_ret = 0;
    int i;
    guint *play_marker = (guint *)thread_data;
    unsigned char *devbuf;

    audio_function_pointers = get_audio_function_pointers();

    /*
    printf("play_thread: calling open_audio_device\n");
    printf("devault outputdev: %s\n", audio_function_pointers->get_outputdev());
    */
    if (audio_function_pointers->audio_open_device(
        audio_function_pointers->get_outputdev(), &sampleInfo) != 0) {

        g_mutex_lock(&mutex);
        playing = 0;
        audio_function_pointers->audio_close_device();
        g_mutex_unlock(&mutex);
        //printf("play_thread: return from open_audio_device != 0\n");
        return NULL;
    }
    //printf("play_thread: return from open_audio_device\n");

    i = 0;

    devbuf = malloc(sampleInfo.bufferSize);
    if (devbuf == NULL) {
        g_mutex_lock(&mutex);
        playing = 0;
        audio_function_pointers->audio_close_device();
        g_mutex_unlock(&mutex);
        printf("play_thread: out of memory\n");
        return NULL;
    }

    if (audio_type == CDDA) {
        read_ret = cdda_read_sample(read_sample_fp, devbuf, sampleInfo.bufferSize,
            sample_start + (sampleInfo.bufferSize * i++));
    } else if (audio_type == WAV) {
        read_ret = wav_read_sample(read_sample_fp, devbuf, sampleInfo.bufferSize,
            sample_start + (sampleInfo.bufferSize * i++));
#if defined(HAVE_MPG123)
    } else if (audio_type == MP3) {
        read_ret = mp3_read_sample(mpg123, devbuf, sampleInfo.bufferSize,
            sample_start + (sampleInfo.bufferSize * i++));
#endif
    }

    while (read_ret > 0 && read_ret <= sampleInfo.bufferSize) {
        /*
        if (read_ret < 0) {
            printf("read_ret: %d\n", read_ret);
        }
        */

        audio_function_pointers->audio_write(devbuf, read_ret);

        if (g_mutex_trylock(&mutex)) {
            if (kill_play_thread == TRUE) {
                audio_function_pointers->audio_close_device();
                playing = 0;
                kill_play_thread = FALSE;
                g_mutex_unlock(&mutex);
                return NULL;
            }
            g_mutex_unlock(&mutex);
        }

        if (audio_type == CDDA) {
            read_ret = cdda_read_sample(read_sample_fp, devbuf,
                sampleInfo.bufferSize,
                sample_start + (sampleInfo.bufferSize * i++));
        } else if (audio_type == WAV) {
            read_ret = wav_read_sample(read_sample_fp, devbuf,
                sampleInfo.bufferSize,
                sample_start + (sampleInfo.bufferSize * i++));
#if defined(HAVE_MPG123)
        } else if (audio_type == MP3) {
            read_ret = mp3_read_sample(mpg123, devbuf,
                sampleInfo.bufferSize,
                sample_start + (sampleInfo.bufferSize * i++));
#endif
        }

        *play_marker = ((sampleInfo.bufferSize * i) + sample_start) /
	    sampleInfo.blockSize;
    }

    g_mutex_lock(&mutex);

    audio_function_pointers->audio_close_device();
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

    if (sample_file == NULL) {
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
    g_thread_yield();
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
                                     "%s", message);
    
    gtk_dialog_add_button( GTK_DIALOG(dialog), _("Big endian"), WB_RESPONSE_BIG_ENDIAN);
    gtk_dialog_add_button( GTK_DIALOG(dialog), _("Little endian"), WB_RESPONSE_LITTLE_ENDIAN);

    gtk_message_dialog_format_secondary_text( dialog, "%s", info_text);
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

    audio_type = UNKNOWN;
    if( wav_read_header(sample_file, &sampleInfo, 0) == 0) {
        audio_type = WAV;
    }

#if defined(HAVE_MPG123)
    if (audio_type == UNKNOWN) {
        fprintf(stderr, "Trying to open as MP3...\n");

        if (mpg123 != NULL) {
            mpg123_close(mpg123), mpg123 = NULL;
        }

        if ((mpg123 = mpg123_new(NULL, NULL)) == NULL) {
            fprintf(stderr, "Failed to create MP3 decoder\n");
        }

        if (mpg123_open(mpg123, sample_file) == MPG123_OK) {
            fprintf(stderr, "Detected MP3 format\n");

            long rate;
            int channels;
            int encoding;
            if (mpg123_getformat(mpg123, &rate, &channels, &encoding) != MPG123_OK ) {
                fprintf(stderr, "Could not get file format\n");
            }

            fprintf(stderr, "Scanning MP3 file...\n");
            if (mpg123_scan(mpg123) != MPG123_OK) {
                fprintf(stderr, "Failed to scan MP3\n");
            }

            struct mpg123_frameinfo fi;
            memset(&fi, 0, sizeof(fi));

            if (mpg123_info(mpg123, &fi) == MPG123_OK) {
                sampleInfo.channels = (fi.mode == MPG123_M_MONO) ? 1 : 2;
                sampleInfo.samplesPerSec = fi.rate;
                sampleInfo.bitsPerSample = 16;

                sampleInfo.blockAlign = sampleInfo.channels * (sampleInfo.bitsPerSample / 8);
                sampleInfo.avgBytesPerSec = sampleInfo.blockAlign * sampleInfo.samplesPerSec;
                sampleInfo.bufferSize = DEFAULT_BUF_SIZE;
                sampleInfo.blockSize = sampleInfo.avgBytesPerSec / CD_BLOCKS_PER_SEC;
                sampleInfo.numBytes = mpg123_length(mpg123) * sampleInfo.blockAlign;
                fprintf(stderr, "Channels: %d, rate: %d, bits: %d, decoded size: %u\n",
                        sampleInfo.channels, sampleInfo.samplesPerSec,
                        sampleInfo.bitsPerSample, sampleInfo.numBytes);

                mpg123_format_none(mpg123);
                if (mpg123_format(mpg123, sampleInfo.samplesPerSec,
                                  (sampleInfo.channels == 1) ? MPG123_STEREO : MPG123_MONO,
                                  MPG123_ENC_SIGNED_16) != MPG123_OK) {
                    fprintf(stderr, "Failed to set mpg123 format\n");
                } else {
                    fprintf(stderr, "MP3 file reading successfully set up\n");
                    audio_type = MP3;
                }
            }
        }

    }
#endif


    if (audio_type == UNKNOWN) {
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

    if (audio_type == WAV || audio_type == CDDA) {
        sampleInfo.blockSize = (((sampleInfo.bitsPerSample / 8) *
                                 sampleInfo.channels * sampleInfo.samplesPerSec) /
                                CD_BLOCKS_PER_SEC);

        if ((read_sample_fp = fopen(sample_file, "rb")) == NULL) {
            snprintf(error_message, ERROR_MESSAGE_SIZE, _("Error opening %s: %s"),
                     sample_file, strerror( errno));
            return 2;
        }
    }

    open_thread_data.graphData = graphData;
    open_thread_data.pct = pct;

    fflush(stdout);
    thread = g_thread_new("open file", open_thread, &open_thread_data);

    g_thread_yield();
    return 0;
}

void sample_close_file()
{
#if defined(HAVE_MPG123)
    if (mpg123 != NULL) {
        mpg123_close(mpg123), mpg123 = NULL;
    }
#endif

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
#if defined(HAVE_MPG123)
    } else if (audio_type == MP3) {
        ret = mp3_read_sample(mpg123, devbuf, sampleInfo.blockSize,
			      sampleInfo.blockSize * i);
#endif
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
#if defined(HAVE_MPG123)
        } else if (audio_type == MP3) {
            ret = mp3_read_sample(mpg123, devbuf,
				  sampleInfo.blockSize,
				  sampleInfo.blockSize * i);
#endif
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
#if defined(HAVE_MPG123)
            } else if ((audio_type == MP3) && (!strstr(filename, ".mp3"))) {
                strcat(filename, ".mp3");
#endif
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
                    cdda_write_file(write_sample_fp, filename,
                        sampleInfo.bufferSize, start_pos, end_pos);
                } else if (audio_type == WAV) {
                    wav_write_file(write_sample_fp, filename,
					 sampleInfo.blockSize,
					 &sampleInfo, start_pos, end_pos,
					 &write_info->pct_done);
#if defined(HAVE_MPG123)
                } else if (audio_type == MP3) {
                    mp3_write_file(write_sample_fp, filename, &sampleInfo, start_pos, end_pos);
#endif
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

    thread = g_thread_new("write data", write_thread, &wtd);
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

    thread = g_thread_new("merge files", merge_thread, &mtd);
}

