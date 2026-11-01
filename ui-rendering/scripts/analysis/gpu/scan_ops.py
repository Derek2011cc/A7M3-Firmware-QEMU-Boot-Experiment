import re, sys
path = sys.argv[1]
want = set(int(x, 16) for x in sys.argv[2:]) if len(sys.argv) > 2 else None
txt = open(path, 'rb').read().decode('latin1', 'replace')
pat = re.compile(r'SUGI-CMDA\[(\d+)\] addr=([0-9a-f]+):([0-9a-f ]+)')
n = 0
for m in pat.finditer(txt):
    idx = int(m.group(1)); addr = m.group(2)
    words = [int(w, 16) for w in m.group(3).split()]
    pairs = []
    i = 0
    while i + 1 < len(words):
        w = words[i]
        if (w & 0xffff) == 0xf000:
            op = w >> 16
            val = words[i+1]
            if want is None or op in want:
                pairs.append('%04x=%08x' % (op, val))
            i += 2
        else:
            i += 1
    n += 1
    print('CMDA[%d] %s :: %s' % (idx, addr, ' '.join(pairs)))
print('jobs', n)
