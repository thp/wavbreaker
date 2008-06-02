/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2003 Timothy Robinson
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
#include <sys/soundcard.h>

#include "linuxaudio.h"

#define DEBUG 0

static int audio_fd;

void oss_audio_close_device()
{
    if (DEBUG) {
        printf("closing audio device\n");
    }
    close(audio_fd);
}

int oss_audio_write(unsigned char *devbuf, int size)
{
    if (DEBUG) {
        printf("writing to audio device: %d\n", size);
    }
    return write(audio_fd, devbuf, size);
}

int oss_audio_open_device(const char *audio_dev, SampleInfo *sampleInfo)
{
    int format, speed, channels;
    int ret;

    format = AFMT_S16_LE;

    if (sampleInfo->bitsPerSample == 16) {
        format = AFMT_S16_LE;
    } else if (sampleInfo->bitsPerSample == 8) {
        format = AFMT_U8;
     } else if (sampleInfo->bitsPerSample == 24) { 
 	format = AFMT_S24_LE; 
    }
    speed = sampleInfo->samplesPerSec;
    channels = sampleInfo->channels;
    sampleInfo->bufferSize = DEFAULT_BUF_SIZE;

    if (DEBUG) {
        printf("opening audio device (%s)\n", audio_dev);
        printf("opening audio device...");
        fflush(stdout);
    }
    /* setup dsp device */
    if ((audio_fd = open(audio_dev, O_WRONLY)) == -1) {
        perror("open");
        printf("error opening %s\n", audio_dev);
        return audio_fd;
    }

    if (DEBUG) {
        printf("done\n");
        printf("setting format of audio device\n");
    }
    /* set format */
    ret = format;
    if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &ret) == -1) {
        perror("SNDCTL_DSP_SETFMT");
        return -1; 
    }

    if (format != ret) {  
        printf("Device, %s, does not support %d for format.\n", audio_dev, format);
        return -1;
    }

    if (DEBUG) {
        printf("setting channel on audio device\n");
    }
    /* set channels */
    ret = channels;
    if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &ret) == -1) {
        perror("SNDCTL_DSP_CHANNELS");
        return -1;
    }       

    if (channels != ret) {
        printf("Device, %s, doesn't support %d channels\n", audio_dev, channels);
        return -1;
    }

    if (DEBUG) {
        printf("setting speed on audio device\n");
    }
    /* set speed */
    ret = speed;
    if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &ret) == -1) {
        perror("SNDCTL_DSP_SPEED");
        return -1;
    }

    if (speed != ret) {
        printf("Device, %s, doesn't support speed of %d\n", audio_dev, speed);
        printf("Speed returned was %d\n", ret);
        return -1;
    }

    return 0;
}

