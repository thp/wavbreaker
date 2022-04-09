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

#include "format.h"

#include "wav.h"
#include "format_mp3.h"

void
format_module_set_error_message(char **error_message, const char *fmt, ...)
{
    if (error_message == NULL) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    if (*error_message) {
        g_free(*error_message);
    }
    *error_message = g_strdup_vprintf(fmt, args);
    va_end(args);
}

gboolean
format_module_open_file(const FormatModule *self, OpenedAudioFile *file, const char *filename, char **error_message)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        format_module_set_error_message(error_message, "Could not open file %s: %s", filename, strerror(errno));
        return FALSE;
    }

    file->mod = self;
    file->filename = g_strdup(filename);
    file->fp = fp;

    return TRUE;
}

void
opened_audio_file_close(OpenedAudioFile *file)
{
    if (file->fp) {
        fclose(g_steal_pointer(&file->fp));
    }

    g_free(g_steal_pointer(&file->filename));
}

static GList *
g_modules = NULL;


void
format_init(void)
{
    static const format_module_load_func
    CANDIDATES[] = {
        &format_module_wav,
        &format_module_mp3,
    };

    for (size_t i=0; i<sizeof(CANDIDATES)/sizeof(CANDIDATES[0]); ++i) {
        const FormatModule *mod = CANDIDATES[i]();
        if (mod != NULL) {
            g_debug("Loaded format module: %s", mod->name);
            g_modules = g_list_append(g_modules, (gpointer)mod);
        }
    }
}

OpenedAudioFile *
format_open_file(const char *filename, char **error_message)
{
    GList *cur = g_list_first(g_modules);
    while (cur != NULL) {
        const FormatModule *mod = cur->data;

        OpenedAudioFile *result = mod->open_file(mod, filename, error_message);
        if (result != NULL) {
            return result;
        }

        if (error_message) {
            g_debug("Open as %s failed: %s", mod->name, *error_message);
            g_free(*error_message);
            *error_message = NULL;
        }

        cur = g_list_next(cur);
    }

    format_module_set_error_message(error_message, "File format unknown/not supported");

    return NULL;
}

void
format_close_file(OpenedAudioFile *file)
{
    file->mod->close_file(file->mod, file);
}

long
format_read_samples(OpenedAudioFile *file, unsigned char *buf, size_t buf_size, unsigned long start_pos)
{
    return file->mod->read_samples(file, buf, buf_size, start_pos);
}

int
format_write_file(OpenedAudioFile *file, const char *output_filename, unsigned long start_pos, unsigned long end_pos, double *progress)
{
    return file->mod->write_file(file, output_filename, start_pos, end_pos, progress);
}
