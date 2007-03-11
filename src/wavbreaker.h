/* wavbreaker - A tool to split a wave file up into multiple wave.
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

#ifndef WAVBREAKER_H
#define WAVBREAKER_H

#define WAVBREAKER_ICON ICONDIR"/wavbreaker.png"

#include <gtk/gtk.h>

typedef struct Points_ Points;
struct Points_ {
        int min, max;
};

typedef struct GraphData_ GraphData;
struct GraphData_{
	unsigned long numSamples;
	unsigned long maxSampleValue;
	Points *data;
};

typedef struct TrackBreak_ TrackBreak;
struct TrackBreak_ {
	gboolean  write;
	guint     offset;
	gchar     *filename;
	gchar     time[128];
	gchar     duration[128];
	gboolean  editable;
};

typedef struct WriteInfo_ WriteInfo;
struct WriteInfo_ {
	guint num_files;
	guint cur_file;
	char *cur_filename;
	double pct_done;
	guint sync;
	gint check_file_exists;
	gint skip_file; /* -1 = waiting for check
                     * 0 = don't overwrite file
                     * 1 = file is ok to overwrite
                     * 2 = overwrite all files
                     */
    gint sync_check_file_overwrite_to_write_progress;
};

void wavbreaker_autosplit(long x);
void track_break_rename();
void wavbreaker_write_files(char *dirname);
GtkWidget *wavbreaker_get_main_window();

void wavbreaker_quit();

#endif /* WAVBREAKER_H */
