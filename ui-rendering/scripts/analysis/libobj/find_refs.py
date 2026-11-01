import struct, sys
d = open('/home/derek/libObj.so', 'rb').read()
vals = [int(x, 16) for x in sys.argv[1:]]
for v in vals:
    pat = struct.pack('<I', v)
    offs = []
    i = d.find(pat)
    while i >= 0 and len(offs) < 40:
        offs.append(i)
        i = d.find(pat, i + 1)
    print('%08x refs:' % v, ' '.join('%x' % o for o in offs))
