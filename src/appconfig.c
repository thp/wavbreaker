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
#include "sample.h"
#include "popupmessage.h"
#include "nosound.h"

#include "gettext.h"

#ifdef HAVE_ALSA
#include "alsaaudio.h"
#endif

#ifdef HAVE_PULSE
#include "pulseaudio.h"
#endif

#ifdef HAVE_OSS
#include "linuxaudio.h"
#endif

#include <gtk/gtk.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define APPCONFIG_FILENAME "/.wavbreaker"

#define NOSOUND 0
#define OSS     1
#define ALSA    2
#define PULSE   3

/**
 * Descriptive name of our output modules
 **/
#define NOSOUND_STR _("Disable audio output")
#define OSS_STR       "OSS"
#define ALSA_STR      "ALSA"
#define PULSE_STR     "PulseAudio"

static GtkWidget *window;

static int config_file_version = 1;

/* Function pointers to the currently selected audio driver. */
static int audio_driver_type = -1;
static AudioFunctionPointers audio_function_pointers;
static GtkWidget *combo_box = NULL;

/* oss audio driver options */
static char *oss_output_device = NULL;

/* alsa audio driver options */
static char *alsa_output_device = NULL;

/* default audio driver options */
static GtkWidget *output_device_entry = NULL;
static GtkWidget *output_device_label = NULL;

/* Output directory for wave files. */
static int use_outputdir = 0;
static GtkWidget *use_outputdir_toggle = NULL;
static char *outputdir = NULL;
static GtkWidget *outputdir_entry = NULL;
static GtkWidget *browse_button = NULL;

/* Etree filename suffix */
/* Filename suffix (not extension) for wave files. */
static int use_etree_filename_suffix = 0;

static char *etree_filename_suffix = NULL;
static GtkWidget *etree_filename_suffix_label = NULL;
static GtkWidget *etree_filename_suffix_entry = NULL;

/* Radio buttons */
static GtkWidget *radio1 = NULL;
static GtkWidget *radio2 = NULL;

/* Prepend File Number for wave files. */
static int prepend_file_number = 0;
static GtkWidget *prepend_file_number_toggle = NULL;

/* CD Length disc cutoff. */
static char *etree_cd_length = NULL;
static GtkWidget *etree_cd_length_label = NULL;
static GtkWidget *etree_cd_length_entry = NULL;

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
static GtkWidget *silence_spin_button = NULL;

/* Ask user if the user really wants to quit wavbreaker. */
static int ask_really_quit = 1;

/* Show toolbar in main window */
static int show_toolbar = 1;

/* Draw moodbar in main window */
static int show_moodbar = 1;

/* function prototypes */
static int appconfig_read_file();
static void default_all_strings();
static void open_select_outputdir();

static int get_combo_box_selected_type();

static char *get_audio_nosound_options_output_device();
static char *get_audio_oss_options_output_device();
static char *get_audio_alsa_options_output_device();
static char *get_audio_pulse_options_output_device();

int get_config_file_version()
{
    return config_file_version;
}

void set_config_file_version(int x)
{
    config_file_version = x;
}

AudioFunctionPointers *get_audio_function_pointers()
{
    return &audio_function_pointers;
}

int get_audio_driver_type()
{
    return audio_driver_type;
}

void set_audio_driver_type(int x)
{
    audio_driver_type = x;
}

void set_audio_close_device(void (*f))
{
    audio_function_pointers.audio_close_device = f;
}

void set_audio_open_device(void (*f))
{
    audio_function_pointers.audio_open_device = f;
}

void set_audio_write(void (*f))
{
    audio_function_pointers.audio_write = f;
}

void set_audio_get_outputdev(void (*f))
{
    audio_function_pointers.get_outputdev = f;
}

void set_nosound_driver() {
    set_audio_driver_type(NOSOUND);
    set_audio_close_device(nosound_audio_close_device);
    set_audio_open_device(nosound_audio_open_device);
    set_audio_write(nosound_audio_write);
    set_audio_get_outputdev(get_audio_nosound_options_output_device);
}

