/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2004 Timothy Robinson
 * Copyright (C) 2007 Thomas Perl
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

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <alsa/asoundlib.h>

#include "alsaaudio.h"

static snd_pcm_t *playback_handle;
static unsigned int channels;
static unsigned int bytesPerFrame;

void alsa_audio_close_device()
{
    #ifdef DEBUG
    printf("closing alsa audio device\n");
    #endif
    if (playback_handle != NULL) {
        snd_pcm_close(playback_handle);
    }
    playback_handle = NULL;
    #ifdef DEBUG
    printf("closed alsa audio device\n");
    #endif
}

int alsa_audio_write(unsigned char *devbuf, int size)
{
    int err;
    unsigned char *ptr = devbuf;

    #ifdef DEBUG
    printf("writing to audio device: %d\n", size);
    printf("alsaaudio - buf[0]: %d\n", devbuf[0]);
    #endif

    /*
     * Alsa takes a short value for each sample.  Also, the size parameter
     * is the number of frames.  So, I divide the size by the number
     * of channels multiplied by 2.  The 2 is the difference between
     * the size of a char and a short.
     */
    err = snd_pcm_writei(playback_handle, ptr, size / (channels * bytesPerFrame));
    if (err < 0) {
        /*
         * This code should let us recover from a buffer underrun in ALSA.
         * This happens, for example, when the user backgrounds (^Z) the 
         * wavbreaker process while playing back audio or if the machine is 
         * too slow to keep up with normal playback speed.
         *
         * After every error, we have to call "snd_pcm_prepare" and hope 
         * that ALSA recovers. If not, we have a problem, so spit out an 
         * error message to let the user know something's wrong.
         */
        if( err == -ESTRPIPE) {
            while( (err = snd_pcm_resume( playback_handle)) == -EAGAIN) {
                sleep( 1);
            }
        }

        err = snd_pcm_prepare( playback_handle);

        if( err != 0) {
            fprintf( stderr, "write to audio interface failed (%s)\n", snd_strerror( err));
        }
    }

    return err;
}

int alsa_audio_open_device(const char *audio_dev, SampleInfo *sampleInfo)
{
    int err;
    int dir;
    unsigned int rate;
    unsigned int rrate;
    snd_pcm_format_t format;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;

    unsigned int buffer_time = 500000;         /* ring buffer length in us */
    unsigned int period_time = 100000;         /* period time in us */

    format = SND_PCM_FORMAT_S16_LE;

    if (sampleInfo->bitsPerSample == 16) {
        bytesPerFrame = 2;
        format = SND_PCM_FORMAT_S16_LE;
    } else if (sampleInfo->bitsPerSample == 8) {
        bytesPerFrame = 1;
        format = SND_PCM_FORMAT_U8;
    } else if (sampleInfo->bitsPerSample == 24) {
        bytesPerFrame = 3;
        format = SND_PCM_FORMAT_S24_3LE;
    }
    rate = sampleInfo->samplesPerSec;
    channels = sampleInfo->channels;

    #ifdef DEBUG
    printf("opening alsa audio device (%s)\n", audio_dev);
    #endif
    /* setup dsp device */
    err = snd_pcm_open(&playback_handle, audio_dev, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "cannot open audio device %s (%s)\n", audio_dev,
            snd_strerror(err));
        return -1;
    }
    #ifdef DEBUG
    printf("finished opening alsa audio device (%s)\n", audio_dev);
    #endif
 
    err = snd_pcm_hw_params_malloc(&hw_params);
    if (err < 0) {
        fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n",
            snd_strerror(err));
        return -1;
    }

    err = snd_pcm_hw_params_any(playback_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "cannot initialize hardware parameter structure (%s)\n",
            snd_strerror(err));
        return -1;
    }

    #ifdef DEBUG
    printf("setting access type %d\n", SND_PCM_ACCESS_RW_INTERLEAVED);
    #endif
    err = snd_pcm_hw_params_set_access(playback_handle, hw_params,
        SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        fprintf(stderr, "cannot set access type (%s)\n", snd_strerror(err));
        return -1;
    }

    /* set format */
    #ifdef DEBUG
    printf("SND_PCM_FORMAT_S24_3LE: %d\n", SND_PCM_FORMAT_S24_3LE);
    printf("SND_PCM_FORMAT_S16_LE: %d\n", SND_PCM_FORMAT_S16_LE);
    printf("SND_PCM_FORMAT_S16: %d\n", SND_PCM_FORMAT_S16);
    printf("SND_PCM_FORMAT_U8: %d\n", SND_PCM_FORMAT_U8);
    printf("SND_PCM_FORMAT_S8: %d\n", SND_PCM_FORMAT_S8);
    printf("setting format of audio device (%d)\n", format);
    #endif
    err = snd_pcm_hw_params_set_format(playback_handle, hw_params, format);
    if (err < 0) {
        fprintf(stderr, "cannot set sample format (%s)\n", snd_strerror(err));
        return -1;
    }

    /* set sample rate */
    rrate = rate;
    #ifdef DEBUG
    printf("setting sample rate on audio device (%d)\n", rrate);
    #endif
    err = snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rrate, 0);
    if (err < 0) {
        fprintf(stderr, "cannot set sample rate (%s)\n", snd_strerror(err));
        return -1;
    }
    /*
    if (rrate != rate) {
        printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
        return -EINVAL;
    }
    */

    /* set channels */
    #ifdef DEBUG
    printf("setting channel on audio device (%d)\n", channels);
    #endif
    err = snd_pcm_hw_params_set_channels(playback_handle, hw_params, channels);
    if (err < 0) {
        fprintf(stderr, "cannot set channel count (%s)\n", snd_strerror(err));
        return -1;
    }

    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(playback_handle, hw_params,
        &buffer_time, &dir);
    if (err < 0) {
        printf("Unable to set buffer time %i for playback: %s\n", buffer_time,
            snd_strerror(err));
        return err;
    }

    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(playback_handle, hw_params,
        &period_time, &dir);
    if (err < 0) {
        printf("Unable to set period time %i for playback: %s\n", period_time,
            snd_strerror(err));
        return err;
    }

    /* commit parameters */
    err = snd_pcm_hw_params(playback_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "cannot set parameters (%s)\n", snd_strerror(err));
        return -1;
    }

    err = snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_size);
    if (err < 0) {
        printf("Unable to get buffer size for playback: %s\n",
            snd_strerror(err));
        return err;
    }
    #ifdef DEBUG
    printf("buffer size for playback: %d\n", buffer_size);
    #endif

    err = snd_pcm_hw_params_get_period_size(hw_params, &period_size, 0);
    if (err < 0) {
        printf("Unable to get period size for playback: %s\n",
            snd_strerror(err));
        return err;
    }
    #ifdef DEBUG
    printf("period size for playback: %d\n", period_size);
    #endif
    sampleInfo->bufferSize =
	snd_pcm_frames_to_bytes(playback_handle, period_size);

    snd_pcm_hw_params_free(hw_params);

    err = snd_pcm_prepare(playback_handle);
    if (err < 0) {
        fprintf(stderr, "cannot prepare audio interface for use (%s)\n",
            snd_strerror(err));
        return -1;
    }

    return 0;
}

