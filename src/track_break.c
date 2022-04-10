/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2002-2005 Timothy Robinson
 * Copyright (C) 2007, 2022 Thomas Perl
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

#include "track_break.h"

#include "sample_info.h"

#include <stdio.h>

gchar *
track_break_format_timestamp(gulong time, gboolean toc_format)
{
    int min = time / (CD_BLOCKS_PER_SEC * 60);
    int sec = time % (CD_BLOCKS_PER_SEC * 60);
    int subsec = sec % CD_BLOCKS_PER_SEC;
    sec = sec / CD_BLOCKS_PER_SEC;

    if (toc_format) {
        return g_strdup_printf("%d:%02d:%02d", min, sec, subsec);
    } else {
        return g_strdup_printf("%d:%02d.%02d", min, sec, subsec);
    }
}

gchar *
track_break_format_offset(TrackBreak *track_break, gboolean toc_format)
{
    return track_break_format_timestamp(track_break->offset, toc_format);
}

gchar *
track_break_format_duration(TrackBreak *track_break, gulong next_offset, gboolean toc_format)
{
    return track_break_format_timestamp(next_offset - track_break->offset, toc_format);
}

/** @param str Time in MM:SS:FF format (where there are CD_BLOCKS_PER_SEC frames per second).
 *  @return offset in frames.
 */
guint
msf_time_to_offset(const gchar *str)
{
    guint   offset;
    int    mm = 0, ss = 0, ff = 0;
    int    consumed;

    consumed = sscanf(str, "%d:%d:%d", &mm, &ss, &ff);
    if (consumed != 3) {
        return 0;
    }

    offset  = mm * CD_BLOCKS_PER_SEC * 60;
    offset += ss * CD_BLOCKS_PER_SEC;
    offset += ff;

    return offset;
}

typedef void (*track_break_visitor_func)(int index, gboolean write, gulong start_offset, gulong end_offset, const gchar *filename, void *user_data);

void
track_break_list_foreach(GList *list, gulong total_duration, track_break_visitor_func visitor, void *visitor_user_data)
{
    int index = 0;

    GList *cur = g_list_first(list);
    while (cur != NULL) {
        TrackBreak *track_break = cur->data;

        GList *next = g_list_next(cur);
        TrackBreak *next_track_break = (next != NULL) ? next->data : NULL;

        gulong start_offset = track_break->offset;
        gulong end_offset = (next_track_break != NULL) ? next_track_break->offset : total_duration;

        visitor(index++, track_break->write, start_offset, end_offset, track_break->filename, visitor_user_data);

        cur = next;
    }
}
