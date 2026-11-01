import re, sys
sites = [int(s, 16) for s in sys.argv[1:]]
syms = []
for line in open('/home/derek/syms.txt'):
    m = re.match(r'\s*\d+:\s+([0-9a-f]{8})\s+(\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+\d+\s+(\S+)', line)
    if m:
        v = int(m.group(1), 16)
        syms.append((v, m.group(3), m.group(2), m.group(6)))
syms.sort()
for s in sites:
    best = None
    nxt = None
    for v, typ, size, name in syms:
        if v <= s:
            best = (v, typ, size, name)
        else:
            nxt = (v, typ, size, name)
            break
    if best:
        print("%08x: %08x (+0x%x) %s %s size=%s | next %08x %s" % (
            s, best[0], s - best[0], best[1], best[3], best[2],
            nxt[0] if nxt else 0, nxt[3] if nxt else ""))
    else:
        print("%08x: none" % s)
