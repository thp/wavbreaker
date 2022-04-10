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

#pragma once

#include <glib.h>

typedef struct TrackBreak_ TrackBreak;
struct TrackBreak_ {
    gboolean  write;
    gulong    offset;
    gchar     *filename;
};

gchar *
track_break_format_time(TrackBreak *track_break, gboolean toc_format);

gchar *
track_break_format_duration(TrackBreak *track_break, gulong next_offset, gboolean toc_format);

guint
msf_time_to_offset(const gchar *str);
