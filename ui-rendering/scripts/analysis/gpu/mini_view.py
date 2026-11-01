import os
import sys
import numpy as np
from PIL import Image

# Usage: mini_view.py [raw-file] [out-dir]
# Defaults match the development host.
raw_path = sys.argv[1] if len(sys.argv) > 1 else '/tmp/a7m3-pm-mini.raw'
outdir = sys.argv[2] if len(sys.argv) > 2 else '/tmp'
d = open(raw_path, 'rb').read()
base = 0x22770000

def render(pa, w, h):
    off = pa - base
    raw = d[off:off + w * h * 2]
    a = np.frombuffer(raw, dtype='<u2').astype(np.uint32).reshape(h, w)
    variants = {}
    variants['rgba4444'] = np.stack([((a >> 12) & 0xF) * 17, ((a >> 8) & 0xF) * 17,
                                     ((a >> 4) & 0xF) * 17], axis=2)
    variants['argb4444'] = np.stack([((a >> 8) & 0xF) * 17, ((a >> 4) & 0xF) * 17,
                                     (a & 0xF) * 17], axis=2)
    variants['rgb565'] = np.stack([((a >> 11) & 0x1F) << 3, ((a >> 5) & 0x3F) << 2,
                                   (a & 0x1F) << 3], axis=2)
    name = os.path.join(outdir, 'tex%x' % pa)
    for k, v in variants.items():
        img = Image.fromarray(v.astype(np.uint8), 'RGB')
        img = img.resize((w * 4, h * 4), Image.NEAREST)
        img.save(name + '-' + k + '.png')
    print('saved', name, w, h)

# 0x22772380 is the orange brick/pattern tile (56x48).
# 0x22773900 is a 64x56 UI element (mostly black with gray bars).
render(0x22772380, 56, 48)
render(0x22773900, 64, 56)
render(0x22775580, 16, 16)
