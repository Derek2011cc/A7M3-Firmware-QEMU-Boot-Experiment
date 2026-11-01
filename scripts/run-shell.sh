#!/bin/bash
# Boot the A7M3 firmware under QEMU with the serial console attached.
#
# What to expect:
#   - kernel boot, camera init mounts all nflasha partitions, loads modules;
#   - /usr/bin/im.elf fails (Sony Sugilite GPU is not emulated) and the
#     kernel enters Sony's exception monitor, showing "Exception>";
#   - type "exit" at that prompt ("Good Bye.") to get the camera rootfs
#     BusyBox shell: "/ #";
#   - the exception monitor also has a "shell" command for /bin/sh;
#   - quit QEMU with Ctrl-A X.
#
# Usage: run-shell.sh [data-dir]   (default: $HOME/firmware/a7m3)
set -euo pipefail

D="${1:-$HOME/firmware/a7m3}"
BUILD="${QEMU_BUILD:-$HOME/qemu-ma1co/build-arm/arm-softmmu}"
QEMU="$BUILD/qemu-system-arm"

for f in vmlinux-console.bin initrd-qemu.img qemu-emmc-2g.img; do
  if [ ! -f "$D/$f" ]; then
    echo "missing $D/$f (see BUILD.md)" >&2
    exit 1
  fi
done
if [ ! -x "$QEMU" ]; then
  echo "missing $QEMU; set QEMU_BUILD to your build-arm/arm-softmmu" >&2
  exit 1
fi

exec "$QEMU" -M cxd90045 \
  -kernel "$D/vmlinux-console.bin" \
  -initrd "$D/initrd-qemu.img" \
  -drive if=mtd,file="$D/qemu-emmc-2g.img",format=raw \
  -serial stdio -display none
