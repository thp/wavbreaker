GTKDIR = c:/apps/gtk-dev
CC = gcc
LDLIBS = -lgtk-win32-2.0.dll -lgdk-win32-2.0.dll -latk-1.0.dll -lgdk_pixbuf-2.0.dll -lm -lpangoft2-1.0.dll -lpango-1.0.dll -lpangowin32-1.0.dll -lgobject-2.0.dll -lgmodule-2.0.dll -lgthread-2.0.dll -lglib-2.0.dll -lwinmm
INCLUDES = -I$(GTKDIR)/include/gtk-2.0 -I$(GTKDIR)/include/gtk-2.0 -I$(GTKDIR)/include/atk-1.0 -I$(GTKDIR)/include/pango-1.0 -I$(GTKDIR)/freetype2 -I$(GTKDIR)/include -I$(GTKDIR)/include/glib-2.0 -I$(GTKDIR)/lib/gtk-2.0/include -I$(GTKDIR)/lib/glib-2.0/include
CFLAGS = -g -O2 -s -mwindows -mno-cygwin -fnative-struct $(INCLUDES)

LDFLAGS = -L$(GTKDIR)/lib

wavbreaker_SOURCES = cdda.c winaudio.c sample.c wav.c wavbreaker.c about.c appconfig.c
wavmerge_SOURCES = wav.c wavmerge.c


all: wavbreaker

cdda: cdda.o
winaudio: winaudio.o
sample: sample.o
about: about.o
appconfig: appconfig.o
wav: wav.o
wavbreaker: wavbreaker.o cdda.o winaudio.o sample.o wav.o about.o appconfig.o

clean:
	del *.o wavbreaker.exe