void set_audio_function_pointers_with_index(int index)
{

    switch (index) {
#ifdef HAVE_OSS
        case OSS:
            set_audio_driver_type(OSS);
            set_audio_close_device(oss_audio_close_device);
            set_audio_open_device(oss_audio_open_device);
            set_audio_write(oss_audio_write);
            set_audio_get_outputdev(get_audio_oss_options_output_device);
            break;
#endif
#ifdef HAVE_ALSA
        case ALSA:
            set_audio_driver_type(ALSA);
            set_audio_close_device(alsa_audio_close_device);
            set_audio_open_device(alsa_audio_open_device);
            set_audio_write(alsa_audio_write);
            set_audio_get_outputdev(get_audio_alsa_options_output_device);
            break;
#endif
#ifdef HAVE_PULSE
        case PULSE:
            set_audio_driver_type(PULSE);
            set_audio_close_device(pulse_audio_close_device);
            set_audio_open_device(pulse_audio_open_device);
            set_audio_get_outputdev(get_audio_pulse_options_output_device);
            set_audio_write(pulse_audio_write);
            break;
#endif
        case NOSOUND:
            set_nosound_driver();
            break;
        default:
            set_nosound_driver();
            break;
    }
}

void set_audio_function_pointers()
{
    if (get_audio_driver_type() == OSS) {
        set_audio_oss_options_output_device(gtk_entry_get_text(
            GTK_ENTRY(output_device_entry)));
    } else if (get_audio_driver_type() == ALSA) {
        set_audio_alsa_options_output_device(gtk_entry_get_text(
            GTK_ENTRY(output_device_entry)));
    }

    set_audio_function_pointers_with_index(get_combo_box_selected_type());

    if (get_audio_driver_type() == NOSOUND) {
        gtk_widget_hide( output_device_label);
        gtk_widget_hide( output_device_entry);
    } else {
        if( get_audio_driver_type() == ALSA ||
            get_audio_driver_type() == PULSE) {
            gtk_widget_hide( output_device_label);
            gtk_widget_hide( output_device_entry);
        } else {
            gtk_widget_show( output_device_label);
            gtk_widget_show( output_device_entry);
        }
        gtk_entry_set_text(GTK_ENTRY(output_device_entry), audio_options_get_output_device());
    }
}

void default_audio_function_pointers()
{
    set_audio_function_pointers_with_index(NOSOUND);
#ifdef HAVE_PULSE
    set_audio_function_pointers_with_index(PULSE);
#endif
#ifdef HAVE_OSS
    set_audio_function_pointers_with_index(OSS);
#endif
#ifdef HAVE_ALSA
    set_audio_function_pointers_with_index(ALSA);
#endif
}

/* nosound audio driver options */
char *get_audio_nosound_options_output_device() {
    return "";
}

/* oss audio driver options */
char *get_audio_oss_options_output_device() {
    return oss_output_device;
}

void set_audio_oss_options_output_device(const char *val) {
    if (oss_output_device != NULL) {
        g_free(oss_output_device);
    }
    oss_output_device = g_strdup(val);
}

void default_audio_oss_options_output_device() {
    set_audio_oss_options_output_device("/dev/dsp");
}

/* alsa audio driver options */
char *get_audio_alsa_options_output_device() {
    return alsa_output_device;
}

void set_audio_alsa_options_output_device(const char *val) {
    if (alsa_output_device != NULL) {
        g_free(alsa_output_device);
    }
    alsa_output_device = g_strdup(val);
}

void default_audio_alsa_options_output_device() {
    set_audio_alsa_options_output_device("default");
}

/* pulseaudio driver options */
char *get_audio_pulse_options_output_device() {
    /**
     * This is currently a placeholder function, and the
     * return value is not used in the PulseAudio driver.
     **/
    return "";
}

/* generic audio driver functions */

