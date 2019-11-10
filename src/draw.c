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

#include <gtk/gtk.h>
#include <math.h>

#include "draw.h"

static void draw_sample_surface(struct WaveformSurface *self, struct WaveformSurfaceDrawContext *ctx);
static void draw_summary_surface(struct WaveformSurface *self, struct WaveformSurfaceDrawContext *ctx);

/**
 * The SAMPLE_COLORS_VALUES array now uses colors from the
 * Tango Icon Theme Guidelines (except "Chocolate", as it looks too much like
 * "Orange" in the waveform sample view), see this URL for more information:
 *
 * http://tango.freedesktop.org/Tango_Icon_Theme_Guidelines
 **/
const unsigned char SAMPLE_COLORS_VALUES[][3] = {
    {  52, 101, 164 }, // Sky Blue
    { 204,   0,   0 }, // Scarlet Red
    { 115, 210,  22 }, // Chameleon
    { 237, 212,   0 }, // Butter
    { 245, 121,   0 }, // Orange
    { 117,  80, 123 }, // Plum
};

#define SAMPLE_COLORS G_N_ELEMENTS(SAMPLE_COLORS_VALUES)

#define SAMPLE_SHADES 3

GdkRGBA sample_colors[SAMPLE_COLORS][SAMPLE_SHADES];
GdkRGBA bg_color;
GdkRGBA nowrite_color;

static inline void
set_cairo_source(cairo_t *cr, GdkRGBA color)
{
    cairo_set_source_rgb(cr, color.red, color.green, color.blue);
}

static inline void
fill_cairo_rectangle(cairo_t *cr, GdkRGBA *color, int width, int height)
{
    set_cairo_source(cr, *color);
    cairo_rectangle(cr, 0.f, 0.f, (float)width, (float)height);
    cairo_fill(cr);
}

static inline void
draw_cairo_line(cairo_t *cr, float x, float y0, float y1)
{
    cairo_move_to(cr, x-0.5f, y0);
    cairo_line_to(cr, x-0.5f, y1);
}

static void
waveform_surface_static_init()
{
    static gboolean inited = FALSE;
    if (inited) {
        return;
    }

    bg_color = (GdkRGBA){ .red = 1.f, .green = 1.f, .blue = 1.f };
    nowrite_color = (GdkRGBA){ .red = 0.86f, .green = 0.86f, .blue = 0.86f };

    for (int i=0; i<SAMPLE_COLORS; i++) {
        for (int x=0; x<SAMPLE_SHADES; x++) {
            float factor_white = 0.5f*((float)x/(float)SAMPLE_SHADES);
            float factor_color = 1.f-factor_white;
            sample_colors[i][x].red = SAMPLE_COLORS_VALUES[i][0]/255.f*factor_color+factor_white;
            sample_colors[i][x].green = SAMPLE_COLORS_VALUES[i][1]/255.f*factor_color+factor_white;
            sample_colors[i][x].blue = SAMPLE_COLORS_VALUES[i][2]/255.f*factor_color+factor_white;
        }
    }

    inited = TRUE;
}

struct WaveformSurface *waveform_surface_create_sample()
{
    waveform_surface_static_init();

    struct WaveformSurface *surface = calloc(sizeof(struct WaveformSurface), 1);

    surface->draw = draw_sample_surface;

    return surface;
}

struct WaveformSurface *waveform_surface_create_summary()
{
    waveform_surface_static_init();

    struct WaveformSurface *surface = calloc(sizeof(struct WaveformSurface), 1);

    surface->draw = draw_summary_surface;

    return surface;
}

void waveform_surface_draw(struct WaveformSurface *surface, struct WaveformSurfaceDrawContext *ctx)
{
    surface->draw(surface, ctx);
}

void waveform_surface_invalidate(struct WaveformSurface *surface)
{
    if (surface->surface) {
        cairo_surface_destroy(surface->surface);
        surface->surface = NULL;
    }

    surface->width = 0;
    surface->height = 0;
}

void waveform_surface_free(struct WaveformSurface *surface)
{
    if (surface->surface) {
        cairo_surface_destroy(surface->surface);
    }

    free(surface);
}

