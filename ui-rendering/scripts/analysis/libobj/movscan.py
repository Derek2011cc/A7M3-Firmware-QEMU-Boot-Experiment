import struct
d = open('/home/derek/libObj.so', 'rb').read()
TEXT_OFF, TEXT_SIZE = 0x107a10, 0xc26bc8
items = []
for a in range(TEXT_OFF, TEXT_OFF + TEXT_SIZE - 4, 2):
    h1, h2 = struct.unpack_from('<HH', d, a)
    kind = None
    if (h1 & 0xFBF0) == 0xF240:
        kind = 'movw'
    elif (h1 & 0xFBF0) == 0xF2C0:
        kind = 'movt'
    if not kind:
        continue
    imm4 = h1 & 0xF
    i = (h1 >> 10) & 1
    imm3 = (h2 >> 12) & 7
    rd = (h2 >> 8) & 0xF
    imm8 = h2 & 0xFF
    imm16 = (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8
    items.append((a, kind, rd, imm16))
print('mov items', len(items))
targets = [0x0580, 0x0112, 0x0589, 0x0202, 0x0047, 0x0048, 0x0116, 0x0450, 0x0460]
# find movw #0xf000 then movt #<target> same rd nearby
pairs = []
for idx, (a, kind, rd, imm) in enumerate(items):
    if kind != 'movw' or imm != 0xF000:
        continue
    for j in range(idx + 1, min(idx + 12, len(items))):
        b, k2, rd2, imm2 = items[j]
        if k2 == 'movt' and rd2 == rd and imm2 in targets:
            pairs.append((a, imm2, rd, b))
            break
for a, tgt, rd, b in pairs:
    print('movw/movt %04x at %08x (movt %08x) rd=%d' % (tgt, a, b, rd))
print('pairs', len(pairs))
