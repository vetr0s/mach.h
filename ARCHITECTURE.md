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
file-scope mutable globals in engine code.

This is what makes hot-reload schemes work. Two copies of the engine *code* (a
host binary and a reloadable game library, each including `mach.h`) can operate
on one shared set of *data*, because the data is all reachable from pointers the
host holds. The engine only has to promise it keeps no hidden state of its own;
the consumer keeps its side of the bargain by not adding any either. The RGFW
host-side calls (which do touch RGFW's internal globals) all sit behind
`mach_init` / `mach_frame_*` / `mach_shutdown`, so only the host half ever makes
them.

## Everything is namespaced

Types are `Mach_*`, functions `mach_*`, macros `MACH_*`. Nothing the header
exports can collide with OS headers (X11's `Font` typedef, `windows.h` macros)
or with consumer code. `scripts/check_namespace.sh` compiles the header against
faked X11/Win32 names and fails on any collision, so the guarantee is tested,
not just intended.

## The renderer

One shader. Textured, vertex-colored triangles in a single draw stream, drawn in
painter's order (submission order is depth order; the consumer sorts when it
needs to). Fills, text, and sprites are all the same primitive: a quad of
`Mach_Vertex`. Text is an 8x8 bitmap font baked into a GL texture atlas at init,
so a string is just more textured quads.

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
`mach_frame_end` presents, samples the FPS counter, and sleeps off the soft
frame cap.

Timing is done in nanoseconds. The cap period is `1e9 / target_fps`, so target
rates whose frame time isn't a whole millisecond (144 fps is 6.944 ms) are
honored instead of truncated. The clocks are `clock_gettime(CLOCK_MONOTONIC)`
and `QueryPerformanceCounter`; the sleep is `nanosleep` (`Sleep`, 1ms-granular,
on Windows). The cap is soft: it yields the rest of a frame's budget, it doesn't
spin.

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
