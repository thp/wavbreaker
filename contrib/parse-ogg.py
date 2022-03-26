#!/usr/bin/env python3
#
# Ogg Vorbis Parser
# 2022-03-25 Thomas Perl <m@thp.io>
#
# References:
# https://www.ietf.org/rfc/rfc3533.txt
# https://xiph.org/vorbis/doc/Vorbis_I_spec.html
# https://wiki.xiph.org/VorbisComment
# https://age.hobba.nl/audio/mirroredpages/ogg-tagging.html
# https://en.wikipedia.org/wiki/Ogg_page
# http://web.mit.edu/cfox/share/doc/libvorbis-1.0/vorbis-ogg.html
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

def hexdump(data):
    for i in range(0, len(data), 16):
        print(end=f'{i:08x}')
        for ch in data[i:i+16]:
            print(end=f' {ch:02x}')
        print(end=' ')
        for ch in data[i:i+16]:
            print(end=f'{chr(ch) if ch > 30 and ch < 126 else "."}')

        print()


with open(args.filename, 'rb') as fp:
    d = fp.read()
    off = 0
    packet_segments = []

    while off < len(d):
        beg = off
        things = struct.unpack('<4sBBqIIIB', d[off:off+27])
        off += 27

        (oggs, stream_structure_version, header_type_flag, granule_position,
         bitstream_serial_number, page_sequence_number, crc_checksum,
         number_page_segments) = things

        if oggs != b'OggS' or stream_structure_version != 0:
            raise ValueError(things)

        segment_sizes = struct.unpack('B'*number_page_segments, d[off:off+number_page_segments])
        off += number_page_segments

        page_size = 27 + number_page_segments + sum(segment_sizes)

        header_type = []
        if header_type_flag & 1:
            header_type.append('continued packet')
        if header_type_flag & 2:
            header_type.append('beginning-of-stream')
        if header_type_flag & 4:
            header_type.append('end-of-stream')

        if packet_segments:
            assert header_type_flag & 1, 'segment is not finished, expected a continued packet'

        print(f"""
Ogg page at offset {beg} ({page_size} bytes) header_type={'|'.join(header_type)} \
granule_pos={granule_position} serial={bitstream_serial_number:08x} page_seq={page_sequence_number} \
crc={crc_checksum:08x}
  segments={segment_sizes}
""".lstrip())

        def flush_packet():
            global packet_segments
            packet = b''.join(packet_segments)
            print('packet:', len(packet), 'bytes', packet[:10])
            if packet[1:7] == b'vorbis':
                print('packet is a vorbis common header type:', packet[0])
                if packet[0] == 1:
                    print('identification header')
                    fmt = '<IBIIIIBB'
                    dt = packet[7:7+struct.calcsize(fmt)]
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
                elif packet[0] == 3:
                    print('comment header')
                    (vendor_length,) = struct.unpack('<I', packet[7:7+4])
                    print(f'vendor length: {vendor_length}')
                    vendor_string = packet[7+4:7+4+vendor_length].decode()
                    print(f'vendor: {vendor_string!r}')
                    so = 7 + 4 + vendor_length
                    (user_comment_list_length,) = struct.unpack('<I', packet[so:so+4])
                    so += 4
                    print(f'{user_comment_list_length=}')
                    for i in range(user_comment_list_length):
                        (comment_length,) = struct.unpack('<I', packet[so:so+4])
                        so += 4
                        comment = packet[so:so+comment_length].decode()
                        print(f'comment[{i}] = {comment!r}')
                        so += comment_length
                    framing_bit = packet[so]
                    so += 1
                    print(f'comment framing bit: {framing_bit}')
                    print(f'comment packet offset at the end: {so}')
                    print(f'comment unused bytes: {len(packet)-so}')
                    unused_comment_bytes = packet[so:]
                    hexdump(unused_comment_bytes)
                elif packet[0] == 5:
                    print('setup header')
            packet_segments = []

        for segment_size in segment_sizes:
            segment = d[off:off+segment_size]
            packet_segments.append(segment)
            if segment_size < 255:
                flush_packet()

            off += segment_size

        assert off - beg == page_size

        if packet_segments:
            print(f'... incomplete packet expected to be continued in next page ...')
        print()
