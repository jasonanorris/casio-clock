#!/usr/bin/env python3
"""Generate LVGL alpha-mask images from the supplied seven-segment SVG."""

import re
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "neat-luulia-densor-8-filled.svg"
HEADER = ROOT / "src" / "seven_segment_assets.h"
IMPLEMENTATION = ROOT / "src" / "seven_segment_assets.cpp"
VIEWBOX = (63.20982360839844, -64.79800033569335,
           74.10757446289062, 132.7838325500488)
# SVG subpaths are bottom, lower-left, lower-right, middle, top,
# upper-left, upper-right. Reorder them to A, B, C, D, E, F, G.
SUBPATH_TO_SEGMENT = (4, 6, 2, 0, 1, 5, 3)
SIZES = {"large": (148, 342), "small": (112, 221), "top": (60, 104)}
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
    if len(polygons) != 7:
        raise RuntimeError(f"Expected 7 SVG subpaths, found {len(polygons)}")
    return [polygons[index] for index in SUBPATH_TO_SEGMENT]


def rasterize(points, width, height):
    view_x, view_y, view_width, view_height = VIEWBOX
    canvas = Image.new("L", (width * SUPERSAMPLE, height * SUPERSAMPLE), 0)
    polygon = [
        ((x - view_x) / view_width * width * SUPERSAMPLE,
         (y - view_y) / view_height * height * SUPERSAMPLE)
        for x, y in points
    ]
    ImageDraw.Draw(canvas).polygon(polygon, fill=255)
    canvas = canvas.resize((width, height), Image.Resampling.LANCZOS)
    bounds = canvas.getbbox()
    if bounds is None:
        raise RuntimeError("Generated an empty segment")
    left, top, right, bottom = bounds
    cropped = canvas.crop(bounds)
    return left, top, cropped


def bytes_literal(data):
    rows = []
    values = list(data)
    for index in range(0, len(values), 20):
        rows.append("    " + ", ".join(str(value) for value in values[index:index + 20]))
    return ",\n".join(rows)


def main():
    polygons = read_subpaths()
    declarations = []
    definitions = []
    tables = []

    for size_name, (width, height) in SIZES.items():
        table_entries = []
        for segment_index, polygon in enumerate(polygons):
            left, top, image = rasterize(polygon, width, height)
            symbol = f"segment_{size_name}_{segment_index}"
            data_symbol = f"{symbol}_data"
            pixels = image.tobytes()
            declarations.append(f"extern const lv_img_dsc_t {symbol};")
            definitions.append(
                f"const uint8_t {data_symbol}[] = {{\n{bytes_literal(pixels)}\n}};\n\n"
                f"const lv_img_dsc_t {symbol} = {{\n"
                f"    .header = {{.cf = LV_IMG_CF_ALPHA_8BIT, .always_zero = 0, "
                f".reserved = 0, .w = {image.width}, .h = {image.height}}},\n"
                f"    .data_size = sizeof({data_symbol}),\n"
                f"    .data = {data_symbol},\n"
                f"}};"
            )
            table_entries.append(f"    {{&{symbol}, {left}, {top}}}")
        tables.append(
            f"const SevenSegmentAsset sevenSegmentAssets{size_name.title()}[7] = {{\n"
            + ",\n".join(table_entries) + "\n};"
        )

    HEADER.write_text(
        "#pragma once\n\n#include <lvgl.h>\n\n"
        "struct SevenSegmentAsset {\n"
        "  const lv_img_dsc_t *image;\n"
        "  lv_coord_t x;\n"
        "  lv_coord_t y;\n"
        "};\n\n"
        + "\n".join(declarations)
        + "\n\n"
        + "\n".join(
            f"extern const SevenSegmentAsset sevenSegmentAssets{name.title()}[7];"
            for name in SIZES
        )
        + "\n"
    )
    IMPLEMENTATION.write_text(
        '#include "seven_segment_assets.h"\n\n'
        + "\n\n".join(definitions)
        + "\n\n" + "\n\n".join(tables) + "\n"
    )


if __name__ == "__main__":
    main()
