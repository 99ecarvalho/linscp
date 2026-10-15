#!/usr/bin/env python3
"""linscp's application icon, rendered from assets/app/linscp.svg.

  make_icon.py [--svg assets/app/linscp.svg] [--out-dir assets/app]
               [--ico port/qt/icons/linscp.ico]

The icon is linscp's own: an original tile around Lucide's ISC-licensed
"folder-sync" glyph (assets/app/SOURCES.md). WinSCP's application icon may
not be reused (its license.txt, part B), so nothing here derives from it.

Writes the multi-size .ico (Windows' executable, installer and window icon)
and the PNG sizes the Linux packages install (hicolor theme). Sizes up to
24 pixels are drawn with thicker strokes on a full-bleed tile, so they stay
legible. Needs rsvg-convert and Pillow; the outputs are committed.
"""
import argparse, io, os, re, subprocess, sys, tempfile

from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
ICO_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
PNG_SIZES = [16, 24, 32, 48, 64, 128, 256, 512]


def svg_for(svg, size):
    if size > 24:
        return svg
    # Small: the tile to the edge, the strokes heavier.
    svg = svg.replace('x="8" y="8" width="240" height="240" rx="52"', 'x="0" y="0" width="256" height="256" rx="44"')
    svg = svg.replace('stroke-width="2.1"', 'stroke-width="2.9"')
    return svg


def render(svg, size):
    with tempfile.NamedTemporaryFile('w', suffix='.svg', delete=False) as f:
        f.write(svg_for(svg, size))
        path = f.name
    try:
        png = subprocess.run(['rsvg-convert', '-w', str(size), '-h', str(size), path],
                             check=True, capture_output=True).stdout
    finally:
        os.unlink(path)
    return Image.open(io.BytesIO(png)).convert('RGBA')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--svg', default=os.path.join(ROOT, 'assets', 'app', 'linscp.svg'))
    ap.add_argument('--out-dir', default=os.path.join(ROOT, 'assets', 'app'))
    ap.add_argument('--ico', default=os.path.join(ROOT, 'port', 'qt', 'icons', 'linscp.ico'))
    a = ap.parse_args()
    svg = open(a.svg, encoding='utf-8').read()
    for size in PNG_SIZES:
        render(svg, size).save(os.path.join(a.out_dir, f'linscp-{size}.png'))
    images = [render(svg, size) for size in ICO_SIZES]
    images[-1].save(a.ico, format='ICO', sizes=[(s, s) for s in ICO_SIZES], append_images=images[:-1])
    print(f'{a.ico}: {len(ICO_SIZES)} sizes; {len(PNG_SIZES)} PNGs in {a.out_dir}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
