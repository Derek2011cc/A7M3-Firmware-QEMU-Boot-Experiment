import struct, sys
IMG = sys.argv[1] if len(sys.argv) > 1 else '/home/derek/firmware/a7m3/qemu-emmc-2g.sen.img'
OFF = 0xb780000
SS = 512; SPC = 32; RESV = 1; NFATS = 1; ROOTENTS = 512; FATSZ = 3; TOT = 24576
ROOT_OFF = OFF + (RESV + NFATS * FATSZ) * SS
DATA_SECT = RESV + NFATS * FATSZ + (ROOTENTS * 32) // SS
DATA_OFF = OFF + DATA_SECT * SS
f = open(IMG, 'rb')
f.seek(ROOT_OFF)
root = f.read(ROOTENTS * 32)
entries = []
lfn = ''
for i in range(ROOTENTS):
    e = root[i*32:(i+1)*32]
    if len(e) < 32:
        break
    if e[0] == 0:
        break
    if e[0] == 0xE5:
        continue
    attr = e[11]
    if attr == 0x0F:
        part = e[1:11] + e[14:26] + e[28:32]
        s = ''
        for j in range(0, len(part), 2):
            c = part[j] | (part[j+1] << 8)
            if c == 0 or c == 0xFFFF:
                break
            s += chr(c)
        lfn = s + lfn
        continue
    name = e[0:8].decode('latin1').rstrip() + '.' + e[8:11].decode('latin1').rstrip()
    size = struct.unpack('<I', e[28:32])[0]
    cl = struct.unpack('<H', e[26:28])[0]
    entries.append((name, lfn, attr, size, cl))
    lfn = ''
for name, lf, attr, size, cl in entries:
    print('%-14s attr=%02x size=%-7d cluster=%-5d lfn=%s' % (name, attr, size, cl, lf))
