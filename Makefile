CC=gcc
LDLIBS=-lpthread `pkg-config --libs gtk+-2.0 gthread-2.0`
CFLAGS=-Wall -g `pkg-config --cflags gtk+-2.0 gthread-2.0`

all: wavbreaker

wavbreaker: wavbreaker.o linuxaudio.o sample.o wav.o cdda.o
linuxaudio: linuxaudio.o
sample: sample.o
wav: wav.o
cdda: cdda.o

clean:
	rm -f core* *.o wavbreaker
