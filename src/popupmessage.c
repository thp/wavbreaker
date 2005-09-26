/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2003 Timothy Robinson
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

void popupmessage_hide(GtkWidget *widget, gpointer user_data)
{
	GtkWidget *window = GTK_WIDGET(user_data);
	gtk_widget_destroy(window);
}

void popupmessage_show(GtkWidget *main_window, const char *message)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbbox;
	GtkWidget *status_label;
	GtkWidget *hseparator;
	GtkWidget *button;

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

	hseparator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, TRUE, 5);
	gtk_widget_show(hseparator);

	hbbox = gtk_hbutton_box_new();
	gtk_container_add(GTK_CONTAINER(vbox), hbbox);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
	gtk_widget_show(hbbox);

	button = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_box_pack_end(GTK_BOX(hbbox), button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(button), "clicked", (GtkSignalFunc)popupmessage_hide, window);
	gtk_widget_show(button);

	gtk_widget_show(window);
}

