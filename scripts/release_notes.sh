#!/usr/bin/env bash
# release_notes.sh: print the release body for a version on stdout.
#
#   ./scripts/release_notes.sh v0.1.5   (the leading v is optional)
#
# The body is that version's section from CHANGELOG.md, followed by the standing
# "how to use this file" text every release carries. release.yml runs this twice:
# once in verify-version, where a missing section fails the tag before anything
# is published, and once in publish, to write the notes.
set -euo pipefail

if [ $# -ne 1 ]; then
  echo "usage: $0 <version>" >&2
  exit 2
fi

version="${1#v}"
root="$(cd "$(dirname "$0")/.." && pwd)"
changelog="$root/CHANGELOG.md"

if [ ! -f "$changelog" ]; then
  echo "release_notes: missing $changelog" >&2
  exit 1
fi

# The section runs from its own "## vX.Y.Z" heading to the next "## v" heading
# (or the end of the file), heading itself excluded: the release page already
# shows the version in its title.
section="$(awk -v want="## v$version" '
  $0 == want { found = 1; next }
  found && /^## v/ { exit }
  found { print }
' "$changelog")"

# Trim the blank lines the split leaves at either end.
section="$(printf '%s\n' "$section" | sed -e '/./,$!d' -e :a -e '/^\n*$/{$d;N;ba' -e '}')"

if [ -z "$section" ]; then
  echo "release_notes: CHANGELOG.md has no section for v$version" >&2
  echo "release_notes: add one under a '## v$version' heading" >&2
  exit 1
fi

cat <<EOF
## What's new

$section

---

The engine is one file. Download \`mach.h\`, drop it in your project, compile.

    #define MACH_IMPLEMENTATION
    #include "mach.h"

Define \`MACH_IMPLEMENTATION\` in exactly one translation unit. No build
system, no submodules, nothing to install. Link the platform's windowing
and GL libraries; see the README for the per-platform line.

RGFW, Clay, and stb_image are embedded in the header, with their license
notices intact.
EOF
