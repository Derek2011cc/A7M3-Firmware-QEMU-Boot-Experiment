import sys, struct
data = open('/home/derek/libObj.so', 'rb').read()
targets = {}
for s in sys.argv[1:]:
    v = int(s, 16)
    targets[v] = targets.get(v, 0) + 1

def sext(v, bits):
    m = 1 << (bits - 1)
    return (v & (m - 1)) - (v & m)

hits = []
n = len(data) - 4
for a in range(0, n, 2):
    h1, h2 = struct.unpack_from("<HH", data, a)
    if (h1 & 0xF800) != 0xF000:
        continue
    bl = (h2 & 0xD000) == 0xD000
    blx = (h2 & 0xD000) == 0xC000
    if not (bl or blx):
        continue
    S = (h1 >> 10) & 1
    J1 = (h2 >> 13) & 1
    J2 = (h2 >> 11) & 1
    I1 = 1 ^ (J1 ^ S)
    I2 = 1 ^ (J2 ^ S)
    off = (S << 24) | (I1 << 23) | (I2 << 22) | ((h1 & 0x3FF) << 12) | ((h2 & 0x7FF) << 1)
    if S:
        off -= (1 << 25)
    tgt = (a + 4 + off) & 0xFFFFFFFF if bl else (((a + 4) & ~3) + off) & 0xFFFFFFFF
    if tgt in targets:
        hits.append((tgt, a))
for a in range(0, n, 4):
    w = struct.unpack_from("<I", data, a)[0]
    if (w >> 28) == 0xF or ((w >> 25) & 7) != 5:
        continue
    L = (w >> 24) & 1
    if L:
        tgt = (a + 8 + sext((w & 0xFFFFFF) << 2, 26)) & 0xFFFFFFFF
    else:
        tgt = (a + 8 + sext(((w & 0xFFFFFF) << 2) | ((w >> 23) & 2), 26)) & 0xFFFFFFFF
    if tgt in targets:
        hits.append((tgt, a))
for tgt, a in sorted(hits):
    print("%08x <- %08x" % (tgt, a))
print("total", len(hits))
