#!/usr/bin/env bash
#
# fetch-submodules.sh — restore the re-downloadable edk2 sub-submodules that were
# stripped from this backup (openssl, mbedtls, googletest, ...). Run after
# extracting the source archive, from the backup dir (where SUBMODULES.txt lives).
#
#   bash fetch-submodules.sh /path/to/extracted/edk2
#
# Default edk2 path: ./<archive-extract>/edk2 — pass the real path as $1.
#
set -uo pipefail
EDK2="${1:-./edk2}"
HERE="$(cd "$(dirname "$0")" && pwd)"
export GIT_TERMINAL_PROMPT=0   # never hang on an auth prompt (dead repos 401)

[ -d "$EDK2" ] || { echo "ERROR: edk2 dir not found: $EDK2"; exit 1; }

ok=0; fail=0
grep -vE '^\s*#|^\s*$' "$HERE/SUBMODULES.txt" | while read -r path url commit; do
  dest="$EDK2/$path"
  echo ">> $path  ($commit)"
  rm -rf "$dest"; mkdir -p "$dest"
  ( cd "$dest"
    git init -q
    # fast path: fetch the exact commit shallowly; fallback to full clone + checkout
    if git fetch -q --depth 1 "$url" "$commit" 2>/dev/null; then
      git checkout -q FETCH_HEAD
    else
      rm -rf "$dest"/* "$dest"/.git 2>/dev/null
      git clone -q "$url" "$dest" && git -C "$dest" checkout -q "$commit"
    fi
  ) && echo "   OK" || echo "   FAILED: $path"
done
echo "Done. Verify with: find $EDK2/CryptoPkg/Library/OpensslLib/openssl -type f | head"
