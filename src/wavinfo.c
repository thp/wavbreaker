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
#include <string.h>
#include <libgen.h>
#include "wav.h"
#include "sample.h"

int main(int argc, char *argv[])
{
    int i;
    
    if( argc < 2) {
        printf( "Usage: %s [file1.wav] [...]\n", basename( argv[0]));
        return 1;
    }
    
    for( i = 1; i < argc; i++) {
        SampleInfo sampleInfo;
    
        printf( "Header info for: %s\n", argv[i]);
    
        if( wav_read_header( argv[i], &sampleInfo, 1) != 0) {
            printf( wav_get_error_message());
        }

        printf("\n");
    }
    
    return 0;
}

