/* -*- c-basic-offset: 4 -*- */
/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2004 Timothy Robinson
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
#include <stdint.h>
#include <inttypes.h>
#include <libgen.h>
#include <math.h>

#include "format.h"

static char *
format_duration(uint64_t duration)
{
    uint64_t seconds = duration / 1000;
    uint64_t fraction = duration % 1000;
    return g_strdup_printf("%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ".%03" PRIu64, seconds / 60 / 60, (seconds / 60) % 60, seconds % 60, fraction);
}

int cmd_wavinfo(int argc, char *argv[])
{
    int i;

    if( argc < 2) {
        printf( "Usage: %s [file1.wav] [...]\n", basename( argv[0]));
        return 1;
    }

    format_init();

    for( i = 1; i < argc; i++) {
        char *error_message = NULL;
        OpenedAudioFile *oaf = format_open_file(argv[i], &error_message);

        if (oaf == NULL) {
            printf("Error opening %s: %s\n", argv[i], error_message);
            g_free(error_message);
            continue;
        }

        char *duration = format_duration((uint64_t)oaf->sample_info.numBytes * 1000 / (uint64_t)oaf->sample_info.avgBytesPerSec);

        printf("File name:      %s\n", argv[i]);
        printf("File format:    %s\n", oaf->mod->name);
        if (oaf->details) {
            printf("Format details: %s\n", oaf->details);
        }
        printf("Duration:       %s (%lu samples)\n", duration, oaf->sample_info.numBytes / oaf->sample_info.blockAlign);
        printf("Format:         %d Hz / %d ch / %d bit",
                oaf->sample_info.samplesPerSec,
                oaf->sample_info.channels,
                oaf->sample_info.bitsPerSample);

        printf("\n");

        //printf("avgBytesPerSec: %d\n", oaf->sample_info.avgBytesPerSec);
        //printf("blockAlign:     %d\n", oaf->sample_info.blockAlign);
        //printf("numBytes:       %lu\n", oaf->sample_info.numBytes);
        //printf("blockSize:      %d\n", oaf->sample_info.blockSize);

        // TODO: Maybe dump extra metadata fields

        g_free(duration);
        format_close_file(oaf);

        printf("\n");
    }

    return 0;
}
