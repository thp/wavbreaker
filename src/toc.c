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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wavbreaker.h"
#include "sample.h"
#include "toc.h"

char *convert_wavbreaker_time_to_toc_time(const char *wavbreakerTime);
guint time_to_offset( gchar * );

int toc_read_file(const char *toc_filename, GList *breaks)
{
    FILE   *fp;
    char    buf[1024];
    gchar  *ptr, *eptr;
    guint   offset;

    fp = fopen( toc_filename, "r" );
    if( !fp ) return 1;

    track_break_clear_list();

    do {
	do {
	    ptr = fgets( buf, 1024, fp );
	    if( !ptr ) {
		if( feof(fp) ) {
		    fclose( fp );
		    return 0;
		}
		else {
		    fclose( fp );
		    return 1;
		}
	    }
	} while( memcmp(buf, "FILE", 4) );

	ptr = strrchr( buf, '"' );
	if( !ptr ) {
	    fclose( fp );
	    return 1;
	}

	ptr += 2;
	eptr = strchr( ptr, ':' );
	if( !eptr ) {
	    fclose( fp );
	    return 1;
	}
	eptr += 6;
	*eptr = '\0';

	offset = time_to_offset( ptr );
	track_break_add_offset( (char *)-1, offset );
    } while( !feof( fp ) );

    fclose( fp );
    return 0;
}

int toc_write_file(const char *toc_filename, const char *wav_filename, GList *breaks)
{
    FILE *fp;
    TrackBreak *next_break = NULL;
    char *tocTime;
    char *tocDuration;

    fp = fopen(toc_filename, "w");
    if(!fp) return 1;

    fprintf(fp, "// Generated with wavbreaker\n\nCD_DA\n");
    int i = 0;
    int len = g_list_length(breaks);
    while (i < len) {
        next_break = (TrackBreak *) g_list_nth_data(breaks, i);
        if (next_break != NULL) {
            fprintf(fp, "\n// track %02d\n", i);
            fprintf(fp, "TRACK AUDIO\n");

            tocTime = convert_wavbreaker_time_to_toc_time(next_break->time);
            if (i != len-1) {
                tocDuration = convert_wavbreaker_time_to_toc_time(next_break->duration);
                fprintf(fp, "FILE \"%s\" %s %s\n",
                        wav_filename, tocTime, tocDuration);
                g_free(tocDuration);
            } else {
                fprintf(fp, "FILE \"%s\" %s\n", wav_filename, tocTime);
            }
            g_free(tocTime);
        }
        i++;
    }
    fclose(fp);
    return 0;
}

char *convert_wavbreaker_time_to_toc_time(const char *wavbreakerTime) {
    char *tocTime;
    int i;

    #ifdef DEBUG
    printf("start of convert_wavbreaker_time_to_toc_time\n");
    printf("called convert_wavbreaker_time_to_toc_time with: %s\n", wavbreakerTime);
    #endif

    tocTime = g_strdup(wavbreakerTime);

    #ifdef DEBUG
    printf("got to: %d\n", p++);
    #endif

    i = 0;
    while (tocTime[i] != '\0') {

        #ifdef DEBUG
	printf("got to: %d\n", p++);
        printf("looping with: %d", tocTime[i]);
        #endif

        if (tocTime[i] == '.') {
            tocTime[i] = ':';
        }
        i++;
    }

    #ifdef DEBUG
    printf("end of convert_wavbreaker_time_to_toc_time\n");
    #endif

    return tocTime;
}

guint
time_to_offset( gchar *str )
{
    char   buf[1024];
    uint   parse[3];
    gchar  *aptr, *bptr;
    int    index;
    uint   offset;


    strncpy( buf, str, 1024 );

    aptr = buf;
    for( index = 0; index < 2; index++ ) {
	bptr = memchr( aptr, ':', 1024 );
	if( aptr == NULL ) {
	    return 0;
	}
	*bptr = '\0';
	parse[index] = atoi( aptr );
	aptr = ++bptr;
    }

    parse[index] = atoi( aptr );

    offset  = parse[0] * CD_BLOCKS_PER_SEC * 60;
    offset += parse[1] * CD_BLOCKS_PER_SEC;
    offset += parse[2];

    return offset;
}

/* min = time / (CD_BLOCKS_PER_SEC * 60); */
/* sec = time % (CD_BLOCKS_PER_SEC * 60); */
/* subsec = sec % CD_BLOCKS_PER_SEC; */
/* sec = sec / CD_BLOCKS_PER_SEC; */
