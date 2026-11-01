#!/usr/bin/env python3
"""EXPERIMENTAL (failed): keep the device alias window through paging_init.

Kept for the record.  With a 512 MiB ATAG, paging_init clears
[VA(end_of_dram)=0x80000000, VMALLOC_START), wiping the bootloader/QEMU
device alias mapping at 0x80000000.  This patch changes the clear-start
calculation for the normal case from "r3 + 0x60000000" to the constant
0xa0000000, so only the area above the alias window is cleared.

It still did not produce a working arch_phys_to_noncache() mapping.  The
working solution is the memrsv=0x20000000@0x20000000:device command line with
a 1 GiB ATAG (see DESIGN.md section 4).

Usage: patch-window-512m.py [src] [dst]
"""
import hashlib
import os
import shutil
import struct
import sys

D = os.path.expanduser("~/firmware/a7m3")
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(D, "vmlinux-console.bin")
DST = sys.argv[2] if len(sys.argv) > 2 else os.path.join(D, "vmlinux-window.bin")

BASE = 0x60108000
VA = 0x6049CB78
ORIG = 0xA2833206   # addge r3, r3, #0x60000000
NEW = 0xA3A0320A    # movge r3, #0xa0000000


def main():
    if not os.path.isfile(SRC):
        sys.exit("no such file: %s" % SRC)
    shutil.copyfile(SRC, DST)
    data = bytearray(open(DST, "rb").read())
    off = VA - BASE
    old = struct.unpack_from("<I", data, off)[0]
    print("word @0x%x (VA 0x%08x): 0x%08x" % (off, VA, old))
    if old == NEW:
        print("already patched")
    elif old != ORIG:
        os.unlink(DST)
        sys.exit("unexpected instruction; refusing to patch")
    else:
        struct.pack_into("<I", data, off, NEW)
        open(DST, "wb").write(data)
        check = struct.unpack_from("<I", open(DST, "rb").read(), off)[0]
        if check != NEW:
            sys.exit("read-back mismatch")
        print("patched: 0x%08x -> 0x%08x" % (old, check))
    print("wrote %s (%d bytes)" % (DST, os.path.getsize(DST)))
    print("sha256 %s" % hashlib.sha256(open(DST, "rb").read()).hexdigest())


if __name__ == "__main__":
    main()
