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
#include "appconfig_gtk.h"

#include "sample_info.h"
#include "popupmessage.h"
#include "wavbreaker.h"

#include "gettext.h"

#include <gtk/gtk.h>
#include <glib.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static GtkWidget *window;

static gboolean loading_ui = FALSE;

/* Output directory for wave files. */
static GtkWidget *use_outputdir_toggle = NULL;
static GtkWidget *outputdir_entry = NULL;
static GtkWidget *browse_button = NULL;

/* Etree filename suffix */
/* Filename suffix (not extension) for wave files. */
static GtkWidget *etree_filename_suffix_label = NULL;
static GtkWidget *etree_filename_suffix_entry = NULL;

/* Radio buttons */
static GtkWidget *radio1 = NULL;
static GtkWidget *radio2 = NULL;

/* Prepend File Number for wave files. */
static GtkWidget *prepend_file_number_toggle = NULL;

/* CD Length disc cutoff. */
static GtkWidget *etree_cd_length_label = NULL;
static GtkWidget *etree_cd_length_entry = NULL;

static GtkWidget *silence_spin_button = NULL;

/* Forward declarations */
static void open_select_outputdir();

static void use_outputdir_toggled(GtkWidget *widget, gpointer user_data)
{
    if (loading_ui) {
        return;
    }

    if (appconfig_get_use_outputdir()) {
        // disable the output dir widget
        gtk_widget_set_sensitive(outputdir_entry, FALSE);
        gtk_widget_set_sensitive(browse_button, FALSE);
        appconfig_set_use_outputdir(0);
    } else {
        // enable the output dir widget
        gtk_widget_set_sensitive(outputdir_entry, TRUE);
        gtk_widget_set_sensitive(browse_button, TRUE);
        appconfig_set_use_outputdir(1);
    }
}

static void use_etree_filename_suffix_toggled(GtkWidget *widget, gpointer user_data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        appconfig_set_use_etree_filename_suffix(1);
    } else {
        appconfig_set_use_etree_filename_suffix(0);
    }
}

static void radio_buttons_toggled(GtkWidget *widget, gpointer user_data)
{
    if (loading_ui) {
        return;
    }

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio1)) == TRUE) {
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
    if (loading_ui) {
        return;
    }

    if (appconfig_get_prepend_file_number()) {
        appconfig_set_prepend_file_number(0);
    } else {
        appconfig_set_prepend_file_number(1);
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
        _("Cancel"), GTK_RESPONSE_CANCEL,
        _("Open"), GTK_RESPONSE_ACCEPT,
        NULL);
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

static void
on_appconfig_close(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    appconfig_set_outputdir(gtk_entry_get_text(GTK_ENTRY(outputdir_entry)));
    appconfig_set_etree_filename_suffix(gtk_entry_get_text(GTK_ENTRY(etree_filename_suffix_entry)));
    appconfig_set_etree_cd_length(gtk_entry_get_text(GTK_ENTRY(etree_cd_length_entry)));
    appconfig_set_silence_percentage( gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(silence_spin_button)));

    wavbreaker_update_listmodel();

    appconfig_hide(GTK_WIDGET(user_data));
    appconfig_write_file();
}

