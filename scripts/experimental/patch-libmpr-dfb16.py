#!/usr/bin/env python3
"""Experimental: redirect libmpr's DFB open path to /dev/nflasha16.

The A7M3 synthetic eMMC image exposes nflasha4 as a one-sector extended
container.  libmpr's DFB init opens /dev/nflasha4; DFA::FlashWrite's lseek
then fails, and the error path locks the MemInfo mutex a second time, which
trips glibc's pthread_mutex_lock assert.  Redirecting the open path to
/dev/nflasha16 (a 140 MiB zero partition) avoids that crash and lets
im.elf run.

This script patches an *extracted copy* of libmpr.so.  It does not touch a
camera and does not modify any eMMC image.  The expected input is the 4.03
runtime libmpr (extracted from nflasha15:/lib/libmpr.so), SHA-256 below.

Offsets/bytes are version-specific; the script verifies all of them and
refuses to write on mismatch.

Usage:
    python3 patch-libmpr-dfb16.py <libmpr.so> [output.so]
"""
import hashlib
import shutil
import sys

EXPECTED_SHA256 = "ab1df757108d123f403188e33b732bf0c8cded33ff29711417dca14c5ab883a5"
EXPECTED_SIZE = 12_516_284

# The string cave is 15 zero bytes at 0xbe54e0; "/dev/nflasha16\0" is
# exactly 15 bytes, so the following byte stays zero.
STRING_OFFSET = 0xBE54E0
STRING_OLD = b"\x00" * 15
STRING_NEW = b"/dev/nflasha16\x00"
assert len(STRING_NEW) == 15

# Two literal pointers to the old "/dev/nflasha4" string (0xbe5498) are
# changed to point at the new string (0xbe54e0).
LITERAL_OFFSETS = (0x338E14, 0x33A858)
LITERAL_OLD = bytes.fromhex("9854be00")
LITERAL_NEW = bytes.fromhex("e054be00")


def fail(msg):
    print("error: " + msg, file=sys.stderr)
    raise SystemExit(1)


def main():
    if len(sys.argv) < 2:
        fail("usage: patch-libmpr-dfb16.py <libmpr.so> [output.so]")
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else src + ".dfb16.so"

    data = bytearray(open(src, "rb").read())
    if len(data) != EXPECTED_SIZE:
        fail("size mismatch: got %d, expected %d" % (len(data), EXPECTED_SIZE))
    sha = hashlib.sha256(data).hexdigest()
    if sha != EXPECTED_SHA256:
        fail("SHA-256 mismatch: got %s, expected %s (4.03 runtime libmpr)"
             % (sha, EXPECTED_SHA256))

    if bytes(data[STRING_OFFSET:STRING_OFFSET + len(STRING_OLD)]) != STRING_OLD:
        fail("string cave at 0x%x is not zero-filled" % STRING_OFFSET)
    for off in LITERAL_OFFSETS:
        if bytes(data[off:off + len(LITERAL_OLD)]) != LITERAL_OLD:
            fail("literal at 0x%x is %s, expected %s"
                 % (off, bytes(data[off:off + 4]).hex(), LITERAL_OLD.hex()))

    shutil.copyfile(src, dst)
    out = bytearray(open(dst, "rb").read())
    out[STRING_OFFSET:STRING_OFFSET + len(STRING_NEW)] = STRING_NEW
    for off in LITERAL_OFFSETS:
        out[off:off + len(LITERAL_NEW)] = LITERAL_NEW
    open(dst, "wb").write(out)

    # Read back and verify.
    check = open(dst, "rb").read()
    if check[STRING_OFFSET:STRING_OFFSET + 15] != STRING_NEW:
        fail("read-back of new string failed")
    for off in LITERAL_OFFSETS:
        if check[off:off + 4] != LITERAL_NEW:
            fail("read-back of literal at 0x%x failed" % off)
    print("patched %s -> %s" % (src, dst))
    print("new string @0x%x: %r" % (STRING_OFFSET, STRING_NEW))
    print("literals @%s -> %s" % (
        ", ".join("0x%x" % o for o in LITERAL_OFFSETS), LITERAL_NEW.hex()))
    print("output size %d sha256 %s"
          % (len(check), hashlib.sha256(check).hexdigest()))
    print("note: put this file back into your own eMMC image; do not publish it")


if __name__ == "__main__":
    main()