char *audio_options_get_output_device()
{
    if (get_audio_driver_type() == NOSOUND) {
        return get_audio_nosound_options_output_device();
    } else if (get_audio_driver_type() == OSS) {
        return get_audio_oss_options_output_device();
    } else if (get_audio_driver_type() == ALSA) {
        return get_audio_alsa_options_output_device();
    } else if (get_audio_driver_type() == PULSE) {
        return get_audio_pulse_options_output_device();
    }

    /* Wrong setting - return empty string */
    return "";
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

int appconfig_get_ask_really_quit()
{
    return ask_really_quit;
}

void appconfig_set_ask_really_quit(int x)
{
    ask_really_quit = x;
}

int appconfig_get_show_toolbar() {
    return show_toolbar;
}

void appconfig_set_show_toolbar(int x)
{
    show_toolbar = x;
}

int appconfig_get_show_moodbar() {
    return show_moodbar;
}

void appconfig_set_show_moodbar(int x)
{
    show_moodbar = x;
}

int get_use_outputdir()
{
    return use_outputdir;
}

void set_use_outputdir(const char *val)
{
    use_outputdir = atoi(val);
}

char *get_outputdir()
{
    return outputdir;
}

void set_outputdir(const char *val)
{
    if (outputdir != NULL) {
        g_free(outputdir);
    }
    outputdir = g_strdup(val);
}

char *default_outputdir()
{
    if (outputdir != NULL) {
        g_free(outputdir);
    }
    outputdir = g_strdup(getenv("PWD"));
    return outputdir;
}

int get_use_etree_filename_suffix()
{
    return use_etree_filename_suffix;
}

void set_use_etree_filename_suffix(const char *val)
{
    use_etree_filename_suffix = atoi(val);
}

char *get_etree_filename_suffix()
{
    return etree_filename_suffix;
}

void set_etree_filename_suffix(const char *val)
{
    if (etree_filename_suffix != NULL) {
        g_free(etree_filename_suffix);
    }
    etree_filename_suffix = g_strdup(val);
}

char *default_etree_filename_suffix()
{
    if (etree_filename_suffix != NULL) {
        g_free(etree_filename_suffix);
    }
    etree_filename_suffix = g_strdup("-");
    return etree_filename_suffix;
}

int get_prepend_file_number()
{
    return prepend_file_number;
}

void set_prepend_file_number(const char *val)
{
    prepend_file_number = atoi(val);
}

char *get_etree_cd_length()
{
    return etree_cd_length;
}

void set_etree_cd_length(const char *val)
{
    if (etree_cd_length != NULL) {
        g_free(etree_cd_length);
    }
    etree_cd_length = g_strdup(val);
}

char *default_etree_cd_length()
{
    if (etree_cd_length != NULL) {
        g_free(etree_cd_length);
    }
    etree_cd_length = g_strdup("80");
    return etree_cd_length;
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

char *default_config_filename()
{
    char str[1024];

    if (config_filename != NULL) {
        g_free(config_filename);
    }

    str[0] = '\0';
    strncat(str, getenv("HOME"), 1024);
    strncat(str, APPCONFIG_FILENAME, 1024);
    config_filename = g_strdup(str);

    return config_filename;
}

static void use_outputdir_toggled(GtkWidget *widget, gpointer user_data)
{
    if (get_use_outputdir()) {
        // disable the output dir widget
        gtk_widget_set_sensitive(outputdir_entry, FALSE);
        gtk_widget_set_sensitive(browse_button, FALSE);
        set_use_outputdir("0");
    } else {
        // enable the output dir widget
        gtk_widget_set_sensitive(outputdir_entry, TRUE);
        gtk_widget_set_sensitive(browse_button, TRUE);
        set_use_outputdir("1");
    }
}

static void use_etree_filename_suffix_toggled(GtkWidget *widget, gpointer user_data)
{
    if (get_use_etree_filename_suffix()) {
        set_use_etree_filename_suffix("0");
    } else {
        set_use_etree_filename_suffix("1");
    }
}

static void radio_buttons_toggled(GtkWidget *widget, gpointer user_data)
{
    if( GTK_TOGGLE_BUTTON(radio1)->active == TRUE) {
        gtk_widget_set_sensitive( prepend_file_number_toggle, TRUE);
        gtk_widget_set_sensitive( etree_filename_suffix_entry, TRUE);
        gtk_widget_set_sensitive( etree_filename_suffix_label, TRUE);
        gtk_widget_set_sensitive( etree_cd_length_entry, FALSE);
        gtk_widget_set_sensitive( etree_cd_length_label, FALSE);
    } else {
        gtk_widget_set_sensitive( prepend_file_number_toggle, FALSE);
        gtk_widget_set_sensitive( etree_filename_suffix_entry, FALSE);
        gtk_widget_set_sensitive( etree_filename_suffix_label, FALSE);
        gtk_widget_set_sensitive( etree_cd_length_entry, TRUE);
        gtk_widget_set_sensitive( etree_cd_length_label, TRUE);
    }
}

static void prepend_file_number_toggled(GtkWidget *widget, gpointer user_data)
{
    if (get_prepend_file_number()) {
        set_prepend_file_number("0");
    } else {
        set_prepend_file_number("1");
    }
}

static void appconfig_hide(GtkWidget *main_window)
{
    gtk_widget_destroy(main_window);
}

static void browse_button_clicked(GtkWidget *widget, gpointer user_data)
{
    open_select_outputdir();
}

static void open_select_outputdir() {
    GtkWidget *dialog;

    dialog = gtk_file_chooser_dialog_new(_("Select Output Directory"),
        GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
        GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog),
        gtk_entry_get_text(GTK_ENTRY(outputdir_entry)));

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(outputdir_entry), filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

static int get_combo_box_selected_type() {
    char *selected;

    selected = gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo_box));
    
    if (selected == NULL) {
        return NOSOUND;
    }

    if (strcmp(selected, OSS_STR) == 0) {
        return OSS;
    } else if (strcmp(selected, ALSA_STR) == 0) {
        return ALSA;
    } else if (strcmp(selected, PULSE_STR) == 0) {
        return PULSE;
    }

    g_free(selected);
    return NOSOUND;
}

