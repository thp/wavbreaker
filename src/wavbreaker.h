/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2002-2005 Timothy Robinson
 * Copyright (C) 2007 Thomas Perl
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

#ifndef WAVBREAKER_H
#define WAVBREAKER_H

#include <gtk/gtk.h>

typedef struct TrackBreak_ TrackBreak;
struct TrackBreak_ {
	gboolean  write;
	gulong    offset;
	gchar     *filename;
	gchar     time[128];
	gchar     duration[128];
	gboolean  editable;
};

void wavbreaker_autosplit(long x);
void track_break_rename();
void wavbreaker_write_files(char *dirname);
GtkWidget *wavbreaker_get_main_window();
void track_break_add_offset(char *filename, gulong offset);
void track_break_clear_list();
void wavbreaker_update_moodbar_state();
void wavbreaker_quit();

guint msf_time_to_offset( gchar *str );

extern char *sample_filename;

enum {
    CHECK_ALL = 0,
    CHECK_NONE,
    CHECK_INVERT
};

/**
 * When the play marker reaches (x-1)/x (where x is the
 * value of PLAY_MARKER_SCROLL), scroll the waveform so
 * that the play marker continues at position 1/x.
 *
 * i.e. play marker at 7/8 of width -> go to 1/8 of width
 **/
#define PLAY_MARKER_SCROLL 8

#endif /* WAVBREAKER_H */
