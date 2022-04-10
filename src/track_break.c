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
track_break_format_time(TrackBreak *track_break, gboolean toc_format)
{
    guint time = track_break->offset;

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
