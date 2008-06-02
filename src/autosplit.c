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

#include <gtk/gtk.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "sample.h"
#include "wavbreaker.h"

#include "gettext.h"

static GtkWidget *window;

static char *autosplit_time = NULL; 
static GtkWidget *autosplit_time_entry = NULL;
extern SampleInfo sampleInfo;

char *get_autosplit_time()
{
    return autosplit_time;
}

void set_autosplit_time(const char *val)
{
    if (autosplit_time != NULL) {
        g_free(autosplit_time);
    }
    autosplit_time = g_strdup(val);
}

static void autosplit_hide(GtkWidget *main_window)
{
    gtk_widget_destroy(main_window);
}

static void cancel_button_clicked(GtkWidget *widget, gpointer user_data)
{
    autosplit_hide(user_data);
}

static long parse_time_string()
{
    char *tmp_str;
    char *str_ptr;
    char min[20];
    char sec[20];
    char subsec[20];
    long time, len;

    str_ptr = strchr(autosplit_time, ':');
    if (str_ptr == NULL) {
        time = atoi(autosplit_time) * 60 * CD_BLOCKS_PER_SEC;
    } else {
        len = str_ptr - autosplit_time;
        strncpy(min, autosplit_time, len);
        time = atoi(min) * 60 * CD_BLOCKS_PER_SEC;

        tmp_str = str_ptr;
        str_ptr = strchr(autosplit_time, '.');
        if (str_ptr == NULL) {
            strcpy(sec, tmp_str + 1);
            time += atoi(sec) * CD_BLOCKS_PER_SEC;
        } else {
            len = str_ptr - tmp_str;
            strncpy(sec, tmp_str + 1, len);
            time += atoi(sec) * CD_BLOCKS_PER_SEC;

            strcpy(subsec, str_ptr + 1);
            time += atoi(subsec);
        }
    }

    return time;
}

static void ok_button_clicked(GtkWidget *widget, gpointer user_data)
{
    long time;

    set_autosplit_time(gtk_entry_get_text(GTK_ENTRY(autosplit_time_entry)));
    autosplit_hide(GTK_WIDGET(user_data));
    time = parse_time_string();
    if (time > 0) {
        wavbreaker_autosplit(time);
    }
}

void autosplit_show(GtkWidget *main_window)
{
    GtkWidget *vbox;
    GtkWidget *table;
    GtkWidget *hbbox;
    GtkWidget *message_label;
    GtkWidget *hseparator;
    GtkWidget *ok_button, *cancel_button;

    if (autosplit_time == NULL) {
        autosplit_time = g_strdup("5:00.00");
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_realize(window);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(main_window));
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
    gdk_window_set_functions(window->window, GDK_FUNC_MOVE);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
    gtk_widget_show(vbox);

    table = gtk_table_new(3, 2, FALSE);
    gtk_container_add(GTK_CONTAINER(vbox), table);
    gtk_widget_show(table);

    message_label = gtk_label_new(_("Enter the time for autosplit:"));
    gtk_misc_set_alignment(GTK_MISC(message_label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), message_label, 0, 1, 0, 1, GTK_FILL, 0, 5, 0);
    gtk_widget_show(message_label);

    message_label = gtk_label_new(_("Example (5min, 32sec, 12subsec):"));
    gtk_misc_set_alignment(GTK_MISC(message_label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), message_label, 0, 1, 1, 2, GTK_FILL, 0, 5, 0);
    gtk_widget_show(message_label);

    message_label = gtk_label_new(" 5:32.12");
    gtk_misc_set_alignment(GTK_MISC(message_label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), message_label, 1, 2, 1, 2, GTK_FILL, 0, 5, 0);
    gtk_widget_show(message_label);

    autosplit_time_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(autosplit_time_entry), autosplit_time);
    gtk_entry_set_width_chars(GTK_ENTRY(autosplit_time_entry), 10);
    gtk_table_attach(GTK_TABLE(table), autosplit_time_entry, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 5, 0);
    gtk_widget_show(autosplit_time_entry);

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

