#!/bin/sh
# Build the guest-side plugins used by the A7M3 QEMU UI experiments.
# The plugins are freestanding ARM32 shared objects: they use raw syscalls and
# must not link against libc (the guest's glibc is old and the plugin lives
# inside im.elf's address space).
#
# Usage: ./build-plugins.sh [output-dir]
# Requires: clang with the ARM target and lld.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
out=${1:-"$here/../../build"}

CC=${CC:-clang}
CFLAGS="--target=arm-linux-gnueabi -mfloat-abi=soft -fPIC -shared -nostdlib -fuse-ld=lld -O2"

mkdir -p "$out"

build() {
    src=$1
    dst=$2
    echo "CC $src -> $dst"
    $CC $CFLAGS -o "$out/$dst" "$here/$src"
}

build scn_openview.c vopen.so
build scn_ui.c       vui.so
build scn_hook.c     vhk2.so
build scn_mapk.c     vmapk.so
build scn_maps.c     vmaps.so
build scn_defs.c     vdefs.so
build scn_osd.c      vosd.so
build scn_gi.c       vgi.so
build scn_grm.c      vgrm.so
build scn_grm2.c     vgrm2.so
build scn_tex.c      vtex.so
build scn_pg.c       vpg.so
build shim_uipc.c    shim_uipc.so
build shim_io.c      shim_io.so

echo "done: $out"
