import struct
d = open('/home/derek/libObj.so', 'rb').read()
lo, hi = 0x127d900, 0x127e200
for off in range(lo, hi, 16):
    w = struct.unpack_from('<4I', d, off)
    asc = ''.join(chr(c) if 32 <= c < 127 else '.' for c in d[off:off+16])
    print('%08x: %08x %08x %08x %08x  %s' % (off, w[0], w[1], w[2], w[3], asc))
