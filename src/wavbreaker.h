#ifndef WAVBREAKER_H
#define WAVBREAKER_H

#include <gtk/gtk.h>

typedef struct Points_ Points;
struct Points_ {
        int min, max;
};

typedef struct GraphData_ GraphData;
struct GraphData_{
	unsigned long numSamples;
	unsigned long maxSampleValue;
	Points *data;
};

typedef struct TrackBreak_ TrackBreak;
struct TrackBreak_ {
	gboolean  write;
	GdkColor  color;
	guint     offset;
	gchar     *filename;
	gboolean  editable;
};

#endif /* WAVBREAKER_H */
