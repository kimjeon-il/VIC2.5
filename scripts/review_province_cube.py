"""Review one explicitly calibrated +X cube window; never generate game adjacency."""

import argparse
from array import array
from collections import Counter
import csv
import json
from math import asinh, atan, atan2, cos, degrees, floor, ceil, hypot, isfinite, radians, tan
import os
from pathlib import Path
import sys
import tempfile

from preflight_province_raster import KINDS, load_palette, load_vic3, open_png_rows

MAX_REVIEW_CELLS = 2_000_000
KIND_BYTES = {"land": b"\x01", "ocean": b"\x02", "lake": b"\x03"}


def projection(lat, name):
    return lat if name == "linear" else degrees(1.25 * asinh(tan(0.8 * radians(lat))))


def local_witness(pixels, width, height, a, b):
    """Check for an orthogonal source witness, including same-label connectivity."""
    if not (0 <= a < width * height and 0 <= b < width * height):
        return False
    if a == b:
        return True
    ya, xa = divmod(a, width)
    yb, xb = divmod(b, width)
    dx, dy = abs(xa - xb), abs(ya - yb)
    if dx + dy == 1:
        return True
    if dx == dy == 1:
        # A four-side edge within this 2x2, not a distant pair elsewhere.
        return (pixels[ya * width + xb] in (pixels[a], pixels[b])
                or pixels[yb * width + xa] in (pixels[a], pixels[b]))
    return False


def grid_edges(cells, width, height):
    for y in range(height):
        for x in range(width):
            i = y * width + x
            if cells[i] is None:
                continue
            if x and cells[i - 1] is not None:
                yield i - 1, i
            if y and cells[i - width] is not None:
                yield i - width, i


def pair(a, b):
    return (a, b) if a < b else (b, a)


class ComponentIndex:
    def __init__(self, count):
        self.parents = array("i", range(count))
        self.ranks = bytearray(count)

    def root(self, x):
        while x != self.parents[x]:
            self.parents[x] = self.parents[self.parents[x]]
            x = self.parents[x]
        return x

    def join(self, a, b):
        a, b = self.root(a), self.root(b)
        if a == b:
            return
        if self.ranks[a] < self.ranks[b]:
            a, b = b, a
        self.parents[b] = a
        if self.ranks[a] == self.ranks[b]:
            self.ranks[a] += 1


def component_changes(source, tile, samples, covered):
    """Check the component identity relation, not just the two graph counts."""
    first_source, first_tile = {}, {}
    merges, splits = set(), set()
    for i, origin in enumerate(samples):
        if origin not in covered:
            continue
        a, b = source.root(origin), tile.root(i)
        if first_source.setdefault(b, a) != a:
            merges.add(b)
        if first_tile.setdefault(a, b) != b:
            splits.add(a)
    return len(merges), len(splits)


def write_review_tile(path, serialized):
    """Publish only a closed, complete file; never replace a destination."""
    out = tempfile.NamedTemporaryFile(dir=path.parent, prefix=".cube-review-",
                                      suffix=".tmp", delete=False)
    temporary = Path(out.name)
    try:
        with out:
            if out.write(serialized) != len(serialized):
                raise OSError("incomplete review tile write")
            out.flush()
            os.fsync(out.fileno())
        # Same-directory hard link is an atomic create-if-absent. If the
        # filesystem does not support it, refuse to publish the tile.
        os.link(temporary, path)
    finally:
        temporary.unlink()


