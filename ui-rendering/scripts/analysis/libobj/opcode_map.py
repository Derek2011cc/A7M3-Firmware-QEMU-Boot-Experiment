#!/usr/bin/env python3
from capstone import *
import struct, re
d=open('/home/derek/libObj.so','rb').read()
md=Cs(CS_ARCH_ARM, CS_MODE_THUMB|CS_MODE_LITTLE_ENDIAN); md.skipdata=True
base=0x107a10; size=0xc26bc8; text=d[base:base+size]
def sx(v,b): m=1<<(b-1); return (v^m)-m
calls=[]
for off in range(0,len(text)-3,2):
    hw1,hw2=struct.unpack_from('<HH',text,off)
    if (hw1&0xF800)!=0xF000 or (hw2&0xD000)!=0xD000: continue
    S=(hw1>>10)&1; imm10=hw1&0x3FF
    J1=(hw2>>13)&1; J2=(hw2>>11)&1; imm11=hw2&0x7FF
    I1=1-(J1^S); I2=1-(J2^S)
    imm=(S<<24)|(I1<<23)|(I2<<22)|(imm10<<12)|(imm11<<1)
    imm=sx(imm,25); addr=base+off
    if addr+4+imm==0x796560:
        calls.append(addr)
print("emit callers:",len(calls))
def last_r1_imm(call):
    start=max(0,call-48)
    ins=list(md.disasm(d[start:call],start))
    val=None; how=''
    for i in ins:
        s=i.mnemonic+' '+i.op_str
        m=re.match(r'(movs?|mov\.w|movw)\s+r1,\s*#(0x[0-9a-f]+|\d+)',s)
        if m:
            val=int(m.group(2),0); how=s
        # movw/movt pair: movw r1,#lo ; movt r1,#hi
    return val,how,ins[-3:] if ins else []
rows=[]
for c in calls:
    v,how,ctx=last_r1_imm(c)
    rows.append((v,c,how,ctx))
rows.sort(key=lambda x:(x[0] is None, x[0] or 0))
for v,c,how,ctx in rows:
    op = ('0x%04x'%v) if v is not None else '????'
    line=' '.join(f"{i.mnemonic} {i.op_str}" for i in ctx)
    print(f"op={op:6s} call=0x{c:08x} via[{how}] ctx: {line}")
