#!/usr/bin/env python3
#
# Ogg Vorbis Parser
# 2022-03-25 Thomas Perl <m@thp.io>
#
# References:
# https://www.ietf.org/rfc/rfc3533.txt
# https://xiph.org/vorbis/doc/Vorbis_I_spec.html
#

import argparse
import struct

# The granule (PCM) position of the first page need not indicate that the stream started at position zero. Although the
# granule position belongs to the last completed packet on the page and a valid granule position must be positive, by
# inference it may indicate that the PCM position of the beginning of audio is positive or negative.

# A positive starting value simply indicates that this stream begins at some positive time offset, potentially within a
# larger program. This is a common case when connecting to the middle of broadcast stream.  A negative value indicates
# that output samples preceeding time zero should be discarded during decoding; this technique is used to allow
# sample-granularity editing of the stream start time of already-encoded Vorbis streams. The number of samples to be
# discarded must not exceed the overlap-add span of the first two audio packets.

parser = argparse.ArgumentParser(description='Parse Ogg')
parser.add_argument('filename', type=str, help='Filename of Ogg file')
args = parser.parse_args()

with open(args.filename, 'rb') as fp:
    d = fp.read()
    off = 0
    while True:
        beg = off
        things = struct.unpack('<4sBBqIIIB', d[off:off+27])
        off += 27

        (oggs, stream_structure_version, header_type_flag, granule_position,
         bitstream_serial_number, page_sequence_number, crc_checksum,
         number_page_segments) = things

        if oggs != b'OggS':
            raise ValueError(things)

        segment_sizes = struct.unpack('B'*number_page_segments, d[off:off+number_page_segments])
        off += number_page_segments

        print('->', beg, things, {
            'header_type': header_type_flag,
            'granule_pos': granule_position,
            'serial': bitstream_serial_number,
            'page_seq': page_sequence_number,
            'crc': crc_checksum,
            'number_segs': number_page_segments,
            'segs': segment_sizes,
        })

        cur_seg = []

        def flush():
            global cur_seg
            seg = b''.join(cur_seg)
            print('seg:', len(seg), 'bytes', seg[:10])
            if seg[1:7] == b'vorbis':
                print('seg is a vorbis common header type:', seg[0])
                if seg[0] == 1:
                    print('identification header')
                    fmt = '<IBIIIIBB'
                    dt = seg[7:7+struct.calcsize(fmt)]
                    (vorbis_version, audio_channels, audio_sample_rate, bitrate_maximum,
                     bitrate_nominal, bitrate_minimum, blocksize_packed, framing_packed) = struct.unpack(fmt, dt)
                    BLOCK_SIZES = [64, 128, 256, 512, 1024, 2048, 4096, 8192]
                    blocksize_pt1 = 2**(blocksize_packed & 0x0F)
                    blocksize_pt2 = 2**((blocksize_packed >> 4) & 0x0F)
                    print(f"""
Vorbis version: {vorbis_version}
Audio channels: {audio_channels}
Sampling rate:  {audio_sample_rate}
Bitrate:        max={bitrate_maximum}, nom={bitrate_nominal}, min={bitrate_minimum}
blocksize0:     {blocksize_pt1}
blocksize1:     {blocksize_pt2}
framing:        {framing_packed}
""")
                elif seg[0] == 3:
                    print('comment header')
                    (vendor_length,) = struct.unpack('<I', seg[7:7+4])
                    print(f'vendor length: {vendor_length}')
                    vendor_string = seg[7+4:7+4+vendor_length].decode()
                    print(f'vendor: {vendor_string!r}')
                    so = 7 + 4 + vendor_length
                    (user_comment_list_length,) = struct.unpack('<I', seg[so:so+4])
                    so += 4
                    print(f'{user_comment_list_length=}')
                    for i in range(user_comment_list_length):
                        (comment_length,) = struct.unpack('<I', seg[so:so+4])
                        so += 4
                        comment = seg[so:so+comment_length].decode()
                        print(f'comment[{i}] = {comment!r}')
                        so += comment_length
                    framing_bit = seg[so]
                    so += 1
                    print(f'comment framing bit: {framing_bit}')
                    print(f'comment seg offset at the end: {so}')
                elif seg[0] == 5:
                    print('setup header')
            cur_seg = []

        for segment_size in segment_sizes:
            segment = d[off:off+segment_size]
            cur_seg.append(segment)
            if segment_size < 255:
                flush()

            off += segment_size

        if cur_seg:
            flush()
