#include "format.h"

#include "wav.h"

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


void
format_init(void)
{
    // TODO
}

OpenedAudioFile *
format_open_file(const char *filename, char **error_message)
{
    const FormatModule *mod = format_module_wav();
    return mod->open_file(mod, filename, error_message);
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
