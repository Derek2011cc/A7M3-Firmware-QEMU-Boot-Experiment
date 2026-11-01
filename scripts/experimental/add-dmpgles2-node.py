#!/usr/bin/env python3
"""EXPERIMENTAL: add /dev/dmpgles2 (char 248:29) to nflasha7 in the eMMC image.

grm_gles.ko registers a cdev using the "dmpgles2" entry from udif_devnodes
(major 248, minor 29), but it does not create a /dev node.  The camera rootfs
/dev is a static ext2 directory (devtmpfs is not mounted), so the node must be
added offline for Appframework to open it.

The script reads the nflasha7 partition from the image, runs debugfs mknod on
a temporary copy, and writes it back.  Only the QEMU image is modified.

Usage: add-dmpgles2-node.py [--lba N] <eMMC image>
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile

BOOT_OFF = 0x80000
NODE = "/dev/dmpgles2"
MAJOR = 248
MINOR = 29


def u32(buf, off):
    return struct.unpack_from("<I", buf, off)[0]


def find_logical(path, partno):
    """Return (start_lba, sectors) of logical partition `partno`."""
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
            rel_start = u32(e, 8)
            sectors = u32(e, 12)
            if rel_start == 0 and sectors == 0:
                break
            if idx == partno:
                return ebr_abs + rel_start, sectors
            idx += 1
            rel_next = u32(ebr[446 + 16:446 + 32], 8)
            ebr_abs = ext_base + rel_next if rel_next else 0
    sys.exit("logical partition %d not found" % partno)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--lba", type=lambda s: int(s, 0),
                    help="nflasha7 start LBA (default: read from MBR)")
    args = ap.parse_args()

    if not os.path.isfile(args.image):
        sys.exit("no such image: %s" % args.image)
    start, sectors = (args.lba, 16384) if args.lba is not None \
        else find_logical(args.image, 7)
    size = sectors * 512
    off = BOOT_OFF + start * 512
    print("nflasha7 lba %d, %d sectors (%d bytes), image offset 0x%x"
          % (start, sectors, size, off))

    fd, tmp = tempfile.mkstemp(prefix="nflasha7-", suffix=".bin")
    os.close(fd)
    try:
        with open(args.image, "rb") as f:
            f.seek(off)
            data = f.read(size)
        if len(data) != size:
            sys.exit("short read from image")
        with open(tmp, "wb") as f:
            f.write(data)

        # Check first: debugfs mknod does not reliably fail on an existing
        # name and can allocate a duplicate inode.
        p = subprocess.run(["debugfs", "-R", "ls -l /dev", tmp],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True)
        if "dmpgles2" in p.stdout:
            print("node already present; nothing to do")
        else:
            cmd = ["debugfs", "-w", "-R",
                   "mknod %s c %d %d" % (NODE, MAJOR, MINOR), tmp]
            p = subprocess.run(cmd, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True)
            print(p.stdout.strip())
            if p.returncode != 0:
                sys.exit("debugfs mknod failed")

            p = subprocess.run(["debugfs", "-R", "ls -l /dev", tmp],
                               stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True)
            if "dmpgles2" not in p.stdout:
                sys.exit("node not found after mknod")

            with open(args.image, "r+b") as f:
                f.seek(off)
                f.write(data)
                f.flush()
    finally:
        os.unlink(tmp)
    print("added %s (%d:%d) to image" % (NODE, MAJOR, MINOR))


if __name__ == "__main__":
    main()
