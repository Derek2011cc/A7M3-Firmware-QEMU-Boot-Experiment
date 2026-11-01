import struct
d = open('/home/derek/libObj.so', 'rb').read()
for name, val in [('0116f000', 0x0116f000), ('0117f000', 0x0117f000),
                  ('0010f000', 0x0010f000), ('0044f000', 0x0044f000),
                  ('0041f000', 0x0041f000), ('0042f000', 0x0042f000),
                  ('0043f000', 0x0043f000), ('0047f000', 0x0047f000),
                  ('0048f000', 0x0048f000), ('0049f000', 0x0049f000),
                  ('010bf000', 0x010bf000), ('0580f000', 0x0580f000)]:
    pat = struct.pack('<I', val)
    offs = []
    i = d.find(pat)
    while i >= 0 and len(offs) < 12:
        offs.append(i)
        i = d.find(pat, i + 1)
    print(name, ' '.join('%x' % o for o in offs))
