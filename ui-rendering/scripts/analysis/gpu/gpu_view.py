import sys, struct
path = '/tmp/a7m3-pm-gpu1.raw'
base = 0x2055f000
d = open(path, 'rb').read()
for pa in (0x2055f840, 0x2055f850, 0x2055f950, 0x2055fd10, 0x2055fe10, 0x2055f9a0):
    off = pa - base
    words = struct.unpack_from('<32I', d, off)
    print('%08x:' % pa, ' '.join('%08x' % w for w in words))
    vals = struct.unpack_from('<64H', d, off)
    print('   u16:', ' '.join('%04x' % w for w in vals[:32]))
    fl = struct.unpack_from('<32f', d, off)
    print('   f32:', ' '.join('%.3g' % w for w in fl[:16]))
