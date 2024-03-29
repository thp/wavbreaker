/* -*- c-basic-offset: 4 -*- */
/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2004 Timothy Robinson
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

#include <stdio.h>

#include "format.h"

int cmd_wavinfo(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s [file1.wav] [...]\n", argv[0]);
        return 1;
    }

    format_init();

    for (int i = 1; i < argc; i++) {
        char *error_message = NULL;
        OpenedAudioFile *oaf = format_open_file(argv[i], &error_message);

        if (oaf == NULL) {
            printf("Error opening %s: %s\n", argv[i], error_message);
            g_free(error_message);
            continue;
        }

        format_print_file_info(oaf);
        format_close_file(oaf);

        printf("\n");
    }

    return 0;
}
