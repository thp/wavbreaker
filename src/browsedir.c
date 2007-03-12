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

#include "gettext.h"

#include <config.h>

static GtkWidget *parent_window = NULL;
static void (*callback_set_func)(const char *) = NULL;
static char *(*callback_get_func)() = NULL;


static void open_select_outputdir() {
    GtkWidget *dialog;
    char *filename;

    dialog = gtk_file_chooser_dialog_new( _("Select Output Directory"),
                                          GTK_WINDOW(parent_window),
                                          GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                          NULL);

    gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(dialog), callback_get_func());

    if( gtk_dialog_run( GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog));
        callback_set_func( filename);
        g_free( filename);
    }

    gtk_widget_destroy(dialog);
}

void browsedir_show(GtkWidget *w, void (*csf)(const char *), char *(*cgf)())
{
    parent_window = w;
    callback_set_func = csf;
    callback_get_func = cgf;

    open_select_outputdir();
}

