import numpy as np
from PIL import Image

d = open('/tmp/a7m3-pm-tex228f.raw', 'rb').read()
base = 0x228f0000

def render(pa, w, h, name, fmt):
    off = pa - base
    if fmt in ('rgba8888', 'bgra8888'):
        need = w * h * 4
        raw = d[off:off + need]
        if len(raw) < need:
            raw = raw + b'\0' * (need - len(raw))
        a = np.frombuffer(raw, dtype=np.uint8).reshape(h, w, 4)
        rgb = a[:, :, [0, 1, 2]] if fmt == 'rgba8888' else a[:, :, [2, 1, 0]]
    else:
        need = w * h * 2
        raw = d[off:off + need]
        if len(raw) < need:
            raw = raw + b'\0' * (need - len(raw))
        a = np.frombuffer(raw, dtype='<u2').astype(np.uint32).reshape(h, w)
        if fmt == 'rgba4444':
            rgb = np.stack([((a >> 12) & 0xF) * 17, ((a >> 8) & 0xF) * 17,
                            ((a >> 4) & 0xF) * 17], axis=2)
        else:
            rgb = np.stack([((a >> 11) & 0x1F) << 3, ((a >> 5) & 0x3F) << 2,
                            (a & 0x1F) << 3], axis=2)
    img = Image.fromarray(rgb.astype(np.uint8), 'RGB')
    s = 3 if w > 100 else 6
    img = img.resize((w * s, h * s), Image.NEAREST)
    img.save(name)

for pa, w, h, tag in ((0x228f6580, 296, 48, 'a'), (0x228f9d80, 152, 40, 'b'), (0x228fb580, 56, 48, 'c')):
    for fmt in ('rgba8888', 'bgra8888', 'rgba4444', 'rgb565'):
        render(pa, w, h, '/mnt/f/a7c2-re-main/a7m3-qemu-logs/tex8-%s-%s.png' % (tag, fmt), fmt)
print('done')
