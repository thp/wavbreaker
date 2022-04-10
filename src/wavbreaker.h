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

#include "track_break.h"

#include <gtk/gtk.h>

void wavbreaker_autosplit(long x);
void track_break_rename();
void wavbreaker_write_files(char *dirname);
GtkWidget *wavbreaker_get_main_window();
void wavbreaker_update_moodbar_state();
void wavbreaker_quit();

void track_break_clear_list();
void track_break_add_offset(char *filename, gulong offset);

guint msf_time_to_offset( gchar *str );

#endif /* WAVBREAKER_H */
