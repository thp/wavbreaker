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

#ifdef _WIN32
#  define IMAGEDIR "../images/"
#else
#  ifdef HAVE_CONFIG_H
#    include <config.h>
#  endif
#endif

#include <gtk/gtk.h>

#define COPYRIGHT "Copyright (C) 2002-2003 Timothy Robinson"
#define APPNAME "<big><big><big>"PACKAGE" "VERSION"</big></big></big>"
#define URL "http://huli.org/wavbreaker/"
#define AUTHOR "Timothy Robinson <tdrobinson@huli.org>"
#define APPDESCRIPTION "A tool to split a wave file up into multiple waves."

void about_hide(GtkWidget *widget, gpointer user_data)
{
	GtkWidget *main_window = GTK_WIDGET(user_data);
	gtk_widget_destroy(main_window);
}

void about_show(GtkWidget *main_window)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbbox;
	GtkWidget *appname_label;
	GtkWidget *appdescription_label;
	GtkWidget *url_label;
	GtkWidget *author_label;
	GtkWidget *copyright_label;
	GtkWidget *hseparator;
	GtkWidget *button;

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

	appname_label = gtk_label_new("");
	gtk_label_set_markup(GTK_LABEL(appname_label), APPNAME);
	gtk_box_pack_start(GTK_BOX(vbox), appname_label, FALSE, TRUE, 5);
	gtk_widget_show(appname_label);

	appdescription_label = gtk_label_new(APPDESCRIPTION);
	gtk_box_pack_start(GTK_BOX(vbox), appdescription_label, FALSE, TRUE, 5);
	gtk_widget_show(appdescription_label);

	url_label = gtk_label_new(URL);
	gtk_label_set_selectable(GTK_LABEL(url_label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), url_label, FALSE, TRUE, 5);
	gtk_widget_show(url_label);

	author_label = gtk_label_new(AUTHOR);
	gtk_label_set_selectable(GTK_LABEL(author_label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), author_label, FALSE, TRUE, 5);
	gtk_widget_show(author_label);

	copyright_label = gtk_label_new(COPYRIGHT);
	gtk_box_pack_start(GTK_BOX(vbox), copyright_label, FALSE, TRUE, 5);
	gtk_widget_show(copyright_label);

	hseparator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, TRUE, 5);
	gtk_widget_show(hseparator);

	hbbox = gtk_hbutton_box_new();
	gtk_container_add(GTK_CONTAINER(vbox), hbbox);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
	gtk_widget_show(hbbox);

	button = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_box_pack_end(GTK_BOX(hbbox), button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(button), "clicked", (GtkSignalFunc)about_hide, window);
	gtk_widget_show(button);

	gtk_widget_show(window);
}