static GdkRGBA moodbar_sample_color(MoodbarData *moodbar, float position)
{
    float index = position * moodbar->numFrames;
    unsigned long iindex = index;
    float fractional = index - iindex;
    if (fractional == 0.f || iindex >= moodbar->numFrames - 1) {
        return moodbar->frames[iindex];
    } else {
        GdkRGBA a = moodbar->frames[iindex];
        GdkRGBA b = moodbar->frames[iindex+1];
        return (GdkRGBA){
            .red =   (1.f - fractional) * a.red   + fractional * b.red,
            .green = (1.f - fractional) * a.green + fractional * b.green,
            .blue =  (1.f - fractional) * a.blue  + fractional * b.blue,
            .alpha = (1.f - fractional) * a.alpha + fractional * b.alpha,
        };
    }
}

static void
draw_sample_surface(struct WaveformSurface *self, struct WaveformSurfaceDrawContext *ctx)
{
    int xaxis;
    int width, height;
    int y_min, y_max;
    int scale;
    long i;

    int shade;

    GdkRGBA new_color;

    {
        GtkAllocation allocation;
        gtk_widget_get_allocation(ctx->widget, &allocation);

        width = allocation.width;
        height = allocation.height;
    }

    if (self->surface != NULL && self->width == width && self->height == height && self->offset == ctx->pixmap_offset &&
        (ctx->moodbarData && ctx->moodbarData->numFrames) == self->moodbar) {
        return;
    }

    if (self->surface) {
        cairo_surface_destroy(self->surface);
    }

    self->surface = gdk_window_create_similar_surface(gtk_widget_get_window(ctx->widget),
            CAIRO_CONTENT_COLOR, width, height);

    if (!self->surface) {
        printf("surface is NULL\n");
        return;
    }

    cairo_t *cr = cairo_create(self->surface);
    cairo_set_line_width(cr, 1.f);

    /* clear sample_surface before drawing */
    fill_cairo_rectangle(cr, &bg_color, width, height);

    if (ctx->graphData->data == NULL) {
        cairo_destroy(cr);
        return;
    }

    xaxis = height / 2;
    if (xaxis != 0) {
        scale = ctx->graphData->maxSampleValue / xaxis;
        if (scale == 0) {
            scale = 1;
        }
    } else {
        scale = 1;
    }

    /* draw sample graph */
    int tb_index = 0;
    GList *tbl = ctx->track_break_list;
    for (i = 0; i < width && i < ctx->graphData->numSamples; i++) {
        y_min = ctx->graphData->data[i + ctx->pixmap_offset].min;
        y_max = ctx->graphData->data[i + ctx->pixmap_offset].max;

        y_min = xaxis + fabs((double)y_min) / scale;
        y_max = xaxis - y_max / scale;

        /* find the track break we are drawing now */
        while (tbl->next && (i + ctx->pixmap_offset) > ((TrackBreak *)(tbl->next->data))->offset) {
            tbl = tbl->next;
            ++tb_index;
        }

        if (ctx->moodbarData && ctx->moodbarData->numFrames) {
            set_cairo_source(cr, moodbar_sample_color(ctx->moodbarData, (float)(i+ctx->pixmap_offset) / (float)ctx->graphData->numSamples));
            draw_cairo_line(cr, i, 0.f, height);
            cairo_stroke(cr);
        }

        for( shade=0; shade<SAMPLE_SHADES; shade++) {
            TrackBreak *tb = tbl->data;
            if (tb->write) {
                new_color = sample_colors[tb_index % SAMPLE_COLORS][shade];
            } else {
                new_color = nowrite_color;
            }

            set_cairo_source(cr, new_color);
            draw_cairo_line(cr, i, y_min+(xaxis-y_min)*shade/SAMPLE_SHADES, y_min+(xaxis-y_min)*(shade+1)/SAMPLE_SHADES);
            draw_cairo_line(cr, i, y_max-(y_max-xaxis)*shade/SAMPLE_SHADES, y_max-(y_max-xaxis)*(shade+1)/SAMPLE_SHADES);
            cairo_stroke(cr);
        }
    }

    cairo_destroy(cr);

    self->width = width;
    self->height = height;
    self->offset = ctx->pixmap_offset;
    self->moodbar = ctx->moodbarData && ctx->moodbarData->numFrames;
}

