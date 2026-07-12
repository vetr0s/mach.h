// mach.h unit tests: everything that runs without a GL context.
//
// This TU defines MACH_IMPLEMENTATION, so it sees the header's internals (GLYPHS,
// mach_region_new, MACH_ARENA_REGION_CAPACITY) directly. That is deliberate: it
// lets the tests reach the font table and the arena's growth path without adding
// test hooks to the engine.
//
// It links the platform windowing/GL libraries because RGFW's implementation is
// compiled in, but it never calls RGFW_init, so it needs no display and runs on a
// headless CI box.
//
//   ./nob test        builds and runs this

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

// --- failing allocator ------------------------------------------------------
//
// The whole point of routing the engine through MACH_MALLOC is that a test can
// starve it. `alloc_budget` is the number of allocations still allowed: -1 means
// unlimited, 0 means the next one fails.

static int alloc_budget = -1;

static void *test_malloc(size_t size) {
    if (alloc_budget == 0)
        return NULL;
    if (alloc_budget > 0)
        alloc_budget--;
    return malloc(size);
}

static void *test_calloc(size_t count, size_t size) {
    if (alloc_budget == 0)
        return NULL;
    if (alloc_budget > 0)
        alloc_budget--;
    return calloc(count, size);
}

#define MACH_MALLOC(size) test_malloc(size)
#define MACH_CALLOC(count, size) test_calloc(count, size)
#define MACH_FREE(ptr) free(ptr)

#define MACH_IMPLEMENTATION
#include "mach.h"

