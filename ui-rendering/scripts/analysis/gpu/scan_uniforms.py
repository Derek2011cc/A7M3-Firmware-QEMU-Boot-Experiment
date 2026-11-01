#!/usr/bin/env python3
import struct
from collections import Counter
def scan(path, base, size, label):
    d=open(path,'rb').read()
    hits=[]
    for i in range(0,len(d)-24,4):
        w=struct.unpack_from('<I',d,i)[0]
        if w==0x0450f026:
            sub,=struct.unpack_from('<I',d,i+4)
            vals=struct.unpack_from('<4f',d,i+8)
            pa=base+i
            hits.append((pa,sub,vals))
    print(f"== {label}: {len(hits)} uniform blocks ==")
    # print blocks whose floats are mostly in [0,1] and not all equal
    for pa,sub,v in hits[:80]:
        if all(-1e-6<=x<=1.000001 for x in v) and len(set(round(x,6) for x in v))>1:
            print(f"{pa:08x} sub={sub} {v}")
scan('/tmp/a7m3-pm-gpu2.raw',0x20530000,0x40000,'gpu2')
scan('/tmp/a7m3-pm-gpu1.raw',0x2055f000,0x40000,'gpu1')
