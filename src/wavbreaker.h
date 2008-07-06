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
        unsigned long maxSampleAmp;
        unsigned long minSampleAmp;
	Points *data;
};

typedef struct MoodbarData_ MoodbarData;
struct MoodbarData_ {
    unsigned long numFrames;
    GdkColor *frames;
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
	char *merge_filename;
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
gboolean open_file_arg( gpointer data);
void track_break_add_offset( char* filename, guint offset);
void track_break_clear_list();
void wavbreaker_quit();

extern char *sample_filename;

enum {
    CHECK_ALL = 0,
    CHECK_NONE,
    CHECK_INVERT
};

#define MB_OVL_MOODBAR 2
#define MB_OVL_WAVEFORM 7
#define MOODBAR_BLEND(waveform,moodbar) (((MB_OVL_WAVEFORM*waveform+MB_OVL_MOODBAR*moodbar))/(MB_OVL_MOODBAR+MB_OVL_WAVEFORM))

/**
 * When the play marker reaches (x-1)/x (where x is the 
 * value of PLAY_MARKER_SCROLL), scroll the waveform so
 * that the play marker continues at position 1/x.
 *
 * i.e. play marker at 7/8 of width -> go to 1/8 of width
 **/
#define PLAY_MARKER_SCROLL 8

#endif /* WAVBREAKER_H */
