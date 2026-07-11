// mach.h: a small 2D game engine in one header. Plain C, zlib license.
//
// >>> GENERATED FILE: do not edit mach.h by hand. <<<
// It is stitched from src/ (mach's own code) and vendor/ (the embedded
// third-party libraries) by scripts/amalgamate.sh. Edit the parts and rerun
// that script; direct edits here are overwritten on the next regenerate.
//
// RGFW opens the window and delivers input; on top of that sits mach's own
// OpenGL 3.3 core batch renderer (one shader, one draw stream), a bitmap font,
// stb_image loading, a Clay UI binding, arenas, and the frame loop.
//
// Usage: include this header wherever the engine is used; in exactly ONE
// translation unit define the implementation first:
//
//     #define MACH_IMPLEMENTATION
//     #include "mach.h"
//
// mach.h is self-contained: its three dependencies are embedded in full, each
// in a clearly marked section with its license text intact:
//   RGFW       windowing/input/GL context    zlib           ColleagueRiley
//   Clay       UI layout                     zlib           Nic Barker
//   stb_image  image loading                 public domain  Sean Barrett
//
// The engine holds no mutable global state: everything lives in the Mach /
// Mach_Renderer / Mach_Arena structs the caller owns and passes by pointer. That is what
// makes the hot-reload dev loop work; keep it that way.
//
// License: zlib, at the bottom of this file.

#ifndef MACH_H
#define MACH_H
