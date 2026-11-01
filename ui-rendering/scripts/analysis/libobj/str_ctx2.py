d = open('/home/derek/libObj.so', 'rb').read()
lo, hi = 0xeb9e00, 0xeba300
cur = []
out = []
for i in range(lo, hi):
    c = d[i]
    if 32 <= c < 127:
        cur.append(chr(c))
    else:
        if len(cur) >= 4:
            out.append('%08x: %s' % (i - len(cur), ''.join(cur)))
        cur = []
print('\n'.join(out))
