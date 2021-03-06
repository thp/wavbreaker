/* wavbreaker - A tool to split a wave file up into multiple waves.
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

#include "cue.h"
#include "wavbreaker.h"
#include <string.h>

int cue_read_file(const char *cue_filename, GList *breaks)
{
	FILE *fp;
	/* These buffers must have the same length */
	char line_buf[1024];
	char buf[1024];
	int next_track = 1;

	fp = fopen(cue_filename, "r");
	if (!fp) return 1;

	track_break_clear_list();

	/* Read the first line: FILE "foo.wav" WAVE */
	if (!fgets(line_buf, sizeof(line_buf), fp)) {
		fclose(fp);
		return 1;
	}

	/* Check that it starts with FILE and ends with WAVE */
	if (sscanf(line_buf, "FILE %s WAVE", buf) != 1) {
		fclose(fp);
		return 1;
	}

	while (!feof(fp)) {
		int N;
		int read;
		guint offset;

		read = fscanf(fp, "TRACK %02d AUDIO\n", &N);
		if (read == EOF) {
			return 0;
		}

		if (read != 1 || N != next_track) {
			fclose(fp);
			return 1;
		}

		if (!fgets(line_buf, sizeof(line_buf), fp)) {
			fclose(fp);
			return 1;
		}

		read = sscanf(line_buf, "INDEX %02d %s", &N, buf);
		if (read != 2 || N != 1) {
			fclose(fp);
			return 1;
		}

		offset = msf_time_to_offset(buf);
		track_break_add_offset((char *)-1, offset);

		++next_track;
	}

	return 0;
}
