import re, sys, collections
path = sys.argv[1]
txt = open(path, 'rb').read().decode('latin1', 'replace')
pat = re.compile(r'SUGI-2D\[(\d+)\] c00=([0-9a-f]+) c04=([0-9a-f]+) c08=([0-9a-f]+) c0c=([0-9a-f]+) c10=([0-9a-f]+) c20=([0-9a-f]+) c24=([0-9a-f]+) c28=([0-9a-f]+)')
srcs = collections.Counter(); dsts = collections.Counter()
for m in pat.finditer(txt):
    srcs[m.group(2)] += 1
    dsts[m.group(3)] += 1
print('unique src(reg)=%d dst(reg)=%d' % (len(srcs), len(dsts)))
def inrange(reg, lo, hi):
    a = int(reg, 16) << 2
    return lo <= a < hi
for name, s in (('src', srcs), ('dst', dsts)):
    hits = [(k, v, int(k, 16) << 2) for k, v in s.items() if 0x04000000 <= (int(k, 16) << 2) < 0x04800000]
    print(name, 'in 0x04000000-0x04800000:', len(hits))
    for k, v, a in hits[:20]:
        print('  ', k, 'count', v, 'pa', hex(a))
print('dst sample:', [hex(int(k,16)<<2) for k, _ in list(dsts.items())[:10]])
