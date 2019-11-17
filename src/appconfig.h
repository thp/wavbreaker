/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2005 Timothy Robinson
 * Copyright (C) 2007 Thomas Perl
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
#include "wavbreaker.h"

typedef struct AudioFunctionPointers_ AudioFunctionPointers;

struct AudioFunctionPointers_ {
    void (*audio_close_device)();
    int (*audio_open_device)(SampleInfo *);
    int (*audio_write)(unsigned char *, int);
};

void appconfig_show(GtkWidget *);
void appconfig_write_file();
void appconfig_init();

AudioFunctionPointers *get_audio_function_pointers();

int appconfig_get_use_outputdir();
char *appconfig_get_outputdir();
int appconfig_get_use_etree_filename_suffix();
int appconfig_get_prepend_file_number();
char *appconfig_get_etree_filename_suffix();
char *appconfig_get_etree_cd_length();

int appconfig_get_main_window_xpos();
void appconfig_set_main_window_xpos(int x);
int appconfig_get_main_window_ypos();
void appconfig_set_main_window_ypos(int x);
int appconfig_get_main_window_width();
void appconfig_set_main_window_width(int x);
int appconfig_get_main_window_height();
void appconfig_set_main_window_height(int x);
int appconfig_get_vpane1_position();
void appconfig_set_vpane1_position(int x);
int appconfig_get_vpane2_position();
void appconfig_set_vpane2_position(int x);
int appconfig_get_silence_percentage();
void appconfig_set_silence_percentage(int x);
int appconfig_get_show_moodbar();
void appconfig_set_show_moodbar(int x);

#endif /* APPCONFIG_H */

