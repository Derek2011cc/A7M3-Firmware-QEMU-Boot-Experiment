import struct,os,sys
# Usage: fat12_add2.py [image] [shim.so]
# Adds SHIM_IO.SO (the preload shim) and LDSOPRE.LOA (/etc/ld.so.preload) to the
# FAT12 partition of a *copy* of the eMMC image. The default shim is the payload
# in this repository; the default image is the base synthetic image.
HERE=os.path.dirname(os.path.abspath(__file__))
DEFAULT_IMG='/home/derek/firmware/a7m3/qemu-emmc-2g.img'
DEFAULT_SHIM=os.path.normpath(os.path.join(HERE,'..','..','payload','shim_uipc.so'))
IMG=sys.argv[1] if len(sys.argv)>1 else os.environ.get('FATIMG',DEFAULT_IMG)
OFF=0xb780000
SHIM=sys.argv[2] if len(sys.argv)>2 else os.environ.get('FATSHIM',DEFAULT_SHIM)
if not os.path.exists(IMG):
    raise SystemExit('image not found: '+IMG)
if not os.path.exists(SHIM):
    raise SystemExit('shim not found: '+SHIM)
print('WARNING: modifying in place:',IMG)
SS=512; SPC=32; RESV=1; NFATS=1; ROOTENTS=512; FATSZ=3; TOT=24576
DATA_SECT=RESV+NFATS*FATSZ+(ROOTENTS*32)//SS
DATA_OFF=OFF+DATA_SECT*SS
FAT_OFF=OFF+RESV*SS
ROOT_OFF=OFF+(RESV+NFATS*FATSZ)*SS
NCLUST=(TOT-DATA_SECT)//SPC
f=open(IMG,'r+b')
f.seek(FAT_OFF); fat=bytearray(f.read(FATSZ*SS))
def get12(i):
    o=i*3//2; v=fat[o]|(fat[o+1]<<8)
    return (v>>4) if (i&1) else (v&0xfff)
def set12(i,val):
    o=i*3//2
    if i&1:
        fat[o]=(fat[o]&0x0f)|((val&0x0f)<<4)
        fat[o+1]=(val>>4)&0xff
    else:
        fat[o]=val&0xff
        fat[o+1]=(fat[o+1]&0xf0)|((val>>8)&0x0f)
def alloc():
    for i in range(2,NCLUST+2):
        if get12(i)==0:
            set12(i,0xfff); return i
    raise SystemExit('no free cluster')
def curoff(c): return DATA_OFF+(c-2)*SPC*SS
def write_file(c,data):
    # write across a FAT chain; c is the first cluster
    off=0
    while off < len(data):
        f.seek(curoff(c))
        chunk=data[off:off+SPC*SS]
        f.write(chunk)
        if len(chunk) < SPC*SS:
            f.write(b'\0'*(SPC*SS-len(chunk)))
        off += SPC*SS
        c = get12(c)
        if off < len(data) and (c == 0 or c >= 0xff8):
            raise SystemExit('FAT chain ended early')
def alloc_chain(ncl):
    first=None; prev=None
    for _ in range(ncl):
        c=alloc()
        if prev is None: first=c
        else: set12(prev,c)
        prev=c
    return first
def csum(name11):
    s=0
    for b in name11:
        s=((s>>1)|((s&1)<<7))+b
    return s&0xff
def mk_lfn(name, short):
    u=name.encode('utf-16-le')
    chars=[u[i:i+2] for i in range(0,len(u),2)]
    e=bytearray(32)
    e[0]=0x41
    pos=[1,3,5,7,9,14,16,18,20,22,24,28,30]
    for j in range(13):
        if j<len(chars): e[pos[j]:pos[j]+2]=chars[j]
        else: e[pos[j]:pos[j]+2]=b'\xff\xff'
        if j==len(chars): e[pos[j]:pos[j]+2]=b'\x00\x00'
    e[11]=0x0f; e[13]=csum(short)
    return e
shim=open(SHIM,'rb').read()
print('shim size',len(shim))
f.seek(ROOT_OFF); root=bytearray(f.read(ROOTENTS*32))
def add(name, short, data, attr=0x20):
    ncl=(len(data)+SPC*SS-1)//(SPC*SS)
    c=alloc_chain(ncl); write_file(c,data)
    slot=None
    for i in range(ROOTENTS):
        if root[i*32]==0x00 or root[i*32]==0xE5:
            # need two consecutive free-ish slots
            if i+1<ROOTENTS and (root[(i+1)*32]==0x00 or root[(i+1)*32]==0xE5):
                slot=i; break
    if slot is None: raise SystemExit('no dir slot')
    root[slot*32:(slot+1)*32]=mk_lfn(name,short)
    se=bytearray(32); se[0:11]=short; se[11]=attr
    struct.pack_into('<H',se,26,c); struct.pack_into('<I',se,28,len(data))
    root[(slot+1)*32:(slot+2)*32]=se
    print(name,'cluster',c,'slot',slot,'size',len(data))
    return c
add('shim_io.so', b'SHIM_IO SO ', shim)
add('ld.so.preload', b'LDSOPRE LOA', b'/etc/shim_io.so\n')
set12(1,0xfff)
f.seek(FAT_OFF); f.write(fat)
f.seek(ROOT_OFF); f.write(root)
f.flush(); os.fsync(f.fileno()); f.close()
print('done')
