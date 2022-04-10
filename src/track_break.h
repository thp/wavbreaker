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

void
track_break_rename(TrackBreak *track_break, const char *new_name);

gchar *
track_break_format_timestamp(gulong time, gboolean toc_format);

guint
msf_time_to_offset(const gchar *str);


typedef struct TrackBreakList {
    gchar *basename;
    GList *breaks;
    gulong total_duration;
} TrackBreakList;

gchar *
track_break_get_filename(TrackBreak *track_break, TrackBreakList *list);


TrackBreakList *
track_break_list_new(const char *basename);

void
track_break_list_clear(TrackBreakList *list);

TrackBreak *
track_break_list_add_offset(TrackBreakList *list, gboolean write, gulong offset, const char *filename);

void
track_break_list_set_total_duration(TrackBreakList *list, gulong total_duration);

typedef void (*track_break_visitor_func)(int index, gboolean write, gulong start_offset, gulong end_offset, const gchar *filename, void *user_data);

void
track_break_list_foreach(TrackBreakList *list, track_break_visitor_func visitor, void *visitor_user_data);

void
track_break_list_remove_nth_element(TrackBreakList *list, int index);

void
track_break_list_reset_filenames(TrackBreakList *list);

void
track_break_list_free(TrackBreakList *list);
