#!/bin/bash
# Populate the local-only files/ directory from an A7M3 service-mode dump.
#
# Source:  $A7M3_DATA (default $HOME/firmware/a7m3)
# Dump:    $A7M3_DATA/service-dump/nflashaN.bin
# Output:  ./files (or --dest DIR)
#
# This script only reads the source dump. It does not touch the camera.
#
# Usage:
#   populate.sh [--dest DIR] [--dumps] [--image]
#
#   --dumps   also copy all nflasha*.bin (about 1.36 GB)
#   --image   also copy qemu-emmc-2g.img (about 2 GB)
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
SRC="${A7M3_DATA:-$HOME/firmware/a7m3}"
DUMP="$SRC/service-dump"
FAT="$HERE/tools/fat-extract.py"
DEST="$HERE/files"
WITH_DUMPS=0
WITH_IMAGE=0

usage() {
  sed -n '2,18p' "$0"
  exit "${1:-0}"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --dest)   DEST="$2"; shift 2 ;;
    --dumps)  WITH_DUMPS=1; shift ;;
    --image)  WITH_IMAGE=1; shift ;;
    -h|--help) usage 0 ;;
    *) echo "unknown argument: $1" >&2; usage 1 ;;
  esac
done

for f in nflasha1.bin nflasha2.bin nflasha3.bin nflasha7.bin nflasha15.bin; do
  [ -f "$DUMP/$f" ] || { echo "missing $DUMP/$f" >&2; exit 1; }
done
command -v python3 >/dev/null || { echo "python3 not found" >&2; exit 1; }
command -v debugfs >/dev/null || { echo "debugfs not found (e2fsprogs)" >&2; exit 1; }

mkdir -p "$DEST/extracted" "$DEST/patched" "$DEST/synthesized"
if [ "$WITH_DUMPS" = 1 ]; then mkdir -p "$DEST/service-dump"; fi

fat() { # partition guest-path output
  python3 "$FAT" "$DUMP/nflasha$1.bin" "$2" -o "$3" >/dev/null
}

dbg() { # image guest-path output
  debugfs -R "dump $2 $3" "$1" >/dev/null 2>&1 || {
    echo "warn: could not extract $2" >&2
    return 1
  }
}

echo "== kernel / initrd"
fat 3 VMLINUX.BIN "$DEST/extracted/vmlinux.bin"
fat 3 INITRD.IMG  "$DEST/extracted/initrd-4.03.img"
fat 1 BOOT/VMLINUX.BIN "$DEST/extracted/recovery-zImage.bin"
fat 1 BOOT/INITRD.IMG  "$DEST/extracted/recovery-cramfs.img"
fat 1 BOOT/KEMCO.TXT   "$DEST/extracted/KEMCO.TXT"
fat 1 BOOT/LOADADDR.TXT "$DEST/extracted/LOADADDR.TXT"
fat 2 UPDATER/DAT4     "$DEST/extracted/DAT4"
fat 2 BACKUP.BIN       "$DEST/extracted/BACKUP.BIN" || true
fat 2 kemco.txt        "$DEST/extracted/kemco-main.txt" || true
fat 3 AV-CAM.BIN       "$DEST/extracted/av-cam.bin" || true

echo "== rootfs key files"
dbg "$DUMP/nflasha7.bin" /sbin/init "$DEST/extracted/init" || true
dbg "$DUMP/nflasha7.bin" /bin/busybox "$DEST/extracted/busybox" || true
dbg "$DUMP/nflasha7.bin" /lib/libc-2.11.2.so "$DEST/extracted/libc-2.11.2.so" || true
dbg "$DUMP/nflasha7.bin" /version.txt "$DEST/extracted/version.txt" || true

echo "== /usr key files"
dbg "$DUMP/nflasha15.bin" /bin/im.elf "$DEST/extracted/im.elf" || true
dbg "$DUMP/nflasha15.bin" /bin/launch_shell.elf "$DEST/extracted/launch_shell.elf" || true
dbg "$DUMP/nflasha15.bin" /lib/libmpr.so "$DEST/extracted/libmpr.so" || true
for k in grm_gles grm_ma upm backup udate dmm; do
  dbg "$DUMP/nflasha15.bin" "/kmod/$k.ko" "$DEST/extracted/$k.ko" || true
done

echo "== early /kmod"
for k in nand lld dmac emmc ldec devno; do
  dbg "$DUMP/nflasha7.bin" "/kmod/$k.ko" "$DEST/extracted/$k.ko" || true
done

echo "== patched variants"
for f in vmlinux-pgdpatch.bin vmlinux-atagfix.bin vmlinux-console.bin \
         vmlinux-noncache.bin vmlinux-window.bin initrd-qemu.img; do
  if [ -f "$SRC/$f" ]; then
    cp -f "$SRC/$f" "$DEST/patched/$f"
  elif [ "$f" = initrd-qemu.img ] && [ -f "$SRC/initrd-backup1-skipudate.img" ]; then
    cp -f "$SRC/initrd-backup1-skipudate.img" "$DEST/patched/initrd-qemu.img"
  else
    echo "warn: $SRC/$f not present (run the patch scripts first)" >&2
  fi
done

echo "== synthesized metadata"
[ -f "$HERE/PARTITIONS.tsv" ] && cp -f "$HERE/PARTITIONS.tsv" "$DEST/synthesized/partitions.tsv"
if [ -f "$SRC/qemu-emmc-2g.img" ]; then
  dd if="$SRC/qemu-emmc-2g.img" bs=1 skip=$((0x80000)) count=512 \
     of="$DEST/synthesized/mbr.bin" status=none
fi
if [ "$WITH_IMAGE" = 1 ]; then
  if [ -f "$SRC/qemu-emmc-2g.img" ]; then
    cp -f "$SRC/qemu-emmc-2g.img" "$DEST/synthesized/qemu-emmc-2g.img"
  else
    echo "warn: --image requested but qemu-emmc-2g.img not found" >&2
  fi
fi

if [ "$WITH_DUMPS" = 1 ]; then
  echo "== copying service dump (about 1.36 GB)"
  cp -f "$DUMP"/nflasha*.bin "$DEST/service-dump/"
  [ -f "$DUMP/MANIFEST.md" ] && cp -f "$DUMP/MANIFEST.md" "$DEST/service-dump/MANIFEST.md"
fi

echo "== manifest"
(
  cd "$DEST"
  find . -type f ! -name MANIFEST.sha256 -print0 \
    | sort -z \
    | xargs -0 sha256sum > MANIFEST.sha256
)
echo "wrote $DEST/MANIFEST.sha256"
echo "done. Verify with: (cd \"$DEST\" && sha256sum -c MANIFEST.sha256)"
