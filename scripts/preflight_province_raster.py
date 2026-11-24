"""Inspect local flat RGB province data without adding imagery or geometry to VIC2.5."""

import argparse
from collections import Counter
from contextlib import contextmanager
import csv
import json
from pathlib import Path
import re
import sys

KINDS = ("land", "ocean", "lake")
EDGE_RULE = "four-neighbor pixel-edge count, no wrap"


def load_palette(path):
    with path.open(newline="", encoding="utf-8-sig") as f:
        rows = csv.reader(f, strict=True)
        if next(rows, None) != ["rgb", "kind"]:
            raise ValueError("expected palette CSV header rgb,kind")
        palette = {}
        for fields in rows:
            if len(fields) != 2:
                raise ValueError(f"expected RGB and kind on CSV line {rows.line_num}")
            rgb, kind = fields
            if not re.fullmatch(r"[0-9A-Fa-f]{6}", rgb) or kind not in KINDS:
                raise ValueError(f"invalid RGB or kind on CSV line {rows.line_num}")
            key = bytes.fromhex(rgb)
            if key in palette:
                raise ValueError(f"duplicate RGB {rgb.upper()}")
            palette[key] = kind
        if not palette:
            raise ValueError("empty palette")
        return palette


# Scope: flat terrain assignments and top-level default.map lists only.
SCRIPT_TOKEN = re.compile(r'\s+|#[^\r\n]*|"(?:[^"\\\r\n]|\\.)*"|[{}=]|[^\s#{}="]+|.')


def script_tokens(path):
    for token in SCRIPT_TOKEN.findall(path.read_text(encoding="utf-8-sig")):
        if token.isspace() or token.startswith("#"):
            continue
        if token == '"':
            raise ValueError(f"{path.name}: invalid quoted value")
        yield token


def decode_province(token):
    match = re.fullmatch(r"[xX]([0-9A-Fa-f]{6})", token.strip('"'))
    if not match:
        raise ValueError(f"expected xRRGGBB province ID, got {token}")
    return bytes.fromhex(match.group(1))


def load_vic3(game):
    maps = game / "map_data"
    terrain = {}
    tokens = iter(script_tokens(maps / "province_terrains.txt"))
    for item in tokens:
        province = decode_province(item)
        equals = next(tokens, None)
        value = next(tokens, "").strip('"')
        if equals != "=" or not re.fullmatch(r"[a-zA-Z_][a-zA-Z_0-9]*", value):
            raise ValueError("province_terrains.txt: expected a flat terrain assignment")
        if province in terrain:
            raise ValueError(f"duplicate province {id_of(province)} in province_terrains.txt")
        terrain[province] = value
    if not terrain:
        raise ValueError("empty province_terrains.txt")

    tokens = iter(script_tokens(maps / "default.map"))
    water = {}
    png_seen = False
    repeated_sea = 0
    for key in tokens:
        if not re.fullmatch(r"[a-zA-Z_][a-zA-Z_0-9]*", key) or next(tokens, None) != "=":
            raise ValueError("default.map: expected a top-level assignment")
        value = next(tokens, None)
        if value is None or value in ("}", "="):
            raise ValueError(f"default.map: missing value for {key}")
        if key == "provinces":
            if png_seen or value.strip('"') != "provinces.png":
                raise ValueError("default.map: expected one provinces.png declaration")
            png_seen = True
        if key in ("sea_starts", "lakes"):
            if key in water:
                raise ValueError(f"duplicate {key} block in default.map")
            if value != "{":
                raise ValueError(f"default.map: expected {key} block")
            ids = water[key] = set()
            for item in tokens:
                if item == "}":
                    break
                province = decode_province(item)
                if province in ids:
                    if key == "sea_starts":
                        repeated_sea += 1
                    else:
                        raise ValueError(f"duplicate province {id_of(province)} in {key}")
                ids.add(province)
            else:
                raise ValueError(f"default.map: unclosed {key} block")
        elif value == "{":
            depth = 1
            for item in tokens:
                depth += (item == "{") - (item == "}")
                if not depth:
                    break
            else:
                raise ValueError(f"default.map: unclosed {key} block")
    for key in ("sea_starts", "lakes"):
        if key not in water:
            raise ValueError(f"missing {key} block in default.map")
    both = water["sea_starts"] & water["lakes"]
    if both:
        raise ValueError(f"province {id_of(min(both))} listed in both sea_starts and lakes")
    declared = water["sea_starts"] | water["lakes"]
    missing = declared - terrain.keys()
    if missing:
        raise ValueError(f"unregistered water province {id_of(min(missing))}")
    for key, kinds in (("sea_starts", {"ocean"}), ("lakes", {"lakes", "lake"})):
        from_registry = {province for province, value in terrain.items() if value in kinds}
        different = from_registry ^ water[key]
        if different:
            raise ValueError(f"{key} disagrees with terrain registry at {id_of(min(different))}")
    if repeated_sea:
        print(f"province preflight: ignored {repeated_sea} repeated sea_starts entries", file=sys.stderr)
    # --game-dir always means the documented canonical map path. If a file
    # overrides it, the parser above rejects the override rather than reading it.
    return maps / "provinces.png", {
        province: "ocean" if province in water["sea_starts"]
        else "lake" if province in water["lakes"] else "land"
        for province in terrain
    }


def id_of(key):
    return key.hex().upper()


