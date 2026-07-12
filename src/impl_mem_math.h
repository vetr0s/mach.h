
// =============================================================================
// mem: arena allocator
// =============================================================================

// Mach_Arena allocator implementation (MACH_IMPLEMENTATION).

#include <stdlib.h>

// Every allocation mach's own code makes goes through these, so a consumer can route
// the engine at a custom allocator by defining them before the include. Define one and
// you must define all three. The embedded libraries keep their own allocators; this
// covers the arena, the font atlas, and Clay's backing block.
//
// This is also the only portable way to reach the out-of-memory paths, which is how
// tests/test_core.c tests them.
#ifndef MACH_MALLOC
#define MACH_MALLOC(size) malloc(size)
#define MACH_CALLOC(count, size) calloc(count, size)
#define MACH_FREE(ptr) free(ptr)
#endif // MACH_MALLOC

// (npt): Default region size in words. 8K words is 64 KiB on a 64-bit target:
// big enough that most arenas live in one region, small enough to not over-commit.
#define MACH_ARENA_REGION_CAPACITY (8 * 1024)

static Mach_Arena_Region *mach_region_new(usize capacity) {
    usize bytes = sizeof(Mach_Arena_Region) + sizeof(uintptr_t) * capacity;
    Mach_Arena_Region *r = (Mach_Arena_Region *)MACH_MALLOC(bytes);
    if (!r) {
        MACH_LOG_ERROR("arena: region allocation failed (%zu bytes)", bytes);
        return NULL;
    }
    r->next = NULL;
    r->count = 0;
    r->capacity = capacity;
    return r;
}

void *mach_arena_alloc(Mach_Arena *a, usize size) {
    // (npt): Round the byte request up to whole words so the next allocation
    // starts word-aligned too.
    usize words = (size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);

    if (a->end == NULL) {
        usize capacity = words > MACH_ARENA_REGION_CAPACITY ? words : MACH_ARENA_REGION_CAPACITY;
        a->begin = a->end = mach_region_new(capacity);
        if (!a->end)
            return NULL;
    }

    // After a reset, `end` points at the first region; walk forward over any
    // already-full regions before deciding to allocate a new one.
    while (a->end->count + words > a->end->capacity && a->end->next != NULL) {
        a->end = a->end->next;
    }
    if (a->end->count + words > a->end->capacity) {
        usize capacity = words > MACH_ARENA_REGION_CAPACITY ? words : MACH_ARENA_REGION_CAPACITY;
        // Link the new region in only once it exists. Assigning straight into
        // `end` would park a NULL there on failure, and the next alloc would take the
        // empty-arena path and overwrite `begin` -- orphaning every region and every
        // pointer the caller still holds. A failed alloc has to leave the arena usable.
        Mach_Arena_Region *region = mach_region_new(capacity);
        if (!region)
            return NULL;
        a->end->next = region;
        a->end = region;
    }

    void *result = &a->end->data[a->end->count];
    a->end->count += words;
    return result;
}

void mach_arena_reset(Mach_Arena *a) {
    for (Mach_Arena_Region *r = a->begin; r != NULL; r = r->next) {
        r->count = 0;
    }
    a->end = a->begin;
}

void mach_arena_free(Mach_Arena *a) {
    Mach_Arena_Region *r = a->begin;
    while (r != NULL) {
        Mach_Arena_Region *next = r->next;
        MACH_FREE(r);
        r = next;
    }
    a->begin = NULL;
    a->end = NULL;
}

// =============================================================================
// math
// =============================================================================

// Math implementation (MACH_IMPLEMENTATION).

#include <math.h>

f32 mach_min(f32 a, f32 b) {
    return a < b ? a : b;
}
f32 mach_max(f32 a, f32 b) {
    return a > b ? a : b;
}
f32 mach_lerp(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

f32 mach_clamp(f32 v, f32 lo, f32 hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

Mach_Vec2 mach_vec2_add(Mach_Vec2 a, Mach_Vec2 b) {
    return (Mach_Vec2){a.x + b.x, a.y + b.y};
}
Mach_Vec2 mach_vec2_sub(Mach_Vec2 a, Mach_Vec2 b) {
    return (Mach_Vec2){a.x - b.x, a.y - b.y};
}
Mach_Vec2 mach_vec2_scale(Mach_Vec2 v, f32 s) {
    return (Mach_Vec2){v.x * s, v.y * s};
}
f32 mach_vec2_dot(Mach_Vec2 a, Mach_Vec2 b) {
    return a.x * b.x + a.y * b.y;
}
f32 mach_vec2_length(Mach_Vec2 v) {
    return sqrtf(mach_vec2_dot(v, v));
}

Mach_Vec2 mach_vec2_normalize(Mach_Vec2 v) {
    f32 len = mach_vec2_length(v);
    if (len == 0.0f)
        return (Mach_Vec2){0.0f, 0.0f};
    return mach_vec2_scale(v, 1.0f / len);
}

Mach_Vec2 mach_vec2_lerp(Mach_Vec2 a, Mach_Vec2 b, f32 t) {
    return (Mach_Vec2){mach_lerp(a.x, b.x, t), mach_lerp(a.y, b.y, t)};
}
