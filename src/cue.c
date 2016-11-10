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
	char buf[1024];
	int next_track = 1;

	fp = fopen( cue_filename, "r" );
	if( !fp ) return 1;

	track_break_clear_list();

	/* Ignore FILE "foo.wav" WAVE line */
	if (!fgets( buf, 1024, fp)) {
		fclose( fp );
		return 1;
	}

	while (!feof(fp)) {
		char A[64];
		int N;
		char B[64];
		int read;
		guint offset;

		read = fscanf( fp, "%s %02d %s", A, &N, B);
		if (read == EOF) {
			return 0;
		}

		if (strcmp( A, "TRACK") || strcmp( B, "AUDIO") || N != next_track) {
			fclose( fp);
			return 1;
		}

		read = fscanf( fp, "%s %02d %s", A, &N, B);
		if (read != 3 || strcmp( A, "INDEX") || N != 1) {
			fclose( fp);
			return 1;
		}

		offset = msf_time_to_offset( B);
		track_break_add_offset( (char *)-1, offset );

		++next_track;
	}

	return 0;
}
