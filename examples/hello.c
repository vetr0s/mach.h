// The README snippet, made visible: a window, a moving square, text, and the
// FPS readout. Build it with examples/build.sh (or by hand, see the README).

#define MACH_IMPLEMENTATION
#include "mach.h"

#include <stdio.h>

int main(void) {
    Mach m = {0};
    if (!mach_init(&m, (Mach_Config){ .title = "hello mach" })) return 1;

    f32 x = 0.0f;
    while (mach_running(&m)) {
        mach_frame_begin(&m);

        // A square drifting right at 120 px/s, wrapping at the window edge.
        x += 120.0f * m.dt;
        if (x > (f32)m.r2d.width) x = -80.0f;
        mach_r2d_fill_rect(&m.r2d, x, (f32)m.r2d.height * 0.5f - 40.0f, 80.0f, 80.0f,
                           MACH_COLOR_BLUE);

        char fps[32];
        snprintf(fps, sizeof fps, "fps %d", m.fps);
        mach_r2d_text(&m.r2d, 20, 20, 2, "hello, mach", MACH_COLOR_FG_MAIN);
        mach_r2d_text(&m.r2d, 20, 44, 1, fps, MACH_COLOR_FG_DIM);

        mach_frame_end(&m);
    }

    mach_shutdown(&m);
    return 0;
}
