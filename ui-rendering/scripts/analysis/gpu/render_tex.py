import numpy as np
from PIL import Image
import sys
path = sys.argv[1]
base = int(sys.argv[2], 16)
pa = int(sys.argv[3], 16)
w = int(sys.argv[4]); h = int(sys.argv[5])
name = sys.argv[6]
d = open(path, 'rb').read()
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
for k, v in variants.items():
    img = Image.fromarray(v.astype(np.uint8), 'RGB').resize((w * 4, h * 4), Image.NEAREST)
    img.save(name + '-' + k + '.png')
print('saved', name)
