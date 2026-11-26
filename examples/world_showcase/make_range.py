#!/usr/bin/env python3
"""Writes the height tiles and the manifest of the `range` terrain (assets/terrain/range.aterrain).

The output is checked in, so this runs by hand only when the terrain shape changes:

    python make_range.py [out-dir]        (default: assets/terrain next to this script)

6 x 6 tiles of 65 x 65 samples, 2 m apart (128 m per tile, 768 m in all). Heights are a function of the world
position, so neighbouring tiles agree on their shared edge samples. 16-bit grayscale PNG, X varies fastest, rows
advance in +Z (the layout `read_terrain_tile` expects). Pure standard library.
"""
import json
import math
import struct
import sys
import zlib
from pathlib import Path

TILES = 6
SAMPLES = 65
SPACING = 2.0
MIN_HEIGHT = 0.0
MAX_HEIGHT = 90.0


def height01(x, z):
    """Terrain height in [0, 1] at a world position (metres)."""
    n = 0.30
    n += 0.20 * math.sin(x * 0.0105 + 1.3) * math.cos(z * 0.0087 + 0.4)
    n += 0.10 * math.sin(x * 0.023 + z * 0.017 + 2.0)
    n += 0.05 * math.cos(x * 0.050 - z * 0.041)
    n += 0.02 * math.sin(x * 0.110 + z * 0.090)
    n += 0.50 * math.exp(-((x - 540.0) ** 2 + (z - 300.0) ** 2) / (2.0 * 110.0 ** 2))  # the mountain
    n -= 0.22 * math.exp(-((x - 200.0) ** 2 + (z - 560.0) ** 2) / (2.0 * 90.0 ** 2))   # a basin
    return min(1.0, max(0.0, n))


def write_png(path, data, n):
    """A 16-bit grayscale n x n PNG; `data` is n*n values, X fastest."""
    raw = bytearray()
    for row in range(n):
        raw.append(0)
        raw += struct.pack(">%dH" % n, *data[row * n:(row + 1) * n])

    def chunk(kind, body):
        crc = zlib.crc32(kind + body) & 0xFFFFFFFF
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", crc)

    header = struct.pack(">IIBBBBB", n, n, 16, 0, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent / "assets" / "terrain"
    out.mkdir(parents=True, exist_ok=True)
    side = (SAMPLES - 1) * SPACING
    tiles = []
    for tz in range(TILES):
        for tx in range(TILES):
            data = []
            for j in range(SAMPLES):
                for i in range(SAMPLES):
                    h = height01(tx * side + i * SPACING, tz * side + j * SPACING)
                    data.append(int(round(h * 65535.0)))
            name = "range-%d-%d.png" % (tx, tz)
            write_png(out / name, data, SAMPLES)
            tiles.append({"x": tx, "z": tz, "file": "terrain/" + name})
    manifest = {"format": "astral.terrain", "version": 1, "samples": SAMPLES, "spacing_m": SPACING,
                "min_height_m": MIN_HEIGHT, "max_height_m": MAX_HEIGHT, "tiles": tiles}
    (out / "range.aterrain").write_text(json.dumps(manifest, indent=2) + "\n")
    print("wrote %d tiles to %s" % (len(tiles), out))


if __name__ == "__main__":
    main()
