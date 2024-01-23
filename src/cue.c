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
#include <libcue.h>

gboolean
cue_read_file(const char *cue_filename, TrackBreakList *list)
{
    FILE *fp;

    fp = fopen(cue_filename, "r");
    if (!fp) {
        g_warning("Error opening CUE file: %s", cue_filename);
        return FALSE;
    }

    Cd *cd = cue_parse_file(fp);
    if (!cd) {
        g_warning("Unable to parse CUE file: %s", cue_filename);
        fclose(fp);
        return FALSE;
    }

    int trackCount = cd_get_ntrack(cd);

    // Track numbers, not array elements
    for (int i = 1; i <= trackCount; i++) {
        Track *track = cd_get_track(cd, i);
        if (!track) {
            g_warning("Track %i information missing from CUE sheet", i);
            continue;
        }

        gulong offset = track_get_start(track);
        track_break_list_add_offset(list, TRUE, offset, NULL);
    }

    fclose(fp);

    return TRUE;
}

static void
track_break_write_cue(int index, gboolean write, gulong start_offset, gulong end_offset, const gchar *filename, void *user_data)
{
    FILE *fp = user_data;

    gchar *time = track_break_format_timestamp(start_offset, TRUE);

    fprintf(fp, "\n\tTRACK %02d AUDIO\n", index + 1);
    fprintf(fp, "\t\tINDEX 01 %s\n", time);

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

    fprintf(fp, "REM Generated with wavbreaker\n");
    fprintf(fp, "FILE \"%s\" WAVE\n", audio_filename);

    track_break_list_foreach(list, track_break_write_cue, fp);

    fclose(fp);

    return TRUE;
}
