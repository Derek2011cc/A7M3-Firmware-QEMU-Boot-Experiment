import re, sys, collections
path = sys.argv[1]
txt = open(path, 'rb').read().decode('latin1', 'replace')
pat = re.compile(r'LDEC> cmd=([0-9a-f]+) arg=([0-9a-f]+) b=((?: [0-9a-f]{8}){8})')
rows = []
for m in pat.finditer(txt):
    cmd = int(m.group(1), 16)
    arg = m.group(2)
    ws = [int(x, 16) for x in m.group(3).split()]
    rows.append((cmd, arg, ws))
print('LDEC requests', len(rows))
ok = collections.Counter()
for cmd, arg, ws in rows:
    src, slen, dst, dlen = ws[0], ws[1], ws[2], ws[3]
    if slen and dlen:
        ok[(cmd, dst, dlen)] += 1
print('unique dst/len', len(ok))
for (cmd, dst, dlen), cnt in sorted(ok.items(), key=lambda kv: -kv[1])[:40]:
    kind = 'PA' if dst >= 0x10000000 else 'VA'
    print('cmd=%x dst=%08x len=%d (%s) count=%d' % (cmd, dst, dlen, kind, cnt))
# print first few full rows
for cmd, arg, ws in rows[:8]:
    print('row cmd=%x arg=%s ws=%s' % (cmd, arg, ' '.join('%08x' % w for w in ws)))
