import re, sys, collections
err = sys.argv[1]
log = sys.argv[2]
et = open(err, 'rb').read().decode('latin1', 'replace')
lt = open(log, 'rb').read().decode('latin1', 'replace')
# ldec destinations
dests = set()
for m in re.finditer(r'LDEC> cmd=([0-9a-f]+) arg=([0-9a-f]+) b=((?: [0-9a-f]{8}){8})', lt):
    if int(m.group(1), 16) != 1:
        continue
    ws = [int(x, 16) for x in m.group(3).split()]
    if ws[3]:
        dests.add(ws[2])
# texture fields from command lists
vals = collections.Counter()
for m in re.finditer(r'SUGI-CMDA\[\d+\] addr=[0-9a-f]+:([0-9a-f ]+)', et):
    words = [int(w, 16) for w in m.group(1).split()]
    for i in range(len(words) - 1):
        if (words[i] & 0xffff) == 0xf000 and (words[i] >> 16) == 0x0589:
            vals[words[i+1]] += 1
print('texture fields', len(vals))
for v, c in sorted(vals.items()):
    a3 = v << 3
    a4 = v << 4
    a2 = v << 2
    hit = 'DEC' if a3 in dests else ''
    print('%08x count=%d  <<3=%08x %s  <<4=%08x  <<2=%08x' % (v, c, a3, hit, a4, a2))
