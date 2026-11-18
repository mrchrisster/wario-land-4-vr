#!/usr/bin/env python3
"""patch_level.py in.state out.state passage level : set WL4 passage/level bytes (IWRAM 0x03000002/3)."""
import struct, sys
src = bytearray(open(sys.argv[1], 'rb').read())
o = 8 + 40 + 4
while True:
    tag = bytes(src[o:o+4]); ln = struct.unpack_from('<I', src, o + 4)[0]
    if tag == b'BUS0': base = o + 8; break
    o += 8 + ln
src[base + 0x40000 + 2] = int(sys.argv[3]); src[base + 0x40000 + 3] = int(sys.argv[4])
open(sys.argv[2], 'wb').write(src)
