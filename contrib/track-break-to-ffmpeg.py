#!/usr/bin/env python3
# track-break-to-ffmpeg.py
# 2022-03-16 Thomas Perl <m@thp.io>

"""
Convert track breaks to ffmpeg cut commands

This script takes a wavbreaker TrackBreak .txt file and the
name of a media file, and prints out commands for the ffmpeg(1)
command line utility to split the tracks.

Useful in situations where one wants to transmit an uncut file
with a simple shell script to "split" it, and/or in situations
where wavbreaker isn't available, or cannot write the file (e.g.
MP2-format files can be decoded using mpg123, but are not yet
understood by wavbreaker).

TrackBreak .txt file format documentation:
https://thp.io/2006/wavbreaker/tb-file-format.txt
"""

import argparse
import shlex
import os

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
parser.add_argument('track_break_txt', type=str, help='Path to a wavbreaker TrackBreak .txt file')
parser.add_argument('media_file_xyz', type=str, help='Input file name (uncut file) to be used')

args = parser.parse_args()

# http://www.herongyang.com/CD-DVD/Audio-CD-Data-Structure.html
# The play back speed is 75 sectors per second
seconds_per_sector = 1/75

breaks = []

with open(args.track_break_txt) as fp:
    for line in fp:
        line = line.rstrip('\n')
        if not line or line.startswith(';'):
            continue

        if '=' in line:
            timestamp, filename = line.split('=', 1)
            timestamp = int(timestamp)
        else:
            timestamp, filename = int(line), None

        if breaks:
            #
            # Update end timestamp in previous entry:
            #
            # ((previous_timestamp, None), previous_filename)
            #                       | | \___
            #                       | v     \
            # ((previous_timestamp, timestamp), previous_filename)
            #
            breaks[-1] = ((breaks[-1][0][0], timestamp), breaks[-1][1])

        breaks.append(((timestamp, None), filename))


def format_time(seconds):
    # ffmpeg accepts "sec.msec" format timestamps
    return f'{seconds:.03f}'


for ((from_timestamp, to_timestamp), filename) in breaks:
    if filename is None:
        continue

    # Put "-ss" after "-i" for potentially more accurate seeking at
    # the expense of slower seek operations (see ffmpeg(1) manpage)
    cmd = [
        'ffmpeg',
        '-i', args.media_file_xyz,
        '-ss', format_time(from_timestamp * seconds_per_sector),
        '-c', 'copy',
    ]
    if to_timestamp is not None:
        cmd.extend([
            '-to', format_time(to_timestamp * seconds_per_sector),
        ])

    output_filename = filename + os.path.splitext(args.media_file_xyz)[1]

    cmd.append(output_filename)

    print(' '.join(shlex.quote(part) for part in cmd))
