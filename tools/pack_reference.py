#!/usr/bin/env python3
"""Convert ignored Mesen screenshots into the runtime's raw ARGB UI assets.

The input screenshots and output files are ROM-derived and must remain untracked.
Only Python's standard library is used so the conversion is reproducible.
"""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path


def paeth(a: int, b: int, c: int) -> int:
    value = a + b - c
    da, db, dc = abs(value - a), abs(value - b), abs(value - c)
    return a if da <= db and da <= dc else b if db <= dc else c


def decode_rgb_png(path: Path) -> tuple[int, int, bytes]:
    source = path.read_bytes()
    if source[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    offset, packed, width, height = 8, bytearray(), 0, 0
    while offset < len(source):
        size = struct.unpack(">I", source[offset : offset + 4])[0]
        kind = source[offset + 4 : offset + 8]
        data = source[offset + 8 : offset + 8 + size]
        offset += 12 + size
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", data)
            if (depth, color, compression, filtering, interlace) != (8, 2, 0, 0, 0):
                raise ValueError(f"{path}: expected non-interlaced 8-bit RGB PNG")
        elif kind == b"IDAT":
            packed.extend(data)
    encoded = zlib.decompress(packed)
    stride, cursor, previous, output = width * 3, 0, bytearray(width * 3), bytearray()
    for _ in range(height):
        filter_type = encoded[cursor]
        cursor += 1
        row = bytearray(encoded[cursor : cursor + stride])
        cursor += stride
        for index in range(stride):
            left = row[index - 3] if index >= 3 else 0
            above = previous[index]
            upper_left = previous[index - 3] if index >= 3 else 0
            if filter_type == 1:
                row[index] = (row[index] + left) & 0xFF
            elif filter_type == 2:
                row[index] = (row[index] + above) & 0xFF
            elif filter_type == 3:
                row[index] = (row[index] + ((left + above) // 2)) & 0xFF
            elif filter_type == 4:
                row[index] = (row[index] + paeth(left, above, upper_left)) & 0xFF
            elif filter_type != 0:
                raise ValueError(f"{path}: unsupported PNG filter {filter_type}")
        output.extend(row)
        previous = row
    return width, height, bytes(output)


def write_argb(path: Path, source: Path, replacements: dict[tuple[int, int, int], tuple[int, int, int]] | None = None) -> None:
    width, height, rgb = decode_rgb_png(source)
    if (width, height) != (256, 240):
        raise ValueError(f"{source}: expected 256x240, got {width}x{height}")
    argb = bytearray()
    for offset in range(0, len(rgb), 3):
        red, green, blue = rgb[offset : offset + 3]
        pixel = offset // 3
        y = pixel // width
        if replacements and 64 <= y < 200:
            red, green, blue = replacements.get((red, green, blue), (red, green, blue))
        argb.extend(struct.pack("<I", 0xFF000000 | red << 16 | green << 8 | blue))
    path.write_bytes(argb)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    frames = {30: "intro_00", 60: "intro_01", 240: "intro_02", 360: "intro_03",
              480: "intro_04", 600: "intro_05"}
    for frame, name in frames.items():
        write_argb(args.out / f"{name}.argb", args.captures / f"frame_{frame:04d}.png")
    write_argb(args.out / "intro_06.argb", args.captures / "audio_boundary_0920.png")
    write_argb(args.out / "intro_07.argb", args.captures / "audio_boundary_0945.png")
    normal = ((254, 110, 204), (183, 30, 123))
    pages = [
        (990, ((100, 176, 255), (21, 95, 217))),
        (1040, ((228, 229, 148), (188, 190, 0))),
        (1120, ((69, 224, 130), (0, 143, 50))),
    ]
    for page, (frame, highlight) in enumerate(pages):
        replacements = {highlight[0]: normal[0], highlight[1]: normal[1]}
        write_argb(args.out / f"menu_page_{page}.argb",
                   args.captures / f"select_page_{frame:04d}.png", replacements)
    print(f"Packed {len(frames)+2} intro frames and {len(pages)} menu pages into {args.out.parent}")


if __name__ == "__main__":
    main()
