import re, sys
path = sys.argv[1]
txt = open(path, 'rb').read().decode('latin1', 'replace')
pat = re.compile(r'LDEC> cmd=([0-9a-f]+) arg=([0-9a-f]+) b=((?: [0-9a-f]{8}){8})')
rows = []
for m in pat.finditer(txt):
    ws = [int(x, 16) for x in m.group(3).split()]
    rows.append((int(m.group(1), 16), ws[0], ws[1], ws[2], ws[3]))
total = sum(r[4] for r in rows)
print('requests', len(rows), 'total dlen', total, hex(total))
minv = min(r[3] for r in rows); maxv = max(r[3] + r[4] for r in rows)
print('dst range %08x - %08x' % (minv, maxv))
sizes = {}
for r in rows:
    sizes[r[4]] = sizes.get(r[4], 0) + 1
print('len histogram', sizes)
outside = [r for r in rows if r[3] < 0x226e0000 or r[3] + r[4] > 0x22760000]
print('outside dump range', len(outside))
for r in outside[:10]:
    print('  dst=%08x len=%d' % (r[3], r[4]))