static void ok_button_clicked(GtkWidget *widget, gpointer user_data)
{
    set_outputdir(gtk_entry_get_text(GTK_ENTRY(outputdir_entry)));
    set_etree_filename_suffix(gtk_entry_get_text(GTK_ENTRY(etree_filename_suffix_entry)));
    set_etree_cd_length(gtk_entry_get_text(GTK_ENTRY(etree_cd_length_entry)));
    appconfig_set_silence_percentage( gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(silence_spin_button)));

    set_audio_function_pointers_with_index(get_combo_box_selected_type());

    if (get_audio_driver_type() == OSS) {
        set_audio_oss_options_output_device(gtk_entry_get_text(
            GTK_ENTRY(output_device_entry)));
    } else if (get_audio_driver_type() == ALSA) {
        set_audio_alsa_options_output_device(gtk_entry_get_text(
            GTK_ENTRY(output_device_entry)));
    }

    track_break_rename( FALSE);
    appconfig_hide(GTK_WIDGET(user_data));
    appconfig_write_file();
}

void appconfig_show(GtkWidget *main_window)
{
    GtkWidget *vbox;
    GtkWidget *table;
    GtkWidget *hbbox;
    GtkWidget *outputdev_label;
    GtkWidget *ok_button;
    GtkWidget *label;

    GtkWidget *notebook;
    int driver_type, pos;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_realize(window);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(main_window));
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
    gdk_window_set_functions( window->window, GDK_FUNC_MOVE);
    gtk_window_set_title( GTK_WINDOW(window), _("wavbreaker Preferences"));

    /* create the vbox for the first tab */
    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add( GTK_CONTAINER(window), vbox);

    notebook = gtk_notebook_new();
    gtk_container_add( GTK_CONTAINER(vbox), notebook);

    /* Selectable Output Directory */
    table = gtk_table_new(2, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    gtk_table_set_row_spacings( GTK_TABLE(table), 5);
    gtk_notebook_append_page( GTK_NOTEBOOK(notebook), table, gtk_label_new( _("General")));

    use_outputdir_toggle = gtk_check_button_new_with_label(_("Save output files in folder:"));
    gtk_table_attach(GTK_TABLE(table), use_outputdir_toggle,
        0, 2, 0, 1, GTK_FILL, 0, 5, 0);
    g_signal_connect(GTK_OBJECT(use_outputdir_toggle), "toggled",
        G_CALLBACK(use_outputdir_toggled), NULL);

    outputdir_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(outputdir_entry), outputdir);
    gtk_entry_set_width_chars(GTK_ENTRY(outputdir_entry), 40);
    gtk_table_attach(GTK_TABLE(table), outputdir_entry,
        0, 1, 1, 2, GTK_EXPAND | GTK_FILL, 0, 5, 0);

    browse_button = gtk_button_new_with_label(_("Browse"));
    gtk_table_attach(GTK_TABLE(table), browse_button,
        1, 2, 1, 2, GTK_FILL, 0, 5, 0);
    g_signal_connect(G_OBJECT(browse_button), "clicked",
            (GtkSignalFunc)browse_button_clicked, window);

    silence_spin_button = (GtkWidget*)gtk_spin_button_new_with_range( 1.0, 100.0, 1.0);
    gtk_spin_button_set_digits( GTK_SPIN_BUTTON(silence_spin_button), 0);
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(silence_spin_button), appconfig_get_silence_percentage());
    
    label = gtk_label_new( _("Maximum volume considered silence (in percent):"));
    gtk_misc_set_alignment( GTK_MISC(label), 0.0, 0.5);

    gtk_table_attach( GTK_TABLE(table), label,
        0, 1, 2, 3, GTK_FILL, 0, 5, 0);
    gtk_table_attach( GTK_TABLE(table), silence_spin_button,
        1, 2, 2, 3, GTK_EXPAND, 0, 5, 0);

    /* Etree Filename Suffix */

    table = gtk_table_new(4, 10, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    gtk_notebook_append_page( GTK_NOTEBOOK(notebook), table, gtk_label_new( _("File Naming")));

    radio1 = gtk_radio_button_new_with_label(NULL, _("Standard (##)"));
    gtk_table_attach(GTK_TABLE(table), radio1, 0, 3, 0, 1, GTK_FILL, 0, 5, 2);
    g_signal_connect(GTK_OBJECT(radio1), "toggled",
        G_CALLBACK(radio_buttons_toggled), NULL);

    label = gtk_label_new("   ");
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
            0, 1, 2, 3, GTK_FILL, 0, 5, 2);

    prepend_file_number_toggle = gtk_check_button_new_with_label(_("Prepend number before filename"));
    gtk_table_attach(GTK_TABLE(table), prepend_file_number_toggle,
            1, 3, 2, 3, GTK_FILL, 0, 5, 0);
    g_signal_connect(GTK_OBJECT(prepend_file_number_toggle), "toggled",
        G_CALLBACK(prepend_file_number_toggled), NULL);

    etree_filename_suffix_label = gtk_label_new(_("Separator:"));
    gtk_misc_set_alignment(GTK_MISC(etree_filename_suffix_label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), etree_filename_suffix_label,
            1, 2, 1, 2, GTK_FILL, 0, 5, 2);

    etree_filename_suffix_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(etree_filename_suffix_entry), etree_filename_suffix);
    gtk_entry_set_width_chars(GTK_ENTRY(etree_filename_suffix_entry), 10);
    gtk_table_attach(GTK_TABLE(table), etree_filename_suffix_entry,
            2, 3, 1, 2, GTK_EXPAND | GTK_FILL, 0, 5, 2);

    radio2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio1),
            _("etree.org (d#t##)"));
    gtk_table_attach(GTK_TABLE(table), radio2, 0, 3, 3, 4, GTK_FILL, 0, 5, 2);

    etree_cd_length_label = gtk_label_new(_("CD Length:"));
    gtk_misc_set_alignment(GTK_MISC(etree_cd_length_label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), etree_cd_length_label,
            1, 2, 5, 6, GTK_FILL, 0, 5, 2);

    etree_cd_length_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(etree_cd_length_entry), etree_cd_length);
    gtk_entry_set_width_chars(GTK_ENTRY(etree_cd_length_entry), 10);
    gtk_table_attach(GTK_TABLE(table), etree_cd_length_entry,
            2, 3, 5, 6, GTK_EXPAND | GTK_FILL, 0, 5, 2);

    /* Audio Output Device */
    table = gtk_table_new(2, 2, FALSE);
    gtk_table_set_row_spacings( GTK_TABLE(table), 10);
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    gtk_notebook_append_page( GTK_NOTEBOOK(notebook), table, gtk_label_new( _("Audio Device")));

    outputdev_label = gtk_label_new(_("Audio Device:"));
    gtk_misc_set_alignment(GTK_MISC(outputdev_label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), outputdev_label,
            0, 1, 0, 1, GTK_FILL, 0, 5, 0);

    combo_box = gtk_combo_box_new_text ();
    
    driver_type = get_audio_driver_type();
    pos = 0;

    gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box), NOSOUND_STR);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), pos);
