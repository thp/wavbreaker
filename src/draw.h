#pragma once

/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2002-2005 Timothy Robinson
 * Copyright (C) 2007-2019 Thomas Perl
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

#include "sample.h"
#include "track_break.h"
#include "moodbar.h"

struct WaveformSurfaceDrawContext {
    // widget to draw into
    GtkWidget *widget;
    // sample offset of sample view
    long pixmap_offset;
    // list of track breaks
    GList *track_break_list;
    // sample information
    GraphData *graphData;
    // moodbar information
    MoodbarData *moodbarData;
};

struct WaveformSurface {
    cairo_surface_t *surface;
    unsigned long width;
    unsigned long height;
    unsigned long offset;
    gboolean moodbar;

    void (*draw)(struct WaveformSurface *, struct WaveformSurfaceDrawContext *);
};

struct WaveformSurface *waveform_surface_create_sample();
struct WaveformSurface *waveform_surface_create_summary();

void waveform_surface_draw(struct WaveformSurface *surface, struct WaveformSurfaceDrawContext *ctx);
void waveform_surface_invalidate(struct WaveformSurface *surface);

void waveform_surface_free(struct WaveformSurface *surface);
