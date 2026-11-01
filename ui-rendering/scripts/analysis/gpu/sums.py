import re, sys, collections
path = sys.argv[1]
txt = open(path, 'rb').read().decode('latin1', 'replace')
pat = re.compile(r'SUGI-2DS\[(\d+)\] addr=([0-9a-f]+) n=(\d+):([0-9a-f ]+)')
seen = collections.OrderedDict()
for m in pat.finditer(txt):
    addr = m.group(2)
    words = [int(w, 16) for w in m.group(4).split()]
    if addr not in seen:
        seen[addr] = words[:8]
for addr, words in seen.items():
    print('%s: %s' % (addr, ' '.join('%08x' % w for w in words)))
print('unique', len(seen))
