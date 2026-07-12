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

## v0.2.1

**The frame cap's precision was a claim, not a fact — now it's a fact.** v0.2.0
went looking for docs that outran the code and missed one: ARCHITECTURE said the
cap "holds 60.0 fps with ~0.01 ms of jitter", and nothing in the repo reproduced
it. Writing the test that would have found it, found it.

The wait slept to a fixed 1 ms short of the deadline and spun out the last
millisecond, on the assumption that a sleep overruns by about a millisecond. It
doesn't: **the overrun scales with the request.** Measured on macOS, a 1 ms sleep
runs over by 0.26 ms, but a 16 ms sleep runs over by 3.6 ms. At 60 fps the wait
asks for ~15.6 ms, blows straight past the 1 ms margin, and the spin that was
supposed to land it on the mark never runs at all. Measured jitter was **1.9 ms**,
not 0.01 ms — the cap had quietly degraded to whatever the scheduler felt like,
which is precisely what the spin existed to prevent.

`mach_wait_until_ns` now sleeps a *fraction* of what's left and re-measures, in a
loop. It never has to know the overshoot: each pass overruns by a fraction of a
smaller number, and the next pass sees the truth and corrects. It converges in a
handful of syscalls, and the last millisecond is still spun by hand. Jitter is now
**0.003 ms mean, 0.044 ms max** — the number ARCHITECTURE has been claiming since
v0.1.2, delivered for the first time.

**The pacing claims are tested now.** `tests/test_core.c` covers both halves of the
cap, headless, in CI. The deadline arithmetic (advance by exactly one period, no
cumulative drift over 600 frames, an overrun gives up the lost time instead of
handing the game a burst of zero-length catch-up frames) is checked as pure
arithmetic — `mach_pace_advance` was split out of `mach_frame_end` to make that
reachable without a window. The timing half asserts the wait never returns early,
that the cap holds its rate, and that jitter stays under half a millisecond, which
the pre-fix wait fails. It also prints the numbers, so the claim is reproducible
rather than trusted: run `./nob test`.

For contrast, the test also runs the naive cap the anchor replaced — do the work,
then sleep a full period. With 4 ms of work against a 60 fps target it measures
**48.4 fps**, because the frame takes `work + period` rather than `period`. That is
the whole reason the deadline is absolute, and now it is a number you can watch
rather than a paragraph you have to believe.

No API changes.

## v0.2.0

**Sprite batching, via atlases.** The batch only breaks when the texture id
changes, so N sprites in N textures cost N draw calls. `Mach_R2D_Atlas` packs
them into one texture at load time (shelf packing, which is near-optimal for the
same-height art a tile game has) and hands back a `Mach_R2D_Region` per sprite.
`mach_r2d_region` draws one. `mach_r2d_region_of` gives you the same thing from a
sheet you packed yourself, if you'd rather. `examples/atlas.c` draws 1500 sprites
in **2 draw calls**, and the count does not grow with the sprite count.

**Untextured fills batch with whatever you're drawing.** This is the half that
actually mattered. `fill_rect` has to sample *something*, and it used to sample a
1x1 white texture all its own — so alternating a rect and a sprite, or a rect and
a label, broke the batch on every single call. A Clay HUD of N elements cost
about 2N draw calls. Now `Mach_Renderer.white` is a `Mach_R2D_Region`, and it
points by default at a white block baked into the font sheet's one spare cell
(16x6 = 96 cells, 95 glyphs). Text and rectangles therefore share a texture:
`examples/pong` dropped from 2 draw calls to 1. Every atlas reserves a white
block too, so `m.r2d.white = world.white` makes fills batch with your sprites,
and `m.r2d.white = m.r2d.font->white` puts it back for the HUD. Two assignments,
no mode flag. `Mach_Renderer.draw_calls` reports the result, so you can check
rather than trust.

**`frame_ms` was measuring the vsync wait.** `mach_frame_end` presented the frame
and *then* sampled the clock — but the buffer swap is exactly where vsync blocks,
so the wait landed inside the measurement. With vsync on (the default since
v0.1.5) `frame_ms` reported the display's refresh period no matter what the frame
actually cost: on a 108 Hz panel a trivial frame measured **8.4 ms** when its real
work was **0.8 ms**. It now submits, samples, and only then swaps, which is why
`mach_r2d_submit` exists as a call separate from `mach_r2d_present`. The number
that v0.1.5 shipped as its headline feature now means what it said it meant.

