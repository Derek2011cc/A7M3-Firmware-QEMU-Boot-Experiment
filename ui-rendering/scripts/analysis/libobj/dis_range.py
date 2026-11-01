import sys
from capstone import *
data = open('/home/derek/libObj.so', 'rb').read()
lo = int(sys.argv[1], 16)
hi = int(sys.argv[2], 16)
mode = CS_MODE_THUMB
if len(sys.argv) > 3 and sys.argv[3] == 'arm':
    mode = CS_MODE_ARM
md = Cs(CS_ARCH_ARM, mode)
md.detail = False
code = data[lo:hi]
for i in md.disasm(code, lo):
    print("%08x: %-10s %s" % (i.address, i.mnemonic, i.op_str))
