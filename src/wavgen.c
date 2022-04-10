/* wavgen - Generate a test wave file for testing wavbreaker
 * Copyright (C) 2015 Thomas Perl <m@thp.io>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <glib.h>


#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif


#include "format_wav.h"
#include "sample_info.h"


typedef void (*ampfreq_func_t)(float, float *, float *);

static inline void
ampfreq1(float pos, float *amp, float *freq)
{
    pos *= 2 * M_PI;
    *amp = fabsf(cosf(pos * 13.f));
    *freq = 440.f + 440.f * sinf(pos * 77.f);
}

typedef void (*generate_audio_func_t)(FILE *, SampleInfo *, void *);

static void
generate_ampfreq(FILE *fp, SampleInfo *si, void *user_data)
{
    ampfreq_func_t f = (ampfreq_func_t)user_data;

    int samples = si->numBytes / si->channels;
    int i;
    for (i=0; i<samples; i++) {
        float pos = (float)i / samples;
        float amp, freq;

        f(pos, &amp, &freq);

        amp *= sinf(freq * M_PI * (float)i / (si->samplesPerSec));

        int ch;
        for (ch=0; ch<si->channels; ch++) {
            if (si->bitsPerSample == 8) {
                char buf = 127 + 127 * amp;
                fwrite(&buf, sizeof(buf), 1, fp);
            } else if (si->bitsPerSample == 16) {
                short buf = amp * 0x7fff;
                fwrite(&buf, sizeof(buf), 1, fp);
            }
        }
    }
}

typedef char (*oneliner_func_t)(int t);

// http://countercomplex.blogspot.com/2011/10/algorithmic-symphonies-from-one-line-of.html
static char oneliner(int t) { return t*(((t>>12)|(t>>8))&(63&(t>>4))); }

static void
generate_oneliner(FILE *fp, SampleInfo *si, void *user_data)
{
    g_assert_cmpint(si->channels, ==, 1);
    g_assert_cmpint(si->bitsPerSample, ==, 8);
    g_assert_cmpint(si->samplesPerSec, ==, 8000);

    oneliner_func_t f = (oneliner_func_t)user_data;

    int t;
    for (t=0; t<si->numBytes; t++) {
        fputc(f(t), fp);
    }
}

int
cmd_wavgen(int argc, char *argv[])
{
    struct {
        char *name;
        int channels;
        int samplesPerSec;
        int bitsPerSample;
        int seconds;
        generate_audio_func_t generate_func;
        void *generate_func_user_data;
    } templates[] = {
        { "gen", 2, 44100, 16, 60, generate_ampfreq, &ampfreq1 },
        { "gen", 2, 44100, 8, 60, generate_ampfreq, &ampfreq1 },
        { "gen", 1, 44100, 16, 60, generate_ampfreq, &ampfreq1 },
        { "gen", 1, 44100, 8, 60, generate_ampfreq, &ampfreq1 },
        { "gen", 1, 22050, 8, 60, generate_ampfreq, &ampfreq1 },
        { "gen", 1, 11025, 8, 60, generate_ampfreq, &ampfreq1 },
        { "oneliner", 1, 8000, 8, 60, generate_oneliner, &oneliner },
        { "gen_long", 2, 44100, 16, 60 * 3, generate_ampfreq, &ampfreq1 },
    };

    int i;

    for (i=0; i<sizeof(templates)/sizeof(templates[0]); i++) {
        SampleInfo si;
        memset(&si, 0, sizeof(si));

        si.channels = templates[i].channels;
        si.samplesPerSec = templates[i].samplesPerSec;
        si.bitsPerSample = templates[i].bitsPerSample;

        si.blockAlign = si.channels * (si.bitsPerSample / 8);
        si.avgBytesPerSec = si.blockAlign * si.samplesPerSec;

        si.numBytes = si.avgBytesPerSec * templates[i].seconds;

        si.blockSize = si.avgBytesPerSec / CD_BLOCKS_PER_SEC; // unused here, just for playback

        char *filename = g_strdup_printf("wav_%s_%dch_%dhz_%dbit.wav", templates[i].name,
                si.channels, si.samplesPerSec, si.bitsPerSample);

        printf("%s ... ", filename);
        fflush(stdout);

        if (g_file_test(filename, G_FILE_TEST_EXISTS)) {
            printf("EXISTS\n");
        } else {
            FILE *fp = fopen(filename, "wb");
            if (fp) {
                wav_write_file_header(fp, &si, si.numBytes);
                templates[i].generate_func(fp, &si, templates[i].generate_func_user_data);
                fclose(fp);
                printf("OK\n");
            } else {
                printf("FAIL\n");
            }
        }

        g_free(filename);
    }

    return 0;
}
