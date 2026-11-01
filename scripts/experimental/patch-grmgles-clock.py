#!/usr/bin/env python3
"""EXPERIMENTAL: let grm_gles.ko init continue past the Sugilite clock BUG.

The original module BUGs at sugi_hw.c:221 when the GPU clock register never
reads 0x15.  This patch:

  - restores the original init_module prologue (removing the "return 0" stub),
    so the module actually registers its character device;
  - changes sugilite_clock_up_internal's final check
        cmp r0, #0x15 ; ldmeq ...
    to
        mov r0, #0    ; ldm ...
    so the function returns success after its register writes.

With the module's init running, it registers the cdev for "dmpgles2"
(major 248, minor 29 from udif_devnodes).  A /dev node is not created by the
module; see add-dmpgles2-node.py.

This is a hardware-free workaround, not a GPU model.  It may cause further
hardware accesses to fail; use only in a disposable QEMU image.

Usage: patch-grmgles-clock.py [--lba N] <eMMC image>
"""
import argparse
import os
import struct
import sys

BOOT_OFF = 0x80000
FIRST_BLOCK = 173897          # first ext2 data block of grm_gles.ko
BLOCK_SIZE = 1024

INIT_OFF = 0x1944
INIT_ORIG = bytes.fromhex("0dc0a0e1f0d82de9")
INIT_STUB = bytes.fromhex("0000a0e31eff2fe1")

CMP_OFF = 0x310               # .text+0x2dc
CMP_ORIG = 0xE3500015         # cmp r0, #0x15
CMP_NEW = 0xE3A00000          # mov r0, #0
LDM_OFF = 0x314               # .text+0x2e0
LDM_ORIG = 0x089DA800         # ldmeq sp, {fp, sp, pc}
LDM_NEW = 0xE89DA800          # ldm sp, {fp, sp, pc}


def u32(buf, off):
    return struct.unpack_from("<I", buf, off)[0]


def find_logical_lba(path, partno):
    with open(path, "rb") as f:
        f.seek(BOOT_OFF)
        mbr = f.read(512)
        if len(mbr) != 512 or mbr[510:512] != b"\x55\xaa":
            sys.exit("no MBR signature at 0x%x" % (BOOT_OFF + 510))
        ext_base = None
        for i in range(4):
            e = mbr[446 + i * 16:446 + (i + 1) * 16]
            if e[4] in (0x05, 0x0F, 0x85):
                ext_base = u32(e, 8)
        if not ext_base:
            sys.exit("no extended partition in MBR")
        idx = 5
        ebr_abs = ext_base
        seen = set()
        while ebr_abs and ebr_abs not in seen:
            seen.add(ebr_abs)
            f.seek(BOOT_OFF + ebr_abs * 512)
            ebr = f.read(512)
            if len(ebr) != 512 or ebr[510:512] != b"\x55\xaa":
                sys.exit("bad EBR at lba %d" % ebr_abs)
            e = ebr[446:462]
            if u32(e, 8) == 0 and u32(e, 12) == 0:
                break
            if idx == partno:
                return ebr_abs + u32(e, 8)
            idx += 1
            rel = u32(ebr[446 + 16:446 + 32], 8)
            ebr_abs = ext_base + rel if rel else 0
    sys.exit("logical partition %d not found" % partno)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--lba", type=lambda s: int(s, 0),
                    help="nflasha15 start LBA (default: read from MBR)")
    args = ap.parse_args()

    if not os.path.isfile(args.image):
        sys.exit("no such image: %s" % args.image)
    lba = args.lba if args.lba is not None else find_logical_lba(args.image, 15)
    base = BOOT_OFF + lba * 512

    def off(fileoff):
        return base + FIRST_BLOCK * BLOCK_SIZE + fileoff

    with open(args.image, "r+b") as f:
        size = os.path.getsize(args.image)
        for fileoff in (INIT_OFF, CMP_OFF, LDM_OFF):
            assert off(fileoff) + 4 <= size
        f.seek(off(INIT_OFF)); init = f.read(8)
        f.seek(off(CMP_OFF)); cmp_w = struct.unpack("<I", f.read(4))[0]
        f.seek(off(LDM_OFF)); ldm_w = struct.unpack("<I", f.read(4))[0]
        print("init:", init.hex(), "cmp:", hex(cmp_w), "ldm:", hex(ldm_w))
        if init not in (INIT_ORIG, INIT_STUB):
            sys.exit("unexpected init_module bytes")
        if (cmp_w, ldm_w) not in ((CMP_ORIG, LDM_ORIG), (CMP_NEW, LDM_NEW)):
            sys.exit("unexpected clock function bytes")

        f.seek(off(INIT_OFF)); f.write(INIT_ORIG)
        f.seek(off(CMP_OFF)); f.write(struct.pack("<I", CMP_NEW))
        f.seek(off(LDM_OFF)); f.write(struct.pack("<I", LDM_NEW))
        f.flush()

        f.seek(off(INIT_OFF)); init = f.read(8)
        f.seek(off(CMP_OFF)); cmp_w = struct.unpack("<I", f.read(4))[0]
        f.seek(off(LDM_OFF)); ldm_w = struct.unpack("<I", f.read(4))[0]
        print("new init:", init.hex(), "cmp:", hex(cmp_w), "ldm:", hex(ldm_w))
        if init != INIT_ORIG or cmp_w != CMP_NEW or ldm_w != LDM_NEW:
            sys.exit("read-back mismatch")
    print("patched nflasha15 lba", lba)


if __name__ == "__main__":
    main()
