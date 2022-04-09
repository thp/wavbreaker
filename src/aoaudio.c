/*
 * libao output module for wavbreaker
 * Copyright (C) 2015 Thomas Perl
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
#include <string.h>

#include <ao/ao.h>

#include "wavbreaker.h"
#include "aoaudio.h"


static ao_device *device;

void ao_audio_close_device()
{
    if (device) {
        ao_close(device);
        device = NULL;

        ao_shutdown();
    }
}

int ao_audio_write(unsigned char *devbuf, int size)
{
    if (device) {
        if (ao_play(device, (char *)devbuf, size) == 0) {
            fprintf(stderr, "Error in ao_play()\n");
            return -1;
        }
        return 0;
    }

    return -1;
}

int ao_audio_open_device(SampleInfo *sampleInfo)
{
    int default_driver;
    ao_sample_format format;

    ao_initialize();

    default_driver = ao_default_driver_id();
    memset(&format, 0, sizeof(format));
    format.bits = sampleInfo->bitsPerSample;
    format.channels = sampleInfo->channels;
    format.rate = sampleInfo->samplesPerSec;
    format.byte_format = AO_FMT_LITTLE;

    device = ao_open_live(default_driver, &format, NULL);

    if (device == NULL) {
        fprintf(stderr, "Cannot open default libao device\n");
        return -1;
    }

    return 0;
}
