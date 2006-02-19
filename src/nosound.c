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

#include "nosound.h"

#define DEBUG 0

void nosound_audio_close_device()
{
    if (DEBUG) {
        printf("closing audio device\n");
    }
}

/**
 * returns int the number of bytes written to the audio device
 */
int nosound_audio_write(unsigned char *devbuf, int size)
{
    if (DEBUG) {
//        printf("writing to audio device: %d\n", size);
    }
    return size;
}

/**
 * returns int anything other than zero indicates an error
 */
int nosound_audio_open_device(const char *audio_dev, SampleInfo *sampleInfo)
{
    if (DEBUG) {
        printf("opening audio device: %s\n", audio_dev);
    }
    sampleInfo->bufferSize = DEFAULT_BUF_SIZE;
    return 0;
}

