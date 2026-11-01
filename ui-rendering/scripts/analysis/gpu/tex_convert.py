#!/usr/bin/env python3
"""Convert a draw-time texture dump (tex_<idx>_<w>x<h>.raw) to PNG.

Usage:
  tex_convert.py <raw> <out.png> [--fmt rgba4444|argb4444|a8|rgb565|argb1555]
                 [--scale N] [--bg RRGGBB]

The raw file is what the QEMU Sugilite model wrote via A7M3_TEX_DIR; its real
format is still unknown.  This tool exists so the candidate interpretations can
be compared side by side instead of guessed one at a time.
"""
import argparse, os, re, struct
from PIL import Image


def parse_size(path):
    m = re.search(r'_(\d+)x(\d+)\.raw$', os.path.basename(path))
    if not m:
        raise SystemExit('cannot parse WxH from filename')
    return int(m.group(1)), int(m.group(2))


def read_px(d, i, fmt):
    if fmt == 'a8':
        v = d[i] if i < len(d) else 0
        return v, v, v, 255
    if i * 2 + 2 > len(d):
        return 0, 0, 0, 0
    v = struct.unpack_from('<H', d, i * 2)[0]
    if fmt == 'rgba4444':
        a, r, g, b = v & 15, (v >> 12) & 15, (v >> 8) & 15, (v >> 4) & 15
        return r * 17, g * 17, b * 17, a * 17
    if fmt == 'argb4444':
        a, r, g, b = (v >> 12) & 15, (v >> 8) & 15, (v >> 4) & 15, v & 15
        return r * 17, g * 17, b * 17, a * 17
    if fmt == 'rgb565':
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        return r * 255 // 31, g * 255 // 63, b * 255 // 31, 255
    if fmt == 'argb1555':
        a, r, g, b = (v >> 15) & 1, (v >> 10) & 31, (v >> 5) & 31, v & 31
        return r * 255 // 31, g * 255 // 31, b * 255 // 31, 255 if a else 0
    raise SystemExit('unknown format ' + fmt)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('raw')
    ap.add_argument('out')
    ap.add_argument('--fmt', default='rgba4444')
    ap.add_argument('--scale', type=int, default=4)
    ap.add_argument('--bg', default='000000')
    a = ap.parse_args()
    w, h = parse_size(a.raw)
    d = open(a.raw, 'rb').read()
    bg = tuple(int(a.bg[i:i + 2], 16) for i in (0, 2, 4))
    im = Image.new('RGBA', (w, h), bg + (255,))
    px = im.load()
    for y in range(h):
        for x in range(w):
            r, g, b, al = read_px(d, y * w + x, a.fmt)
            px[x, y] = (r, g, b, al if al else 255)
    if a.scale > 1:
        im = im.resize((w * a.scale, h * a.scale), Image.NEAREST)
    im.convert('RGB').save(a.out)
    print('wrote', a.out, w, h, a.fmt)


if __name__ == '__main__':
    main()