// --- harness ----------------------------------------------------------------

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        checks++;                                                                                  \
        if (!(cond)) {                                                                             \
            failures++;                                                                            \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                      \
        }                                                                                          \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                                      \
    do {                                                                                           \
        checks++;                                                                                  \
        if (fabsf((float)(a) - (float)(b)) > (eps)) {                                              \
            failures++;                                                                            \
            fprintf(stderr, "  FAIL %s:%d: %s (%f) != %s (%f)\n", __FILE__, __LINE__, #a,          \
                    (double)(a), #b, (double)(b));                                                 \
        }                                                                                          \
    } while (0)

static void section(const char *name) {
    printf("-- %s\n", name);
}

// --- mem --------------------------------------------------------------------

static void test_arena_basics(void) {
    section("arena: basics");

    Mach_Arena a = {0};

    // A zeroed arena is a valid empty arena: the first alloc brings up the region.
    void *p = mach_arena_alloc(&a, 1);
    CHECK(p != NULL);
    CHECK(a.begin != NULL);
    CHECK(a.end == a.begin);

    // Every allocation is word-aligned, whatever the requested size.
    for (usize size = 1; size <= 64; size++) {
        void *q = mach_arena_alloc(&a, size);
        CHECK(q != NULL);
        CHECK(((uintptr_t)q % sizeof(uintptr_t)) == 0);
    }

    // A request bigger than the default region gets a region sized to fit, rather
    // than failing or silently truncating.
    usize huge = MACH_ARENA_REGION_CAPACITY * sizeof(uintptr_t) * 2;
    u8 *big = (u8 *)mach_arena_alloc(&a, huge);
    CHECK(big != NULL);
    if (big) {
        memset(big, 0x5A, huge); // must be fully writable
        CHECK(big[0] == 0x5A);
        CHECK(big[huge - 1] == 0x5A);
    }

    mach_arena_free(&a);
    CHECK(a.begin == NULL);
    CHECK(a.end == NULL);

    // Freeing twice is not a crash.
    mach_arena_free(&a);
    CHECK(a.begin == NULL);
}

static void test_arena_reset(void) {
    section("arena: reset reuses regions");

    Mach_Arena a = {0};

    // Spill past one region so there is a chain to walk on reuse.
    for (int i = 0; i < 4000; i++)
        CHECK(mach_arena_alloc(&a, 64) != NULL);

    Mach_Arena_Region *begin = a.begin;
    mach_arena_reset(&a);

    CHECK(a.begin == begin); // reset keeps the memory ...
    CHECK(a.end == begin);   // ... and rewinds to the first region
    CHECK(a.begin->count == 0);

    // Reuse must not allocate: the regions are already there.
    alloc_budget = 0;
    void *p = mach_arena_alloc(&a, 64);
    alloc_budget = -1;
    CHECK(p != NULL);

    mach_arena_free(&a);
}

// The regression test for the OOM path. Before the fix, a failed region allocation
// left `end` NULL while `begin` still pointed at the live chain; the *next* alloc
// took the empty-arena branch and overwrote `begin`, orphaning every region and
// every pointer the caller was still holding.
static void test_arena_oom_keeps_arena_intact(void) {
    section("arena: a failed alloc leaves the arena usable");

    Mach_Arena a = {0};

    alloc_budget = 1; // exactly one region, then the tap runs dry
    u8 *first = (u8 *)mach_arena_alloc(&a, 64);
    CHECK(first != NULL);
    if (!first)
        return;
    memset(first, 0xAB, 64);

    Mach_Arena_Region *begin = a.begin;

    // Force a second region. The allocator refuses.
    usize huge = MACH_ARENA_REGION_CAPACITY * sizeof(uintptr_t);
    CHECK(mach_arena_alloc(&a, huge) == NULL);

    // The arena must still be coherent: the failure is reported, not absorbed.
    CHECK(a.begin == begin);
    CHECK(a.end != NULL);

    // And the next allocation must not walk over the chain. This is the assertion
    // that fails on the pre-fix code.
    alloc_budget = -1;
    void *next = mach_arena_alloc(&a, 64);
    CHECK(next != NULL);
    CHECK(a.begin == begin);

    // The pointer handed out before the failure is still ours, and still intact.
    CHECK(first[0] == 0xAB);
    CHECK(first[63] == 0xAB);

    mach_arena_free(&a);
}

// --- math -------------------------------------------------------------------

static void test_math(void) {
    section("math");

    CHECK_NEAR(mach_min(2.0f, -3.0f), -3.0f, 1e-6f);
    CHECK_NEAR(mach_max(2.0f, -3.0f), 2.0f, 1e-6f);

    CHECK_NEAR(mach_clamp(5.0f, 0.0f, 1.0f), 1.0f, 1e-6f);
    CHECK_NEAR(mach_clamp(-5.0f, 0.0f, 1.0f), 0.0f, 1e-6f);
    CHECK_NEAR(mach_clamp(0.25f, 0.0f, 1.0f), 0.25f, 1e-6f);

    CHECK_NEAR(mach_lerp(0.0f, 10.0f, 0.0f), 0.0f, 1e-6f);
    CHECK_NEAR(mach_lerp(0.0f, 10.0f, 1.0f), 10.0f, 1e-6f);
    CHECK_NEAR(mach_lerp(0.0f, 10.0f, 0.5f), 5.0f, 1e-6f);

    Mach_Vec2 a = {3.0f, 4.0f};
    CHECK_NEAR(mach_vec2_length(a), 5.0f, 1e-6f);
    CHECK_NEAR(mach_vec2_dot(a, a), 25.0f, 1e-5f);

    Mach_Vec2 n = mach_vec2_normalize(a);
    CHECK_NEAR(mach_vec2_length(n), 1.0f, 1e-6f);

    // Normalizing the zero vector must not divide by zero.
    Mach_Vec2 z = mach_vec2_normalize((Mach_Vec2){0.0f, 0.0f});
    CHECK_NEAR(z.x, 0.0f, 1e-6f);
    CHECK_NEAR(z.y, 0.0f, 1e-6f);
}

// mach_screen_to_iso inverts the elev-0 ground plane by construction, so that is
// the only round-trip there is. An elevated point does NOT round-trip, and that is
// correct behavior, not a bug -- see the note on mach_screen_to_iso.
static void test_iso_ground_plane_roundtrip(void) {
    section("iso: ground-plane round-trip");

    const f32 w = 1280.0f, h = 720.0f;
    Mach_Camera2D cams[] = {
        {{0.0f, 0.0f}, 1.0f},
        {{120.0f, -80.0f}, 1.0f},
        {{-40.0f, 210.0f}, 2.5f},
        {{15.5f, 15.5f}, 0.35f},
    };

    for (usize c = 0; c < MACH_ARRAY_COUNT(cams); c++) {
        for (f32 gx = -8.0f; gx <= 8.0f; gx += 4.0f) {
            for (f32 gy = -8.0f; gy <= 8.0f; gy += 4.0f) {
                Mach_Vec2 s = mach_iso_to_screen(&cams[c], w, h, gx, gy, 0.0f);
                Mach_Vec2 g = mach_screen_to_iso(&cams[c], w, h, s.x, s.y);
                CHECK_NEAR(g.x, gx, 1e-3f);
                CHECK_NEAR(g.y, gy, 1e-3f);
            }
        }
    }
}

// --- color ------------------------------------------------------------------

static void test_color(void) {
    section("color");

    Mach_Color c = MACH_COLOR_HEX(0x336699);
    CHECK_NEAR(c.x, 0x33 / 255.0f, 1e-6f); // r
    CHECK_NEAR(c.y, 0x66 / 255.0f, 1e-6f); // g
    CHECK_NEAR(c.z, 0x99 / 255.0f, 1e-6f); // b
    CHECK_NEAR(c.w, 1.0f, 1e-6f);          // opaque

    Mach_Color a = MACH_COLOR_HEX(0x000000);
    Mach_Color b = MACH_COLOR_HEX(0xFFFFFF);
    Mach_Color lo = mach_color_lerp(a, b, 0.0f);
    Mach_Color hi = mach_color_lerp(a, b, 1.0f);
    CHECK_NEAR(lo.x, 0.0f, 1e-6f);
    CHECK_NEAR(hi.x, 1.0f, 1e-6f);

    CHECK_NEAR(mach_color_alpha(b, 0.5f).w, 0.5f, 1e-6f);
}

// --- font -------------------------------------------------------------------

// mach_font_glyph_uv returns MACH_TRUE for every printable ASCII code, so a blank
// cell in GLYPHS is an undetectable hole: the text just renders a space and nobody
// finds out. This asserts the table is actually complete, which is what makes the
// header's "8x8 bitmap font" claim true.
static void test_font_covers_printable_ascii(void) {
    section("font: every printable ASCII glyph is drawn");

    for (int ch = MACH_FONT_FIRST_CHAR; ch <= MACH_FONT_LAST_CHAR; ch++) {
        int idx = ch - MACH_FONT_FIRST_CHAR;

        int blank = 1;
        for (int row = 0; row < MACH_FONT_CELL; row++) {
            if (GLYPHS[idx][row] != 0) {
                blank = 0;
                break;
            }
        }

        checks++;
        if (blank != (ch == ' ')) {
            failures++;
            fprintf(stderr, "  FAIL %s:%d: glyph '%c' (0x%02X) is %s\n", __FILE__, __LINE__, ch, ch,
                    blank ? "blank but should be drawn" : "drawn but should be blank");
        }
    }

    // The UV lookup agrees on the range it claims to cover.
    f32 u0, v0, u1, v1;
    CHECK(mach_font_glyph_uv(NULL, 'A', &u0, &v0, &u1, &v1) == MACH_TRUE);
    CHECK(u1 > u0 && v1 > v0);
    CHECK(mach_font_glyph_uv(NULL, '\n', &u0, &v0, &u1, &v1) == MACH_FALSE);
    CHECK(mach_font_glyph_uv(NULL, (char)0x7F, &u0, &v0, &u1, &v1) == MACH_FALSE);
}

// The white block that untextured draws sample lives in the font sheet's spare cell.
// Two things have to hold or every fill_rect in the engine is subtly wrong, and both
// are invisible without a GPU -- which is exactly why they are checked here.
static void test_font_white_block(void) {
    section("font: the white block untextured draws sample");

    u32 *px = mach_font_build_pixels();
    CHECK(px != NULL);
    if (!px)
        return;

    // A texture id is all mach_font_white_region reads.
    Mach_R2D_Texture atlas = {1, (f32)MACH_FONT_SHEET_W, (f32)MACH_FONT_SHEET_H};
    Mach_R2D_Region white = mach_font_white_region(atlas);

    // 1. The UVs are collapsed to a single point. If they spanned the cell, the sample
    //    at a quad's edge could round into the transparent gutter and every filled rect
    //    would wear a 1px fringe.
    CHECK(white.u0 == white.u1);
    CHECK(white.v0 == white.v1);

    // 2. That point lands on an opaque white texel. Get the cell wrong and fills would
    //    silently sample a glyph, or nothing at all.
    i32 tx = (i32)(white.u0 * (f32)MACH_FONT_SHEET_W);
    i32 ty = (i32)(white.v0 * (f32)MACH_FONT_SHEET_H);
    CHECK(tx >= 0 && tx < MACH_FONT_SHEET_W);
    CHECK(ty >= 0 && ty < MACH_FONT_SHEET_H);
    CHECK(px[ty * MACH_FONT_SHEET_W + tx] == 0xFFFFFFFFu);

    // And the whole reserved cell is opaque, so there is margin around that sample.
    i32 wx = (MACH_FONT_WHITE_CELL % MACH_FONT_SHEET_COLS) * MACH_FONT_CELL;
    i32 wy = (MACH_FONT_WHITE_CELL / MACH_FONT_SHEET_COLS) * MACH_FONT_CELL;
    for (i32 row = 0; row < MACH_FONT_CELL; row++) {
        for (i32 col = 0; col < MACH_FONT_CELL; col++) {
            CHECK(px[(wy + row) * MACH_FONT_SHEET_W + (wx + col)] == 0xFFFFFFFFu);
        }
    }

    // The block must not have eaten a glyph: it sits past the last one.
    CHECK(MACH_FONT_WHITE_CELL >= MACH_FONT_GLYPH_COUNT);
    CHECK(MACH_FONT_WHITE_CELL < MACH_FONT_SHEET_COLS * MACH_FONT_SHEET_ROWS);

    MACH_FREE(px);
}

// --- pacing -----------------------------------------------------------------
//
// ARCHITECTURE.md makes a specific claim about the frame cap: that anchoring to an
// absolute deadline holds the target rate, where sleeping the remainder of each frame
// drifts below it (a 60 cap measuring 56.7). Nothing reproduced that number, which put
// it in the same category as the claims v0.2.0 spent its time fixing. These tests are
// the reproduction. They need no window: the cap is arithmetic plus a wait.

// The arithmetic half. Deterministic, so these are hard assertions.
static void test_pacing_deadline_arithmetic(void) {
    section("pacing: the deadline advances without drifting");

    const u64 cap = 16666666ull; // 60 fps

    // A frame that finishes early advances the deadline by exactly one period. It does
    // not get pushed out by however long the frame happened to take.
    u64 deadline = 1000000000ull;
    u64 next = mach_pace_advance(deadline, cap, deadline - 5000000ull);
    CHECK(next == deadline + cap);

    // Over many frames the deadline is the start plus N periods, exactly. This is the
    // no-drift property: a per-frame sleep would accumulate its overshoot here.
    u64 start = 1000000000ull;
    deadline = start;
    u64 now = start;
    for (i32 i = 0; i < 600; i++) {
        deadline = mach_pace_advance(deadline, cap, now);
        now = deadline; // a frame that lands exactly on the mark
    }
    CHECK(deadline == start + 600ull * cap);

    // A frame that blows the budget leaves the deadline in the past. We give up the
    // lost time instead of chasing it...
    deadline = start;
    u64 late = start + cap * 10ull; // a stall ten frames long
    u64 after = mach_pace_advance(deadline, cap, late);
    CHECK(after == late);

    // ...so the frame after a stall gets a full period, not a zero-length catch-up
    // frame. That burst is what "resetting to now" exists to prevent.
    u64 following = mach_pace_advance(after, cap, late);
    CHECK(following == late + cap);
    CHECK(following - late == cap);
}

// Burn `ns` of wall clock without sleeping: a stand-in for a frame's actual work.
static void spin_for_ns(u64 ns) {
    u64 until = mach_ticks_ns() + ns;
    while (mach_ticks_ns() < until) {
    }
}

// The timing half. This one really sleeps, so the hard assertions are the properties
// that must hold on any machine, and the sharp numbers are printed as well -- a loaded
// CI runner has every right to be jittery, and a flaky test guards nothing.
static void test_pacing_holds_the_rate(void) {
    section("pacing: the cap hits its target rate");

    const u64 cap = 16666666ull;  // 60 fps
    const u64 work = 4000000ull;  // 4ms of "frame work", so the two loops differ
    const i32 frames = 30;        // ~0.5s per loop

    // The engine's loop: anchor to an absolute deadline, wait, repeat. This is what
    // mach_frame_end does, minus the drawing.
    u64 t0 = mach_ticks_ns();
    u64 deadline = t0;
    f64 jitter_sum = 0.0;
    f64 jitter_max = 0.0;
    i32 woke_early = 0;

    for (i32 i = 0; i < frames; i++) {
        spin_for_ns(work);

        u64 now = mach_ticks_ns();
        deadline = mach_pace_advance(deadline, cap, now);
        if (now < deadline)
            mach_wait_until_ns(deadline);

        // The wait must never return before its deadline. If it does, the cap is not a
        // cap, and every rate the engine reports is fiction.
        u64 woke = mach_ticks_ns();
        if (woke < deadline)
            woke_early++;

        f64 late_ms = (f64)(woke - deadline) / 1000000.0;
        jitter_sum += late_ms;
        if (late_ms > jitter_max)
            jitter_max = late_ms;
    }
    f64 anchored_fps = (f64)frames / ((f64)(mach_ticks_ns() - t0) / 1000000000.0);
    f64 jitter_mean = jitter_sum / (f64)frames;

    // The naive cap, for contrast: do the work, then sleep one period. The period ends
    // up being work + cap rather than cap, so the rate sits below target no matter how
    // precise the sleep is -- and the shortfall grows with the work. This is what the
    // engine did before v0.1.5, and it is why a 60 cap measured 56.7.
    u64 n0 = mach_ticks_ns();
    for (i32 i = 0; i < frames; i++) {
        spin_for_ns(work);
        mach_wait_until_ns(mach_ticks_ns() + cap);
    }
    f64 naive_fps = (f64)frames / ((f64)(mach_ticks_ns() - n0) / 1000000000.0);

    printf("     target   60.00 fps  (16.67ms period, 4.00ms of work per frame)\n");
    printf("     anchored %6.2f fps  (jitter: %.3f ms mean, %.3f ms max)\n", anchored_fps,
           jitter_mean, jitter_max);
    printf("     naive    %6.2f fps  (work + a full period, no anchor)\n", naive_fps);

    // Only two things are asserted here, and both are true of any host.
    //
    // The absolute numbers above are NOT asserted, deliberately. Jitter is a property of
    // this code *on a host that will schedule it*: a shared CI runner can deschedule the
    // whole VM for 25ms, and no pacing code in userspace survives that. Asserting 0.5ms
    // there measures GitHub's fleet and calls it a regression -- which is exactly what an
    // earlier version of this test did. The precision invariant is guarded instead by
    // test_pacing_sleep_request, which needs no clock at all. Numbers here are reported
    // so a human can read them on a machine that is quiet enough to mean something.

    // The wait never returns early. If it did, the cap would not be a cap.
    CHECK(woke_early == 0);

    // Anchoring beats sleeping a period per frame. This is the claim the absolute
    // deadline exists to make, and being relative it cannot be faked by a slow host: on
    // the runner where the anchored loop managed only 51 fps, the naive one managed 27.
    CHECK(naive_fps < anchored_fps);
}

// The precision half of the cap, guarded without a clock.
//
// The bug this pins down: the wait used to ask for one sleep of (remaining - 1ms) and
// assume it would overshoot by less than that 1ms. But a sleep's overshoot scales with
// its length, so at 60fps it asked for ~15.6ms, overran by ~3.6ms, and landed past the
// deadline -- and the spin that was supposed to put it on the mark never ran. Jitter was
// 1.9ms rather than the advertised 0.01ms.
//
// The fix is an invariant, not a magic number: never ask for a sleep so long that its own
// overshoot can carry you past the deadline. Half of what remains always leaves more slack
// than the error, whatever the host's constant of proportionality is. That is checkable as
// pure arithmetic, on any machine, loaded or not.
static void test_pacing_sleep_request(void) {
    section("pacing: a sleep never gambles more than half the remaining budget");

    // Below the spin margin we don't sleep at all -- we spin.
    CHECK(mach_pace_sleep_request(0) == 0);
    CHECK(mach_pace_sleep_request(MACH_SPIN_MARGIN_NS) == 0);
    CHECK(mach_pace_sleep_request(MACH_SPIN_MARGIN_NS - 1) == 0);

    // Above it, across every wait length a real cap produces (a 1000fps cap through a
    // 10fps one), the request must leave at least half the budget in hand. The pre-fix
    // code asked for `remaining - MACH_SPIN_MARGIN_NS`, which fails this from ~2ms up.
    for (u64 remaining = MACH_SPIN_MARGIN_NS + 1; remaining <= 100000000ull;
         remaining += 97321ull) {
        u64 ns = mach_pace_sleep_request(remaining);
        CHECK(ns <= remaining / 2);
    }

    // And it has to make progress, or the loop that calls it never terminates: any wait
    // meaningfully longer than the margin must produce a real sleep.
    CHECK(mach_pace_sleep_request(2 * MACH_SPIN_MARGIN_NS + 2) > 0);
    CHECK(mach_pace_sleep_request(16666666ull) > 0); // one 60fps frame
}

// --- timing -----------------------------------------------------------------

static void test_time_monotonic(void) {
    section("time: monotonic");

    u64 a_ns = mach_ticks_ns();
    u32 a_ms = mach_ticks_ms();

    // Burn a little wall-clock without sleeping.
    volatile u64 spin = 0;
    for (int i = 0; i < 2000000; i++)
        spin += (u64)i;
    (void)spin;

    u64 b_ns = mach_ticks_ns();
    u32 b_ms = mach_ticks_ms();

    CHECK(b_ns >= a_ns);
    CHECK(b_ms >= a_ms);
    CHECK(b_ns > a_ns); // the clock actually advanced
}

// --- main -------------------------------------------------------------------

int main(void) {
    printf("mach.h v%d.%d.%d -- tests\n", MACH_VERSION_MAJOR, MACH_VERSION_MINOR,
           MACH_VERSION_PATCH);

    test_arena_basics();
    test_arena_reset();
    test_arena_oom_keeps_arena_intact();
    test_math();
    test_iso_ground_plane_roundtrip();
    test_color();
    test_font_covers_printable_ascii();
    test_font_white_block();
    test_time_monotonic();
    test_pacing_deadline_arithmetic();
    test_pacing_sleep_request();
    test_pacing_holds_the_rate();

    printf("\n%d checks, %d failed\n", checks, failures);
    if (failures) {
        printf("FAILED\n");
        return 1;
    }
    printf("ok\n");
    return 0;
}
