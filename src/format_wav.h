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

#pragma once

#include "format.h"

const FormatModule *
format_module_wav(void);

int wav_read_header(char *, SampleInfo *, int);

int
wav_write_file_header(FILE *fp,
                      SampleInfo *sample_info,
                      unsigned long num_bytes);

int
wav_merge_files(char *filename,
                int num_files,
                char *filenames[],
                int buf_size,
                WriteInfo *write_info);
