/*
 * PulseAudio output module for wavbreaker
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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <stdlib.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#include "wavbreaker.h"
#include "pulseaudio.h"

static pa_simple *handle;

void pulse_audio_close_device()
{
    int error;
    if (handle != NULL) {
        if (pa_simple_drain(handle, &error) < 0) {
            fprintf(stderr, "Cannot drain PulseAudio: %s\n", pa_strerror(error));
        }
        pa_simple_free(handle);
        handle = NULL;
    }
}

int pulse_audio_write(unsigned char *devbuf, int size)
{
    int error;

    if (pa_simple_write(handle, devbuf, size, &error) < 0) {
        fprintf(stderr, "Cannot write to PulseAudio: %s\n", pa_strerror(error));
        return -1;
    } else {
        return 0;
    }
}

int pulse_audio_open_device(const char *audio_dev, SampleInfo *sampleInfo)
{
    int error;
    static pa_sample_spec ss;
    gchar* fn;

    if (sampleInfo->bitsPerSample == 16) {
        ss.format = PA_SAMPLE_S16LE;
    } else if (sampleInfo->bitsPerSample == 8) {
        ss.format = PA_SAMPLE_U8;
    } else {
        ss.format = PA_SAMPLE_S16LE;
    }

    ss.rate = sampleInfo->samplesPerSec;
    ss.channels = sampleInfo->channels;
    sampleInfo->bufferSize = DEFAULT_BUF_SIZE;

    fn = g_strdup(sample_filename);
    handle = pa_simple_new(NULL, "wavbreaker", PA_STREAM_PLAYBACK, NULL, basename(fn), &ss, NULL, NULL, &error);
    g_free(fn);

    if (handle == NULL) {
        fprintf(stderr, "Cannot connect to PulseAudio: %s\n", pa_strerror(error));
        return -1;
    }

    return 0;
}