void appconfig_show(GtkWidget *main_window)
{
    GtkWidget *vbox;
    GtkWidget *grid;
    GtkWidget *label;

    GtkWidget *stack;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(main_window));
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);

    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), _("Preferences"));
    gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

    /* create the vbox for the first tab */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add( GTK_CONTAINER(window), vbox);

    stack = gtk_stack_new();
    gtk_container_add(GTK_CONTAINER(vbox), stack);

    GtkWidget *stack_switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(stack_switcher), GTK_STACK(stack));
    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(header_bar), stack_switcher);

    /* Selectable Output Directory */
    grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_stack_add_titled(GTK_STACK(stack), grid, "general", _("General"));

    use_outputdir_toggle = gtk_check_button_new_with_label(_("Save output files in folder:"));
    gtk_grid_attach(GTK_GRID(grid), use_outputdir_toggle,
            0, 0, 2, 1);
    g_signal_connect(G_OBJECT(use_outputdir_toggle), "toggled",
        G_CALLBACK(use_outputdir_toggled), NULL);

    outputdir_entry = gtk_entry_new();
    g_object_set(outputdir_entry, "hexpand", TRUE, NULL);
    gtk_entry_set_text(GTK_ENTRY(outputdir_entry), appconfig_get_outputdir());
    gtk_entry_set_width_chars(GTK_ENTRY(outputdir_entry), 40);
    gtk_grid_attach(GTK_GRID(grid), outputdir_entry,
        0, 1, 1, 1);

    browse_button = gtk_button_new_with_label(_("Browse"));
    gtk_grid_attach(GTK_GRID(grid), browse_button,
        1, 1, 1, 1);
    g_signal_connect(G_OBJECT(browse_button), "clicked",
            (GCallback)browse_button_clicked, window);

    silence_spin_button = (GtkWidget*)gtk_spin_button_new_with_range( 1.0, 100.0, 1.0);
    gtk_spin_button_set_digits( GTK_SPIN_BUTTON(silence_spin_button), 0);
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(silence_spin_button), appconfig_get_silence_percentage());
    
    label = gtk_label_new( _("Maximum volume considered silence (in percent):"));
    g_object_set(G_OBJECT(label), "xalign", 0.0f, "yalign", 0.5f, NULL);

    gtk_grid_attach(GTK_GRID(grid), label,
        0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), silence_spin_button,
        1, 2, 1, 1);

    /* Etree Filename Suffix */

    grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_stack_add_titled(GTK_STACK(stack), grid, "naming", _("File Naming"));

    radio1 = gtk_radio_button_new_with_label(NULL, _("Standard (##)"));
    gtk_grid_attach(GTK_GRID(grid), radio1, 0, 0, 3, 1);
    g_signal_connect(G_OBJECT(radio1), "toggled",
        G_CALLBACK(radio_buttons_toggled), NULL);

    etree_filename_suffix_label = gtk_label_new(_("Separator:"));
    g_object_set(G_OBJECT(etree_filename_suffix_label), "xalign", 0.0f, "yalign", 0.5f, NULL);
    gtk_grid_attach(GTK_GRID(grid), etree_filename_suffix_label,
            1, 1, 1, 1);

    etree_filename_suffix_entry = gtk_entry_new();
    g_object_set(etree_filename_suffix_entry, "hexpand", TRUE, NULL);
    gtk_entry_set_text(GTK_ENTRY(etree_filename_suffix_entry), appconfig_get_etree_filename_suffix());
    gtk_entry_set_width_chars(GTK_ENTRY(etree_filename_suffix_entry), 10);
    gtk_grid_attach(GTK_GRID(grid), etree_filename_suffix_entry,
            2, 1, 1, 1);

    label = gtk_label_new("   ");
    g_object_set(G_OBJECT(label), "xalign", 0.0f, "yalign", 0.5f, NULL);
    gtk_grid_attach(GTK_GRID(grid), label,
            0, 2, 1, 1);

    prepend_file_number_toggle = gtk_check_button_new_with_label(_("Prepend number before filename"));
    gtk_grid_attach(GTK_GRID(grid), prepend_file_number_toggle,
            1, 2, 2, 1);
    g_signal_connect(G_OBJECT(prepend_file_number_toggle), "toggled",
        G_CALLBACK(prepend_file_number_toggled), NULL);

    radio2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio1),
            _("etree.org (d#t##)"));
    gtk_grid_attach(GTK_GRID(grid), radio2, 0, 3, 3, 1);

    etree_cd_length_label = gtk_label_new(_("CD Length:"));
    g_object_set(G_OBJECT(etree_cd_length_label), "xalign", 0.0f, "yalign", 0.5f, NULL);
    gtk_grid_attach(GTK_GRID(grid), etree_cd_length_label,
            1, 4, 1, 1);

    etree_cd_length_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(etree_cd_length_entry), appconfig_get_etree_cd_length());
    gtk_entry_set_width_chars(GTK_ENTRY(etree_cd_length_entry), 10);
    gtk_grid_attach(GTK_GRID(grid), etree_cd_length_entry,
            2, 4, 1, 1);

    g_signal_connect(G_OBJECT(window),
        "delete-event", G_CALLBACK(on_appconfig_close), window);

    g_signal_connect(G_OBJECT(radio2), "toggled",
        G_CALLBACK(use_etree_filename_suffix_toggled), NULL);

    loading_ui = TRUE;

    gboolean use_output_dir = appconfig_get_use_outputdir() ? TRUE : FALSE;
    gtk_widget_set_sensitive(outputdir_entry, use_output_dir);
    gtk_widget_set_sensitive(browse_button, use_output_dir);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_outputdir_toggle), use_output_dir);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prepend_file_number_toggle),
            appconfig_get_prepend_file_number() ? TRUE : FALSE);

    gboolean use_etree = appconfig_get_use_etree_filename_suffix() ? TRUE : FALSE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio1), !use_etree);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), use_etree);

    loading_ui = FALSE;

    gtk_widget_show_all(window);
    radio_buttons_toggled( NULL, NULL);
}
