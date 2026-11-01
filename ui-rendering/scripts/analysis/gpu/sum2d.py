import re, sys, collections
path = sys.argv[1]
txt = open(path, 'rb').read().decode('latin1', 'replace')
pat = re.compile(r'SUGI-2D\[(\d+)\] c00=([0-9a-f]+) c04=([0-9a-f]+) c08=([0-9a-f]+) c0c=([0-9a-f]+) c10=([0-9a-f]+) c20=([0-9a-f]+) c24=([0-9a-f]+) c28=([0-9a-f]+)')
cnt = collections.Counter()
c20s = collections.Counter()
rows = []
for m in pat.finditer(txt):
    idx = int(m.group(1))
    c10 = int(m.group(6), 16)
    c20 = int(m.group(7), 16)
    cnt[c10] += 1
    c20s[(c10, c20)] += 1
    rows.append(m.groups())
print('jobs', len(rows))
print('c10 flags:', {hex(k): v for k, v in cnt.items()})
for (c10, c20), v in sorted(c20s.items()):
    print('  c10=%08x c20=%08x count=%d' % (c10, c20, v))
for r in rows[:8]:
    print(r)
