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

#include <gtk/gtk.h>
#include <string.h>

#include "browsedir.h"

#include <config.h>

static GtkWidget *parent_window = NULL;
static void (*callback_set_func)(const char *) = NULL;
static char *(*callback_get_func)() = NULL;

static void filesel_ok_clicked(GtkWidget *widget, gpointer user_data)
{
    GtkFileSelection *filesel = GTK_FILE_SELECTION(user_data);

    //printf("file: %s\n", gtk_file_selection_get_filename(filesel));
    callback_set_func((char *)gtk_file_selection_get_filename(filesel));

    gtk_widget_destroy(user_data);
}

static void filesel_cancel_clicked(GtkWidget *widget, gpointer user_data)
{
    gtk_widget_destroy(user_data);
}

static void open_select_outputdir_2() {
    GtkWidget *dialog;

    dialog = gtk_file_chooser_dialog_new("Select Output Directory",
            GTK_WINDOW(parent_window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
            GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), callback_get_func());

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
//        printf("filename: %s\n", filename);
        callback_set_func(filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

static void open_select_outputdir() {
    GtkWidget *filesel;
    gchar filename[4096];

    filesel = gtk_file_selection_new("select output directory");
    gtk_window_set_modal(GTK_WINDOW(filesel), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(filesel), GTK_WINDOW(parent_window));
    gtk_window_set_type_hint(GTK_WINDOW(filesel), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_resizable(GTK_WINDOW(filesel), TRUE);

    strcpy(filename, callback_get_func());
    strcat(filename, "/");
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), filename);

//    gtk_dialog_set_has_separator(GTK_DIALOG(filesel), TRUE);
    gtk_widget_hide(GTK_FILE_SELECTION(filesel)->file_list->parent);
    gtk_widget_hide(GTK_FILE_SELECTION(filesel)->file_list);
    gtk_widget_hide(GTK_FILE_SELECTION(filesel)->selection_entry);

    gtk_signal_connect(GTK_OBJECT( GTK_FILE_SELECTION(filesel)->ok_button),
        "clicked", (GtkSignalFunc)filesel_ok_clicked, filesel);

    gtk_signal_connect(GTK_OBJECT( GTK_FILE_SELECTION(filesel)->cancel_button),
        "clicked", (GtkSignalFunc)filesel_cancel_clicked, filesel);

    gtk_widget_show(filesel);
}

void browsedir_show(GtkWidget *w, void (*csf)(const char *), char *(*cgf)())
{
    parent_window = w;
    callback_set_func = csf;
    callback_get_func = cgf;

#ifdef HAVE_GTK_2_3
    open_select_outputdir_2();
#else
    open_select_outputdir();
#endif
}

