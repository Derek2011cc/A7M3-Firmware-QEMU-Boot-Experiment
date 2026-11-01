import sys, os
from PIL import Image
import numpy as np

src = sys.argv[1]
out = sys.argv[2]
W, H = 640, 480
d = open(src, 'rb').read()[:W * H * 2]
a = np.frombuffer(d, dtype='<u2').astype(np.uint32)
r = (a >> 12) & 0xF
g = (a >> 8) & 0xF
b = (a >> 4) & 0xF
al = a & 0xF
rgb = np.stack([r, g, b], axis=1).reshape(H, W, 3).astype(np.uint8) * 17
Image.fromarray(rgb, 'RGB').save(out + '-rgba4444.png')
# ARGB4444 (alpha high)
r2 = (a >> 8) & 0xF
g2 = (a >> 4) & 0xF
b2 = a & 0xF
rgb2 = np.stack([r2, g2, b2], axis=1).reshape(H, W, 3).astype(np.uint8) * 17
Image.fromarray(rgb2, 'RGB').save(out + '-argb4444.png')
# RGB565
r3 = (a >> 11) & 0x1F
g3 = (a >> 5) & 0x3F
b3 = a & 0x1F
rgb3 = np.stack([r3 << 3, g3 << 2, b3 << 3], axis=1).reshape(H, W, 3).astype(np.uint8)
Image.fromarray(rgb3, 'RGB').save(out + '-rgb565.png')
# RGBA8888
d4 = open(src, 'rb').read()[:W * H * 4]
if len(d4) >= W * H * 4:
    b4 = np.frombuffer(d4, dtype=np.uint8).reshape(H, W, 4)
    Image.fromarray(b4[:, :, [0, 1, 2]], 'RGB').save(out + '-rgba8888.png')
print('saved', out)
