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

#include "autosplit.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "sample.h"
#include "wavbreaker.h"

#include "gettext.h"

static GtkWidget *
autosplit_time_entry = NULL;

static long
parse_time_string(const char *autosplit_time)
{
    long min, sec, subsec;
    if (sscanf(autosplit_time, "%ld:%ld.%ld", &min, &sec, &subsec) == 3) {
        return (min * 60 + sec) * CD_BLOCKS_PER_SEC + subsec;
    } else if (sscanf(autosplit_time, "%ld:%ld", &min, &sec) == 2) {
        return (min * 60 + sec) * CD_BLOCKS_PER_SEC;
    } else if (sscanf(autosplit_time, "%ld.%ld", &sec, &subsec) == 2) {
        return sec * CD_BLOCKS_PER_SEC + subsec;
    }

    return atoi(autosplit_time) * 60 * CD_BLOCKS_PER_SEC;
}

static void
on_split_button_clicked(GtkWidget *widget, gpointer user_data)
{
    gtk_popover_popdown(GTK_POPOVER(user_data));

    long time = parse_time_string(gtk_entry_get_text(GTK_ENTRY(autosplit_time_entry)));
    if (time > 0) {
        wavbreaker_autosplit(time);

        long subsec = time % CD_BLOCKS_PER_SEC;
        time -= subsec;
        time /= CD_BLOCKS_PER_SEC;

        long sec = time % 60;
        time -= sec;
        time /= 60;

        gchar *tmp = g_strdup_printf("%02ld:%02ld.%02ld", time, sec, subsec);
        gtk_entry_set_text(GTK_ENTRY(autosplit_time_entry), tmp);
        g_free(tmp);
    }
}

GtkWidget *
autosplit_create(GtkPopover *popover)
{
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    GtkWidget *message_label = gtk_label_new(_("Interval (MM:SS.FF, MM:SS, SS.FF or MM):"));
    gtk_box_pack_start(GTK_BOX(hbox), message_label, FALSE, FALSE, 0);

    autosplit_time_entry = gtk_entry_new();
    g_signal_connect(autosplit_time_entry, "activate", G_CALLBACK(on_split_button_clicked), popover);
    gtk_entry_set_text(GTK_ENTRY(autosplit_time_entry), "5:00.00");
    gtk_entry_set_width_chars(GTK_ENTRY(autosplit_time_entry), 10);
    gtk_box_pack_start(GTK_BOX(hbox), autosplit_time_entry, FALSE, FALSE, 0);

    GtkWidget *ok_button = gtk_button_new_with_label(_("Split"));
    g_signal_connect(G_OBJECT(ok_button), "clicked", G_CALLBACK(on_split_button_clicked), popover);
    gtk_box_pack_start(GTK_BOX(hbox), ok_button, FALSE, FALSE, 0);

    gtk_widget_show_all(hbox);

    return hbox;
}
