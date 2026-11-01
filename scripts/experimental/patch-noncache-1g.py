#!/usr/bin/env python3
"""EXPERIMENTAL (failed): make arch_phys_to_noncache accept DRAM addresses.

Kept for the record.  This patch swaps the compare operands at 0x6011f370
(cmp r1,r2 -> cmp r2,r1) so the helper accepts PAs below end-of-DRAM and
returns the linear alias PA+0x60000000 instead of NULL.

It makes upm.ko load with a 1 GiB all-RAM ATAG, but the semantics are wrong:
the helper then reads RAM instead of the device window.  The working solution
is the memrsv=0x20000000@0x20000000:device command line (see DESIGN.md section 4).

Usage: patch-noncache-1g.py [src] [dst]
"""
import hashlib
import os
import shutil
import struct
import sys

D = os.path.expanduser("~/firmware/a7m3")
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(D, "vmlinux-console.bin")
DST = sys.argv[2] if len(sys.argv) > 2 else os.path.join(D, "vmlinux-noncache.bin")

BASE = 0x60108000
VA = 0x6011F370
ORIG = 0xE1510002   # cmp r1, r2
NEW = 0xE1520001    # cmp r2, r1


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