#ifdef HAVE_OSS
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box), OSS_STR);
    pos++;
    if (driver_type == OSS) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), pos);
    }
#endif
#ifdef HAVE_ALSA
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box), ALSA_STR);
    pos++;
    if (driver_type == ALSA) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), pos);
    }
#endif
#ifdef HAVE_PULSE
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box), PULSE_STR);
    pos++;
    if (driver_type == PULSE) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), pos);
    }
#endif
    gtk_widget_set_sensitive( GTK_WIDGET(combo_box), sample_is_playing()?FALSE:TRUE);
    gtk_table_attach(GTK_TABLE(table), combo_box,
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 5, 0);
    g_signal_connect(G_OBJECT(combo_box), "changed",
            (GtkSignalFunc)set_audio_function_pointers, NULL);

    output_device_label = gtk_label_new(_("Output device:"));
    gtk_misc_set_alignment( GTK_MISC(output_device_label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), output_device_label,
            0, 1, 1, 2, GTK_FILL, 0, 5, 0);

    output_device_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(output_device_entry), audio_options_get_output_device());
    gtk_widget_set_sensitive( GTK_WIDGET(output_device_entry), sample_is_playing()?FALSE:TRUE);
    gtk_table_attach(GTK_TABLE(table), output_device_entry,
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 5, 0);


    /* OK and Cancel Buttons */
    hbbox = gtk_hbutton_box_new();
    gtk_container_add(GTK_CONTAINER(vbox), hbbox);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(hbbox), 10);

    ok_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    gtk_box_pack_end(GTK_BOX(hbbox), ok_button, FALSE, FALSE, 5);
    g_signal_connect(G_OBJECT(ok_button), "clicked",
        (GtkSignalFunc)ok_button_clicked, window);

    g_signal_connect(GTK_OBJECT(radio2), "toggled",
        G_CALLBACK(use_etree_filename_suffix_toggled), NULL);

    if (get_use_outputdir()) {
        // enable the output dir widget
        gtk_widget_set_sensitive(outputdir_entry, TRUE);
        gtk_widget_set_sensitive(browse_button, TRUE);

        //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_outputdir_toggle), TRUE);
        // set directly so the toggled event is not emitted
        GTK_TOGGLE_BUTTON(use_outputdir_toggle)->active = TRUE;
    } else {
        // disable the output dir widget
        gtk_widget_set_sensitive(outputdir_entry, FALSE);
        gtk_widget_set_sensitive(browse_button, FALSE);

        //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_outputdir_toggle), FALSE);
        // set directly so the toggled event is not emitted
        GTK_TOGGLE_BUTTON(use_outputdir_toggle)->active = FALSE;
    }

    if (get_prepend_file_number()) {
        GTK_TOGGLE_BUTTON(prepend_file_number_toggle)->active = TRUE;
    } else {
        GTK_TOGGLE_BUTTON(prepend_file_number_toggle)->active = FALSE;
    }

    if (get_use_etree_filename_suffix()) {
        // set directly so the toggled event is not emitted
        GTK_TOGGLE_BUTTON(radio1)->active = FALSE;
        GTK_TOGGLE_BUTTON(radio2)->active = TRUE;
    } else {
        // set directly so the toggled event is not emitted
        GTK_TOGGLE_BUTTON(radio1)->active = TRUE;
        GTK_TOGGLE_BUTTON(radio2)->active = FALSE;
    }

    gtk_widget_show_all(window);
    set_audio_function_pointers();
    radio_buttons_toggled( NULL, NULL);
}

