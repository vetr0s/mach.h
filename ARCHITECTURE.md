# mach.h: architecture

This is the engine's architecture: what's inside `mach.h` and why it's shaped
this way. The game that drove these requirements lives in its own repo (`mach`)
and has its own `ARCHITECTURE.md` for the game side of the boundary.

## What the engine is

One header. Windowing and input (RGFW), an OpenGL 3.3 core batch renderer, an
8x8 bitmap font, image loading (stb_image), a UI layout binding (Clay), arena
allocators, and a frame loop. A consumer copies `mach.h` into their project,
defines `MACH_IMPLEMENTATION` in one translation unit, and links the platform's
window/GL libraries. Nothing to install, no build system to adopt.

The engine is a toolbox, not a framework. It never owns control flow: the
consumer writes `main()`, calls `mach_init`, and runs its own loop of
`mach_frame_begin` / `mach_frame_end`. raylib-style. This is the single most
important design fact; everything below serves it.

## The generated header

`mach.h` is a **generated, committed artifact**. You don't edit it. The real
sources are:

| Location | What |
|---|---|
| `src/` | mach's own code (~1.9k lines), split into ordered parts |
| `vendor/` | RGFW, Clay, stb_image, pristine upstream bodies |
| `scripts/manifest.txt` | the assembly order |
| `scripts/amalgamate.sh` | concatenates the parts into `mach.h` |

The header emits in two phases, matching its two include guards: the public
**interface** (inside `MACH_H`), then the **implementation** (inside
`MACH_IMPLEMENTATION`). Each vendored library is wrapped by a `vendor_*_pre.h` /
`vendor_*_post.h` pair that carries the mach-side glue (warning pragmas, the
`*_IMPLEMENTATION` define, banner comments), so the body in `vendor/` stays a
verbatim upstream file and updating a library is a plain file replace. See
`src/README.md` for the part layout and `vendor/README.md` for the update steps.

Two scripts keep it honest: `check_generated.sh` fails if `mach.h` was
hand-edited or left stale after a part changed (run it in CI), and
`check_namespace.sh` guards against OS-header collisions (below).

## No mutable global state

Every piece of engine state lives in a struct the consumer owns and passes by
pointer: the `Mach` struct (window, renderer, input, frame timing, the frame
arena) and whatever the consumer allocates through the engine. There are no
file-scope mutable globals in engine code — not one, including the GL hints RGFW
retains a pointer to, which is why `Mach.gl_hints` is a field rather than the
`static` it looks like it wants to be.

This is what makes hot-reload schemes work. Two copies of the engine *code* (a
host binary and a reloadable game library, each including `mach.h`) can operate
on one shared set of *data*, because the data is all reachable from pointers the
host holds. The engine only has to promise it keeps no hidden state of its own;
the consumer keeps its side of the bargain by not adding any either.

The **embedded libraries do keep globals** — RGFW's window/context state, and
Clay's "current context" plus its callback pointers. That is not something mach
can fix from the outside, so it quarantines them instead: every call that touches
them sits behind `mach_init` / `mach_frame_*` / `mach_shutdown`, so only the host
half ever makes one, and `mach_clay_ui_begin` re-points Clay's context every
frame precisely so a reloaded library (whose globals start empty) picks up the
host's data rather than its own zeroes. The rule is "mach's own code holds no
mutable globals", not "no globals exist anywhere in the binary" — the second
would be a lie, and the whole point of the rule is that you can trust it.

## Everything is namespaced

Types are `Mach_*`, functions `mach_*`, macros `MACH_*`. Nothing under those
prefixes collides with OS headers (X11's `Font` typedef, `windows.h` macros).
`scripts/check_namespace.sh` compiles the header against faked X11/Win32 names
and fails on any collision, so *that* guarantee is tested, not just intended.

It does not test the header against your code, and two things are unprefixed on
purpose: the scalar aliases (`u8`..`isize`, `b32`). Both have escape hatches
(`MACH_INT_DEFINED`, `MACH_B32_DEFINED`) precisely because a game is likely to
have its own. The embedded libraries also export `RGFW_*`, `Clay_*`, `stbi_*`
and the `GL_*` constants.

## The renderer

One shader. Textured, vertex-colored triangles in a single draw stream, drawn in
painter's order (submission order is depth order; the consumer sorts when it
needs to). Fills, text, and sprites are all the same primitive: a quad of
`Mach_Vertex`. Text is an 8x8 bitmap font baked into a GL texture atlas at init,
so a string is just more textured quads.

### The batching invariant

The batch is flushed when the texture id changes, the clip changes, the batch
fills, or the frame ends. **A frame therefore costs one draw call per contiguous
run of draws that share a texture id.** Painter's order survives it, because a
flush is a draw, not a reorder.

Everything about atlases follows from that one sentence. N sprites in N textures
cost N draws; pack them into one `Mach_R2D_Atlas` and they cost one. The subtler
half is untextured geometry: `mach_r2d_fill_rect` and friends have to sample
*something*, and if that something is a different texture from the sprites, then
alternating a sprite and a rect breaks the batch on every single call.

