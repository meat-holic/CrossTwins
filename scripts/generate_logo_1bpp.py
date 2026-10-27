#!/usr/bin/env python3
"""Render the approved embedded-PNG SVG into the firmware's 1bpp logo."""

from __future__ import annotations

import argparse
import base64
import binascii
import io
import math
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SVG_PATH = ROOT / "src" / "images" / "logo.svg"
PNG_PATH = ROOT / "src" / "images" / "Logo120.png"
HEADER_PATH = ROOT / "src" / "images" / "Logo120.h"
SIZE = 120
THRESHOLD = 128
X_LINK_HREF = "{http://www.w3.org/1999/xlink}href"


def rounded(value: float) -> int:
    return int(math.floor(value + 0.5))


def render() -> Image.Image:
    svg_text = SVG_PATH.read_text(encoding="utf-8")
    if "data:img/png" in svg_text:
        raise ValueError("logo.svg contains the non-standard data:img/png URI")
    root = ET.fromstring(svg_text)
    view_box = [float(value) for value in root.attrib.get("viewBox", "").replace(",", " ").split()]
    if len(view_box) != 4 or view_box[2] <= 0 or view_box[3] <= 0:
        raise ValueError("logo.svg must have a positive four-value viewBox")

    images = [element for element in root.iter() if element.tag.rsplit("}", 1)[-1] == "image"]
    if len(images) != 1:
        raise ValueError("logo.svg must contain exactly one embedded image")
    image_element = images[0]
    data_uri = image_element.attrib.get(X_LINK_HREF, image_element.attrib.get("href", ""))
    match = re.fullmatch(r"data:image/png;base64,([A-Za-z0-9+/=\s]+)", data_uri)
    if not match:
        raise ValueError("logo.svg must contain a normalized base64 PNG data URI")
    try:
        png_data = base64.b64decode(match.group(1), validate=True)
    except binascii.Error as error:
        raise ValueError("logo.svg contains invalid PNG base64") from error

    with Image.open(io.BytesIO(png_data)) as embedded:
        source = embedded.convert("RGBA")
    view_x, view_y, view_width, view_height = view_box
    scale = min(SIZE / view_width, SIZE / view_height)
    viewport_x = (SIZE - view_width * scale) / 2
    viewport_y = (SIZE - view_height * scale) / 2
    box_x = viewport_x + (float(image_element.attrib.get("x", "0")) - view_x) * scale
    box_y = viewport_y + (float(image_element.attrib.get("y", "0")) - view_y) * scale
    box_width = float(image_element.attrib["width"]) * scale
    box_height = float(image_element.attrib["height"]) * scale

    image_scale = min(box_width / source.width, box_height / source.height)
    rendered_width = max(1, rounded(source.width * image_scale))
    rendered_height = max(1, rounded(source.height * image_scale))
    resized = source.resize((rendered_width, rendered_height), Image.Resampling.LANCZOS)
    x = rounded(box_x + (box_width - rendered_width) / 2)
    y = rounded(box_y + (box_height - rendered_height) / 2)

    canvas = Image.new("RGBA", (SIZE, SIZE), (255, 255, 255, 255))
    canvas.alpha_composite(resized, (x, y))
    grayscale = canvas.convert("L")
    return grayscale.point(lambda value: 255 if value >= THRESHOLD else 0, mode="L")


def header_content(image: Image.Image) -> str:
    pixels = image.load()
    packed: list[int] = []
    for y in range(SIZE):
        for x in range(0, SIZE, 8):
            value = 0
            for bit_index in range(8):
                if pixels[x + bit_index, y] >= THRESHOLD:
                    value |= 1 << (7 - bit_index)
            packed.append(value)

    lines = [
        "#pragma once",
        "#include <cstdint>",
        "",
        "// Image dimensions: 120x120; 1bpp, MSB-first, 1=white and 0=black.",
        "// clang-format off",
        "static const uint8_t Logo120[] = {",
    ]
    for index in range(0, len(packed), 16):
        row = ", ".join(f"0x{value:02X}" for value in packed[index : index + 16])
        lines.append(f"    {row},")
    lines.extend(["};", "// clang-format on", ""])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="verify the committed PNG and C array")
    args = parser.parse_args()
    try:
        image = render()
        expected_header = header_content(image)
        if args.check:
            if not PNG_PATH.exists() or not HEADER_PATH.exists():
                raise ValueError("generated logo PNG or header is missing")
            with Image.open(PNG_PATH) as committed_png:
                actual = committed_png.convert("L")
                if actual.size != image.size or actual.tobytes() != image.tobytes():
                    raise ValueError("Logo120.png pixels do not match the approved SVG")
            if HEADER_PATH.read_text(encoding="utf-8") != expected_header:
                raise ValueError("Logo120.h is stale or does not match the approved SVG")
            print("Logo SVG, PNG, and 1bpp firmware bitmap match")
            return 0

        image.save(PNG_PATH, format="PNG", optimize=False, compress_level=9)
        HEADER_PATH.write_text(expected_header, encoding="utf-8", newline="\n")
        print(f"Wrote {PNG_PATH.relative_to(ROOT)} and {HEADER_PATH.relative_to(ROOT)}")
        return 0
    except (OSError, ValueError, ET.ParseError) as error:
        print(f"logo conversion failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
