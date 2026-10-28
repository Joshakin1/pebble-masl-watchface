"""Convert full-size team logos into Pebble bitmap resources.

Usage: python tools/convert_logos.py [input_dir]

Each <team>.png in input_dir (default: logos_src/, named by the team's short
name in lowercase, e.g. comets.png) becomes two images per screen shape in
resources/images/<shape>/:

  logo_<team>.png  the big logo shown when that team is selected
  half_<team>.png  the smaller logo used when two teams share the screen

Color shapes are snapped to Pebble's 64 colors with 4 levels of alpha, so
edges blend into whatever background the face draws. The bw shape (Pebble 2
and 2 Duo) is flattened onto white and dithered to black and white.

After adding a team, also add it to package.json (tools/gen_resources.py),
s_teams in src/c/main.c and src/pkjs/teams.js.
"""

import os
import sys

from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
OUT_DIR = os.path.join(ROOT, 'resources', 'images')

# (big box, half box) in pixels for each screen shape. Keep in sync with the
# layouts in src/c/main.c.
SHAPES = {
    'rect': ((128, 72), (64, 58)),     # basalt 144x168
    'bw': ((128, 72), (64, 58)),       # diorite, flint 144x168
    'chalk': ((112, 64), (62, 54)),    # chalk 180x180 round
    'emery': ((176, 100), (90, 84)),   # emery 200x228
    'gabbro': ((184, 104), (104, 92)), # gabbro 260x260 round
}

# Colors swapped so a logo stays readable on its team's dark background
# (color shapes only). Wave's lettering is black, Stars' is dark teal.
RECOLOR = {
    'wave': {(0, 0, 0): (255, 255, 255)},
    'stars': {(0, 85, 85): (255, 255, 255)},
}


# Gray levels for the bw shape: below BW_DARK is black, above BW_LIGHT white.
BW_DARK, BW_LIGHT = 90, 200


def snap(v):
    """Pebble color uses 2 bits per channel: 0x00, 0x55, 0xAA, 0xFF."""
    return min((0, 85, 170, 255), key=lambda c: abs(c - v))


def fit(im, box):
    im = im.convert('RGBA')
    im = im.crop(im.getbbox())
    im.thumbnail(box, Image.LANCZOS)
    return im


def to_color(im, recolor):
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            a = snap(a)
            if a == 0:
                px[x, y] = (0, 0, 0, 0)
                continue
            rgb = (snap(r), snap(g), snap(b))
            px[x, y] = recolor.get(rgb, rgb) + (a,)
    return im


def to_bw(im):
    """Three tones: dark -> black, light -> white, mid -> 50% checkerboard.
    Cleaner than error-diffusion dithering at these sizes."""
    flat = Image.new('RGBA', im.size, (255, 255, 255, 255))
    flat.alpha_composite(im)
    gray = flat.convert('L')
    out = Image.new('1', im.size, 1)
    src, dst = gray.load(), out.load()
    for y in range(im.height):
        for x in range(im.width):
            v = src[x, y]
            dst[x, y] = 0 if v < BW_DARK else 1 if v > BW_LIGHT else (x + y) % 2
    return out


def main(in_dir):
    for shape, boxes in SHAPES.items():
        os.makedirs(os.path.join(OUT_DIR, shape), exist_ok=True)
    for name in sorted(os.listdir(in_dir)):
        team, ext = os.path.splitext(name)
        if ext.lower() != '.png':
            continue
        src = Image.open(os.path.join(in_dir, name))
        for shape, boxes in SHAPES.items():
            for prefix, box in zip(('logo', 'half'), boxes):
                im = fit(src, box)
                im = to_bw(im) if shape == 'bw' else to_color(im, RECOLOR.get(team, {}))
                im.save(os.path.join(OUT_DIR, shape, '%s_%s.png' % (prefix, team)),
                        optimize=True)
        print(team)


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'logos_src'))
