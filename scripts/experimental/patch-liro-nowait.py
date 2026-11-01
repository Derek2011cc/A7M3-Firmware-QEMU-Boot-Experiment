#!/usr/bin/env python3
"""Experimental (negative result): skip one LIRO firmware wait loop.

LIRO's entry_main busy-waits on bit 1 of 0xf2a1102c:
    0x63a1d28e  ldr r0, [0xf2a1102c]
    0x63a1d290  bl  read helper
    0x63a1d294  tst.w r0, #2
    0x63a1d298  beq 0x63a1d28e

QEMU does not model the 0xf2a1xxxx block, so the read returns 0 and the
firmware spins in an RT thread, starving Linux.  This script NOPs the
branch (file offset 0x457298, Thumb bytes f9 d0 -> 00 bf).

Only one wait is skipped.  entry_main returns, but the system still goes
silent, so this is a diagnostic step, not a fix.  Use only with QEMU on
your own firmware copy; never flash the patched image to a camera.

Usage:
    python3 patch-liro-nowait.py <av-cam.bin> [output.bin]
"""
import hashlib
import shutil
import sys

EXPECTED_SHA256 = "e1b0f84e8323005a34b43de6dd0083fca712d2c654f0d9716512e0a54631bbe7"
EXPECTED_SIZE = 18_449_784
BASE_VA = 0x635C6000
PATCH_VA = 0x63A1D298
OFFSET = PATCH_VA - BASE_VA  # 0x457298
OLD = bytes.fromhex("f9d0")  # Thumb BEQ
NEW = bytes.fromhex("00bf")  # Thumb NOP


def fail(msg):
    print("error: " + msg, file=sys.stderr)
    raise SystemExit(1)


def main():
    if len(sys.argv) < 2:
        fail("usage: patch-liro-nowait.py <av-cam.bin> [output.bin]")
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else src + ".nowait1.bin"

    data = bytearray(open(src, "rb").read())
    if len(data) != EXPECTED_SIZE:
        fail("size mismatch: got %d, expected %d" % (len(data), EXPECTED_SIZE))
    sha = hashlib.sha256(data).hexdigest()
    if sha != EXPECTED_SHA256:
        fail("SHA-256 mismatch: got %s, expected %s (4.03 av-cam.bin)"
             % (sha, EXPECTED_SHA256))
    if bytes(data[OFFSET:OFFSET + 2]) != OLD:
        fail("bytes at 0x%x are %s, expected %s"
             % (OFFSET, bytes(data[OFFSET:OFFSET + 2]).hex(), OLD.hex()))

    shutil.copyfile(src, dst)
    out = bytearray(open(dst, "rb").read())
    out[OFFSET:OFFSET + 2] = NEW
    open(dst, "wb").write(out)

    check = open(dst, "rb").read()
    if check[OFFSET:OFFSET + 2] != NEW:
        fail("read-back failed")
    print("patched %s -> %s" % (src, dst))
    print("VA 0x%x (file 0x%x): %s -> %s (BEQ -> NOP)"
          % (PATCH_VA, OFFSET, OLD.hex(), NEW.hex()))
    print("output size %d sha256 %s"
          % (len(check), hashlib.sha256(check).hexdigest()))
    print("note: negative experiment; entry_main returns but the system still stalls")


if __name__ == "__main__":
    main()
