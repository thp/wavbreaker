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

#include "format_wav.h"
#include "format_cdda_raw.h"
#include "format_mp3.h"
#include "format_ogg_vorbis.h"

#include <stdio.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <errno.h>

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
format_module_filename_extension_check(const FormatModule *self, const char *filename, const char *extension)
{
    if (extension == NULL) {
        extension = self->default_file_extension;
    }

    return (g_ascii_strcasecmp(filename + strlen(filename) - strlen(extension), extension) == 0);
}

gboolean
format_module_open_file(const FormatModule *self, OpenedAudioFile *file, const char *filename, char **error_message)
{
    struct stat st;
    if (stat(filename, &st) != 0) {
        format_module_set_error_message(error_message, "Could not stat %s: %s", filename, strerror(errno));
        return FALSE;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        format_module_set_error_message(error_message, "Could not open file %s: %s", filename, strerror(errno));
        return FALSE;
    }

    file->mod = self;
    file->filename = g_strdup(filename);
    file->fp = fp;
    file->file_size = st.st_size;

    return TRUE;
}

void
opened_audio_file_close(OpenedAudioFile *file)
{
    if (file->details) {
        g_free(g_steal_pointer(&file->details));
    }

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
    static gboolean format_inited = FALSE;

    static const format_module_load_func
    CANDIDATES[] = {
        &format_module_wav,
        &format_module_cdda_raw,
        &format_module_mp3,
        &format_module_ogg_vorbis,
    };

    if (!format_inited) {
        for (size_t i=0; i<sizeof(CANDIDATES)/sizeof(CANDIDATES[0]); ++i) {
            const FormatModule *mod = CANDIDATES[i]();
            if (mod != NULL) {
                g_debug("Loaded format module: %s", mod->name);
                g_modules = g_list_append(g_modules, (gpointer)mod);
            }
        }

        format_inited = TRUE;
    }
}

void
format_print_supported(void)
{
    GList *cur = g_list_first(g_modules);
    while (cur != NULL) {
        const FormatModule *mod = cur->data;

        printf("Format:    %s\n", mod->name);
        printf("Library:   %s\n", mod->library_name);
        printf("Extension: %s\n", mod->default_file_extension);
        printf("\n");

        cur = g_list_next(cur);
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

static char *
do_format_duration(uint64_t duration)
{
    uint64_t seconds = duration / 1000;
    uint64_t fraction = duration % 1000;
    return g_strdup_printf("%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64 ".%03" PRIu64, seconds / 60 / 60, (seconds / 60) % 60, seconds % 60, fraction);
}

void
format_print_file_info(OpenedAudioFile *file)
{
    SampleInfo *si = &file->sample_info;

    char *duration = do_format_duration((uint64_t)si->numBytes * 1000 / (uint64_t)si->avgBytesPerSec);

    printf("File name:      %s\n", file->filename);
    printf("File format:    %s\n", file->mod->name);
    if (file->details) {
        printf("Format details: %s\n", file->details);
    }
    printf("Duration:       %s (%lu samples)\n", duration, si->numBytes / si->blockAlign);
    printf("Format:         %d Hz / %d ch / %d bit",
            si->samplesPerSec,
            si->channels,
            si->bitsPerSample);

    printf("\n");

    g_free(duration);
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
