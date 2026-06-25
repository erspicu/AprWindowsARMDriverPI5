#!/usr/bin/env bash
#
# build-uefi.sh — overlay our uefi_fixed/ deltas onto the upstream worproject
# EDK2 tree and build our RPI_EFI.fd. Run inside WSL Ubuntu.
#
#   wsl bash uefi_fixed/build-uefi.sh
#
# Output: uefi_build/RPI_EFI.fd (copy onto the SD card to replace the stock UEFI).
#
set -euo pipefail

# Repo root = parent of this script's dir.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SRC="$ROOT/uefi_sources"
FIXED="$ROOT/uefi_fixed"
OUT="$ROOT/uefi_build"

echo "== Checking prerequisites =="
for d in edk2 edk2-platforms edk2-non-osi; do
  [ -d "$SRC/$d" ] || { echo "ERROR: $SRC/$d missing — clone uefi_sources first (see uefi_fixed/README.md)"; exit 1; }
done
# Toolchain (install once): build-essential uuid-dev iasl gcc-aarch64-linux-gnu python3 python3-distutils
for t in iasl aarch64-linux-gnu-gcc python3 make gcc; do
  command -v "$t" >/dev/null || { echo "ERROR: '$t' not found. Run: sudo apt install build-essential uuid-dev iasl gcc-aarch64-linux-gnu python3 python3-distutils"; exit 1; }
done

echo "== Overlaying uefi_fixed/ onto edk2-platforms =="
# Copy every file under uefi_fixed/ (except docs/scripts) to the matching path
# in edk2-platforms, preserving directory structure.
while IFS= read -r -d '' f; do
  rel="${f#$FIXED/}"
  case "$rel" in
    README.md|build-uefi.sh) continue ;;
  esac
  dest="$SRC/edk2-platforms/$rel"
  mkdir -p "$(dirname "$dest")"
  cp -v "$f" "$dest"
done < <(find "$FIXED" -type f -print0)

echo "== Setting up EDK2 build environment =="
export WORKSPACE="$SRC"
export PACKAGES_PATH="$SRC/edk2:$SRC/edk2-platforms:$SRC/edk2-non-osi"
export GCC5_AARCH64_PREFIX=aarch64-linux-gnu-
cd "$SRC"
# shellcheck disable=SC1091
source edk2/edksetup.sh
make -C edk2/BaseTools   # not parallel-safe; no -j

echo "== Building RPi5 UEFI (AARCH64, RELEASE) =="
NUM_CPUS=$(( $(getconf _NPROCESSORS_ONLN) + 2 ))
build -n "$NUM_CPUS" -a AARCH64 -t GCC5 -b RELEASE \
      -p Platform/RaspberryPi/RPi5/RPi5.dsc

FD="$SRC/Build/RPi5/RELEASE_GCC5/FV/RPI_EFI.fd"
[ -f "$FD" ] || { echo "ERROR: build produced no RPI_EFI.fd at $FD"; exit 1; }

mkdir -p "$OUT"
cp -v "$FD" "$OUT/RPI_EFI.fd"
echo "== DONE: $OUT/RPI_EFI.fd  ($(stat -c%s "$OUT/RPI_EFI.fd") bytes) =="
echo "Copy it onto the SD card to replace the stock RPI_EFI.fd (see uefi_build/README.md)."