static int appconfig_read_file() {
    xmlDocPtr doc;
    xmlNodePtr cur;

    doc = xmlParseFile(get_config_filename());

    if (doc == NULL) {
        fprintf(stderr, "error reading wavbreaker config file\n");
        fprintf(stderr, "Document not parsed successfully.\n");
        return 1;
    }

    cur = xmlDocGetRootElement(doc);

    if (cur == NULL) {
        fprintf(stderr, "error reading wavbreaker config file\n");
        fprintf(stderr, "empty document\n");
        xmlFreeDoc(doc);
        return 2;
    }

    if (xmlStrcmp(cur->name, (const xmlChar *) "wavbreaker")) {
        fprintf(stderr, "error reading wavbreaker config file\n");
        fprintf(stderr, "wrong document type, root node != wavbreaker\n");
        fprintf(stderr, "document type is: %s\n", cur->name);
        xmlFreeDoc(doc);
        return 3;
    }

    cur = cur->xmlChildrenNode;
    while (cur != NULL) {
        xmlChar *key;
        int nkey;

        if (!(xmlStrcmp(cur->name, (const xmlChar *) "use_outputdir"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            set_use_outputdir((char *)key);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "config_file_version"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            nkey = atoi((char *)key);
            set_config_file_version(nkey);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "oss_options_output_device"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            set_audio_oss_options_output_device((char *)key);
            xmlFree(key);
        /***
         * Don't load ALSA output device config parameter (always use "default")
         *
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "alsa_options_output_device"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            set_audio_alsa_options_output_device((char *)key);
            xmlFree(key);
         *
         ***/
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "outputdir"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            set_outputdir((char *)key);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "use_etree_filename_suffix"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            set_use_etree_filename_suffix((char *)key);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "prepend_file_number"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            set_prepend_file_number((char *)key);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "etree_filename_suffix"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            set_etree_filename_suffix((char *)key);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "etree_cd_length"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            set_etree_cd_length((char *)key);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "audio_driver_type"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            nkey = atoi((char *)key);
            set_audio_function_pointers_with_index(nkey);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "main_window_xpos"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            nkey = atoi((char *)key);
            appconfig_set_main_window_xpos(nkey);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "main_window_ypos"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            nkey = atoi((char *)key);
            appconfig_set_main_window_ypos(nkey);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "main_window_width"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            nkey = atoi((char *)key);
            appconfig_set_main_window_width(nkey);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "main_window_height"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            nkey = atoi((char *)key);
            appconfig_set_main_window_height(nkey);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "vpane1_position"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            nkey = atoi((char *)key);
            appconfig_set_vpane1_position(nkey);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "vpane2_position"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            nkey = atoi((char *)key);
            appconfig_set_vpane2_position(nkey);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "silence_percentage"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            nkey = atoi((char *)key);
            appconfig_set_silence_percentage(nkey);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "ask_really_quit"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            nkey = atoi((char *)key);
            appconfig_set_ask_really_quit(nkey);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "show_toolbar"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            nkey = atoi((char *)key);
            appconfig_set_show_toolbar(nkey);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "show_moodbar"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            nkey = atoi((char *)key);
            appconfig_set_show_moodbar(nkey);
            xmlFree(key);
        }
        cur = cur->next;
    }

    return 0;
}

/*
static int check_xml_node_ptr_for_null(xmlNodePtr ptr) {
}
*/

int appconfig_write_file() {
    xmlDocPtr doc;
    xmlNodePtr root;
    xmlNodePtr cur;
    char tmp_str[32];

    doc = xmlNewDoc((const xmlChar *)"1.0");

    if (doc == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error with xmlNewDoc\n");
        return 1;
    }

    root = xmlNewDocNode(doc, NULL, (const xmlChar *)"wavbreaker", (xmlChar*) "");

    if (root == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating doc node\n");
        xmlFreeDoc(doc);
        return 2;
    }

    sprintf(tmp_str, "%d", get_config_file_version());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"config_file_version", (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating config_file_version node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", get_use_outputdir());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"use_outputdir", (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating use_outputdir node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    cur = xmlNewChild(root, NULL, (const xmlChar *)"outputdir",
        (const xmlChar *) get_outputdir());

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating outputdir node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    cur = xmlNewChild(root, NULL, (const xmlChar *)"oss_options_output_device",
        (const xmlChar *) get_audio_oss_options_output_device());

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating oss_options_output_device node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    cur = xmlNewChild(root, NULL, (const xmlChar *)"alsa_options_output_device",
            (const xmlChar *) get_audio_alsa_options_output_device());

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating alsa_options_output_device node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", get_use_etree_filename_suffix());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"use_etree_filename_suffix",
            (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating use_etree_filename_suffix node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", get_prepend_file_number());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"prepend_file_number",
            (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating prepend_file_number node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", appconfig_get_main_window_xpos());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"main_window_xpos",
        (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating main_window_xpos node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", appconfig_get_main_window_ypos());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"main_window_ypos",
        (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating main_window_ypos node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    cur = xmlNewChild(root, NULL, (const xmlChar *)"etree_filename_suffix",
        (const xmlChar *) get_etree_filename_suffix());

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating etree_filename_suffix node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    cur = xmlNewChild(root, NULL, (const xmlChar *)"etree_cd_length",
        (const xmlChar *) get_etree_cd_length());

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating etree_cd_length node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", get_audio_driver_type());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"audio_driver_type",
        (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating audio_driver_type node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }
 
    sprintf(tmp_str, "%d", appconfig_get_main_window_width());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"main_window_width",
        (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating main_window_width node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", appconfig_get_main_window_height());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"main_window_height",
        (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating main_window_height node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", appconfig_get_vpane1_position());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"vpane1_position",
        (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating vpane1_position node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", appconfig_get_vpane2_position());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"vpane2_position",
        (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating vpane2_position node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", appconfig_get_silence_percentage());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"silence_percentage",
        (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating silence_percentage node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", appconfig_get_ask_really_quit());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"ask_really_quit",
        (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating ask_really_quit node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", appconfig_get_show_toolbar());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"show_toolbar",
        (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating show_toolbar node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", appconfig_get_show_moodbar());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"show_moodbar",
        (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating show_moodbar node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    root = xmlDocSetRootElement(doc, root);
    /*
    if (root == NULL) {
        fprintf(stderr, "error setting doc root node\n");
        xmlFreeDoc(doc);
        return 4;
    }
    */

    xmlIndentTreeOutput = 1;

    if (! xmlSaveFormatFile(get_config_filename(), doc, 1)) {
        fprintf(stderr, "error writing config file: %s", get_config_filename());
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 5;
    }

    xmlFreeNodeList(root);
    xmlFreeDoc(doc);

    return 0;
}

void appconfig_init()
{
    default_config_filename();

    /*
     * See if we can read and write the config file,
     * then try to read in the values.
     */
    if( access( get_config_filename(), W_OK | F_OK) || appconfig_read_file()) {
        default_all_strings();
        appconfig_write_file();
    } else {
        default_all_strings();
    }
}

void default_all_strings() {
    /* default any values that where not in the config file */
    if (get_outputdir() == NULL) {
        default_outputdir();
    }
    if (get_etree_filename_suffix() == NULL) {
        default_etree_filename_suffix();
    }
    if (get_etree_cd_length() == NULL) {
        default_etree_cd_length();
    }

    if (get_audio_oss_options_output_device() == NULL) {
        default_audio_oss_options_output_device();
    }
    if (get_audio_alsa_options_output_device() == NULL) {
        default_audio_alsa_options_output_device();
    }

    if (get_audio_driver_type() == -1) {
        default_audio_function_pointers();
    }
}

