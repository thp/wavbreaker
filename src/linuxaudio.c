#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "linuxaudio.h"

#define LEN 50000

static char *play_audio_dev = "/dev/dsp";
static char *play_filename = "cddata.dat";

static pthread_t thread;
static pthread_attr_t thread_attr;

static int sample_start;

static int open_device(int *audio_fd)
{
	int format, channels, speed;

	format = AFMT_S16_LE;
	channels = 2;
	speed = 44100;

	/* setup dsp device */
	if ((*audio_fd = open(play_audio_dev, O_WRONLY)) == -1) {
	        perror("open");  
	        printf("error opening %s\n", play_audio_dev);
	        return 1;
	}
 
	/* set format */
	if (ioctl(*audio_fd, SNDCTL_DSP_SETFMT, &format) == -1) {
	        perror("SNDCTL_DSP_SETFMT");
	        return 1; 
	}

	if (format != AFMT_S16_LE) {  
	        printf("Device, %s, does not support "
	                "little endian signed 16 bit samples.", play_audio_dev);
	        return 1;
	}

	/* set channels */
	if (ioctl(*audio_fd, SNDCTL_DSP_CHANNELS, &channels) == -1) {
	        perror("SNDCTL_DSP_CHANNELS");
	        return 1;
	}       

	if (channels != 2) {
	        printf("Device, %s, doesn't support stereo\n", play_audio_dev);
	        return 1;
	}

	/* set speed */
	if (ioctl(*audio_fd, SNDCTL_DSP_SPEED, &speed) == -1) {
	        perror("SNDCTL_DSP_SPEED");
	        return 1;
	}

	if (speed != 44100) {
	        printf("Device, %s, doesn't support speed of 44100\n",
	                play_audio_dev);
	        printf("Speed returned was %d\n", speed);
		return 1;
	}

	return 0;
}

static void * play_thread(void *thread_data)
{
	while(1) {
		printf("thread\n");
	}

	return NULL;
}

int play_sample(int start)
{       
	int ret;

	int i, p;
	int nwritten = 0, len = 500000;
	int left[LEN], right[LEN];
	unsigned char devbuf[LEN * 2];
	unsigned char buf[256];
	int audio_fd;
	FILE *infile;

	printf("play!!\n");

	sample_start = start;

	/* setup thread */
	if ((ret = pthread_attr_init(&thread_attr)) != 0) {
		perror("Return from pthread_attr_init");
		printf("Error #%d\n", ret);
		return 1;
	}

	if ((ret = pthread_attr_setdetachstate(&thread_attr,
		PTHREAD_CREATE_DETACHED)) != 0) {

		perror("Return from pthread_attr_setdetachstate");
		printf("Error #%d\n", ret);
		return 1;
	}

	if ((ret = pthread_create(&thread, &thread_attr, play_thread,
			NULL)) != 0) {

		perror("Return from pthread_create");
		printf("Error #%d\n", ret);
		return 1;
	}


printf("start of thread\n");

	if (open_device(&audio_fd)) {
		return 1;
	}

	if ((infile = fopen(play_filename, "r")) == NULL) {
	        printf("error opening %s\n", play_filename);
	        return 1;
	}

	/* read in and play sample */
	fseek(infile, sample_start, SEEK_SET);

	i = 0;

	while (fread(buf, 1, 4, infile) && i++ < LEN) {
	        unsigned char tmp;
	        
	        tmp = buf[0];
	        buf[0] = buf[1];
	        buf[1] = tmp;

	        tmp = buf[2];
	        buf[2] = buf[3];
	        buf[3] = tmp;
	
	        left[i] = buf[1] << 8 | buf[0];
	        right[i] = buf[3] << 8 | buf[2];
	}

printf("middle of thread\n");
	p = 0;
	for (i = 0; i < LEN; i++) {
	        devbuf[p++] = (unsigned char) (left[i] & 0xff);
	        devbuf[p++] = (unsigned char) ((left[i] >> 8) & 0xff);
 
	        devbuf[p++] = (unsigned char) (right[i] & 0xff);
	        devbuf[p++] = (unsigned char) ((right[i] >> 8) & 0xff);
	}       

	nwritten = 0; 
	len = LEN;
	while (len > 0) {
	        int ret; 

	        ret = write(audio_fd, devbuf + nwritten, len);

	printf("ret was: %d\n", ret);
	
	        nwritten += ret; 
	        len -= ret;
	}

	fclose(infile);
	close(audio_fd);
printf("end of thread\n");

	printf("done playing\n");

	return 0;
}               

void stop_sample()
{       
	if (pthread_cancel(thread)) {
	        printf("trouble cancelling the thread\n");
	}
}
