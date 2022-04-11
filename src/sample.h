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
#include "track_break.h"

#include <glib.h>
#include <stdio.h>
#include <stdint.h>

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

enum OverwriteDecision {
    OVERWRITE_DECISION_NONE = 0,
    OVERWRITE_DECISION_ASK,
    OVERWRITE_DECISION_SKIP,
    OVERWRITE_DECISION_SKIP_ALL,
    OVERWRITE_DECISION_OVERWRITE,
    OVERWRITE_DECISION_OVERWRITE_ALL,
};

typedef struct WriteStatusCallbacks_ WriteStatusCallbacks;
struct WriteStatusCallbacks_ {
    // Write thread reporting to the UI
    void (*on_file_changed)(guint position, guint total, const char *filename, void *user_data);
    void (*on_file_progress_changed)(double percentage, void *user_data);
    void (*on_error)(const char *message, void *user_data);
    void (*on_finished)(void *user_data);

    // Write thread querying the UI
    gboolean (*is_cancelled)(void *user_data);
    enum OverwriteDecision (*ask_overwrite)(const char *filename, void *user_data);

    // Closure pointer passed to the callbacks
    void *user_data;
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
	GList *errors;
};

void sample_init();

typedef struct Sample_ Sample;

Sample *
sample_open(const char *filename, char **error_message);

void
sample_print_file_info(Sample *sample);

void
sample_close(Sample *sample);

gboolean
sample_is_loaded(Sample *sample);

double
sample_get_load_percentage(Sample *sample);

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
sample_write_files(Sample *sample, TrackBreakList *list, WriteStatusCallbacks *callbacks, const char *output_dir);

GraphData *
sample_get_graph_data(Sample *sample);

unsigned long
sample_get_num_sample_blocks(Sample *sample);

gboolean
sample_is_playing(Sample *sample);

gboolean
sample_is_writing(Sample *sample);

#endif /* SAMPLE_H*/
