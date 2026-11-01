import sys
import numpy as np
from PIL import Image

path = sys.argv[1]
out = sys.argv[2]
d = open(path, 'rb').read()
# first decode slot at offset 0x200, stride 0x2000, tile 64x64 RGBA4444
TW = TH = 64
slot_off = 0x200
stride = 0x2000
tiles = []
off = slot_off
while off + TW * TH * 2 <= len(d):
    a = np.frombuffer(d[off:off + TW * TH * 2], dtype='<u2').astype(np.uint32)
    tiles.append(a.reshape(TH, TW))
    off += stride
print('tiles', len(tiles))
if not tiles:
    sys.exit(1)
cols = 8
rows = (len(tiles) + cols - 1) // cols
def grid(fmt):
    img = np.zeros((rows * TH, cols * TW, 3), np.uint8)
    for i, a in enumerate(tiles):
        r, c = divmod(i, cols)
        if fmt == 'rgba4444':
            rr = ((a >> 12) & 0xF) * 17
            gg = ((a >> 8) & 0xF) * 17
            bb = ((a >> 4) & 0xF) * 17
        elif fmt == 'argb4444':
            rr = ((a >> 8) & 0xF) * 17
            gg = ((a >> 4) & 0xF) * 17
            bb = (a & 0xF) * 17
        else:
            rr = ((a >> 11) & 0x1F) << 3
            gg = ((a >> 5) & 0x3F) << 2
            bb = (a & 0x1F) << 3
        img[r*TH:(r+1)*TH, c*TW:(c+1)*TW] = np.stack([rr, gg, bb], axis=2).astype(np.uint8)
    Image.fromarray(img, 'RGB').save(out + '-' + fmt + '.png')
for fmt in ('rgba4444', 'argb4444', 'rgb565'):
    grid(fmt)
print('saved', out)
