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

#include "track_break.h"
#include "sample_info.h"
#include "toc.h"


gboolean
toc_read_file(const char *toc_filename, TrackBreakList *list)
{
    char    buf[1024];
    gchar  *ptr, *eptr;
    guint   offset;

    FILE *fp = fopen(toc_filename, "r");
    if (!fp) {
        return FALSE;
    }

    track_break_list_clear(list);

    do {
        do {
            ptr = fgets(buf, sizeof(buf), fp);
            if (!ptr) {
                if (feof(fp)) {
                    fclose(fp);
                    return TRUE;
                } else {
                    goto error;
                }
            }
        } while(memcmp(buf, "FILE", 4));

        ptr = strrchr(buf, '"');
        if (!ptr) {
            goto error;
        }

        ptr += 2;
        eptr = strchr(ptr, ':');
        if (!eptr) {
            goto error;
        }
        eptr += 6;
        *eptr = '\0';

        offset = msf_time_to_offset(ptr);
        track_break_list_add_offset(list, TRUE, offset, NULL);
    } while (!feof(fp));

    fclose(fp);
    return TRUE;

error:
    fclose( fp );
    return FALSE;
}

struct WriteTOCForEach {
    FILE *fp;
    const char *wav_filename;
};

static void
write_toc_entry(int index, gboolean write, gulong start_offset, gulong end_offset, const gchar *filename, void *user_data)
{
    struct WriteTOCForEach *wtfe = user_data;

    fprintf(wtfe->fp, "\n// track %02d\n", index);
    fprintf(wtfe->fp, "TRACK AUDIO\n");

    gchar *tocTime = track_break_format_timestamp(start_offset, TRUE);
    gchar *tocDuration = track_break_format_timestamp(end_offset - start_offset, TRUE);

    fprintf(wtfe->fp, "FILE \"%s\" %s %s\n", wtfe->wav_filename, tocTime, tocDuration);

    g_free(tocDuration);
    g_free(tocTime);
}

gboolean
toc_write_file(const char *toc_filename, const char *wav_filename, TrackBreakList *list)
{
    FILE *fp = fopen(toc_filename, "w");

    if (!fp) {
        return FALSE;
    }

    fprintf(fp, "// Generated with wavbreaker\n\nCD_DA\n");

    struct WriteTOCForEach wtfe = {
        .fp = fp,
        .wav_filename = wav_filename,
    };

    track_break_list_foreach(list, write_toc_entry, &wtfe);

    fclose(fp);

    return TRUE;
}
