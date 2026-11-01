import re, sys, collections
path = sys.argv[1]
txt = open(path, 'rb').read().decode('latin1', 'replace')
pat = re.compile(r'SUGI-2D\[(\d+)\] c00=([0-9a-f]+) c04=([0-9a-f]+) c08=([0-9a-f]+) c0c=([0-9a-f]+) c10=([0-9a-f]+) c20=([0-9a-f]+) c24=([0-9a-f]+) c28=([0-9a-f]+)')
agg = collections.defaultdict(lambda: [0, 0])
for m in pat.finditer(txt):
    flags = int(m.group(6), 16); dst = int(m.group(3), 16) << 2
    n = int(m.group(7), 16)
    agg[dst][0] += 1
    agg[dst][1] += n
for dst, (cnt, tot) in sorted(agg.items(), key=lambda kv: -kv[1][1])[:20]:
    print('dst=%08x jobs=%d bytes=%d (0x%x)' % (dst, cnt, tot, tot))
print('total dsts', len(agg))
