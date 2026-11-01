import struct
data = open('/home/derek/libObj.so', 'rb').read()

def ror32(v, r):
    r &= 31
    return ((v >> r) | (v << (32 - r))) & 0xffffffff if r else v & 0xffffffff

def arm_imm(w):
    return ror32(w & 0xff, ((w >> 8) & 0xf) * 2)

targets = {0x126af2c: "GRM_gpermFillrect", 0x126c460: "GRM_gpermRectblit"}
found = {}
start, end = 0xffc24, 0xffc24 + 0x7de8
for a in range(start, end - 12, 4):
    w0, w1, w2 = struct.unpack_from("<III", data, a)
    if (w0 & 0xFFFFF000) != 0xE28FC000:
        continue
    if (w1 & 0xFFFFF000) != 0xE28CC000:
        continue
    if (w2 & 0xFFFFF000) != 0xE5BCF000:
        continue
    imm2 = w2 & 0xfff
    if not (w2 & 0x00800000):
        imm2 = -imm2
    slot = a + 8 + arm_imm(w0) + arm_imm(w1) + imm2
    if slot in targets:
        found[a] = (slot, targets[slot])
for a, (slot, name) in sorted(found.items()):
    print("plt %08x -> slot %08x %s" % (a, slot, name))
