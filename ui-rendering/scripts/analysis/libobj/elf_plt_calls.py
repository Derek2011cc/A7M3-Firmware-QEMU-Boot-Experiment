import sys, re, struct, subprocess

path = sys.argv[1]
pats = sys.argv[2:]
data = open(path, 'rb').read()

def readelf(*args):
    return subprocess.run(['readelf'] + list(args), capture_output=True, text=True).stdout

# sections
secs = {}
for line in readelf('-SW', path).splitlines():
    m = re.match(r'\s*\[\s*\d+\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)', line)
    if m:
        secs[m.group(1)] = (int(m.group(4), 16), int(m.group(3), 16), int(m.group(5), 16))
print('sections:', {k: secs[k] for k in ('.plt', '.text', '.rel.plt', '.dynsym') if k in secs})

# relocations
slot_syms = {}
relname = '.rel.plt' if '.rel.plt' in secs else '.rel.dyn'
for line in readelf('-rW', path).splitlines():
    if 'JUMP_SLOT' not in line:
        continue
    parts = line.split()
    if len(parts) < 5:
        continue
    off = int(parts[0], 16)
    name = parts[4]
    if 'JUMP_SLOT' in parts[2]:
        name = parts[4]
    slot_syms[off] = name

want = {p: None for p in pats}
for off, nm in slot_syms.items():
    for p in pats:
        if p in nm:
            want[p] = off
print('slots:', {k: (hex(v) if v else None) for k, v in want.items()})

# PLT stubs
plt_off, plt_addr, plt_size = secs.get('.plt', (0, 0, 0))
def ror32(v, r):
    r &= 31
    return ((v >> r) | (v << (32 - r))) & 0xffffffff if r else v & 0xffffffff
def arm_imm(w):
    return ror32(w & 0xff, ((w >> 8) & 0xf) * 2)
stub_by_slot = {}
for a in range(plt_off, plt_off + plt_size - 12, 4):
    w0, w1, w2 = struct.unpack_from('<III', data, a)
    if (w0 & 0xFFFFF000) != 0xE28FC000:
        continue
    if (w1 & 0xFFFFF000) != 0xE28CC000:
        continue
    if (w2 & 0xFFFFF000) != 0xE5BCF000:
        continue
    vaddr = plt_addr + (a - plt_off)
    imm2 = w2 & 0xfff
    if not (w2 & 0x00800000):
        imm2 = -imm2
    slot = vaddr + 8 + arm_imm(w0) + arm_imm(w1) + imm2
    stub_by_slot[slot] = vaddr
stubs = {p: stub_by_slot.get(off) for p, off in want.items() if off}
print('stubs:', {k: (hex(v) if v else None) for k, v in stubs.items()})
if not stubs:
    sys.exit(0)

targets = {v: k for k, v in stubs.items()}
def sext(v, bits):
    m = 1 << (bits - 1)
    return (v & (m - 1)) - (v & m)

txt_off, txt_addr, txt_size = secs.get('.text', (0, 0, len(data)))
hits = []
end = txt_off + txt_size - 4
for a in range(txt_off, end, 2):
    if a & 1:
        continue
    h1, h2 = struct.unpack_from('<HH', data, a)
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
    va = txt_addr + (a - txt_off)
    tgt = (va + 4 + off) & 0xFFFFFFFF if bl else (((va + 4) & ~3) + off) & 0xFFFFFFFF
    if tgt in targets:
        hits.append((tgt, va))
for a in range(txt_off, end, 4):
    w = struct.unpack_from('<I', data, a)[0]
    if (w >> 28) == 0xF or ((w >> 25) & 7) != 5:
        continue
    va = txt_addr + (a - txt_off)
    L = (w >> 24) & 1
    if L:
        tgt = (va + 8 + sext((w & 0xFFFFFF) << 2, 26)) & 0xFFFFFFFF
    else:
        tgt = (va + 8 + sext(((w & 0xFFFFFF) << 2) | ((w >> 23) & 2), 26)) & 0xFFFFFFFF
    if tgt in targets:
        hits.append((tgt, va))
for tgt, a in sorted(hits):
    print('%08x <- %08x  (%s)' % (tgt, a, targets[tgt]))
print('total', len(hits))
