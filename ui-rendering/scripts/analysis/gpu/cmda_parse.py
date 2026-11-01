import re, sys
path = sys.argv[1]
limit = int(sys.argv[2]) if len(sys.argv) > 2 else 6
txt = open(path, 'rb').read().decode('latin1', 'replace')
pat = re.compile(r'SUGI-CMDA\[(\d+)\] addr=([0-9a-f]+):([0-9a-f ]+)')
shown = 0
for m in pat.finditer(txt):
    if shown >= limit:
        break
    idx = int(m.group(1)); addr = m.group(2)
    words = [int(w, 16) for w in m.group(3).split()]
    print('=== CMDA[%d] addr=%s nwords=%d' % (idx, addr, len(words)))
    i = 0
    while i < len(words):
        w = words[i]
        if w == 0 and all(x == 0 for x in words[i:]):
            print('   ... zeros')
            break
        if (w & 0xffff) == 0xf000:
            op = w >> 16
            val = words[i+1] if i+1 < len(words) else 0
            print('   %04x = %08x' % (op, val))
            i += 2
        else:
            print('   raw  %08x' % w)
            i += 1
    shown += 1