static void
draw_summary_surface(struct WaveformSurface *self, struct WaveformSurfaceDrawContext *ctx)
{
    int xaxis;
    int width, height;
    int y_min, y_max;
    int min, max;
    int scale;
    int i, k;
    int loop_end, array_offset;
    int shade;

    float x_scale;

    GdkRGBA new_color;

    {
        GtkAllocation allocation;
        gtk_widget_get_allocation(ctx->widget, &allocation);
        width = allocation.width;
        height = allocation.height;
    }

    if (self->surface != NULL && self->width == width && self->height == height &&
        (ctx->moodbarData && ctx->moodbarData->numFrames) == self->moodbar) {
        return;
    }

    if (self->surface) {
        cairo_surface_destroy(self->surface);
    }

    self->surface = gdk_window_create_similar_surface(gtk_widget_get_window(ctx->widget),
            CAIRO_CONTENT_COLOR, width, height);

    if (!self->surface) {
        printf("summary_surface is NULL\n");
        return;
    }

    cairo_t *cr = cairo_create(self->surface);
    cairo_set_line_width(cr, 1.f);

    /* clear sample_surface before drawing */
    fill_cairo_rectangle(cr, &bg_color, width, height);

    if (ctx->graphData->data == NULL) {
        cairo_destroy(cr);
        return;
    }

    xaxis = height / 2;
    if (xaxis != 0) {
        scale = ctx->graphData->maxSampleValue / xaxis;
        if (scale == 0) {
            scale = 1;
        }
    } else {
        scale = 1;
    }

    /* draw sample graph */

    x_scale = (float)(ctx->graphData->numSamples) / (float)(width);
    if (x_scale == 0) {
        x_scale = 1;
    }

    int tb_index = 0;
    GList *tbl = ctx->track_break_list;
    for (i = 0; i < width && i < ctx->graphData->numSamples; i++) {
        min = max = 0;
        array_offset = (int)(i * x_scale);

        if (x_scale != 1) {
            loop_end = (int)x_scale;

            for (k = 0; k < loop_end; k++) {
                if (ctx->graphData->data[array_offset + k].max > max) {
                    max = ctx->graphData->data[array_offset + k].max;
                } else if (ctx->graphData->data[array_offset + k].min < min) {
                    min = ctx->graphData->data[array_offset + k].min;
                }
            }
        } else {
            min = ctx->graphData->data[i].min;
            max = ctx->graphData->data[i].max;
        }

        y_min = min;
        y_max = max;

        y_min = xaxis + fabs((double)y_min) / scale;
        y_max = xaxis - y_max / scale;

        /* find the track break we are drawing now */
        while (tbl->next && array_offset > ((TrackBreak *)(tbl->next->data))->offset) {
            tbl = tbl->next;
            ++tb_index;
        }

        float alpha = 1.f;
        if (ctx->moodbarData && ctx->moodbarData->numFrames) {
            set_cairo_source(cr, moodbar_sample_color(ctx->moodbarData, (float)(array_offset) / (float)(ctx->graphData->numSamples)));
            draw_cairo_line(cr, i, 0.f, height);
            cairo_stroke(cr);
            alpha = 0.77f;
        }

        for( shade=0; shade<SAMPLE_SHADES; shade++) {
            TrackBreak *tb = tbl->data;
            if (tb->write) {
                new_color = sample_colors[tb_index % SAMPLE_COLORS][shade];
            } else {
                new_color = nowrite_color;
            }

            cairo_set_source_rgba(cr, new_color.red, new_color.green, new_color.blue, alpha);
            draw_cairo_line(cr, i, y_min+(xaxis-y_min)*shade/SAMPLE_SHADES, y_min+(xaxis-y_min)*(shade+1)/SAMPLE_SHADES);
            draw_cairo_line(cr, i, y_max-(y_max-xaxis)*shade/SAMPLE_SHADES, y_max-(y_max-xaxis)*(shade+1)/SAMPLE_SHADES);
            cairo_stroke(cr);
        }
    }

    cairo_destroy(cr);

    self->width = width;
    self->height = height;
    self->moodbar = ctx->moodbarData && ctx->moodbarData->numFrames;
}

