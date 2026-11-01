#!/usr/bin/env python3
"""Draw the captured Sugilite 2D draw list over the final software-rendered
frame, so each visible region can be traced back to a command stream record.

Input:
  --err   QEMU stderr log with SUGI-SD lines (A7M3_SUGI_EXEC=1)
  --raw   frame_XXXX.raw (640x480 RGBA4444, 2 bytes/px)
  --out   PNG path
  --list  optional text table output
"""
import argparse, re, struct
from PIL import Image, ImageDraw, ImageFont

SD = re.compile(r'SUGI-SD\[(\d+)\] surf=([0-9a-f]+) tex=([0-9a-f]+) (\d+)x(\d+) '
                r'r47=([0-9a-f]+) r48=([0-9a-f]+) col=([0-9a-f]+)')


def load_frame(path, w=640, h=480):
    d = open(path, 'rb').read()
    im = Image.new('RGB', (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            v = struct.unpack_from('<H', d, (y * w + x) * 2)[0]
            px[x, y] = ((v >> 12) & 15) * 17, ((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17
    return im


def transform(r47, r48, w=640, h=480):
    x0 = struct.unpack('<h', struct.pack('<H', r47 & 0xffff))[0]
    y0 = struct.unpack('<h', struct.pack('<H', r47 >> 16))[0]
    x1 = struct.unpack('<h', struct.pack('<H', r48 & 0xffff))[0]
    y1 = struct.unpack('<h', struct.pack('<H', r48 >> 16))[0]
    return ((x0 + w) >> 1, (y0 + h) >> 1, (x1 + w) >> 1, (y1 + h) >> 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--err', required=True)
    ap.add_argument('--raw', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--list')
    a = ap.parse_args()
    draws = []
    for line in open(a.err, errors='replace'):
        m = SD.search(line)
        if not m:
            continue
        idx, surf, tex, tw, th, r47, r48, col = m.groups()
        rect = transform(int(r47, 16), int(r48, 16))
        draws.append((int(idx), int(surf, 16), int(tex, 16), int(tw), int(th), rect, int(col, 16)))
    im = load_frame(a.raw).convert('RGB')
    dr = ImageDraw.Draw(im)
    for idx, surf, tex, tw, th, (x0, y0, x1, y1), col in draws:
        if x1 <= x0 or y1 <= y0:
            continue
        dr.rectangle([x0, y0, x1 - 1, y1 - 1], outline=(0, 255, 0))
        dr.text((x0 + 1, y0), str(idx), fill=(255, 255, 0))
    im.save(a.out)
    if a.list:
        with open(a.list, 'w') as f:
            f.write('idx surf tex texW texH screen_x0 screen_y0 screen_x1 screen_y1\n')
            for idx, surf, tex, tw, th, (x0, y0, x1, y1), col in draws:
                f.write('%d %08x %08x %d %d %d %d %d %d\n' % (
                    idx, surf, tex, tw, th, x0, y0, x1, y1))
    print('draws', len(draws), '->', a.out)


if __name__ == '__main__':
    main()
