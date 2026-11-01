import struct, sys
# Usage: fat12_cat.py [image] <8.3-name>
# With one argument, the default development image is used (legacy form).
DEFAULT_IMG = '/home/derek/firmware/a7m3/qemu-emmc-2g.sen.img'
args = sys.argv[1:]
if len(args) >= 2:
    IMG = args[0]
    want = args[1].upper()
elif len(args) == 1:
    IMG = DEFAULT_IMG
    want = args[0].upper()
else:
    raise SystemExit('usage: fat12_cat.py [image] <8.3-name>')
OFF = 0xb780000
SS = 512; SPC = 32; RESV = 1; NFATS = 1; ROOTENTS = 512; FATSZ = 3; TOT = 24576
ROOT_OFF = OFF + (RESV + NFATS * FATSZ) * SS
DATA_SECT = RESV + NFATS * FATSZ + (ROOTENTS * 32) // SS
DATA_OFF = OFF + DATA_SECT * SS
FAT_OFF = OFF + RESV * SS
f = open(IMG, 'rb')
def get12(i):
    f.seek(FAT_OFF + i * 3 // 2)
    b = f.read(2)
    v = b[0] | (b[1] << 8)
    return (v >> 4) if (i & 1) else (v & 0xfff)
f.seek(ROOT_OFF)
root = f.read(ROOTENTS * 32)
for i in range(ROOTENTS):
    e = root[i*32:(i+1)*32]
    if not e or e[0] == 0:
        break
    if e[0] == 0xE5 or e[11] == 0x0F:
        continue
    name = e[0:8].decode('latin1').rstrip() + '.' + e[8:11].decode('latin1').rstrip()
    if name != want:
        continue
    size = struct.unpack('<I', e[28:32])[0]
    cl = struct.unpack('<H', e[26:28])[0]
    out = bytearray()
    while len(out) < size and 2 <= cl < 0xff8:
        f.seek(DATA_OFF + (cl - 2) * SPC * SS)
        out += f.read(min(SPC * SS, size - len(out)))
        cl = get12(cl)
    sys.stdout.buffer.write(out)
    break
