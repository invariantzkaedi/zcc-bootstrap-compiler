#!/bin/sh
# Snapshot/rollback for the lab distro (run from Windows side via wsl.exe,
# or adapt to overlayfs inside the distro). UNVERIFIED in build container.
set -eu
STAMP=$(date -u +%Y%m%dT%H%M%SZ)
case "${1:-}" in
  snapshot) wsl.exe --export zkaedi-lab "H:/wsl/zkaedi-lab/snapshots/$STAMP.tar" ;;
  rollback)
    [ -n "${2:-}" ] || { echo "usage: snapshot_lab.sh rollback <snapshot.tar>"; exit 2; }
    wsl.exe --terminate zkaedi-lab
    wsl.exe --unregister zkaedi-lab
    wsl.exe --import zkaedi-lab "H:/wsl/zkaedi-lab" "$2" --version 2 ;;
  *) echo "usage: snapshot_lab.sh {snapshot|rollback <tar>}"; exit 2 ;;
esac
