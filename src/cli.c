/* wavbreaker - A tool to split a wave file up into multiple wave.
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

#include "config.h"

#include "track_break.h"
#include "list.h"
#include "appconfig.h"
#include "appinfo.h"
#include "sample.h"
#include "format.h"

#include <stdio.h>


static void
cmd_list_print_track_break(int index, gboolean write, gulong start_offset, gulong end_offset, const gchar *filename, void *user_data)
{
    gchar *time = track_break_format_timestamp(start_offset, FALSE);

    if (end_offset == (gulong)-1) {
        printf("%2d. [%c] %10s - %10s \"%s\"\n", index + 1, write ? 'x' : '_', time, "(end)", filename);
    } else {
        gchar *duration = track_break_format_timestamp(end_offset - start_offset, FALSE);
        gchar *end = track_break_format_timestamp(end_offset, FALSE);
        printf("%2d. [%c] %10s - %10s \"%s\" (duration: %s)\n", index + 1, write ? 'x' : '_', time, end, filename, duration);
        g_free(end);
        g_free(duration);
    }

    g_free(time);
}

static int
cmd_list(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s [filename.txt|filename.cue|filename.toc] ...\n", argv[0]);
        return 1;
    }

    for (int i=1; i<argc; ++i) {
        gchar *base_filename = g_strdup(argv[i]);
        char *dot = strrchr(base_filename, '.');
        if (dot != NULL) {
            *dot = '\0';
        }
        TrackBreakList *list = track_break_list_new(base_filename);
        g_free(base_filename);

        // We don't know the duration, so just allow the maximum; we'll
        // capture that maximum above and just show "(end)". The real end
        // will depend on which audio file is loaded.
        track_break_list_set_total_duration(list, (gulong)-1);

        if (list_read_file(argv[i], list)) {
            printf("== %s ==\n", argv[i]);
            track_break_list_foreach(list, cmd_list_print_track_break, NULL);
            printf("\n");
        } else {
            printf("Could not parse file\n");
            return 2;
        }

        track_break_list_free(list);
    }

    return 0;
}

static int
cmd_analyze(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s [filename.wav]\n", argv[0]);
        return 1;
    }

    sample_init();

    char *error_message = NULL;
    Sample *sample = sample_open(argv[1], &error_message);
    if (sample == NULL) {
        printf("Could not open %s: %s\n", argv[1], error_message);
        g_free(error_message);
        return 2;
    }

    sample_print_file_info(sample);

    gint64 analyze_started = g_get_monotonic_time();

    do {
        fprintf(stderr, "\r\033[KAnalyzing... [%3.0f%%]", 100.0 * sample_get_load_percentage(sample));
        fflush(stderr);
        g_usleep(G_USEC_PER_SEC / 30);
    } while (!sample_is_loaded(sample));

    gint64 analyze_duration = g_get_monotonic_time() - analyze_started;
    unsigned long num_sample_blocks = sample_get_num_sample_blocks(sample);

    fprintf(stderr, "\r\033[KAnalyzing... [DONE]\n");
    fflush(stderr);

    printf("%lu sample blocks analyzed in %.2f seconds = %lu blocks/sec\n",
            num_sample_blocks,
            (double)analyze_duration / (float)G_USEC_PER_SEC,
            num_sample_blocks * G_USEC_PER_SEC / analyze_duration);

    gint64 started = g_get_monotonic_time();

    sample_play(sample, 0);

    do {
        gulong pos = sample_get_play_marker(sample);

        fprintf(stderr, "\r\033[KPreviewing... [%lu]", pos);
        fflush(stderr);
        g_usleep(G_USEC_PER_SEC / 30);
    } while (sample_is_playing(sample) && g_get_monotonic_time() < started + G_USEC_PER_SEC * 10);

    fprintf(stderr, "\r\033[KPreviewing... [DONE]\n");
    fflush(stderr);

    sample_stop(sample);

    sample_close(sample);

    return 0;
}

static int
cmd_version(int argc, char *argv[])
{
    printf("wavcli %s -- %s\n%s\n",
            appinfo_version(),
            appinfo_url(),
            appinfo_copyright());

    printf("\n== Supported file formats ==\n\n");

    format_init();
    format_print_supported();

    return 0;
}

int cmd_wavgen(int argc, char *argv[]);
int cmd_wavinfo(int argc, char *argv[]);
int cmd_wavmerge(int argc, char *argv[]);

struct SubCommand {
    const char *name;
    int (*main)(int, char *[]);
    const char *help;
};

int main(int argc, char *argv[])
{
    appconfig_init();

    static const struct SubCommand
    SUBCOMMANDS[] = {
        { "list", cmd_list, "List track breaks from file (TXT/CUE/TOC)" },
        { "analyze", cmd_analyze, "Open, analyze and preview audio file" },
        { "gen", cmd_wavgen, "Generate example WAV files (formerly 'wavgen')" },
        { "info", cmd_wavinfo, "Print audio format information (WAV/MP2/MP3/OGG) (formerly 'wavinfo')" },
        { "merge", cmd_wavmerge, "Merge multiple WAV files into a single file (formerly 'wavmerge')" },
        { "version", cmd_version, "Print version and software information" },
        { NULL, NULL, NULL },
    };

    gchar *argv0;
    if (argc > 1) {
        const struct SubCommand *cur = &SUBCOMMANDS[0];
        while (cur->name != NULL) {
            if (strcmp(argv[1], cur->name) == 0) {
                argv0 = g_strdup_printf("%s %s", argv[0], argv[1]);
                argv[1] = argv0;
                ++argv;
                --argc;

                int res = cur->main(argc, argv);
                g_free(argv0);
                return res;
            }

            ++cur;
        }
    }

    printf("Usage: %s SUBCOMMAND <options>\n\n", argv[0]);
    printf("wavcli(1) is the command-line interface for wavbreaker(1).\n\n");
    printf("Valid SUBCOMMANDs are:\n\n");

    const struct SubCommand *cur = &SUBCOMMANDS[0];
    while (cur->name != NULL) {
        printf("  %-10s  ..... %s\n", cur->name, cur->help);

        ++cur;
    }
    printf("\n");

    return 1;
}
