/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2004 Timothy Robinson
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

#include <config.h>

#include "appconfig.h"
#include "sample_info.h"

#include "gettext.h"

#include <glib.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static int config_file_version = 2;

/* Output directory for wave files. */
static int use_outputdir = 0;
static char *outputdir = NULL;

/* Etree filename suffix */
/* Filename suffix (not extension) for wave files. */
static int use_etree_filename_suffix = 0;

static char *etree_filename_suffix = NULL;

/* Prepend File Number for wave files. */
static int prepend_file_number = 0;

/* CD Length disc cutoff. */
static char *etree_cd_length = NULL;

/* Config Filename */
static char *config_filename = NULL;

/* Window and pane sizes. */
static int main_window_xpos = -1;
static int main_window_ypos = -1;
static int main_window_width = -1;
static int main_window_height = -1;
static int vpane1_position = -1;
static int vpane2_position = -1;

/* Percentage for silence detection */
static int silence_percentage = 2;

/* Draw moodbar in main window */
static int show_moodbar = 1;

/* function prototypes */
static int appconfig_read_file();
static void default_all_strings();

int appconfig_get_config_file_version()
{
    return config_file_version;
}

void appconfig_set_config_file_version(int x)
{
    config_file_version = x;
}

int appconfig_get_main_window_xpos()
{
    return main_window_xpos;
}

void appconfig_set_main_window_xpos(int x)
{
    main_window_xpos = x;
}

int appconfig_get_main_window_ypos()
{
    return main_window_ypos;
}

void appconfig_set_main_window_ypos(int x)
{
    main_window_ypos = x;
}

int appconfig_get_main_window_width()
{
    return main_window_width;
}

void appconfig_set_main_window_width(int x)
{
    main_window_width = x;
}

int appconfig_get_main_window_height()
{
    return main_window_height;
}

void appconfig_set_main_window_height(int x)
{
    main_window_height = x;
}

int appconfig_get_vpane1_position()
{
    return vpane1_position;
}

void appconfig_set_vpane1_position(int x)
{
    vpane1_position = x;
}

int appconfig_get_vpane2_position()
{
    return vpane2_position;
}

void appconfig_set_vpane2_position(int x)
{
    vpane2_position = x;
}

int appconfig_get_silence_percentage()
{
    return silence_percentage;
}

void appconfig_set_silence_percentage(int x)
{
    silence_percentage = x;
}

int appconfig_get_show_moodbar() {
    return show_moodbar;
}

void appconfig_set_show_moodbar(int x)
{
    show_moodbar = x;
}

int appconfig_get_use_outputdir()
{
    return use_outputdir;
}

void appconfig_set_use_outputdir(int x)
{
    use_outputdir = x;
}

char *appconfig_get_outputdir()
{
    return outputdir;
}

void appconfig_set_outputdir(const char *val)
{
    if (outputdir != NULL) {
        g_free(outputdir);
    }
    outputdir = g_strdup(val);
}

int appconfig_get_use_etree_filename_suffix()
{
    return use_etree_filename_suffix;
}

void appconfig_set_use_etree_filename_suffix(int x)
{
    use_etree_filename_suffix = x;
}

char *appconfig_get_etree_filename_suffix()
{
    return etree_filename_suffix;
}

void appconfig_set_etree_filename_suffix(const char *val)
{
    if (etree_filename_suffix != NULL) {
        g_free(etree_filename_suffix);
    }
    etree_filename_suffix = g_strdup(val);
}

int appconfig_get_prepend_file_number()
{
    return prepend_file_number;
}

void appconfig_set_prepend_file_number(int x)
{
    prepend_file_number = x;
}

char *appconfig_get_etree_cd_length()
{
    return etree_cd_length;
}

void appconfig_set_etree_cd_length(const char *val)
{
    if (etree_cd_length != NULL) {
        g_free(etree_cd_length);
    }
    etree_cd_length = g_strdup(val);
}

char *get_config_filename()
{
    return config_filename;
}

void set_config_filename(const char *val)
{
    if (config_filename != NULL) {
        g_free(config_filename);
    }
    config_filename = g_strdup(val);
}

enum ConfigOptionType {
    INVALID = 0,
    STRING,
    INTEGER,
    BOOLEAN,
};

typedef struct ConfigOption_ ConfigOption;

