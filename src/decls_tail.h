
// Clay UI binding: layout by Clay (third_party/clay), drawing by our 2D renderer.
//
// Clay does the UI layout and hands back a list of render commands; mach_clay_ui_render
// walks that list and draws each with r2d. Clay keeps an internal "current context"
// global plus callback pointers, so those are re-pointed every frame in mach_clay_ui_begin
// to survive hot reload (the reloaded library's globals start empty). The context data
// itself lives in a malloc'd block held in host-owned App memory, so it persists across
// a code swap.

typedef struct {
    Clay_Context *ctx;
    void *memory; // malloc backing for Clay's arena (survives hot reload)
    b32 ready;
} Mach_ClayUI;

// One-time setup: allocate Clay's arena and initialize it against the renderer's size.
b32 mach_clay_ui_init(Mach_ClayUI *ui, Mach_Renderer *r);
void mach_clay_ui_shutdown(Mach_ClayUI *ui);

// Per-frame. Call mach_clay_ui_begin, declare the layout with CLAY(...) / CLAY_TEXT(...),
// then mach_clay_ui_render to draw it. `mouse`/`mouse_down` feed Clay's pointer state for
// hover/click handling (pass zero/false when there's nothing interactive yet).
void mach_clay_ui_begin(Mach_ClayUI *ui, Mach_Renderer *r, Clay_Vector2 mouse, b32 mouse_down);
void mach_clay_ui_render(Mach_ClayUI *ui, Mach_Renderer *r);

// A Clay_String over a null-terminated C string. The chars are not copied, so they
// must outlive this frame's mach_clay_ui_render call.
static inline Clay_String mach_clay_string(const char *s) {
    return (Clay_String){.length = (i32)strlen(s), .chars = s};
}

// An engine Mach_Color ([0,1] RGBA) in Clay's 0-255 convention, so the palette in
// render/color.h works for UI declarations too.
static inline Clay_Color mach_clay_color_of(Mach_Color c) {
    return (Clay_Color){c.x * 255.0f, c.y * 255.0f, c.z * 255.0f, c.w * 255.0f};
}

// =============================================================================
// core: engine lifecycle and the frame loop
// =============================================================================

// Core engine lifecycle and the frame loop. The game owns the loop and calls
// three functions; everything a frame produces (input, dt, fps) is read off
// the Mach struct. This is the whole program:
//
//     Mach m = {0};
//     if (!mach_init(&m, (Mach_Config){ .title = "game" })) return 1;
//     while (mach_running(&m)) {
//         mach_frame_begin(&m);      // drain events into m.input, set m.dt, clear
//         ...update from m.input / m.dt, draw through &m.r2d...
//         mach_frame_end(&m);        // present, sample fps/frame_ms, pace the frame
//     }
//     mach_shutdown(&m);

// Window setup plus the per-frame policy. Zeroed fields get defaults, so
// (Mach_Config){0} is a valid 1280x720 resizable window titled "mach" with a
// black clear, Escape reaching the game, and frames paced by vsync.
//
// Pacing, which is three fields talking to each other, resolved in this order:
//
//   default                  vsync: the display paces the loop at its own rate,
//                            no tearing, no busy-wait. What most games want.
//   target_fps > 0           vsync off, and the loop paces itself to that rate
//                            with its own deadline cap. For running off the
//                            display's rate on purpose: a 30fps lock, a capture,
//                            a benchmark above the refresh rate.
//   vsync_off, no target_fps uncapped. Frames as fast as the machine will make
//                            them, most of them never shown. A measurement tool,
//                            not a way to ship: read Mach.frame_ms for headroom
//                            instead of counting frames nobody sees.
typedef struct {
    const char *title; // NULL: "mach"
    i32 width, height; // <= 0: 1280x720
    b32 fullscreen;
    b32 fixed_size; // window can't be resized (default: resizable)

    Mach_Color clear_color; // frame clear color; zero alpha means opaque black
    b32 escape_quits;       // Escape closes the window (dev convenience); otherwise
                            // Escape reaches the game through the input snapshot
    i32 target_fps;         // self-paced frame cap; implies vsync_off. <= 0: use vsync
    b32 vsync_off;          // don't sync to the display (see the pacing table above)
} Mach_Config;

typedef struct {
    // What a frame is made of. The game reads these directly.
    Mach_Renderer r2d; // draw through this: mach_r2d_fill_rect(&m.r2d, ...)
    Mach_Input input;  // this frame's input snapshot, filled by mach_frame_begin
    f32 dt;            // seconds since the previous frame (clamped, so a stall
                       // can't produce a giant simulation step)
    i32 fps;           // frames counted over the last completed 1s window

    // What a frame cost, in milliseconds: the work (update, draw, present) with
    // the frame cap's wait excluded. This, not fps, is the headroom number --
    // under a 60fps cap fps reads 60 whether a frame takes 2ms or 16ms, while
    // frame_ms keeps telling the truth. frame_ms_peak is the worst frame of the
    // last completed 1s window, which is where a hitch shows up that the average
    // buries.
    f32 frame_ms;
    f32 frame_ms_peak;

    // Per-frame scratch: reset at every mach_frame_begin, so anything allocated
    // from it lives exactly one frame (sort buffers, transient strings). The
    // regions are reused, not freed, so steady-state allocation is malloc-free.
    Mach_Arena frame_arena;

    // Internals: window handle, policy copied out of Mach_Config, frame timing.
    RGFW_window *window;
    b32 running;
    Mach_Color clear_color;
    b32 escape_quits;
    u64 frame_cap_ns;      // 0 = uncapped; nanoseconds per frame at the target rate
    u64 frame_start;       // tick (ns) at the current frame's start (for frame_ms)
    u64 frame_deadline;    // tick (ns) the current frame is paced to; advances by
                           // frame_cap_ns per frame, so sleep error can't accumulate
    u64 last_frame_time;   // tick (ns) at the previous frame's start (for dt)
    u64 fps_timer;         // tick (ns) at the start of the current 1s FPS window
    i32 frame_count;       // frames seen in the current window
    f32 frame_ms_peak_acc; // worst frame_ms so far in the current window
} Mach;

// Open the window with a GL 3.3 core context and bring up the renderer.
b32 mach_init(Mach *m, Mach_Config cfg);
void mach_shutdown(Mach *m);

// True until a quit is requested (window close, or Escape with escape_quits).
b32 mach_running(const Mach *m);

// Start a frame: reset the frame arena, drain events into m->input (consuming
// window lifecycle: quit, Escape, resize), set m->dt, clear the screen.
void mach_frame_begin(Mach *m);

// Finish a frame: present, sample fps and frame_ms, and wait out the frame's
// pacing (vsync blocks in the present; a target_fps waits on its deadline).
void mach_frame_end(Mach *m);

// Monotonic milliseconds from an arbitrary origin; wraps every ~49 days, so
// only differences are meaningful.
u32 mach_ticks_ms(void);

#endif // MACH_H

// ///////////////////////////////////////////////////////////////////////////////
//
//                                IMPLEMENTATION
//
// ///////////////////////////////////////////////////////////////////////////////

#if defined(MACH_IMPLEMENTATION) && !defined(MACH_IMPLEMENTATION_INCLUDED)
#define MACH_IMPLEMENTATION_INCLUDED
