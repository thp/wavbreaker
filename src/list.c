/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2022 Thomas Perl
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

#include "list.h"

#include "toc.h"
#include "cue.h"
#include "txt.h"

gboolean
list_read_file(const char *list_filename, TrackBreakList *list)
{
    if (g_str_has_suffix(list_filename, ".toc") || g_str_has_suffix(list_filename, ".TOC")) {
        if (toc_read_file(list_filename, list)) {
            return TRUE;
        }
    } else if (g_str_has_suffix(list_filename, ".cue") || g_str_has_suffix(list_filename, ".CUE")) {
        if (cue_read_file(list_filename, list)) {
            return TRUE;
        }
    }

    return txt_read_file(list_filename, list);
}

gboolean
list_write_file(const char *list_filename, const char *wav_filename, TrackBreakList *list)
{
    if (g_str_has_suffix(list_filename, ".toc")) {
        return toc_write_file(list_filename, wav_filename, list);
    } else if (g_str_has_suffix(list_filename, ".cue")) {
        return cue_write_file(list_filename, wav_filename, list);
    }

    return txt_write_file(list_filename, wav_filename, list);
}