struct ConfigOption_ {
    const char *key;
    enum ConfigOptionType type;
    void *setter;
    void *getter;
} config_options[] = {
#define OPTION(name, type) { #name, type, appconfig_set_ ## name, appconfig_get_ ## name }
    OPTION(config_file_version, INTEGER),

    OPTION(use_outputdir, BOOLEAN),
    OPTION(outputdir, STRING),

    OPTION(use_etree_filename_suffix, BOOLEAN),
    OPTION(etree_filename_suffix, STRING),
    OPTION(etree_cd_length, STRING),
    OPTION(prepend_file_number, BOOLEAN),

    OPTION(main_window_xpos, INTEGER),
    OPTION(main_window_ypos, INTEGER),
    OPTION(main_window_width, INTEGER),
    OPTION(main_window_height, INTEGER),

    OPTION(vpane1_position, INTEGER),
    OPTION(vpane2_position, INTEGER),

    OPTION(silence_percentage, INTEGER),
    OPTION(show_moodbar, BOOLEAN),
#undef OPTION
    { NULL, INVALID, NULL, NULL },
};


void config_option_set_string(ConfigOption *option, gchar *value)
{
    ((void (*)(const char *))(option->setter))(value);
    g_free(value);
}

void config_option_set_integer(ConfigOption *option, int value)
{
    ((void (*)(int))(option->setter))(value);
}

const char *config_option_get_string(ConfigOption *option)
{
    return ((const char *(*)())(option->getter))();
}

int config_option_get_integer(ConfigOption *option)
{
    return ((int (*)())(option->getter))();
}

static int appconfig_read_file() {
    GKeyFile *keyfile = g_key_file_new();

    if (!g_key_file_load_from_file(keyfile, config_filename, G_KEY_FILE_NONE, NULL)) {
        g_key_file_free(keyfile);
        return 0;
    }

    ConfigOption *option = config_options;
    for (option=config_options; option->key; option++) {
        switch (option->type) {
            case INTEGER:
                config_option_set_integer(option,
                        g_key_file_get_integer(keyfile, "wavbreaker", option->key, NULL));
                break;
            case BOOLEAN:
                config_option_set_integer(option,
                        g_key_file_get_boolean(keyfile, "wavbreaker", option->key, NULL));
                break;
            case STRING:
                config_option_set_string(option,
                        g_key_file_get_string(keyfile, "wavbreaker", option->key, NULL));
                break;
            default:
                g_warning("Invalid option type: %d\n", option->type);
                break;
        }
    }

    g_key_file_free(keyfile);

    return 1;
}


void appconfig_write_file() {
    GKeyFile *keyfile = g_key_file_new();

    g_key_file_load_from_file(keyfile, config_filename, G_KEY_FILE_KEEP_COMMENTS, NULL);

    ConfigOption *option = config_options;
    for (option=config_options; option->key; option++) {
        switch (option->type) {
            case INTEGER:
                g_key_file_set_integer(keyfile, "wavbreaker", option->key,
                        config_option_get_integer(option));
                break;
            case BOOLEAN:
                g_key_file_set_boolean(keyfile, "wavbreaker", option->key,
                        config_option_get_integer(option));
                break;
            case STRING:
                g_key_file_set_string(keyfile, "wavbreaker", option->key,
                        config_option_get_string(option));
                break;
            default:
                g_warning("Invalid option type: %d\n", option->type);
                break;
        }
    }

    if (!g_key_file_save_to_file(keyfile, config_filename, NULL)) {
        g_warning("Could not save settings");
    }

    g_key_file_free(keyfile);
}

void appconfig_init()
{
    gchar *config_filename = g_build_path("/", g_get_user_config_dir(),
            "wavbreaker", "wavbreaker.conf", NULL);
    set_config_filename(config_filename);

    gchar *config_dir = g_path_get_dirname(config_filename);
    if (g_mkdir_with_parents(config_dir, 0700) != 0) {
        g_warning("Could not create configuration directory: %s", config_dir);
    }
    g_free(config_dir);
    g_free(config_filename);

    if (!appconfig_read_file()) {
        default_all_strings();
        appconfig_write_file();
    } else {
        default_all_strings();
    }
}

void default_all_strings() {
    /* default any values that where not in the config file */
    if (appconfig_get_outputdir() == NULL) {
        outputdir = g_strdup(getenv("PWD"));
    }
    if (appconfig_get_etree_filename_suffix() == NULL) {
        etree_filename_suffix = g_strdup("-");
    }
    if (appconfig_get_etree_cd_length() == NULL) {
        etree_cd_length = g_strdup("80");
    }
}