def prepare(bounds, uv_bounds, transform, p, size):
    w, s, e, n = bounds
    u0, v0, u1, v1 = uv_bounds
    x0, xscale, y0, yscale = transform
    if not all(isfinite(k) for group in (bounds, uv_bounds, transform) for k in group):
        raise ValueError("nonfinite geographic, cube, or transform input")
    if not -180 <= w < e <= 180 or not -90 < s < n < 90:
        raise ValueError("require ordered geographic bounds; no longitude wrap")
    cos_lon = cos(radians(max(abs(w), abs(e))))
    if max(abs(w), abs(e)) >= 45 or max(abs(tan(radians(s))), abs(tan(radians(n)))) >= cos_lon:
        raise ValueError("geographic bounds must be strictly inside the +X cube face")
    if not (-1 <= u0 < u1 <= 1 and -1 <= v0 < v1 <= 1):
        raise ValueError("UV bounds must be increasing within [-1, 1]")
    if xscale <= 0 or yscale <= 0 or not 1 <= size <= 1024:
        raise ValueError("require positive transform scales and size from 1 to 1024")
    # Edge coordinates: y increases south, x increases east. Pixel centres lie
    # at integer + 0.5; floor selects the nearest centre with a consistent tie.
    px = (x0 + xscale * w, x0 + xscale * e)
    py = (y0 - yscale * projection(n, p), y0 - yscale * projection(s, p))
    if not all(isfinite(k) for k in (*px, *py)) or not (px[0] < px[1] and py[0] < py[1]):
        raise ValueError("degenerate or nonfinite projected source bounds")
    return px, py


