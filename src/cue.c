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

#include "cue.h"

#include "track_break.h"

#include <stdio.h>
#include <string.h>

gboolean
cue_read_file(const char *cue_filename, TrackBreakList *list)
{
    FILE *fp;
    /* These buffers must have the same length */
    char line_buf[1024];
    char buf[1024];
    int next_track = 1;

    fp = fopen(cue_filename, "r");
    if (!fp) {
        return FALSE;
    }

    track_break_list_clear(list);

    /* Read the first line: FILE "foo.wav" WAVE */
    if (!fgets(line_buf, sizeof(line_buf), fp)) {
        fclose(fp);
        return FALSE;
    }

    /* Check that it starts with FILE and ends with WAVE */
    if (sscanf(line_buf, "FILE %s WAVE", buf) != 1) {
        goto error;
    }

    while (!feof(fp)) {
        int N;
        int read;
        guint offset;

        read = fscanf(fp, "TRACK %02d AUDIO\n", &N);
        if (feof(fp)) {
            break;
        }

        if (read != 1 || N != next_track) {
            goto error;
        }

        if (!fgets(line_buf, sizeof(line_buf), fp)) {
            goto error;
        }

        read = sscanf(line_buf, "INDEX %02d %s", &N, buf);
        if (read != 2 || N != 1) {
            goto error;
        }

        offset = msf_time_to_offset(buf);
        track_break_list_add_offset(list, TRUE, offset, NULL);

        ++next_track;
    }

    return TRUE;

error:
    fclose(fp);
    return FALSE;
}

static void
track_break_write_cue(int index, gboolean write, gulong start_offset, gulong end_offset, const gchar *filename, void *user_data)
{
    FILE *fp = user_data;

    gchar *time = track_break_format_timestamp(start_offset, TRUE);

    fprintf(fp, "TRACK %02d AUDIO\n", index + 1);
    fprintf(fp, "INDEX 01 %s\n", time);

    g_free(time);
}

gboolean
cue_write_file(const char *cue_filename, const char *audio_filename, TrackBreakList *list)
{
    FILE *fp = fopen(cue_filename, "w");
    if (!fp) {
        g_warning("Error opening %s.", cue_filename);
        return FALSE;
    }

    fprintf(fp, "FILE \"%s\" WAVE\n", audio_filename);

    track_break_list_foreach(list, track_break_write_cue, fp);

    fclose(fp);

    return TRUE;
}
