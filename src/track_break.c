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

// FIXME: Remove appconfig dependency
#include "appconfig.h"

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

void
track_break_rename(TrackBreak *track_break, const char *new_name)
{
    if (track_break->filename != NULL) {
        g_free(g_steal_pointer(&track_break->filename));
    }

    if (new_name != NULL && strlen(new_name) > 0) {
        track_break->filename = g_strdup(new_name);
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

TrackBreakList *
track_break_list_new(const char *basename)
{
    TrackBreakList *list = g_new0(TrackBreakList, 1);
    list->basename = g_strdup(basename);
    return list;
}

void
track_break_list_set_total_duration(TrackBreakList *list, gulong total_duration)
{
    list->total_duration = total_duration;
}

void
track_break_list_foreach(TrackBreakList *list, track_break_visitor_func visitor, void *visitor_user_data)
{
    int index = 0;

    GList *cur = g_list_first(list->breaks);
    while (cur != NULL) {
        TrackBreak *track_break = cur->data;

        GList *next = g_list_next(cur);
        TrackBreak *next_track_break = (next != NULL) ? next->data : NULL;

        gulong start_offset = track_break->offset;
        gulong end_offset = (next_track_break != NULL) ? next_track_break->offset : list->total_duration;

        gchar *filename = track_break_get_filename(track_break, list);

        visitor(index++, track_break->write, start_offset, end_offset, filename, visitor_user_data);

        g_free(filename);

        cur = next;
    }
}

static void
track_break_free_element(gpointer data, gpointer user_data)
{
    TrackBreak *track_break = data;

    g_free(track_break->filename);
    g_free(track_break);
}

void
track_break_list_remove_nth_element(TrackBreakList *list, int index)
{
    TrackBreak *track_break = g_list_nth_data(list->breaks, index);
    list->breaks = g_list_remove(list->breaks, track_break);
    track_break_free_element(track_break, NULL);
}

void
track_break_list_clear(TrackBreakList *list)
{
    g_list_foreach(list->breaks, track_break_free_element, NULL);
    g_list_free(g_steal_pointer(&list->breaks));
}

void
track_break_list_free(TrackBreakList *list)
{
    track_break_list_clear(list);
    g_free(list->basename);
    g_free(list);
}

static gint
track_break_cmp(gconstpointer a, gconstpointer b)
{
    const TrackBreak *x = a;
    const TrackBreak *y = b;

    if (x->offset < y->offset) {
        return -1;
    } else if (x->offset > y->offset) {
        return 1;
    } else {
        return 0;
    }
}

TrackBreak *
track_break_list_add_offset(TrackBreakList *list, gboolean write, gulong offset, const char *filename)
{
    if (offset > list->total_duration) {
        g_warning("Offset out of range");
        return NULL;
    }

    // Check for existing track break
    GList *cur = list->breaks;
    while (cur != NULL) {
        TrackBreak *existing = cur->data;

        if (existing->offset == offset) {
            return existing;
        }

        cur = g_list_next(cur);
    }

    TrackBreak *track_break = g_new0(TrackBreak, 1);

    track_break->write = write;
    track_break->offset = offset;
    track_break->filename = filename ? g_strdup(filename) : NULL;

    list->breaks = g_list_insert_sorted(list->breaks, track_break, track_break_cmp);

    return track_break;
}

struct GetDiscNumber {
    int track_index;

    int disc_num;
    int track_num;

    int cd_length_minutes;
    int disc_remaining_minutes;
};

static void
get_disc_number_visitor(int index, gboolean write, gulong start_offset, gulong end_offset, const char *filename, void *user_data)
{
    struct GetDiscNumber *gdn = user_data;

    if (index > gdn->track_index) {
        return;
    }

    const gulong CD_BLOCKS_PER_MINUTE = 60 * CD_BLOCKS_PER_SEC;

    gulong duration_minutes = (end_offset - start_offset + CD_BLOCKS_PER_MINUTE - 1) / CD_BLOCKS_PER_MINUTE;

    if (duration_minutes > gdn->disc_remaining_minutes) {
        gdn->disc_num++;
        gdn->track_num = 0;
        gdn->disc_remaining_minutes = gdn->cd_length_minutes;
    }

    gdn->track_num++;
    gdn->disc_remaining_minutes -= duration_minutes;
}

gchar *
track_break_get_filename(TrackBreak *track_break, TrackBreakList *list)
{
    if (track_break->filename != NULL) {
        return g_strdup(track_break->filename);
    }

    int index = g_list_index(list->breaks, track_break);

    if (appconfig_get_use_etree_filename_suffix()) {
        struct GetDiscNumber gdn = {
            .track_index = index,
            .disc_num = 0,
            .track_num = 0,
            .cd_length_minutes = atoi(appconfig_get_etree_cd_length()),
            .disc_remaining_minutes = 0,
        };

        track_break_list_foreach(list, get_disc_number_visitor, &gdn);

        return g_strdup_printf("%sd%dt%02d", list->basename, gdn.disc_num, gdn.track_num);
    } else if (appconfig_get_prepend_file_number()) {
        return g_strdup_printf("%02d%s%s", index + 1, appconfig_get_etree_filename_suffix(), list->basename);
    }

    return g_strdup_printf("%s%s%02d", list->basename, appconfig_get_etree_filename_suffix(), index + 1);
}

void
track_break_list_reset_filenames(TrackBreakList *list)
{
    GList *cur = list->breaks;
    while (cur != NULL) {
        TrackBreak *track_break = cur->data;

        track_break_rename(track_break, NULL);

        cur = g_list_next(cur);
    }
}
