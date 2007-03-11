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

static GtkWidget *window;

static char *saveas_dirname = NULL; 
static char *saveas_old_dirname = NULL; 
static GtkWidget *saveas_dirname_entry = NULL;

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

void saveas_set_dirname_entry(const char *val)
{
    if (saveas_dirname_entry == NULL) {
        return;
    }

    gtk_entry_set_text(GTK_ENTRY(saveas_dirname_entry), val);
    saveas_set_dirname(val);
}

char *saveas_get_old_dirname()
{
    return saveas_old_dirname;
}

void saveas_set_old_dirname(const char *val)
{
    if (saveas_old_dirname != NULL) {
        g_free(saveas_old_dirname);
    }
    saveas_old_dirname = g_strdup(val);
}

static void saveas_hide(GtkWidget *main_window)
{
    gtk_widget_destroy(main_window);
}

static void cancel_button_clicked(GtkWidget *widget, gpointer user_data)
{
    saveas_set_dirname(saveas_get_old_dirname());
    saveas_hide(user_data);
}

static void ok_button_clicked(GtkWidget *widget, gpointer user_data)
{
    char *dirname;

    saveas_set_dirname(gtk_entry_get_text(GTK_ENTRY(saveas_dirname_entry)));
    saveas_hide(GTK_WIDGET(user_data));
    wavbreaker_write_files(saveas_get_dirname());
}

static void browse_button_clicked(GtkWidget *widget, gpointer user_data)
{
    browsedir_show(window, saveas_set_dirname_entry, saveas_get_dirname);
}

void saveas_show(GtkWidget *parent_window)
{
    GtkWidget *vbox;
    GtkWidget *table;
    GtkWidget *hbbox;
    GtkWidget *message_label;
    GtkWidget *outputdev_label;
    GtkWidget *hseparator;
    GtkWidget *ok_button, *cancel_button;
    GtkWidget *browse_button;

    if (saveas_get_dirname() == NULL) {
        saveas_set_dirname(getenv("PWD"));
    }
    saveas_set_old_dirname(saveas_get_dirname());

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_realize(window);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(parent_window));
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
    gdk_window_set_functions(window->window, GDK_FUNC_MOVE);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
    gtk_widget_show(vbox);

    table = gtk_table_new(2, 2, FALSE);
    gtk_container_add(GTK_CONTAINER(vbox), table);
    gtk_widget_show(table);

    message_label = gtk_label_new(_("Enter the dirname to save files:"));
    gtk_misc_set_alignment(GTK_MISC(message_label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), message_label,
            0, 2, 0, 1, GTK_FILL, 0, 5, 0);
    gtk_widget_show(message_label);

    saveas_dirname_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(saveas_dirname_entry), saveas_dirname);
    gtk_entry_set_width_chars(GTK_ENTRY(saveas_dirname_entry), 40);
    gtk_table_attach(GTK_TABLE(table), saveas_dirname_entry,
            0, 1, 1, 2, GTK_EXPAND | GTK_FILL, 0, 5, 0);
    gtk_widget_show(saveas_dirname_entry);

    browse_button = gtk_button_new_with_label(_("Browse"));
    gtk_table_attach(GTK_TABLE(table), browse_button,
        1, 2, 1, 2, GTK_FILL, 0, 5, 0);
    g_signal_connect(G_OBJECT(browse_button), "clicked",
            (GtkSignalFunc)browse_button_clicked, window);
    gtk_widget_show(browse_button);

    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, TRUE, 5);
    gtk_widget_show(hseparator);

    hbbox = gtk_hbutton_box_new();
    gtk_container_add(GTK_CONTAINER(vbox), hbbox);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(hbbox), 10);
    gtk_widget_show(hbbox);

    cancel_button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    gtk_box_pack_end(GTK_BOX(hbbox), cancel_button, FALSE, FALSE, 5);
    g_signal_connect(G_OBJECT(cancel_button), "clicked", (GtkSignalFunc)cancel_button_clicked, window);
    gtk_widget_show(cancel_button);

    ok_button = gtk_button_new_from_stock(GTK_STOCK_OK);
    gtk_box_pack_end(GTK_BOX(hbbox), ok_button, FALSE, FALSE, 5);
    g_signal_connect(G_OBJECT(ok_button), "clicked", (GtkSignalFunc)ok_button_clicked, window);
    gtk_widget_show(ok_button);

    gtk_widget_show(window);
}

