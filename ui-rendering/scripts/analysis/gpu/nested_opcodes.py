#!/usr/bin/env python3
import struct
d=open('/tmp/a7m3-pm-gpu2.raw','rb').read(); base=0x20530000
def words(pa,n):
    o=pa-base
    return [struct.unpack_from('<I',d,o+4*i)[0] for i in range(n)]
def scan(pa,count,label):
    ws=words(pa,min(count,2000))
    ops={}
    for w in ws:
        if (w&0xffff)==0xf000:
            op=w>>16; ops[op]=ops.get(op,0)+1
    print(f"{label} @{pa:08x} n={count}: ops " + ' '.join(f"{k:04x}x{v}" for k,v in sorted(ops.items())[:30]))
scan(0x20556210,0x229,'list A')
scan(0x205564b0,0xf9,'list B')
scan(0x20556460,0x29,'list C')
scan(0x205565d0,0x19,'list D')
scan(0x2053a4b0,0x81,'list E')
