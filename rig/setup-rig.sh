#!/usr/bin/env bash
# Populate rig/boot/ with Workbench essentials (C: Libs: L:) extracted from
# the AmigaVision HDF, plus the freshly built ant-server. Idempotent.
#
# The extracted OS files are copyrighted (AmigaOS 3.x) and gitignored —
# the repo carries only this script and S/startup-sequence. Rebuilding the
# rig on a new machine needs the HDF + the pfs3 tool (see amiga-tools
# README for both).
set -euo pipefail

HDF="${HDF:-$HOME/local/Amiberry/Harddrives/AmigaVision.hdf}"
PFS3="${PFS3:-$HOME/src/pfs3/target/release/pfs3}"
RIG="$(cd "$(dirname "$0")" && pwd)"
BOOT="$RIG/boot"

[[ -f "$HDF" ]] || { echo "missing HDF: $HDF (override with HDF=)" >&2; exit 1; }
[[ -x "$PFS3" ]] || { echo "missing pfs3: $PFS3 (override with PFS3=)" >&2; exit 1; }

# pfs3 extract writes the drawer *contents* into -o, so give each drawer
# its own target dir.
for drawer in C Libs L; do
    if [[ -f "$BOOT/$drawer/.complete" ]]; then
        echo "skip /$drawer (already extracted; rm -rf boot/$drawer to redo)"
        continue
    fi
    echo "extracting /$drawer ..."
    mkdir -p "$BOOT/$drawer"
    "$PFS3" extract "$HDF" --partition DH0 -o "$BOOT/$drawer" "/$drawer" >/dev/null
    touch "$BOOT/$drawer/.complete"
done

# ant-server goes into C/ where the startup-sequence finds it via path.
if [[ -f "$RIG/../server/ant-server" ]]; then
    cp "$RIG/../server/ant-server" "$BOOT/C/ant-server"
    echo "installed ant-server -> boot/C/"
else
    echo "warning: server/ant-server not built yet (make in server/)" >&2
fi

echo "rig ready: $BOOT"
