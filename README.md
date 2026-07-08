# mach.h

A small 2D game engine in one header. C99, zlib license.

One file is the whole engine: windowing and input (RGFW, embedded), an OpenGL
3.3 core batch renderer, an 8x8 bitmap font, image loading (stb_image,
embedded), a UI layout binding (Clay, embedded), arena allocators, and a frame
loop. There is nothing to install and nothing else to download: copy `mach.h`
into your project and compile.

```c
#define MACH_IMPLEMENTATION
#include "mach.h"

int main(void) {
    Mach m = {0};
    if (!mach_init(&m, (Mach_Config){ .title = "game" })) return 1;

    while (mach_running(&m)) {
        mach_frame_begin(&m);   // events -> m.input, m.dt, clear
        mach_r2d_text(&m.r2d, 20, 20, 2, "hello", MACH_COLOR_FG_MAIN);
        mach_frame_end(&m);     // present, m.fps, frame cap
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

On Linux you need the X11/GL dev headers once:

```
Void:          sudo xbps-install -S libX11-devel libXrandr-devel libXcursor-devel libglvnd-devel
Debian/Ubuntu: sudo apt install libx11-dev libxrandr-dev libxcursor-dev libgl1-mesa-dev
Fedora:        sudo dnf install libX11-devel libXrandr-devel libXcursor-devel mesa-libGL-devel
Arch:          sudo pacman -S libx11 libxrandr libxcursor mesa
```

`examples/hello.c` is the snippet above with a moving square; `examples/build.sh`
compiles it.

## What's inside

The header reads top to bottom as sections:

```
base, debug        sized ints (u8..b32), MACH_LOG_* / MACH_DEBUG_ASSERT
RGFW (embedded)    windowing, input events, GL context
mem                arena allocator (region list, whole-arena free/reset)
math               Mach_Vec2 + scalar helpers (mach_clamp, mach_lerp, ...)
color              Mach_Color + a stock palette (modus-vivendi), MACH_COLOR_*
gl                 the ~40 GL 3.3 core entry points, declared by hand, loaded at runtime
font               8x8 bitmap font baked into a GL texture atlas
image (stb)        mach_image_load / mach_image_free
render2d           the batch renderer: one shader, one draw stream (mach_r2d_*),
                   plus an isometric camera and transforms
input              per-frame snapshot: key/mouse down, pressed, released, wheel
clay_ui (embedded) Clay layout bound to the renderer (mach_clay_ui_*)
core               mach_init / mach_running / mach_frame_begin / mach_frame_end / mach_shutdown
```

Design rules the header holds itself to:

- **You own the loop.** The engine never drives anything; it's a toolbox of
  calls your `main()` makes. raylib-style.
- **No mutable global state.** Everything lives in the `Mach` struct you own
  and pass by pointer. This is what makes hot-reload schemes work: two copies
  of the engine code can operate on the same data.
- **Everything is namespaced.** Types are `Mach_*`, functions `mach_*`, macros
  `MACH_*`. Nothing the header exports can collide with OS headers (X11's
  `Font`, windows.h macros) or with your code. `scripts/check_namespace.sh`
  enforces this against faked X11/Win32 names.
- **2D on purpose.** One shader, textured vertex-colored triangles, painter's
  order. Isometric is a coordinate transform, not a projection. 3D can come
  back when something concrete needs it.

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
src/       mach's own code (~1.9k lines), split into parts
vendor/    the three dependencies, pristine upstream bodies
scripts/amalgamate.sh    stitches src/ + vendor/ -> mach.h (order in scripts/manifest.txt)
```

The other ~94% of `mach.h`'s size is those three vendored libraries; mach's own
code is the ~1.9k lines in `src/`. Edit `src/` (or drop a new release into
`vendor/`), then regenerate:

```
scripts/amalgamate.sh          # rewrite mach.h from the parts
scripts/check_generated.sh     # verify mach.h matches src/ + vendor/ (run in CI)
scripts/check_namespace.sh     # namespace guard against OS-header collisions
```

`check_generated.sh` fails if `mach.h` was hand-edited or left stale after a
part changed. See `src/README.md` and `vendor/README.md` for the part layout and
the library-update steps, and `ARCHITECTURE.md` for how the engine is built
internally (the renderer, memory, the frame loop, the no-global-state rule).

## Versioning

Current version: **v0.1.2**

mach.h grew out of (and still powers) a factory-builder game; the engine now
lives here on its own. History from before the split is in that project's log.

## License

zlib. See LICENSE, which also carries the embedded libraries' notices.
