#!/bin/sh
# cam-shell (Linux/WSL) — run as root, or install udev rules for the camera.
#   sudo ./cam-shell.sh raw
#   sudo ./cam-shell.sh cmd "uname -a"
#   sudo ./cam-shell.sh push local /tmp/remote
here=$(cd "$(dirname "$0")" && pwd)
exec python3 "$here/cam_shell.py" "$@"
