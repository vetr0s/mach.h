# Changelog

What changed in each release, written for someone deciding whether to upgrade.
`scripts/release_notes.sh` reads the section for a version out of this file and
the Release workflow publishes it as the release body, so the notes on the
release page and the notes here cannot drift apart.

Adding a version here is part of cutting a release: the workflow fails the tag
if the section is missing. The heading must be exactly `## vMAJOR.MINOR.PATCH`.

Releases before v0.1.5 predate this file; their pages carry the stock install
text only.

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
