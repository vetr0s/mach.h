#!/usr/bin/env bash
# check_generated.sh: fail if mach.h is out of sync with src/ and vendor/.
#
# Run in CI (and locally before committing) so a hand-edit to mach.h, or a
# forgotten regenerate after touching a part, cannot land. Regenerates into a
# temp file and byte-compares against the committed mach.h.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
committed="$root/mach.h"
regen="$(mktemp)"
trap 'rm -f "$regen"' EXIT

# Amalgamate to a temp path without touching the committed file.
MACH_AMALGAMATE_OUT="$regen" bash "$root/scripts/amalgamate.sh" >/dev/null

if cmp -s "$committed" "$regen"; then
  echo "check_generated: mach.h is in sync with src/ + vendor/ ✅"
else
  echo "check_generated: mach.h is STALE ❌" >&2
  echo "  regenerate it with: scripts/amalgamate.sh" >&2
  echo "  (do not edit mach.h directly; edit src/ or vendor/)" >&2
  diff "$committed" "$regen" | head -40 >&2 || true
  exit 1
fi
