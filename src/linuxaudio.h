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

#ifndef LINUXAUDIO_H
#define LINUXAUDIO_H

#include "sample.h"

/**
 * Some versions of soundcard.h are missing this #define, see:
 *   http://mail.directfb.org/pipermail/directfb-dev/2005-July/000541.html 
 *   http://manuals.opensound.com/developer/AFMT_S24_LE.html
 *   http://manuals.opensound.com/developer/soundcard.h.html
 **/
#ifndef AFMT_S24_LE
#define AFMT_S24_LE              0x00000800
#endif

void oss_audio_close_device();
int oss_audio_open_device(const char *, SampleInfo *);
int oss_audio_write(unsigned char *, int);

#endif /* LINUXAUDIO_H */

