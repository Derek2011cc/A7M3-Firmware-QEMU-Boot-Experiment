import glob, os, sys
import numpy as np
from PIL import Image

indir = sys.argv[1] if len(sys.argv) > 1 else '/tmp/a7m3-frames'
outprefix = sys.argv[2] if len(sys.argv) > 2 else os.path.join(indir, 'menu')
files = sorted(glob.glob(os.path.join(indir, 'frame_*.raw')))
print('frames', len(files))
if not files:
    raise SystemExit(1)
W, H = 640, 480
imgs = []
for i, f in enumerate(files):
    d = open(f, 'rb').read()[:W * H * 2]
    a = np.frombuffer(d, dtype='<u2').astype(np.uint32).reshape(H, W)
    # RGBA4444 with alpha in the low nibble
    r = ((a >> 12) & 0xF) * 17
    g = ((a >> 8) & 0xF) * 17
    b = ((a >> 4) & 0xF) * 17
    rgb = np.stack([r, g, b], axis=2).astype(np.uint8)
    imgs.append(Image.fromarray(rgb, 'RGB'))
# contact sheet: 6x5 of every nth frame
step = max(1, len(imgs) // 30)
sel = imgs[::step][:30]
cols, rows = 6, 5
cell_w, cell_h = 213, 160  # 640/3, 480/3
sheet = Image.new('RGB', (cols * cell_w, rows * cell_h), (0, 0, 0))
for i, im in enumerate(sel):
    t = im.resize((cell_w, cell_h), Image.BILINEAR)
    sheet.paste(t, ((i % cols) * cell_w, (i // cols) * cell_h))
sheet.save(outprefix + '-contact-sheet.png')
# gif
gif_imgs = [im.resize((320, 240), Image.BILINEAR).convert('P', palette=Image.ADAPTIVE, colors=64) for im in imgs]
gif_imgs[0].save(outprefix + '-anim.gif', save_all=True,
                 append_images=gif_imgs[1:], duration=120, loop=0, optimize=True)
print('saved sheet and gif')
