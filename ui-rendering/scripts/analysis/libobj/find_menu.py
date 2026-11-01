import os
files = [
    '/home/derek/a7m3-viewUnified2.so',
    '/home/derek/a7m3-viewUnified3.so',
    '/home/derek/a7m3-viewUnified4.so',
    '/home/derek/a7m3-viewUnified5.so',
    '/home/derek/a7m3-viewUnified6.so',
    '/home/derek/a7m3-viewUnified7.so',
    '/home/derek/a7m3-viewUnified8.so',
    '/home/derek/libmpr.so',
    '/home/derek/libObj.so',
    '/home/derek/a7m3-im.elf',
]
for f in files:
    try:
        d = open(f, 'rb').read()
    except OSError as e:
        print(f, 'ERR', e)
        continue
    n = d.count(b'BASE_MENU')
    print('==', os.path.basename(f), 'size', len(d), 'BASE_MENU', n)
    idx = d.find(b'BASE_MENU')
    while idx >= 0 and n <= 8:
        s = max(0, idx - 32)
        raw = d[s:idx + 40]
        txt = ''.join(chr(c) if 32 <= c < 127 else '.' for c in raw)
        print('   off %x: %s' % (idx, txt))
        idx = d.find(b'BASE_MENU', idx + 1)
