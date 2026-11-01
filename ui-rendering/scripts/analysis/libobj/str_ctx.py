import sys
path = sys.argv[1]
off = int(sys.argv[2], 16)
ln = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x400
d = open(path, 'rb').read()
s = max(0, off - ln)
e = min(len(d), off + ln)
chunk = d[s:e]
out = []
cur = []
for i, c in enumerate(chunk):
    if 32 <= c < 127:
        cur.append(chr(c))
    else:
        if len(cur) >= 4:
            out.append('%08x: %s' % (s + i - len(cur), ''.join(cur)))
        cur = []
if len(cur) >= 4:
    out.append('%08x: %s' % (s + len(chunk) - len(cur), ''.join(cur)))
print('\n'.join(out))