def inspect(width, height, rows, palette):
    occupied = Counter()
    contacts = Counter()
    previous = []
    for y, row in enumerate(rows):
        row = bytes(row)
        if len(row) != width * 3:
            raise ValueError("unexpected decoded row length")
        colors = [row[i:i + 3] for i in range(0, len(row), 3)]
        for color, count in Counter(colors).items():
            if color not in palette:
                raise ValueError(f"unregistered color {id_of(color)} at pixel ({colors.index(color)}, {y})")
            occupied[color] += count
        for left, right in zip(colors, colors[1:]):
            if left != right:
                contacts[min(left, right), max(left, right)] += 1
        for north, south in zip(previous, colors):
            if north != south:
                contacts[min(north, south), max(north, south)] += 1
        previous = colors
    missing = palette.keys() - occupied.keys()
    if missing:
        raise ValueError(f"unused registered color {id_of(min(missing))}")
    # Stable per-source IDs are the literal RGB values, never inferred from order.
    counts = Counter(palette.values())
    return {
        "schema": "province_raster_preflight_v1",
        "coordinate_space": "source raster pixels only; not geographic coordinates",
        "contact_rule": EDGE_RULE,
        "dimensions_pixels": [width, height],
        "counts": {kind: counts[kind] for kind in KINDS},
        "provinces": [
            {"id": id_of(key), "kind": palette[key], "pixels": occupied[key]}
            for key in sorted(occupied)
        ],
        "contacts": [[id_of(a), id_of(b), length] for (a, b), length in sorted(contacts.items())],
    }



def check_png_header(png):
    with png.open("rb") as stream:
        header = stream.read(33)
        if len(header) != 33 or header[:16] != b"\x89PNG\r\n\x1a\n\x00\x00\x00\x0dIHDR":
            raise ValueError("expected PNG with a valid header")
        if (header[24], header[25]) not in {(8, 2), (1, 3), (2, 3), (4, 3), (8, 3)}:
            raise ValueError("province colors require an 8-bit RGB or indexed PNG")
        while True:
            chunk = stream.read(8)
            if len(chunk) != 8 or chunk[4:] == b"IEND":
                raise ValueError("PNG has no pixel data")
            if chunk[4:] == b"IDAT":
                return
            if chunk[4:] in (b"acTL", b"tRNS"):
                raise ValueError("expected one opaque PNG frame")
            stream.seek(int.from_bytes(chunk[:4], "big") + 4, 1)


@contextmanager
def open_png_rows(png, reader):
    """Open RGB rows for callers sharing the same exact-class decoder."""
    check_png_header(png)
    if reader == "gdal":
        from osgeo import gdal
        gdal.UseExceptions()
        with gdal.OpenEx(str(png), gdal.OF_RASTER | gdal.OF_READONLY,
                         allowed_drivers=["PNG"]) as image:
            if image is None or image.RasterCount != 3:
                raise ValueError("GDAL reader requires an opaque 8-bit RGB PNG")
            bands = (1, 2, 3)
            for i, color in zip(bands, (gdal.GCI_RedBand, gdal.GCI_GreenBand, gdal.GCI_BlueBand)):
                band = image.GetRasterBand(i)
                if (band.DataType != gdal.GDT_Byte or band.GetColorInterpretation() != color
                        or band.GetMaskFlags() != gdal.GMF_ALL_VALID):
                    raise ValueError("GDAL reader requires an opaque 8-bit RGB PNG")
            width, height = image.RasterXSize, image.RasterYSize
            rows = (image.ReadRaster(0, y, width, 1, buf_type=gdal.GDT_Byte,
                                     band_list=bands, buf_pixel_space=3,
                                     buf_line_space=3 * width, buf_band_space=1)
                    for y in range(height))
            yield width, height, rows
        return
    from PIL import Image
    try:
        with Image.open(png, formats=("PNG",)) as image:
            if (image.mode not in {"RGB", "P"} or "transparency" in image.info
                    or getattr(image, "n_frames", 1) != 1):
                raise ValueError("expected one opaque RGB or indexed PNG frame")
            rgb = image.convert("RGB") if image.mode == "P" else image
            width, height = rgb.size
            rows = (rgb.crop((0, y, width, y + 1)).tobytes() for y in range(height))
            yield width, height, rows
    except Image.DecompressionBombError as error:
        raise ValueError(str(error)) from error


def inspect_png(png, palette, reader):
    with open_png_rows(png, reader) as (width, height, rows):
        return inspect(width, height, rows, palette)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("png", nargs="?", type=Path, help="local opaque RGB or indexed PNG")
    parser.add_argument("palette", nargs="?", type=Path, help="local CSV with rgb,kind columns")
    parser.add_argument("--game-dir", type=Path, help="read-only Victoria 3 game directory containing map_data")
    parser.add_argument("--png-reader", choices=("pillow", "gdal"), default="pillow",
                        help="use an already installed PNG reader (default: pillow)")
    args = parser.parse_args()
    if (args.game_dir and (args.png or args.palette)) or (not args.game_dir and not (args.png and args.palette)):
        parser.error("use --game-dir GAME_DIR or give both PNG and PALETTE")
    try:
        png, palette = load_vic3(args.game_dir) if args.game_dir else (args.png, load_palette(args.palette))
        report = inspect_png(png, palette, args.png_reader)
    except ImportError:
        parser.exit(2, f"requires an existing {args.png_reader} installation; nothing was installed\n")
    except (OSError, ValueError, csv.Error, RuntimeError) as error:
        parser.exit(2, f"province preflight: {error}\n")
    json.dump(report, sys.stdout, indent=2)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
