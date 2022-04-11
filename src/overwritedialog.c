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

#include "overwritedialog.h"

#include "gettext.h"

enum OverwriteDecision
overwritedialog_show(GtkWidget *main_window, const char *filename)
{
    GtkDialog *dialog = GTK_DIALOG(gtk_message_dialog_new(GTK_WINDOW(main_window),
            GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
            GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
            _("%s already exists. Overwrite?"), filename));

    gtk_dialog_add_buttons(dialog,
            _("Skip all"), OVERWRITE_DECISION_SKIP_ALL,
            _("Skip"), OVERWRITE_DECISION_SKIP,
            _("Overwrite"), OVERWRITE_DECISION_OVERWRITE,
            _("Overwrite all"), OVERWRITE_DECISION_OVERWRITE_ALL,
            NULL);

    gtk_dialog_set_default_response(dialog, OVERWRITE_DECISION_OVERWRITE);

    enum OverwriteDecision decision = OVERWRITE_DECISION_SKIP_ALL;

    int result = gtk_dialog_run(dialog);

    switch (result) {
        case OVERWRITE_DECISION_SKIP_ALL:
        case OVERWRITE_DECISION_SKIP:
        case OVERWRITE_DECISION_OVERWRITE:
        case OVERWRITE_DECISION_OVERWRITE_ALL:
            decision = result;
            break;
        default:
        case GTK_RESPONSE_NONE:
            break;
    }

    gtk_widget_destroy(GTK_WIDGET(dialog));

    return decision;
}
