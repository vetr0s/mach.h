# Changelog

What changed in each release, written for someone deciding whether to upgrade.
`scripts/release_notes.sh` reads the section for a version out of this file and
the Release workflow publishes it as the release body, so the notes on the
release page and the notes here cannot drift apart.

Adding a version here is part of cutting a release: the workflow fails the tag
if the section is missing. The heading must be exactly `## vMAJOR.MINOR.PATCH`.

Versions before v0.1.5 were backfilled from the commits and the release pages
after the fact. Their published pages still read as they did at the time; this
file is the record from here on.

## v0.1.5

**Frames are paced by vsync now.** A default `Mach_Config{}` syncs to the
display: no tearing, no core burned on frames nobody sees. `target_fps` means
"give me a rate the display isn't offering" — it turns vsync off and hands
pacing to the loop's own cap. `vsync_off` with no target runs uncapped, which is
a measurement tool rather than a way to ship.

**The frame cap actually hits its target.** It used to sleep the remainder of
each frame in isolation, but `nanosleep` and `Sleep` only guarantee *at least*
what you ask for, so each overshoot pushed the next frame out and the error
walked: a 60 fps cap measured 56.7, a 144 cap measured 130.7. It now paces to an
absolute deadline, which holds 60.0 fps with ~0.01 ms of jitter.

**`Mach.frame_ms` and `Mach.frame_ms_peak`** report what a frame actually cost —
update, draw, present, with the pacing wait excluded — and the worst frame of the
last second. This is the headroom number: `fps` reads a flat 60 under a cap
whether a frame takes 2 ms or 16 ms, and a one-second average buries the single
long frame that hitches.

**`examples/pong.c`**: a one-player pong, for input, collision, and game state on
the same three-call loop.

Upgrading from 0.1.4: both pacing changes are behavioral. A config that used to
free-run now syncs to the display, and a `target_fps` that used to run a few
percent under now lands on its target. Nothing in the API breaks.

## v0.1.4

**Images from memory.** `mach_image_load_from_memory` and
`mach_r2d_texture_from_memory` decode an encoded PNG/BMP/JPG that is already in
memory, so a game can bake its assets into the executable instead of loading
them from a path at runtime.

**CI that actually guards the release.** The examples are compiled on Linux,
macOS, and Windows on every push, and tagging publishes `mach.h` to a release
page after checking that the tag, `VERSION`, `src/base.h`, the generated header,
and the README all agree on the version — the numbers had drifted from the tags
twice by this point.

This release also carried 0.1.2 and 0.1.3, neither of which was ever published.

## v0.1.3

Tagged but never released, so its changes reached consumers in 0.1.4.

The examples build with [nob](https://github.com/tsoding/nob.h) instead of a
shell script, which is what made a new example a file rather than a file plus a
build edit. `ARCHITECTURE.md` was written, and the `src/` parts were put under
clang-format.

## v0.1.2

Bumped in `VERSION` but never tagged, so its changes reached consumers in 0.1.4.

Frame timing moved to nanoseconds. In milliseconds a target rate whose period
isn't a whole number (144 fps is 6.944 ms) truncates, and the cap silently
enforces a different rate than the one asked for.

## v0.1.1

Packaging only, no API change: `mach.h` became a generated amalgamation, stitched
by `scripts/amalgamate.sh` from `src/` (the engine's own code) and `vendor/`
(pristine RGFW, Clay, stb_image). The shipped header is functionally identical to
0.1.0 — the point is that the engine is now editable as parts and shipped as one
file, and `scripts/check_generated.sh` fails a build whose header has drifted
from its sources.

## v0.1.0

The first release, split out of the game it grew inside. One self-contained
header: RGFW, Clay, and stb_image embedded verbatim with their license notices,
everything exported under `Mach_` / `mach_` / `MACH_` (enforced by
`scripts/check_namespace.sh` against faked X11 and Win32 names), and
`examples/hello.c` as a runnable version of the README snippet.
