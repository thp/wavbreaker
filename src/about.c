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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gettext.h"
#include "wavbreaker.h"

#define COPYRIGHT "Copyright (C) 2002-2007 Timothy Robinson\nCopyright (C) 2006-2007 Thomas Perl"
#define APPDESCRIPTION _("Split a wave file into multiple chunks")
#define URL "http://wavbreaker.sourceforge.net/"

#define AUTHOR_A "Timothy Robinson <tdrobinson@huli.org>"
#define AUTHOR_B "Thomas Perl <thp@perli.net>"


void about_show(GtkWidget *main_window)
{
        GdkPixbuf* icon;
        const char* authors[] = { AUTHOR_A, AUTHOR_B, 0 };

        GtkAboutDialog* about = (GtkAboutDialog*)gtk_about_dialog_new();

        icon = gdk_pixbuf_new_from_file( WAVBREAKER_ICON, NULL);

        gtk_about_dialog_set_name( about, PACKAGE);
        gtk_about_dialog_set_version( about, VERSION);
        gtk_about_dialog_set_copyright( about, COPYRIGHT);
        gtk_about_dialog_set_comments( about, APPDESCRIPTION);
        gtk_about_dialog_set_website( about, URL);
        gtk_about_dialog_set_website_label( about, URL);
        gtk_about_dialog_set_authors( about, authors);
        gtk_about_dialog_set_logo( about, icon);

        gtk_dialog_run( (GtkDialog*)about);
        gtk_widget_destroy( about);
}

