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

#include <stdlib.h>
#include <unistd.h>

#include "sample.h"
#include "wavbreaker.h"
#include "browsedir.h"

#include "gettext.h"


static char *saveas_dirname = NULL; 

char *saveas_get_dirname()
{
    return saveas_dirname;
}

void saveas_set_dirname(const char *val)
{
    if (saveas_dirname != NULL) {
        g_free(saveas_dirname);
    }
    saveas_dirname = g_strdup(val);
}

void saveas_show(GtkWidget *parent_window)
{
    GtkWidget *dialog;

    dialog = gtk_file_chooser_dialog_new( _("Select folder to save wave files"),
                                          GTK_WINDOW(parent_window),
                                          GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
                                          GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                          NULL);

    if( saveas_get_dirname() == NULL) {
        saveas_set_dirname( getenv( "PWD"));
    }

    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), saveas_get_dirname());

    if( gtk_dialog_run( GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        saveas_set_dirname( gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog)));
        wavbreaker_write_files( saveas_get_dirname());
    }

    gtk_widget_destroy( GTK_WIDGET(dialog));
}


