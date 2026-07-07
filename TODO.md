# mach.h — engine roadmap

Engine work only. The game that drives these requirements has its own repo and
its own TODO.

## Core
- [ ] Event system beyond the input snapshot (collision events, user-defined events)
- [ ] Generational handles for the arena-backed object patterns (stale-id safety)

## Rendering
- [ ] Sprite batching / atlas support for many entities
- [ ] (later) Real 3D — only when there's a concrete need and the GPU grasp to own it

## Math
- [ ] 3D vector/matrix math — returns with 3D, if it does

## Content
- [ ] Asset loading pipeline

## Debugging
- [ ] Debug draw: collision bounds, vectors
- [ ] Performance profiler

## Audio (later)
- [ ] Audio system
- [ ] Sound and music loading
- [ ] Spatial audio

## Scripting (future)
- [ ] Embedded Lua integration
- [ ] Script hot-reload

## Platform
- [x] macOS: building and running
- [ ] Linux: verify on real hardware (X11 path; deps preflight and namespace
      guard are in place, needs an actual build + run)
- [ ] Windows: verify on real hardware (MSVC; GL loader falls back to
      GetProcAddress for GL 1.1 entry points, timing uses QPC/Sleep)
