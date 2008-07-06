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

typedef struct AudioFunctionPointers_ AudioFunctionPointers;

struct AudioFunctionPointers_ {
    void (*audio_close_device)();
    int (*audio_open_device)(const char *, SampleInfo *);
    int (*audio_write)(unsigned char *, int);
    char *(*get_outputdev)();
};

void appconfig_show(GtkWidget *);
int appconfig_write_file();
void appconfig_init();

int get_use_outputdir();
char *get_outputdir();
AudioFunctionPointers *get_audio_function_pointers();
int get_use_etree_filename_suffix();
int get_prepend_file_number();
char *get_etree_filename_suffix();
char *get_etree_cd_length();

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
int appconfig_get_ask_really_quit();
void appconfig_set_ask_really_quit(int x);
int appconfig_get_show_toolbar();
void appconfig_set_show_toolbar(int x);
int appconfig_get_show_moodbar();
void appconfig_set_show_moodbar(int x);

char *audio_options_get_output_device();
void set_audio_oss_options_output_device(const char *);
void set_audio_alsa_options_output_device(const char *);

#endif /* APPCONFIG_H */

