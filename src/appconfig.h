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

#ifndef APPCONFIG_H
#define APPCONFIG_H

#include "sample.h"

typedef struct AudioFunctionPointers_ AudioFunctionPointers;

struct AudioFunctionPointers_ {
    void (*audio_close_device)();
    int (*audio_open_device)(const char *, SampleInfo *);
    int (*audio_write)(char *, int);
    char *(*get_outputdev)();
};

void appconfig_show(GtkWidget *);
char *get_outputdir();
AudioFunctionPointers *get_audio_function_pointers();
char *get_audio_oss_options_output_device();
char *get_audio_alsa_options_output_device();
int get_use_etree_filename_suffix();
char *get_etree_filename_suffix();
char *get_etree_cd_length();

#endif /* APPCONFIG_H */

