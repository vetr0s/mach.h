# mach.h: engine roadmap

Engine work only. The game that drives these requirements has its own repo and
its own TODO.

## Next

The game is starting real sprite work, so these are the engine's critical path.

- [x] Image decode from memory (`mach_image_load_from_memory`, landing now):
      lets a consumer bake assets into the executable instead of shipping a
      directory of PNGs next to the binary. stb_image's `stbi_load_from_memory`
      was already embedded but sat behind `MACH_IMPLEMENTATION`, so it was not
      public API; a consumer reaching for it directly would be calling a vendored
      symbol and breaking the one-way engine/consumer dependency. A companion
      `mach_r2d_texture_from_memory` (decode + upload in one step) is landing
      alongside it.
- [x] Sprite batching / atlas support (`Mach_R2D_Atlas`, `Mach_R2D_Region`,
      landed in v0.2.0): shelf-packed atlas, one draw call for many sprites. The
      subtler half was `Mach_Renderer.white` becoming a region, so untextured
      fills batch with whatever atlas is in force instead of splitting the batch
      on every rect. 1500 sprites + a HUD is 2 draw calls.
- [ ] Asset loading pipeline: the broader item the above feeds into. The atlas is
      the runtime half; what's missing is the packaging half (a manifest, baked
      blobs, hot-reload of art).

## Core
- [ ] Event system beyond the input snapshot (collision events, user-defined events)
- [ ] Generational handles for the arena-backed object patterns (stale-id safety).
      Atlas regions are values that dangle when their atlas is destroyed, which is
      the first concrete case that wants this.

## Debugging
- [ ] Debug draw: collision bounds, vectors
- [ ] Performance profiler. `Mach_Renderer.draw_calls` and `Mach.frame_ms` /
      `frame_ms_peak` are the beginnings of one.

## Platform

All three build in CI on every push (linux.yml / macos.yml / windows.yml). The
checkbox below tracks running on real hardware, which is a separate thing from
building.

Running the game (which embeds this engine) on a platform exercises that
platform's backend, so the checkboxes below track the game's v0.6.2 binaries.

- [x] macOS: builds in CI, run on real hardware
- [x] Linux: builds in CI, run on real hardware (X11 path; needs libxi-dev and
      libxext-dev for the XInput2 and shape headers; deps preflight and namespace
      guard are in place)
- [x] Windows: builds in CI, run on real hardware (MSVC; GL loader falls back to
      GetProcAddress for GL 1.1 entry points, timing uses QPC/Sleep)

## Someday / speculative

Ideas, not commitments. Kept here so they are not forgotten, not because they
are scheduled. Each returns only when a concrete need pulls it in.

- [ ] Real 3D: only when there's a concrete need and the GPU grasp to own it
- [ ] 3D vector/matrix math: returns with 3D, if it does
- [ ] Audio system
- [ ] Sound and music loading
- [ ] Spatial audio
- [ ] Embedded Lua integration
- [ ] Script hot-reload
