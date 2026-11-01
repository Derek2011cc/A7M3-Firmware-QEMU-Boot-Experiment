import re
sites = [0x146206, 0x1607dc, 0x1f9c70, 0x61181e, 0x6136c4, 0x64efea, 0x64f0fe, 0x6c42be]
syms = []
for line in open('/home/derek/syms.txt'):
    m = re.match(r'\s*(\d+):\s+([0-9a-f]{8})\s+(\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+\d+\s+(\S+)', line)
    if m:
        v = int(m.group(2), 16)
        syms.append((v, m.group(4), m.group(3), m.group(7)))
        if m.group(1) == 0:
            print("header:", line.strip())
syms.sort()
for s in sites:
    best = None
    for v, typ, size, name in syms:
        if v <= s:
            best = (v, typ, size, name)
        else:
            break
    if best:
        v, typ, size, name = best
        print("%08x: near %08x (+0x%x) %s %s size=%s" % (s, v, s - v, typ, name, size))
    else:
        print("%08x: none" % s)
print("nsyms", len(syms))
