/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2005 Timothy Robinson
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

#include "wavbreaker.h"
#include "toc.h"

char *convert_wavbreaker_time_to_toc_time(const char *wavbreakerTime);

int toc_read_file(const char *toc_filename, GList *breaks)
{
    return 0;
}

int toc_write_file(const char *toc_filename, const char *wav_filename, GList *breaks)
{
    FILE *fp;
    TrackBreak *next_break = NULL;
    char *tocTime;
    char *tocDuration;

    fp = fopen(toc_filename, "w");
    if(!fp) return 1;

    fprintf(fp, "// Generated with wavbreaker\n\nCD_DA\n");
    int i = 0;
    int len = g_list_length(breaks);
    while (i < len) {
        next_break = (TrackBreak *) g_list_nth_data(breaks, i);
        if (next_break != NULL) {
            fprintf(fp, "\n// track %02d\n", i);
            fprintf(fp, "TRACK AUDIO\n");

            tocTime = convert_wavbreaker_time_to_toc_time(next_break->time);
            if (i != len-1) {
                tocDuration = convert_wavbreaker_time_to_toc_time(next_break->duration);
                fprintf(fp, "FILE \"%s\" %s %s\n",
                        wav_filename, tocTime, tocDuration);
                g_free(tocDuration);
            } else {
                fprintf(fp, "FILE \"%s\" %s\n", wav_filename, tocTime);
            }
            g_free(tocTime);
        }
        i++;
    }
    fclose(fp);
    return 0;
}

char *convert_wavbreaker_time_to_toc_time(const char *wavbreakerTime) {
    char *tocTime;
    int i;
    int p = 0;

    printf("start of convert_wavbreaker_time_to_toc_time\n");
    printf("called convert_wavbreaker_time_to_toc_time with: %s\n", wavbreakerTime);
    tocTime = g_strdup(wavbreakerTime);
    printf("got to: %d\n", p++);

    i = 0;
    while (tocTime[i] != '\0') {
    printf("got to: %d\n", p++);
        printf("looping with: %d", tocTime[i]);
        if (tocTime[i] == '.') {
            tocTime[i] = ':';
        }
        i++;
    }

    printf("end of convert_wavbreaker_time_to_toc_time\n");
    return tocTime;
}

