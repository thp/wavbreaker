/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2002 Timothy Robinson
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

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "linuxaudio.h"

void close_device(int audio_fd)
{
	close(audio_fd);
}

int open_device(const char *audio_dev, const SampleInfo *sampleInfo)
{
	int format, speed, channels;
	int ret;
	int audio_fd;

	if (sampleInfo->bitsPerSample == 16) {
		format = AFMT_S16_LE;
	} else if (sampleInfo->bitsPerSample == 8) {
		format = AFMT_U8;
	}
	speed = sampleInfo->samplesPerSec;
	channels = sampleInfo->channels;

	/* setup dsp device */
	if ((audio_fd = open(audio_dev, O_WRONLY)) == -1) {
	        perror("open");  
	        printf("error opening %s\n", audio_dev);
	        return audio_fd;
	}
 
	/* set format */
	ret = format;
	if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &ret) == -1) {
	        perror("SNDCTL_DSP_SETFMT");
	        return audio_fd; 
	}

	if (format != ret) {  
	        printf("Device, %s, does not support %d for format.\n",
			audio_dev, format);
	        return audio_fd;
	}

	/* set channels */
	ret = channels;
	if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &ret) == -1) {
	        perror("SNDCTL_DSP_CHANNELS");
	        return audio_fd;
	}       

	if (channels != ret) {
	        printf("Device, %s, doesn't support %d channels\n", audio_dev,
			channels);
	        return audio_fd;
	}

	/* set speed */
	ret = speed;
	if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &ret) == -1) {
	        perror("SNDCTL_DSP_SPEED");
	        return audio_fd;
	}

	if (speed != ret) {
	        printf("Device, %s, doesn't support speed of %d\n",
	                audio_dev, speed);
	        printf("Speed returned was %d\n", ret);
		return audio_fd;
	}

	return audio_fd;
}
