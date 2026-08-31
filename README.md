# mach.h

A small 2D game engine in one C header. Copy `mach.h` into a project and
compile it with the platform windowing and OpenGL libraries.

The API is under active development and can change between releases.

[![Linux](https://github.com/vetr0s/mach.h/actions/workflows/linux.yml/badge.svg)](https://github.com/vetr0s/mach.h/actions/workflows/linux.yml)
[![macOS](https://github.com/vetr0s/mach.h/actions/workflows/macos.yml/badge.svg)](https://github.com/vetr0s/mach.h/actions/workflows/macos.yml)
[![Windows](https://github.com/vetr0s/mach.h/actions/workflows/windows.yml/badge.svg)](https://github.com/vetr0s/mach.h/actions/workflows/windows.yml)

The header includes windowing, input, an OpenGL 3.3 batch renderer, sprite
atlases, an 8 by 8 bitmap font, image loading, Clay UI layout, arena allocators,
and a frame loop.

```c
#define MACH_IMPLEMENTATION
#include "mach.h"

int main(void) {
    Mach m = {0};
    if (!mach_init(&m, (Mach_Config){ .title = "game" })) return 1;

    while (mach_running(&m)) {
        mach_frame_begin(&m);
        mach_r2d_text(&m.r2d, 20, 20, 2, "hello", MACH_COLOR_FG_MAIN);
        mach_frame_end(&m);
    }

    mach_shutdown(&m);
    return 0;
}
```

Define `MACH_IMPLEMENTATION` in exactly one translation unit. A zeroed
`Mach_Config` opens a resizable 1280 by 720 window with a black clear color.
Frame state is available through `Mach`, including input, delta time, frame
rate, frame time, and the renderer.

## Build

```text
macOS:   clang -std=c99 game.c -framework Cocoa -framework CoreVideo -framework IOKit -framework OpenGL
Linux:   clang -std=c99 game.c -lX11 -lXrandr -lGL -lm -ldl
Windows: cl /std:c11 game.c /link opengl32.lib winmm.lib
```

CI uses C99 with clang and gcc. It uses C11 with MSVC. The logging macros use
the widely supported `##__VA_ARGS__` extension, so the full header is not strict
C99 and is not clean under `-pedantic`.

Linux support uses X11. Pure Wayland sessions need XWayland. Install the X11 and
OpenGL development headers once:

```sh
# Void
sudo xbps-install -S libX11-devel libXrandr-devel libXcursor-devel libXext-devel libXi-devel libglvnd-devel

# Debian and Ubuntu
sudo apt install libx11-dev libxrandr-dev libxcursor-dev libxext-dev libxi-dev libgl1-mesa-dev

# Fedora
sudo dnf install libX11-devel libXrandr-devel libXcursor-devel libXext-devel libXi-devel mesa-libGL-devel

# Arch
sudo pacman -S libx11 libxrandr libxcursor libxext libxi mesa
```

## Examples

`examples/hello.c` opens a window and moves a square. `examples/pong.c` is a
one-player pong game. `examples/atlas.c` draws thousands of sprites from one
atlas and reports the draw-call count.

Build every example with [nob](https://github.com/tsoding/nob.h):

```sh
cc -o nob nob.c
./nob
```

The binaries are written to `examples/build/`. Set `CC` to select another
compiler. The default is clang, or MSVC on Windows.

## Development

`mach.h` is generated from the files in `src/` and `vendor/`. Edit those files,
then regenerate and verify the committed header:

```sh
scripts/amalgamate.sh
scripts/check_generated.sh
scripts/check_namespace.sh
./nob test
```

The tests cover code that runs without an OpenGL context and run on all three
supported platforms in CI. [ARCHITECTURE.md](ARCHITECTURE.md) explains the
renderer, memory model, frame loop, and no-global-state rule.

## Embedded libraries

| Library | Use | License |
| --- | --- | --- |
| [RGFW](https://github.com/ColleagueRiley/RGFW) | Windowing, input, OpenGL context | zlib |
| [Clay](https://github.com/nicbarker/clay) v0.14 | UI layout | zlib |
| [stb_image](https://github.com/nothings/stb) v2.30 | Image loading | Public domain or MIT |

The vendored source and license text live under `vendor/`.

## Version

The current version is **v0.2.2**. See [CHANGELOG.md](CHANGELOG.md) for release
history.

## License

mach.h uses the zlib license. [LICENSE](LICENSE) also contains the notices for
the embedded libraries.
