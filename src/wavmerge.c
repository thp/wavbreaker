/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2002-2006 Timothy Robinson
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

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "sample.h"
#include "wav.h"
#include "fileutils.h"

void usage() {
    printf("Must pass filenames of wave files to merge.\n");
    printf("Usage: wavmerge [-o outfile] mergefiles...\n");
}

int main(int argc, char *argv[])
{
    int ret;
    char *merge_filename;
    int num_files;
    char **filenames;

    if (argc > 1 && strcmp(argv[1], "-o") == 0) {
        if (argc < 5) {
            usage();
            return 1;
        }
        merge_filename = strdup(argv[2]);
        num_files = argc - 3;
        filenames = &argv[3];
    } else {
        if (argc < 3) {
            usage();
            return 1;
        }
        merge_filename = strdup("merged.wav");
        num_files = argc - 1;
        filenames = &argv[1];
    }

    if (check_file_exists(merge_filename)) {
        fprintf(stderr, "ERROR: The output file %s already exists.\n", merge_filename);
        return 2;
    }

    ret = wav_merge_files(merge_filename, num_files, filenames, DEFAULT_BUF_SIZE, NULL);

    if (ret != 0) {
        fprintf(stderr,
"ERROR: The files are not of the same format.\n\n"
"This means that the sample rate, bits per sample, etc. are different.\n"
"Please use a tool, like sox, to convert the files to the same format and\n"
"try again.\n"
);
        return 1;
    }

    return 0;
}