**The arena no longer corrupts itself when malloc fails.** A failed region
allocation left `end` NULL while `begin` still pointed at the live chain, so the
*next* allocation took the empty-arena path and overwrote `begin` — orphaning
every region and invalidating every pointer the caller was still holding. A
failed `mach_arena_alloc` now returns NULL and leaves the arena exactly as it
was.

**The keyboard has a release edge.** `Mach_Input.key_released` exists, which the
README has been claiming for some time while the struct had no such field. Also,
`escape_quits` no longer swallows the Escape keypress before the input snapshot
sees it.

**The font has all 95 of its glyphs.** 21 were missing — `? ' " = @ [ ] _ ~` and
others — and `mach_font_glyph_uv` returned success for them anyway, so
`mach_r2d_text(..., "what?")` silently rendered `what ` with no way to detect it.

**Nested clips work.** `mach_r2d_clip_end` used to disable the scissor test
outright, so an inner `clip_end` destroyed the enclosing clip and the rest of the
outer container drew unclipped. Clay emits a scissor pair per clipped container
and they nest, so this was reachable from any scrolling panel inside another.
There is a clip stack now (8 deep), and an inner rect is intersected with its
parent.

**Tests.** `tests/test_core.c` — the first in this repo. It covers the arena
(including the out-of-memory path, through a new `MACH_MALLOC` / `MACH_CALLOC` /
`MACH_FREE` hook), the math and iso transforms, the color helpers, and the font
atlas. It needs no display, and `./nob test` runs it in CI on all three
platforms, which is the first time CI has executed engine code rather than merely
compiled it. The arena and font tests both fail on v0.1.5, which is the point of
them.

**CI builds with gcc.** It never did, despite the README saying so: `nob.c`
hardcoded clang on Linux, and gcc only ever compiled the build tool. `nob` now
honors `$CC`, so `cc -o nob nob.c && ./nob` also works on a box that has gcc and
no clang — which the README's own instructions previously did not.

Smaller things: `MACH_DEBUG_ASSERT` no longer *evaluates* its condition in
`NDEBUG` builds (it expanded to `(void)(x)`, so a release build still ran the
check); `MACH_DEBUGBREAK` used `__builtin_debugbreak`, which is not a clang
builtin and had never been compiled; a failed `mach_r2d_init` cleans up after
itself instead of leaking shaders and GL objects; `mach_r2d_destroy_texture`
flushes the batch before deleting a texture the batch is still naming.

Docs: the README and ARCHITECTURE claims were audited against the code, and the
ones that outran it were fixed — in the code where the claim was the right
intent, in the prose where it wasn't. `src/` is ~2.7k lines, not 1.9k. The
"no mutable global state" rule is now literally true of engine code (the GL hints
RGFW retains a pointer to moved into `Mach`); what the *embedded* libraries keep,
and how it's quarantined, is now stated rather than glossed. Linux is X11.

### Upgrading from v0.1.5

Three things a consumer can notice:

- **`frame_ms` will read lower**, often much lower, because it no longer includes
  the vsync wait. That is the fix, not a regression. If you were treating it as a
  frame-period number, you want `1000.0f / m.fps` instead.
- **`Mach_Renderer.white` changed type**, from `Mach_R2D_Texture` to
  `Mach_R2D_Region`. Reading `r->white.id` becomes `r->white.tex`. Most consumers
  never touched it.
- **`b32` is now guardable.** It sat outside the `MACH_INT_DEFINED` guard, so a
  project with its own `b32` got a redefinition error — a hard error in C99, the
  standard the build lines ask for. Define `MACH_B32_DEFINED` to opt out. It has
  its own guard because `MACH_INT_DEFINED` means "I have `u8`..`isize`", which
  doesn't imply you have a `b32`.

`Mach_Font` gained a `white` field, and `Mach_Renderer` gained `draw_calls`,
`clips`, and `clip_depth`. If you were zero-initializing these structs (`Mach m =
{0};`, as the README shows), nothing changes.

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
