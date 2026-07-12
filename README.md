# mach.h

A small 2D game engine in one header. Plain C, zlib license.

[![Linux](https://github.com/vetr0s/mach.h/actions/workflows/linux.yml/badge.svg)](https://github.com/vetr0s/mach.h/actions/workflows/linux.yml)
[![macOS](https://github.com/vetr0s/mach.h/actions/workflows/macos.yml/badge.svg)](https://github.com/vetr0s/mach.h/actions/workflows/macos.yml)
[![Windows](https://github.com/vetr0s/mach.h/actions/workflows/windows.yml/badge.svg)](https://github.com/vetr0s/mach.h/actions/workflows/windows.yml)

One file is the whole engine: windowing and input (RGFW, embedded), an OpenGL
3.3 core batch renderer with sprite atlases, an 8x8 bitmap font, image loading
(stb_image, embedded), a UI layout binding (Clay, embedded), arena allocators,
and a frame loop. There is nothing to install and nothing else to download: copy
`mach.h` into your project and compile.

> [!WARNING]
> **Personal project, under active development.** This is a hobby project, not a
> commercial or professional product. It changes frequently, isn't stable, and
> carries no support or warranty: expect breaking changes and rough edges.

```c
#define MACH_IMPLEMENTATION
#include "mach.h"

int main(void) {
    Mach m = {0};
    if (!mach_init(&m, (Mach_Config){ .title = "game" })) return 1;

    while (mach_running(&m)) {
        mach_frame_begin(&m);   // events -> m.input, m.dt, clear
        mach_r2d_text(&m.r2d, 20, 20, 2, "hello", MACH_COLOR_FG_MAIN);
        mach_frame_end(&m);     // present, m.fps / m.frame_ms, pacing (vsync)
    }

    mach_shutdown(&m);
    return 0;
}
```

Include `mach.h` wherever the engine is used; define `MACH_IMPLEMENTATION`
before the include in exactly one translation unit. A zeroed `Mach_Config` is a
working window (1280x720, resizable, black clear); set only what you care
about. Everything a frame produces is read off the `Mach` struct: `m.input`,
`m.dt`, `m.fps`, and the renderer at `&m.r2d`.

## Building

No include paths needed beyond wherever you put `mach.h`. Link the platform's
windowing and GL libraries:

```
macOS:    clang -std=c99 game.c -framework Cocoa -framework CoreVideo -framework IOKit -framework OpenGL
Linux:    clang -std=c99 game.c -lX11 -lXrandr -lGL -lm -ldl
Windows:  cl /std:c11 game.c /link opengl32.lib winmm.lib     (gdi32 via #pragma comment)
```

Those are the flags CI builds with: C99 on clang and gcc, C11 on MSVC. Calling
the header *strictly* C99 would be overselling it — the `MACH_LOG_*` macros use
`##__VA_ARGS__`, which every one of those compilers accepts but no C standard
does, and the embedded libraries are not `-pedantic` clean. It wants a normal C
compiler, not a particular standard.

Linux means **X11**. RGFW's Wayland backend is not enabled, so a pure-Wayland
session needs XWayland (which most run by default). You need the X11/GL dev
headers once:

```
Void:          sudo xbps-install -S libX11-devel libXrandr-devel libXcursor-devel libXext-devel libXi-devel libglvnd-devel
Debian/Ubuntu: sudo apt install libx11-dev libxrandr-dev libxcursor-dev libxext-dev libxi-dev libgl1-mesa-dev
Fedora:        sudo dnf install libX11-devel libXrandr-devel libXcursor-devel libXext-devel libXi-devel mesa-libGL-devel
Arch:          sudo pacman -S libx11 libxrandr libxcursor libxext libxi mesa
```

`examples/hello.c` is the snippet above with a moving square. `examples/pong.c` is
a one-player pong — keep the ball alive against the back wall — for input,
collision, and game state in the same loop. `examples/atlas.c` draws a couple of
thousand sprites out of one atlas and reports the draw-call count live, so you
can watch it stay flat as the sprite count climbs.

Build them with [nob](https://github.com/tsoding/nob.h): `cc -o nob nob.c` once,
then `./nob` compiles every example under `examples/` into `examples/build/`.
`./nob test` builds and runs the unit tests in `tests/`. Set `CC` to pick the
compiler (`CC=gcc ./nob`); it defaults to clang, or MSVC on Windows.

## What's inside

The header reads top to bottom as sections:

```
base, debug        sized ints (u8..b32), MACH_LOG_* / MACH_DEBUG_ASSERT
RGFW (embedded)    windowing, input events, GL context
mem                arena allocator (region list, whole-arena free/reset)
math               Mach_Vec2 + scalar helpers (mach_clamp, mach_lerp, ...)
color              Mach_Color + a stock palette (modus-vivendi), MACH_COLOR_*
gl                 the ~40 GL 3.3 core entry points, declared by hand, loaded at runtime
font               8x8 bitmap font baked into a GL texture atlas (all 95 printable ASCII)
image (stb)        mach_image_load / mach_image_load_from_memory / mach_image_free
render2d           the batch renderer: one shader, one draw stream (mach_r2d_*),
                   sprite atlases (mach_r2d_atlas_*), an isometric camera and transforms
input              per-frame snapshot: key/mouse down, pressed, released, wheel
clay_ui (embedded) Clay layout bound to the renderer (mach_clay_ui_*)
core               mach_init / mach_running / mach_frame_begin / mach_frame_end / mach_shutdown
```

**Batching, in one sentence:** the batch is flushed when the texture id changes,
the clip changes, the batch fills, or the frame ends — so a frame costs one draw
call per contiguous run of draws that share a texture. Pack sprites into a
`Mach_R2D_Atlas` and they share one. `Mach_Renderer.white`, which untextured
draws like `mach_r2d_fill_rect` sample, is a *region* you can point at any
atlas's white block, so fills batch with whatever you're drawing instead of
splitting it. `Mach_Renderer.draw_calls` reports the result each frame; don't
take the claim on faith, read the number.

Design rules the header holds itself to:

- **You own the loop.** The engine never drives anything; it's a toolbox of
  calls your `main()` makes. raylib-style.
- **No mutable global state in engine code.** Everything mach owns lives in the
  `Mach` struct you own and pass by pointer — there is not one file-scope mutable
  global in `src/`. This is what makes hot-reload schemes work: two copies of the
  engine code can operate on the same data. The *embedded* libraries do keep
  globals (Clay's context, RGFW's), and those are quarantined behind
  `mach_init` / `mach_frame_*` / `mach_shutdown`, which re-point them each frame.
- **Everything is namespaced.** Types are `Mach_*`, functions `mach_*`, macros
  `MACH_*`, and none of them collide with OS headers (X11's `Font`, windows.h
  macros) — `scripts/check_namespace.sh` enforces that against faked X11/Win32
  names. Two deliberate exceptions, both opt-out-able: the scalar aliases
  (`u8`..`isize`, behind `MACH_INT_DEFINED`; `b32`, behind `MACH_B32_DEFINED`)
  are unprefixed on purpose, and the embedded libraries export their own
  `RGFW_*` / `Clay_*` / `stbi_*` / `GL_*` names.
- **2D on purpose.** One shader, textured vertex-colored triangles, painter's
  order. Isometric is a coordinate transform, not a projection. 3D can come
  back when something concrete needs it.
- **A claim in these docs is a claim the code keeps.** `tests/` runs in CI on
  all three platforms, and where a number is quoted, something reproduces it.

## Embedded third parties

Each is vendored verbatim (license text intact) as a pristine file under
`vendor/`, and stitched into `mach.h` by the amalgamation step below. See
LICENSE for the notices and `vendor/README.md` for how to update one.

| library | what for | license | author |
|---|---|---|---|
| [RGFW](https://github.com/ColleagueRiley/RGFW) | windowing, input, GL context | zlib | ColleagueRiley |
| [Clay](https://github.com/nicbarker/clay) (v0.14) | UI layout | zlib | Nic Barker |
| [stb_image](https://github.com/nothings/stb) (v2.30) | image loading | public domain / MIT | Sean Barrett |

## Working on the engine

`mach.h` is a **generated, committed artifact**: one file to ship, so consumers
still just copy it and compile. You don't develop in it. The sources are:

```
src/       mach's own code (~2.7k lines), split into parts
tests/     unit tests: everything that runs without a GL context
vendor/    the three dependencies, pristine upstream bodies
scripts/amalgamate.sh    stitches src/ + vendor/ -> mach.h (order in scripts/manifest.txt)
```

The other ~92% of `mach.h`'s size is those three vendored libraries; mach's own
code is the ~2.7k lines in `src/`. Edit `src/` (or drop a new release into
`vendor/`), then regenerate:

```
scripts/amalgamate.sh          # rewrite mach.h from the parts
scripts/check_generated.sh     # verify mach.h matches src/ + vendor/ (run in CI)
scripts/check_namespace.sh     # namespace guard against OS-header collisions
./nob test                     # build and run the unit tests (run in CI)
```

`tests/test_core.c` defines `MACH_IMPLEMENTATION`, so it can reach the header's
internals directly, and it never opens a window — so it runs headless. It covers
the arena (including the out-of-memory path, via the `MACH_MALLOC` hook), the
math and iso transforms, the color helpers, and the font atlas.

`check_generated.sh` fails if `mach.h` was hand-edited or left stale after a
part changed. See `src/README.md` and `vendor/README.md` for the part layout and
the library-update steps, and `ARCHITECTURE.md` for how the engine is built
internally (the renderer, memory, the frame loop, the no-global-state rule).

## Versioning

Current version: **v0.2.1**. What changed in each release is in
[CHANGELOG.md](CHANGELOG.md), which is also what the release pages publish.

mach.h grew out of (and still powers) a factory-builder game; the engine now
lives here on its own. History from before the split is in that project's log.

## License

zlib. See LICENSE, which also carries the embedded libraries' notices.
