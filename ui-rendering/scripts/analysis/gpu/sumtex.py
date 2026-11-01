import re, sys, collections
path = sys.argv[1]
txt = open(path, 'rb').read().decode('latin1', 'replace')
for tag in ('TEX', 'DST', 'SRC', 'PTR'):
    pat = re.compile(r'SUGI-%s\[(\d+)\] addr=([0-9a-f]+):([0-9a-f ]+)' % tag)
    groups = collections.defaultdict(lambda: [0, 0, None])
    for m in pat.finditer(txt):
        idx = int(m.group(1)); addr = m.group(2)
        words = [int(w, 16) for w in m.group(3).split()]
        g = groups[addr]
        g[0] += 1
        if any(words):
            g[1] += 1
            if g[2] is None:
                g[2] = words
    print('== %s addresses=%d' % (tag, len(groups)))
    for addr, (cnt, nz, sample) in sorted(groups.items()):
        s = ''
        if sample:
            s = ' firstnz=' + ' '.join('%08x' % w for w in sample[:8])
        print('  %s count=%d nonzero=%d%s' % (addr, cnt, nz, s))
