#!/usr/bin/env python3
"""lkpack.py — LithoKraft thin-layer asset packer (hand-written, ours).

Replaces the Emscripten file-packager payload that the old glue consumed.
Packs the same five preload roots the historical link line used into ONE
blob with a compact binary header, read by our own loader
(wasm/full/lkboot.js):

    offset 0   : magic  "LKP2"
    offset 4   : u32le  header length H (bytes that follow, before payload)
    offset 8   : dirs table — u16le count; per dir: u16le byte length +
                 UTF-8 bytes (dirs sorted; "/" is always entry 0's kind:
                 the root is included in the table)
                 files table — u32le count; per file:
                   u16le dir index, u16le name byte length + name bytes,
                   u32 start delta (unsigned LEB128, from the previous
                   file's start), u32 length (unsigned LEB128)
    offset 8+H : payload — file contents concatenated in header order.

Paths are destination paths exactly as the old --preload-file mappings
named them. Packing is deterministic: files sorted by destination path,
dirs sorted, varints canonical, no timestamps inside the blob.

(LKP1, the JSON-header format, was retired in the 2026-10-20 bug-fix
run per the code review's bit-packing ledger: ~230 KB of JSON header
became ~50 KB of binary for the same 2,043 files. The loader accepts
LKP2 only and rejects anything else loudly at boot.)

Usage:
    python3 lkpack.py <tree-root> <output-file>
"""
import os
import struct
import sys

# (source path relative to tree root, destination prefix) — mirrors the
# --preload-file arguments of the historical Emscripten link line.
ROOTS = [
    ("wasm/stage/engine-assets", "/assets"),
    ("game/LithoKraft/assets", "/LithoKraft/assets"),
    ("game/LithoKraft/scripts", "/scripts"),
    ("game/LithoKraft/config", "/config"),
    ("game/LithoKraft/mod.json", "/mod.json"),  # single file, not a dir
]


def varint(n: int) -> bytes:
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    tree = os.path.abspath(sys.argv[1])
    out_path = os.path.abspath(sys.argv[2])

    files = []  # (dest_path, source_file)
    dirs = set(["/"])
    for src_rel, dest in ROOTS:
        src = os.path.join(tree, src_rel)
        if os.path.isfile(src):
            files.append((dest, src))
            parent = os.path.dirname(dest)
            while parent and parent != "/":
                dirs.add(parent)
                parent = os.path.dirname(parent)
            continue
        if not os.path.isdir(src):
            print(f"error: preload root missing: {src}")
            return 1
        for base, dirnames, filenames in os.walk(src):
            dirnames.sort()
            rel_dir = os.path.relpath(base, src).replace(os.sep, "/")
            dest_dir = dest if rel_dir == "." else dest + "/" + rel_dir
            dirs.add(dest_dir)
            parent = os.path.dirname(dest_dir)
            while parent and parent != "/":
                dirs.add(parent)
                parent = os.path.dirname(parent)
            for name in sorted(filenames):
                files.append((dest_dir + "/" + name, os.path.join(base, name)))

    files.sort(key=lambda t: t[0])
    dir_list = sorted(dirs)
    dir_index = {d: i for i, d in enumerate(dir_list)}

    header = bytearray()
    header += struct.pack("<H", len(dir_list))
    for d in dir_list:
        db = d.encode("utf-8")
        header += struct.pack("<H", len(db)) + db
    header += struct.pack("<I", len(files))

    payload = bytearray()
    prev_start = 0
    for dest, src in files:
        with open(src, "rb") as f:
            data = f.read()
        parent = os.path.dirname(dest) or "/"
        name = os.path.basename(dest)
        nb = name.encode("utf-8")
        start = len(payload)
        header += struct.pack("<HH", dir_index[parent], len(nb)) + nb
        header += varint(start - prev_start)
        header += varint(len(data))
        prev_start = start
        payload += data

    blob = b"LKP2" + struct.pack("<I", len(header)) + bytes(header) + bytes(payload)
    with open(out_path, "wb") as f:
        f.write(blob)

    import hashlib
    print(f"lkpack: {len(files)} files, {len(dir_list)} dirs, "
          f"header {len(header)} B, payload {len(payload)} B, "
          f"total {len(blob)} B -> {out_path}")
    print(f"lkpack: sha256 {hashlib.sha256(blob).hexdigest()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
