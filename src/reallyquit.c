/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2006 Timothy Robinson
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
#include "wavbreaker.h"

typedef struct ReallyQuitData_ ReallyQuitData;
struct ReallyQuitData_ {
    GtkWidget* window;
    GtkWidget* never_ask_checkbox;
};

void reallyquit_no(GtkWidget *widget, gpointer user_data)
{
    ReallyQuitData* data = (ReallyQuitData*) user_data;
    GtkWidget *window = GTK_WIDGET(data->window);
    g_free(data);
    gtk_widget_destroy(window);
}

void reallyquit_yes(GtkWidget *widget, gpointer user_data)
{
    ReallyQuitData* data = (ReallyQuitData*) user_data;
    GtkWidget *window = GTK_WIDGET(data->window);
    GtkWidget *checkbox = GTK_WIDGET(data->never_ask_checkbox);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox))) {
        appconfig_set_ask_really_quit(0);
    }
    g_free(data);
    gtk_widget_destroy(window);
    wavbreaker_quit();
}

void reallyquit_show(GtkWidget *main_window)
{
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *hbbox;
    GtkWidget *status_label;
    GtkWidget *hseparator;
    GtkWidget *checkbox;
    GtkWidget *button;
    gchar *message = "Are you sure you want to quit wavbreaker?";
    ReallyQuitData* really_quit_data = g_malloc(sizeof(ReallyQuitData));

    if (main_window == NULL) {
        main_window = wavbreaker_get_main_window();
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_realize(window);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(window),
            GTK_WINDOW(main_window));
    gtk_window_set_type_hint(GTK_WINDOW(window),
            GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_position(GTK_WINDOW(window),
            GTK_WIN_POS_CENTER_ON_PARENT);
    gdk_window_set_functions(window->window, GDK_FUNC_MOVE);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
    gtk_widget_show(vbox);

    status_label = gtk_label_new(message);
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, TRUE, 5);
    gtk_widget_show(status_label);

    checkbox = gtk_check_button_new_with_label("Never ask this question again");
    gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, TRUE, 5);
    gtk_widget_show(checkbox);

    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, TRUE, 5);
    gtk_widget_show(hseparator);

    hbbox = gtk_hbutton_box_new();
    gtk_container_add(GTK_CONTAINER(vbox), hbbox);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
    gtk_widget_show(hbbox);

    button = gtk_button_new_from_stock(GTK_STOCK_NO);
    gtk_box_pack_end(GTK_BOX(hbbox), button, FALSE, FALSE, 5);
    g_signal_connect(G_OBJECT(button), "clicked", (GtkSignalFunc)reallyquit_no, really_quit_data);
    gtk_widget_show(button);

    button = gtk_button_new_from_stock(GTK_STOCK_YES);
    gtk_box_pack_end(GTK_BOX(hbbox), button, FALSE, FALSE, 5);
    g_signal_connect(G_OBJECT(button), "clicked", (GtkSignalFunc)reallyquit_yes, really_quit_data);
    gtk_widget_show(button);

    really_quit_data->window = window;
    really_quit_data->never_ask_checkbox = checkbox;

    gtk_widget_show(window);
}

