#!/usr/bin/env python3
"""linscp's icon: WinSCP's application icon with an "L" on its padlock.

  make_icon.py [--out port/qt/icons/linscp.ico]

Reads the pinned submodule's Application.ico as it is, draws the letter on
each of its sizes (supersampled, so small sizes stay clean), and writes a
multi-size .ico. Needs Pillow. The output is committed; rerun after a pin
bump that changes WinSCP's icon.
"""
import argparse, os, sys

from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
SOURCE = os.path.join(ROOT, '3rd-party', 'winscp', 'source', 'resource', 'Application.ico')

# The letter's two bars on the 256-pixel image: on the padlock's body, left
# of the green arrow.
BARS = [(84, 110, 102, 178), (84, 160, 134, 178)]
COLOUR = (52, 64, 78, 255)
SUPERSAMPLE = 4
# WinSCP draws its small sizes by hand, the padlock placed differently: the
# letter's bars there are whole pixels (inclusive), fitted to each.
PIXEL_BARS = {
    16: [(5, 8, 5, 11), (5, 11, 7, 11)],
    20: [(6, 9, 7, 14), (6, 13, 9, 14)],
    24: [(7, 11, 8, 17), (7, 16, 11, 17)],
    32: [(10, 16, 12, 23), (10, 21, 15, 23)],
}


def with_letter(image):
    size = image.size[0]
    if size in PIXEL_BARS:
        image = image.convert('RGBA')
        draw = ImageDraw.Draw(image)
        for bar in PIXEL_BARS[size]:
            draw.rectangle(bar, fill=COLOUR)
        return image
    scale = size * SUPERSAMPLE / 256.0
    layer = Image.new('RGBA', (size * SUPERSAMPLE, size * SUPERSAMPLE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    for left, top, right, bottom in BARS:
        draw.rectangle([round(left * scale), round(top * scale), round(right * scale) - 1, round(bottom * scale) - 1], fill=COLOUR)
    layer = layer.resize((size, size), Image.LANCZOS)
    return Image.alpha_composite(image.convert('RGBA'), layer)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=os.path.join(ROOT, 'port', 'qt', 'icons', 'linscp.ico'))
    a = ap.parse_args()
    source = Image.open(SOURCE)
    frames = []
    for size in sorted(source.info['sizes'], reverse=True):
        source.size = size
        source.load()
        frames.append(with_letter(source.copy()))
    frames[0].save(a.out, format='ICO', sizes=[f.size for f in frames], append_images=frames[1:])
    print(a.out, [f.size[0] for f in frames])
    return 0


if __name__ == '__main__':
    sys.exit(main())
