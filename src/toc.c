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

int toc_read_file(char *toc_filename, GList *breaks)
{
    return 0;
}

int toc_write_file(char *toc_filename, char *wav_filename, GList *breaks)
{
    FILE *fp;
    TrackBreak *next_break = NULL;

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
            if (i != len-1) {
                fprintf(fp, "FILE \"%s\" %s %s\n",
                        wav_filename, next_break->time, next_break->duration);
            } else {
                fprintf(fp, "FILE \"%s\" %s\n", wav_filename, next_break->time);
            }
        }
        i++;
    }
    fclose(fp);
    return 0;
}

