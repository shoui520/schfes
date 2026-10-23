#!/usr/bin/env python3
"""Packs NPPS4 beatmaps into one file for the Vita build.

  npps_beatmaps.py BEATMAP_DIR OUTPUT

Installing ~1900 loose files from a VPK is very slow on Vita storage, so the
server reads beatmaps from one archive instead (src/server/external/beatmap.cpp):

  "NPPSBMP1", u32 count, count x (u16 name length, name, u32 offset, u32 size),
  then each beatmap gzip-compressed at its offset. Little endian.
"""
import gzip
import os
import struct
import sys

MAGIC = b"NPPSBMP1"


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    names = sorted(n for n in os.listdir(src) if n.endswith(".json"))
    blobs = []
    for name in names:
        with open(os.path.join(src, name), "rb") as f:
            blobs.append(gzip.compress(f.read(), 9, mtime=0))
    index_size = len(MAGIC) + 4 + sum(2 + len(n.encode()) + 8 for n in names)
    offset = index_size
    index = [MAGIC, struct.pack("<I", len(names))]
    for name, blob in zip(names, blobs):
        encoded = name.encode()
        index.append(struct.pack("<H", len(encoded)) + encoded + struct.pack("<II", offset, len(blob)))
        offset += len(blob)
    tmp = out + ".tmp"
    with open(tmp, "wb") as f:
        f.write(b"".join(index))
        for blob in blobs:
            f.write(blob)
    os.replace(tmp, out)


if __name__ == "__main__":
    main()
