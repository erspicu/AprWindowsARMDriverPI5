#!/usr/bin/env bash
#
# build-uefi.sh — build OUR modified Pi5 UEFI: overlay uefi_fixed/ onto the
# worproject rpi5-uefi build tree, run its build.sh, copy RPI_EFI.fd to
# uefi_build/. Run inside WSL Ubuntu (native FS, NOT /mnt/c for the build tree).
#
#   wsl bash /mnt/c/ai_project/AprWindowsDriver/uefi_fixed/build-uefi.sh
#
# Prereqs: a working ~/rpi5-uefi tree with all submodules — set up per
# MD/Skill/pi5-uefi-build.md (the verified recipe). Override its location with
# RPI5_UEFI_DIR=/path  if you keep it elsewhere.
#
set -euo pipefail

ROOT="/mnt/c/ai_project/AprWindowsDriver"
FIXED="$ROOT/uefi_fixed"
OUT="$ROOT/uefi_build"
RPI5_UEFI_DIR="${RPI5_UEFI_DIR:-$HOME/rpi5-uefi}"

[ -f "$RPI5_UEFI_DIR/build.sh" ] || { echo "ERROR: $RPI5_UEFI_DIR not set up — see MD/Skill/pi5-uefi-build.md"; exit 1; }

# python alias for BaseTools + never hang on a git auth prompt.
mkdir -p "$HOME/bin"; ln -sf "$(command -v python3)" "$HOME/bin/python"
export PATH="$HOME/bin:$PATH"
export GIT_TERMINAL_PROMPT=0

echo "== Overlaying uefi_fixed/<component>/ onto $RPI5_UEFI_DIR/<component>/ =="
# uefi_fixed mirrors by build component: edk2-platforms/, edk2-non-osi/, edk2/, etc.
for comp in edk2 edk2-platforms edk2-non-osi arm-trusted-firmware; do
  [ -d "$FIXED/$comp" ] || continue
  while IFS= read -r -d '' f; do
    rel="${f#$FIXED/$comp/}"
    dest="$RPI5_UEFI_DIR/$comp/$rel"
    mkdir -p "$(dirname "$dest")"
    cp -v "$f" "$dest"
  done < <(find "$FIXED/$comp" -type f -print0)
done

echo "== Building (worproject build.sh, model 5) =="
cd "$RPI5_UEFI_DIR"
bash build.sh --model 5

FD="$RPI5_UEFI_DIR/RPI_EFI.fd"
[ -f "$FD" ] || { echo "ERROR: no RPI_EFI.fd produced"; exit 1; }
mkdir -p "$OUT"
cp -v "$FD" "$OUT/RPI_EFI.fd"
echo "== DONE: $OUT/RPI_EFI.fd  ($(stat -c%s "$OUT/RPI_EFI.fd") bytes) =="
echo "Copy onto the SD card to replace the stock RPI_EFI.fd (see uefi_build/README.md)."
