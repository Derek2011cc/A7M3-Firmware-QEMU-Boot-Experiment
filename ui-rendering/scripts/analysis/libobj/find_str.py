import sys
files = [
    '/home/derek/libObj.so',
    '/home/derek/a7m3-viewUnified2.so',
    '/home/derek/libmpr-runtime.so',
    '/home/derek/a7m3-im.elf',
]
needle = sys.argv[1].encode() if len(sys.argv) > 1 else b'debugDumpScreen'
for f in files:
    try:
        d = open(f, 'rb').read()
    except OSError as e:
        print(f, 'ERR', e)
        continue
    offs = []
    i = d.find(needle)
    while i >= 0:
        offs.append(i)
        i = d.find(needle, i + 1)
    print(f, len(offs), ['%x' % o for o in offs])
