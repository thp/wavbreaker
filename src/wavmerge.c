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

#include <stdio.h>
#include <string.h>
#include "sample.h"
#include "wav.h"

int main(int argc, char *argv[])
{
	int ret;

	if (argc < 3) {
		printf("Must pass filenames of wave files to merge.\n");
		printf("Usage: wavmerge [-o outfile] mergefiles...\n");
		return 1;
	}

	if (strcmp(argv[1], "-o") == 0) {
		ret = wav_merge_files(argv[2], argc - 3, &argv[3], DEFAULT_BUF_SIZE);
	} else {
		ret = wav_merge_files("merged.wav", argc - 1, &argv[1],
            DEFAULT_BUF_SIZE);
	}

	if (ret != 0) {
		return 1;
	}

	return 0;
}
