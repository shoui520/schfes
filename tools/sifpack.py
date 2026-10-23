#!/usr/bin/env python3
"""Build, verify and lay out SIF Vita game data.

assets.bin packs the extracted install/ and external/ trees into one file,
which is much faster to copy to and read from Vita storage than ~250k loose
files. Copy it to ux0:data/sif/assets.bin.

  sifpack.py build assets.bin --install DIR --external DIR [--external DIR...]
  sifpack.py verify assets.bin [--install DIR --external DIR]
  sifpack.py loose SRC DST --root external

`loose` copies an extracted tree for the loose-file form (ux0:data/sif/sif-<root>),
joining the leading directories of paths deeper than the Vita accepts.
"""
import argparse
import os
import shutil
import struct
import sys
import time
import zlib

MAGIC = b"SIFPACK\0"
VERSION = 1
HEADER = struct.Struct("<8sIIIIQQI28x")
ENTRY = struct.Struct("<QII")
DIRECTORY = 0x80000000
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
MASK = (1 << 64) - 1
# Must match SIF_MAX_PATH_COMPONENTS/SIF_COLLAPSE_SEPARATOR in src/asset_paths.h.
MAX_COMPONENTS = 10
ROOT_COMPONENTS = 3  # data/sif/sif-<root>
COLLAPSE = "%"
ROOTS = ("install", "external")


def fnv(text: str) -> int:
    h = FNV_OFFSET
    for c in text.encode("utf-8"):
        if 65 <= c <= 90:
            c += 32
        h = ((h ^ c) * FNV_PRIME) & MASK
    return h


def walk(root: str, top: str):
    """Yields (logical path, absolute path, is_dir) in sorted order."""
    yield top, root, True
    for current, dirs, files in os.walk(root):
        dirs.sort()
        rel = os.path.relpath(current, root).replace(os.sep, "/")
        base = top if rel == "." else f"{top}/{rel}"
        for name in dirs:
            yield f"{base}/{name}", os.path.join(current, name), True
        for name in sorted(files):
            yield f"{base}/{name}", os.path.join(current, name), False


def collect(args):
    """Merges the source trees; later directories of a root override earlier ones."""
    items, folded = {}, {}
    for top in ROOTS:
        for root in getattr(args, top) or []:
            if not os.path.isdir(root):
                sys.exit(f"not a directory: {root}")
            for logical, path, is_dir in walk(root, top):
                key = logical.lower()
                if key in folded and folded[key] != logical:
                    sys.exit(f"paths differ only by case: {folded[key]} {logical}")
                previous = items.get(logical)
                if previous and previous[1] != is_dir:
                    sys.exit(f"file and directory share a path: {logical}")
                folded[key] = logical
                items[logical] = (path, is_dir)
    if not items:
        sys.exit("give --install and/or --external")
    return items