def review(width, height, rows, palette, args):
    px, py = prepare(args.bounds, args.uv_bounds, args.transform, args.projection, args.size)
    if not (0 <= px[0] and px[1] <= width and 0 <= py[0] and py[1] <= height):
        raise ValueError("geographic coverage projects outside source raster")
    left, right = floor(px[0]), ceil(px[1])
    top, bottom = floor(py[0]), ceil(py[1])
    cw, ch = right - left, bottom - top
    if cw * ch > MAX_REVIEW_CELLS:
        raise ValueError("review source crop exceeds 2,000,000 cells; use a smaller scope")
    pixels = []
    for i, row in enumerate(rows):
        # Use the physically bounded crop only; no invented class outside it.
        if i >= bottom:
            break
        if i < top:
            continue
        row = bytes(row)
        if len(row) != width * 3:
            raise ValueError("unexpected decoded row length")
        for j in range(left * 3, right * 3, 3):
            color = row[j:j + 3]
            if color not in palette:
                raise ValueError(f"unregistered color {color.hex().upper()} in source crop")
            pixels.append(color)
    if len(pixels) != cw * ch:
        raise ValueError("incomplete source crop")
    centres = [
        color if (px[0] <= left + x + .5 < px[1] and py[0] <= top + y + .5 < py[1]) else None
        for y in range(ch) for x in range(cw) for color in (pixels[y * cw + x],)
    ]
    source_positions = {i for i, c in enumerate(centres) if c is not None}
    source_labels = {centres[i] for i in source_positions}
    if not source_positions:
        raise ValueError("geographic scope contains no source pixel centres")
    source_components = ComponentIndex(len(centres))
    source_pairs = set()
    for i, j in grid_edges(centres, cw, ch):
        if centres[i] == centres[j]:
            source_components.join(i, j)
        else:
            source_pairs.add(pair(centres[i], centres[j]))

    w, s, e, n = args.bounds
    u0, v0, u1, v1 = args.uv_bounds
    ax, sx, ay, sy = args.transform
    size = args.size
    samples = []
    # Match VIC2.5's RasterIdTile/archived cube tile: column u, row v.
    # phase_e_raster.hpp uses direction (1, v, -u) on +X (face 0).
    for row in range(size):
        v = v0 + (v1 - v0) * (row + .5) / size
        lon = degrees(atan(v))
        for col in range(size):
            u = u0 + (u1 - u0) * (col + .5) / size
            lat = degrees(atan2(-u, hypot(1, v)))
            if not (w <= lon < e and s < lat <= n):
                samples.append(None)
                continue
            x = floor(ax + sx * lon) - left
            y = floor(ay - sy * projection(lat, args.projection)) - top
            if not (0 <= x < cw and 0 <= y < ch):
                raise ValueError("in-scope cube sample maps outside source crop")
            samples.append(y * cw + x)
    taken = {i for i in samples if i is not None}
    if not taken:
        raise ValueError("UV window contains no covered cells")
    labels = {pixels[i] for i in taken}
    tile_components = ComponentIndex(len(samples))
    tile_pairs = set()
    unwitnessed = unwitnessed_land = unwitnessed_same_label = 0
    for i, j in grid_edges(samples, size, size):
        a, b = samples[i], samples[j]
        ca, cb = pixels[a], pixels[b]
        if ca == cb:
            tile_components.join(i, j)
            if not local_witness(pixels, cw, ch, a, b):
                unwitnessed_same_label += 1
            continue
        tile_pairs.add(pair(ca, cb))
        if not local_witness(pixels, cw, ch, a, b):
            unwitnessed += 1
            unwitnessed_land += palette[ca] == palette[cb] == "land"
    merges, splits = component_changes(source_components, tile_components, samples, source_positions)
    missed_cells = len(source_positions - taken)
    missed_ids = len(source_labels - labels)
    unretained_pairs = len(source_pairs - tile_pairs)
    new_pairs = len(tile_pairs - source_pairs)
    tile_kinds = Counter("missing" if i is None else palette[pixels[i]] for i in samples)
    source_kinds = Counter(palette[key] for key in source_labels)
    passed = not (missed_cells or missed_ids or unretained_pairs or new_pairs or unwitnessed
                  or unwitnessed_same_label or merges or splits or (taken - source_positions))
    return {
        "schema": "province_cube_review_v1", "scope": "private, review only, no game adjacency",
        "face": 0, "size": size, "uv_bounds": args.uv_bounds, "geographic_bounds": args.bounds,
        "projection": args.projection, "transform": args.transform,
        "source_cells": len(source_positions), "source_ids": {k: source_kinds[k] for k in KINDS},
        "missed_source_cells": missed_cells, "missed_source_ids": missed_ids,
        "sampled_cells_outside_source_centre_scope": len(taken - source_positions),
        "source_contact_pairs": len(source_pairs), "tile_contact_pairs": len(tile_pairs),
        "missed_source_contact_pairs": unretained_pairs, "new_tile_contact_pairs": new_pairs,
        "unwitnessed_edges": unwitnessed, "unwitnessed_land_edges": unwitnessed_land,
        "unwitnessed_same_label_edges": unwitnessed_same_label,
        "merged_tile_components": merges, "split_source_components": splits,
        "tile_cells_by_kind": {k: tile_kinds[k] for k in ("missing", *KINDS)},
        "preservation_passed": passed,
    }, (samples, pixels)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("png", nargs="?", type=Path)
    parser.add_argument("palette", nargs="?", type=Path)
    parser.add_argument("--game-dir", type=Path, help="local review copy with map_data")
    parser.add_argument("--png-reader", choices=("pillow", "gdal"), default="pillow")
    parser.add_argument("--bounds", nargs=4, type=float, required=True, metavar=("W", "S", "E", "N"))
    parser.add_argument("--uv-bounds", nargs=4, type=float, required=True,
                        metavar=("UMIN", "VMIN", "UMAX", "VMAX"))
    parser.add_argument("--size", type=int, required=True)
    parser.add_argument("--projection", choices=("linear", "miller"), required=True)
    parser.add_argument("--transform", nargs=4, type=float, required=True,
                        metavar=("X0", "XSCALE", "Y0", "YSCALE"))
    parser.add_argument("--tile-output", type=Path,
                        help="new private RGB-kind raster file, only after preservation passes")
    args = parser.parse_args()
    if (args.game_dir and (args.png or args.palette)) or (not args.game_dir and not (args.png and args.palette)):
        parser.error("use --game-dir GAME_DIR or give both PNG and PALETTE")
    try:
        png, palette = load_vic3(args.game_dir) if args.game_dir else (args.png, load_palette(args.palette))
        with open_png_rows(png, args.png_reader) as (width, height, rows):
            report, state = review(width, height, rows, palette, args)
        if args.tile_output and report["preservation_passed"]:
            samples, pixels = state
            serialized = bytearray()
            for i in samples:
                serialized.extend(b"\x00\x00\x00\x00" if i is None
                                  else pixels[i] + KIND_BYTES[palette[pixels[i]]])
            write_review_tile(args.tile_output, serialized)
        report["tile_written"] = bool(args.tile_output and report["preservation_passed"])
    except ImportError:
        parser.exit(2, f"requires an existing {args.png_reader} installation; nothing was installed\n")
    except (OSError, ValueError, csv.Error, RuntimeError) as error:
        parser.exit(2, f"province cube review: {error}\n")
    json.dump(report, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0 if report["preservation_passed"] else 3


if __name__ == "__main__":
    sys.exit(main())
