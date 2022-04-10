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

#ifndef SAMPLE_H
#define SAMPLE_H

#include "sample_info.h"

#include <glib.h>
#include <stdio.h>

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
	GList *errors;
};

void sample_init();

typedef struct Sample_ Sample;

Sample *
sample_open(const char *filename, char **error_message);

void
sample_close(Sample *sample);

double
sample_get_percentage(Sample *sample);

uint64_t
sample_get_file_size(Sample *sample);

const char *
sample_get_dirname(Sample *sample);

const char *
sample_get_filename(Sample *sample);

const char *
sample_get_basename(Sample *sample);

const char *
sample_get_basename_without_extension(Sample *sample);

int
sample_play(Sample *sample, gulong startpos);

gulong
sample_get_play_marker(Sample *sample);

void
sample_stop(Sample *sample);

void
sample_write_files(Sample *sample, GList *, WriteInfo *, char *);

GraphData *
sample_get_graph_data(Sample *sample);

unsigned long
sample_get_num_sample_blocks(Sample *sample);

gboolean
sample_is_playing(Sample *sample);

gboolean
sample_is_writing(Sample *sample);

#endif /* SAMPLE_H*/
