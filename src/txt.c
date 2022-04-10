/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2022 Thomas Perl
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

#include "txt.h"

#include "config.h"

#include "track_break.h"

#include <stdio.h>

static void
track_break_write_text(int index, gboolean write, gulong start_offset, gulong end_offset, const gchar *filename, void *user_data)
{
    FILE *fp = user_data;

    if (write) {
        fprintf(fp, "%lu=%s\n", start_offset, filename);
    } else {
        fprintf(fp, "%lu\n", start_offset);
    }
}

gboolean
txt_write_file(const char *txt_filename, const char *wav_filename, TrackBreakList *list)
{
    FILE *fp = fopen(txt_filename, "w");

    if (!fp) {
        g_warning("Could not open %s for writing: %s", txt_filename, strerror(errno));
        return FALSE;
    }

    fprintf(fp, "\n; Created by " PACKAGE " " VERSION "\n; http://thpinfo.com/2006/wavbreaker/tb-file-format.txt\n\n");

    track_break_list_foreach(list, track_break_write_text, fp);

    fprintf(fp, "\n; Total breaks: %d\n; Original file: %s\n\n", g_list_length(list->breaks), wav_filename);
    fclose(fp);
    return TRUE;
}

gboolean
txt_read_file(const char *filename, TrackBreakList *list)
{
    char tmp[1024];
    char *fname;

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        g_warning("Cannot open %s: %s", filename, strerror(errno));
        return FALSE;
    }

    track_break_list_clear(list);

    char *ptr = tmp;
    while(!feof(fp)) {
        int c = fgetc(fp);
        if (c == EOF) {
            break;
        }

        if (c == '\n') {
            *ptr = '\0';
            if (ptr != tmp && tmp[0] != ';') {
                fname = strchr(tmp, '=');
                if (fname == NULL) {
                    track_break_list_add_offset(list, FALSE, atol(tmp), NULL);
                } else {
                    *(fname++) = '\0';
                    while (*fname == ' ') {
                        fname++;
                    }
                    track_break_list_add_offset(list, TRUE, atol(tmp), fname);
                }
            }
            ptr = tmp;
        } else {
            *ptr++ = c;

            if (ptr > tmp+sizeof(tmp)) {
                g_warning("Error parsing file %s", filename);
                fclose(fp);
                return FALSE;
            }
        }
    }

    fclose(fp);
    return TRUE;
}
