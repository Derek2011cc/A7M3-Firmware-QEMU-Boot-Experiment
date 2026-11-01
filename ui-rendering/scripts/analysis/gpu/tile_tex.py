import numpy as np
from PIL import Image

d = open('/tmp/a7m3-pm-tex228f.raw', 'rb').read()
base = 0x228f0000
pa = 0x228f6580
w, h = 296, 48
off = pa - base
raw = d[off:off + w * h * 2]
a = np.frombuffer(raw, dtype='<u2').astype(np.uint32)
pal = np.stack([((a >> 12) & 0xF) * 17, ((a >> 8) & 0xF) * 17, ((a >> 4) & 0xF) * 17], axis=1).astype(np.uint8)

def render(idx, name):
    img = pal[idx].reshape(h, w, 3)
    Image.fromarray(img, 'RGB').resize((w * 3, h * 3), Image.NEAREST).save(name)

# linear
render(np.arange(w * h), '/mnt/f/a7c2-re-main/a7m3-qemu-logs/tile-linear.png')

def tile_order(ts):
    idx = np.zeros((h, w), dtype=np.int64)
    tw = w // ts
    for y in range(h):
        for x in range(w):
            tx, ty = x // ts, y // ts
            src = (ty * tw + tx) * ts * ts + (y % ts) * ts + (x % ts)
            idx[y, x] = src
    return idx.ravel()

for ts in (2, 4, 8, 16):
    if w % ts == 0 and h % ts == 0:
        render(tile_order(ts), '/mnt/f/a7c2-re-main/a7m3-qemu-logs/tile-%dx%d.png' % (ts, ts))

# morton over padded power-of-two dims
W2, H2 = 512, 64
def morton(x, y):
    z = 0
    for i in range(10):
        z |= ((x >> i) & 1) << (2 * i)
        z |= ((y >> i) & 1) << (2 * i + 1)
    return z
idx = np.zeros((h, w), dtype=np.int64)
for y in range(h):
    for x in range(w):
        idx[y, x] = morton(x, y)
render(idx.ravel(), '/mnt/f/a7c2-re-main/a7m3-qemu-logs/tile-morton.png')
print('done')
