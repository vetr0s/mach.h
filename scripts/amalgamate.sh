#!/usr/bin/env bash
# amalgamate.sh — stitch src/ and vendor/ into the single-file mach.h.
#
# mach.h is a GENERATED artifact. Do not edit it by hand; edit the parts in
# src/ (mach's own code) or drop a new upstream release into vendor/, then run
# this script to regenerate. The order of the parts is scripts/manifest.txt.
#
# The vendored libraries (vendor/RGFW.h, vendor/clay.h, vendor/stb_image.h) are
# pristine upstream bodies. Updating one is a file replace; the mach-side glue
# that wraps each (warning pragmas, the #define that turns on its implementation,
# the banner comments) lives in the src/vendor_*_pre.h / _post.h parts, so an
# update does not have to touch it.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
manifest="$root/scripts/manifest.txt"
# Output defaults to the committed mach.h; the sync check overrides it to a temp
# path via MACH_AMALGAMATE_OUT so it can regenerate without touching the tree.
out="${MACH_AMALGAMATE_OUT:-$root/mach.h}"
tmp="$out.tmp.$$"

if [ ! -f "$manifest" ]; then
  echo "amalgamate: missing $manifest" >&2
  exit 1
fi

: > "$tmp"
while IFS= read -r part; do
  # Skip blank lines and comments in the manifest.
  case "$part" in
    ''|\#*) continue ;;
  esac
  path="$root/$part"
  if [ ! -f "$path" ]; then
    echo "amalgamate: manifest lists missing file: $part" >&2
    rm -f "$tmp"
    exit 1
  fi
  cat "$path" >> "$tmp"
done < "$manifest"

mv "$tmp" "$out"
echo "amalgamate: wrote $out ($(wc -l < "$out" | tr -d ' ') lines)"
