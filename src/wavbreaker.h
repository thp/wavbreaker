/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2002 Timothy Robinson
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
	gboolean  editable;
};

typedef struct WriteInfo_ WriteInfo;
struct WriteInfo_ {
	guint num_files;
	guint cur_file;
	char *cur_filename;
	double pct_done;
	guint sync;
};

void wavbreaker_autosplit(long x);

#endif /* WAVBREAKER_H */
