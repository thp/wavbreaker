/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2022 Thomas Perl
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

#include "sample.h"

typedef struct FormatModule_ FormatModule;
typedef struct OpenedAudioFile_ OpenedAudioFile;

struct FormatModule_ {
    const char *name;
    enum AudioType type;
    const char *default_file_extension;

    OpenedAudioFile *(*open_file)(const FormatModule *self, const char *filename, char **error_message);
    void (*close_file)(const FormatModule *self, OpenedAudioFile *file);

    long (*read_samples)(OpenedAudioFile *self, unsigned char *buf, size_t buf_size, unsigned long start_pos);
    int (*write_file)(OpenedAudioFile *self, const char *output_filename, unsigned long start_pos, unsigned long end_pos, double *progress);
};

typedef const FormatModule *(*format_module_load_func)(void);

void
format_module_set_error_message(char **error_message, const char *fmt, ...);

struct OpenedAudioFile_ {
    const FormatModule *mod;

    char *filename;
    FILE *fp;
    SampleInfo sample_info;
};

gboolean
format_module_open_file(const FormatModule *self, OpenedAudioFile *file, const char *filename, char **error_message);

void
opened_audio_file_close(OpenedAudioFile *file);


/* Public API */

void
format_init(void);

OpenedAudioFile *
format_open_file(const char *filename, char **error_message);

void
format_close_file(OpenedAudioFile *file);

long
format_read_samples(OpenedAudioFile *file, unsigned char *buf, size_t buf_size, unsigned long start_pos);

int
format_write_file(OpenedAudioFile *file, const char *output_filename, unsigned long start_pos, unsigned long end_pos, double *progress);
