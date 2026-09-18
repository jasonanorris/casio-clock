#!/usr/bin/env python3
"""Generate LVGL alpha masks for the nine-segment weekday glyph."""

import re
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "neat-luulia-densor-9-filled.svg"
HEADER = ROOT / "src" / "weekday_segment_assets.h"
IMPLEMENTATION = ROOT / "src" / "weekday_segment_assets.cpp"
VIEWBOX = (193.65167236328125, 137.00300598144528,
           77.76980590820312, 130.07731628417966)
# Source order: bottom, lower-left, center-lower, center-upper, top,
# upper-left, lower-right, middle, upper-right. Export as A-I.
SUBPATH_TO_SEGMENT = (4, 8, 6, 0, 1, 5, 7, 3, 2)
WIDTH = 60
HEIGHT = 104
SUPERSAMPLE = 4


def read_subpaths():
    source = SOURCE.read_text()
    path_data = re.search(r'<path d="(.*?)" fill=', source, re.S).group(1)
    subpaths = [part for part in re.split(r"(?=M\s)", path_data.strip())
                if part.strip()]
    polygons = []
    for subpath in subpaths:
        numbers = [float(value) for value in re.findall(
            r"[-+]?\d+(?:\.\d*)?(?:[eE][-+]?\d+)?", subpath)]
        polygons.append(list(zip(numbers[::2], numbers[1::2])))
    if len(polygons) != 9:
        raise RuntimeError(f"Expected 9 SVG subpaths, found {len(polygons)}")
    return [polygons[index] for index in SUBPATH_TO_SEGMENT]


def rasterize(points):
    view_x, view_y, view_width, view_height = VIEWBOX
    canvas = Image.new("L", (WIDTH * SUPERSAMPLE, HEIGHT * SUPERSAMPLE), 0)
    polygon = [
        ((x - view_x) / view_width * WIDTH * SUPERSAMPLE,
         (y - view_y) / view_height * HEIGHT * SUPERSAMPLE)
        for x, y in points
    ]
    ImageDraw.Draw(canvas).polygon(polygon, fill=255)
    canvas = canvas.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    bounds = canvas.getbbox()
    if bounds is None:
        raise RuntimeError("Generated an empty segment")
    left, top, right, bottom = bounds
    return left, top, canvas.crop(bounds)


def bytes_literal(data):
    values = list(data)
    return ",\n".join(
        "    " + ", ".join(str(value) for value in values[index:index + 20])
        for index in range(0, len(values), 20)
    )


def main():
    declarations = []
    definitions = []
    entries = []
    for index, polygon in enumerate(read_subpaths()):
        left, top, image = rasterize(polygon)
        symbol = f"weekday_segment_{index}"
        data_symbol = f"{symbol}_data"
        declarations.append(f"extern const lv_img_dsc_t {symbol};")
        definitions.append(
            f"const uint8_t {data_symbol}[] = {{\n{bytes_literal(image.tobytes())}\n}};\n\n"
            f"const lv_img_dsc_t {symbol} = {{\n"
            f"    .header = {{.cf = LV_IMG_CF_ALPHA_8BIT, .always_zero = 0, "
            f".reserved = 0, .w = {image.width}, .h = {image.height}}},\n"
            f"    .data_size = sizeof({data_symbol}),\n"
            f"    .data = {data_symbol},\n"
            f"}};"
        )
        entries.append(f"    {{&{symbol}, {left}, {top}}}")

    HEADER.write_text(
        "#pragma once\n\n#include <lvgl.h>\n\n"
        "struct WeekdaySegmentAsset {\n"
        "  const lv_img_dsc_t *image;\n"
        "  lv_coord_t x;\n"
        "  lv_coord_t y;\n"
        "};\n\n"
        + "\n".join(declarations)
        + "\n\nextern const WeekdaySegmentAsset weekdaySegmentAssets[9];\n"
    )
    IMPLEMENTATION.write_text(
        '#include "weekday_segment_assets.h"\n\n'
        + "\n\n".join(definitions)
        + "\n\nconst WeekdaySegmentAsset weekdaySegmentAssets[9] = {\n"
        + ",\n".join(entries) + "\n};\n"
    )


if __name__ == "__main__":
    main()
