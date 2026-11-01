import re, sys
path = sys.argv[1]
want = set(int(x) for x in sys.argv[2:])
txt = open(path, 'rb').read().decode('latin1', 'replace')
pat = re.compile(r'SUGI-CMDA\[(\d+)\] addr=([0-9a-f]+):([0-9a-f ]+)')
for m in pat.finditer(txt):
    idx = int(m.group(1))
    if idx not in want:
        continue
    addr = m.group(2)
    words = [int(w, 16) for w in m.group(3).split()]
    print('=== CMDA[%d] addr=%s nwords=%d' % (idx, addr, len(words)))
    i = 0
    seq = 0
    while i < len(words):
        w = words[i]
        if w == 0 and all(x == 0 for x in words[i:]):
            print('   [%d] ... zeros' % seq)
            break
        if (w & 0xffff) == 0xf000:
            op = w >> 16
            val = words[i+1] if i+1 < len(words) else 0
            print('   [%d] %04x = %08x' % (seq, op, val))
            seq += 1
            i += 2
        else:
            print('   [%d] raw  %08x' % (seq, w))
            seq += 1
            i += 1