def build(args):
    items = collect(args)
    kids = {}
    for logical, (_, is_dir) in items.items():
        if "/" in logical:
            parent, name = logical.rsplit("/", 1)
            kids.setdefault(parent, []).append((name, is_dir))
    hashes = {}
    for logical in items:
        h = fnv(logical)
        if h in hashes:
            sys.exit(f"hash collision: {hashes[h]} {logical}")
        hashes[h] = logical
    order = sorted(items)
    total = sum(os.path.getsize(p) for p, d in items.values() if not d)
    print(f"{len(items)} entries, {total} file bytes", flush=True)
    tmp = args.output + ".tmp"
    index, written, started = [], 0, time.time()
    with open(tmp, "wb") as out:
        out.write(b"\0" * HEADER.size)

        def align():
            out.write(b"\0" * (-out.tell() % 16))
            return out.tell()

        for n, logical in enumerate(order):
            path, is_dir = items[logical]
            offset = align()
            if is_dir:
                blob = bytearray()
                for name, child_dir in sorted(kids.get(logical, [])):
                    raw = name.encode("utf-8")
                    if len(raw) > 255:
                        sys.exit(f"name too long: {logical}/{name}")
                    blob += bytes((2 if child_dir else 1, len(raw))) + raw
                out.write(blob)
                size = len(blob) | DIRECTORY
            else:
                size = os.path.getsize(path)
                if size >= DIRECTORY:
                    sys.exit(f"file too large: {logical}")
                with open(path, "rb") as f:
                    shutil.copyfileobj(f, out, 1 << 20)
                if out.tell() - offset != size:
                    sys.exit(f"file changed while packing: {logical}")
                written += size
            if offset // 16 > 0xFFFFFFFF:
                sys.exit("pack exceeds 64 GiB")
            index.append((fnv(logical), offset // 16, size))
            if n % 5000 == 0 and n:
                rate = written / max(time.time() - started, 1e-6) / 1e6
                print(f"  {n}/{len(items)} entries, {written}/{total} bytes, {rate:.0f} MB/s", flush=True)
        index_offset = align()
        index.sort()
        raw = b"".join(ENTRY.pack(*e) for e in index)
        out.write(raw)
        file_size = out.tell()
        out.seek(0)
        out.write(HEADER.pack(MAGIC, VERSION, HEADER.size, len(index), zlib.crc32(raw),
                              index_offset, file_size, int(time.time())))
    os.replace(tmp, args.output)
    print(f"wrote {args.output}: {file_size} bytes")


def read_index(path):
    with open(path, "rb") as f:
        header = f.read(HEADER.size)
        magic, version, header_size, count, crc, index_offset, file_size, _ = HEADER.unpack(header)
        if magic != MAGIC or version != VERSION or header_size != HEADER.size:
            sys.exit("not a version 1 assets.bin")
        if os.path.getsize(path) != file_size:
            sys.exit(f"size {os.path.getsize(path)} != recorded {file_size}: incomplete copy")
        f.seek(index_offset)
        raw = f.read(count * ENTRY.size)
        if zlib.crc32(raw) != crc:
            sys.exit("index checksum mismatch")
        return {h: (o * 16, s) for h, o, s in ENTRY.iter_unpack(raw)}


def verify(args):
    index = read_index(args.pack)
    print(f"{len(index)} entries; header and index valid")
    if not (args.install or args.external):
        return
    bad = 0
    with open(args.pack, "rb") as pack:
        for logical, (path, is_dir) in sorted(collect(args).items()):
            entry = index.get(fnv(logical))
            if entry is None or bool(entry[1] & DIRECTORY) != is_dir:
                print(f"missing: {logical}")
                bad += 1
                continue
            if is_dir:
                continue
            pack.seek(entry[0])
            with open(path, "rb") as f:
                if entry[1] != os.path.getsize(path) or pack.read(entry[1]) != f.read():
                    print(f"differs: {logical}")
                    bad += 1
    if bad:
        sys.exit(f"{bad} problems")
    print("contents match")


def physical(rel: str) -> str:
    parts = rel.split("/")
    budget = MAX_COMPONENTS - ROOT_COMPONENTS
    if len(parts) > budget:
        joins = len(parts) - budget
        parts = [COLLAPSE.join(parts[: joins + 1])] + parts[joins + 1 :]
    return "/".join(parts)


def loose(args):
    collapsed = 0
    for logical, path, is_dir in walk(args.source, args.root):
        if is_dir:
            continue
        rel = logical[len(args.root) + 1 :]
        target = os.path.join(args.destination, physical(rel))
        collapsed += target != os.path.join(args.destination, rel)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        if not os.path.exists(target) or os.path.getsize(target) != os.path.getsize(path):
            shutil.copy2(path, target)
    print(f"done; {collapsed} deep files use collapsed directory names")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="command", required=True)
    b = sub.add_parser("build", help="pack extracted trees into assets.bin")
    b.add_argument("output")
    v = sub.add_parser("verify", help="check a pack, optionally against the source trees")
    v.add_argument("pack")
    for q in (b, v):
        q.add_argument("--install", action="append", help="extracted install tree (AppAssets.zip contents)")
        q.add_argument("--external", action="append",
                       help="extracted external tree (downloaded packages); repeat to merge, later wins")
    lo = sub.add_parser("loose", help="copy a tree in the Vita's loose-file layout")
    lo.add_argument("source")
    lo.add_argument("destination")
    lo.add_argument("--root", choices=ROOTS, required=True)
    args = p.parse_args()
    {"build": build, "verify": verify, "loose": loose}[args.command](args)


if __name__ == "__main__":
    main()
