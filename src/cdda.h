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

#ifndef CDDA_H
#define CDDA_H

#include "sample.h"

int cdda_read_header(const char *, SampleInfo *);
int cdda_read_sample(FILE *, unsigned char *, int, unsigned long);
int
cdda_write_file(FILE *fp,
                char *filename,
                int buf_size,
                unsigned long start_pos,
                unsigned long end_pos);

#endif /* CDDA_H */