So `white` is not a texture, it is a `Mach_R2D_Region` — a block reserved inside
an atlas. The font's sheet is 16x6 = 96 cells holding 95 glyphs, so the spare
cell holds an opaque white block, and `Mach_Renderer.white` points at it by
default. That alone means text and rectangles share one texture: a Clay HUD of N
elements went from roughly 2N draw calls to one. Every `Mach_R2D_Atlas` reserves
a white block of its own, so a consumer drawing world sprites can point
`r->white` at the world atlas, draw sprites and selection rects interleaved, and
still pay one draw call — then point it back at the font for the HUD. Two
assignments, no mode flag, and `Mach_Renderer.draw_calls` tells you whether you
got it right.

(A region's white UVs are all collapsed onto the block's center rather than
spanning it. A quad interpolates its UVs across its face, so a spanning rect
would sample the texel boundary at the quad's edges and, with any floating-point
slop, bleed the transparent gutter in — a 1px fringe on every filled rectangle.
A point samples the same opaque texel at every size.)

GL itself is loaded by hand: the ~40 GL 3.3 core entry points the renderer uses
are declared in the header and resolved at runtime (no GLEW, no GLAD). Isometric
support is a coordinate transform, not a projection: `mach_iso_to_screen` /
`mach_screen_to_iso` map a grid to 2:1 diamond tiles, and a `Mach_Camera2D`
carries pan/zoom. 2D is a deliberate scope choice; 3D can return when something
concrete needs it, and the roadmap keeps a slot for it.

## Memory

Arenas are the allocation idiom. An arena is a linked list of regions; you
allocate by bumping a pointer and free by resetting or freeing the whole arena,
never an individual allocation. The default region is 64 KiB (8K words), big
enough that most arenas live in a single region.

The engine owns one arena directly: `Mach.frame_arena`, reset at every
`mach_frame_begin`. Anything allocated from it lives exactly one frame, which is
where transient per-frame scratch (sort buffers, formatted strings) should come
from. Consumers make their own arenas for longer-lived data.

## The frame loop

`mach_frame_begin` computes `dt`, resets the frame arena, drains the RGFW event
queue into `Mach.input` (a per-frame snapshot the consumer reads as
`key_pressed[...]`, `mouse_pressed[...]`, `wheel`, ...), consumes window
lifecycle events (quit, Escape, resize) itself, and clears the screen.
`mach_frame_end` presents, samples the frame's cost and the FPS counter, and
waits out the frame cap.

Timing is done in nanoseconds. The cap period is `1e9 / target_fps`, so target
rates whose frame time isn't a whole millisecond (144 fps is 6.944 ms) are
honored instead of truncated. The clocks are `clock_gettime(CLOCK_MONOTONIC)`
and `QueryPerformanceCounter`.

Frames are paced by **vsync** by default: the display is already a clock, and
letting it drive the loop costs no CPU and cannot tear. `target_fps` is for the
cases where the game wants a rate the display isn't offering (a 30fps lock, a
capture, a benchmark above the refresh rate) — it turns vsync off and hands
pacing to the loop's own cap, since the two would otherwise each wait on the
other's schedule. `vsync_off` with no `target_fps` runs uncapped, which is a
measurement tool rather than a way to ship.

The cap paces to an **absolute deadline** that advances by one period per frame,
rather than sleeping the remainder of each frame in isolation. Sleeping is only
approximate — `nanosleep` and `Sleep` guarantee *at least* what you ask for and
routinely overshoot by a millisecond — so per-frame sleeps push the next frame
out by their own error and the rate drifts below target (a 60 cap measured 56.7
fps). Anchoring to a deadline lets a long frame be absorbed by the next short
one; the wait then sleeps to a millisecond short of the deadline and spins out
the remainder, which holds 60.0 fps with ~0.01 ms of jitter. A frame that blows
its budget outright resets the deadline to now, so a stall can't leave a debt
that gets repaid as a burst of zero-length frames.

`Mach.frame_ms` is what a frame actually cost — the update, plus handing the
draws to the driver — with the vsync and cap waits **excluded**, and
`Mach.frame_ms_peak` is the worst frame of the last completed 1s window. These,
not `fps`, are the headroom numbers: under vsync or a cap, `fps` reads a flat 60
whether a frame takes 2 ms or 16 ms, and a 1s average hides the single long frame
that hitches.

Getting that exclusion right is fiddlier than it sounds, because the buffer swap
is where vsync blocks. `mach_frame_end` therefore submits the batch, samples the
clock, and *only then* swaps — which is why `mach_r2d_submit` exists as a
separate call from `mach_r2d_present`. Sampling after the swap (as the loop did
before v0.2.0) means `frame_ms` reports the display's refresh period instead of
the frame's cost: on a 108 Hz panel a trivial frame measured 8.4 ms, when the
work it did was 0.8 ms.

`frame_ms` is CPU cost, not GPU time. The GPU may still be chewing on the batch
when the clock is read. It answers "is the CPU keeping up", which is the question
a game asks first.

## Design rules

| Rule | Why |
|---|---|
| You own the loop | The engine is a library, not a runtime. It never calls you. |
| No mutable global state | Makes hot-reload possible; keeps state inspectable and owned. |
| Everything namespaced | Drop the header into any codebase without collisions. |
| 2D on purpose | One shader, painter's order, iso by transform. Scope is a feature. |
| Vendored bodies stay pristine | Library updates are file replaces, not merges. |

## Non-goals

A plugin system, a scene graph, an entity-component framework, or anything that
would take control flow away from the consumer. The engine grows by concrete
need from the projects that use it, not by speculation.
