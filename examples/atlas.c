// Sprite batching: many sprites, one draw call.
//
// The renderer flushes whenever the texture id changes, so N sprites in N textures
// cost N draw calls. Pack them into one atlas and they cost one. This draws a few
// hundred sprites plus a HUD and reports the draw-call count live, so you can watch
// the claim hold.
//
// The interesting part is `r->white`. Untextured draws (fill_rect) sample it, and it
// defaults to a white block inside the *font* atlas -- so text and rectangles batch
// together. Point it at the world atlas's white block while drawing world sprites and
// the fills batch with the sprites instead. Press SPACE to stop doing that and watch
// the draw calls explode: that contrast is the whole feature.
//
//   controls: SPACE toggles the white-block trick, UP/DOWN changes the sprite count
//
//   cc -o nob nob.c && ./nob && ./examples/build/atlas

#define MACH_IMPLEMENTATION
#include "mach.h"

#define ATLAS_SIZE 256
#define SPRITE_PX 16
#define SPRITE_KINDS 6
#define START_COUNT 400
#define MAX_COUNT 2000

// Build a little RGBA sprite in memory, so the example needs no asset files.
static void make_sprite(u32 *px, i32 size, Mach_Color tint, i32 kind) {
    for (i32 y = 0; y < size; y++) {
        for (i32 x = 0; x < size; x++) {
            i32 cx = x - size / 2;
            i32 cy = y - size / 2;
            b32 on = MACH_FALSE;
            switch (kind % 3) {
            case 0: // disc
                on = (cx * cx + cy * cy) < (size / 2) * (size / 2);
                break;
            case 1: // diamond
                on = (cx < 0 ? -cx : cx) + (cy < 0 ? -cy : cy) < size / 2;
                break;
            default: // ring
                on = (cx * cx + cy * cy) < (size / 2) * (size / 2) &&
                     (cx * cx + cy * cy) > (size / 5) * (size / 5);
                break;
            }
            u32 a = on ? 0xFFu : 0x00u;
            u32 rr = (u32)(tint.x * 255.0f);
            u32 gg = (u32)(tint.y * 255.0f);
            u32 bb = (u32)(tint.z * 255.0f);
            px[y * size + x] = (a << 24) | (bb << 16) | (gg << 8) | rr; // RGBA, little-endian
        }
    }
}

int main(void) {
    Mach m = {0};
    if (!mach_init(&m, (Mach_Config){
                           .title = "mach: atlas batching",
                           .width = 1100,
                           .height = 700,
                           .clear_color = MACH_COLOR_BG_MAIN,
                           .escape_quits = 1,
                       }))
        return 1;

    // Pack every sprite kind into one atlas. One texture id for the lot of them, which
    // is what lets them batch.
    Mach_R2D_Atlas atlas = {0};
    if (!mach_r2d_atlas_create(&m.r2d, &atlas, ATLAS_SIZE, ATLAS_SIZE)) {
        mach_shutdown(&m);
        return 1;
    }

    Mach_Color palette[SPRITE_KINDS] = {
        MACH_COLOR_RED,  MACH_COLOR_GREEN,   MACH_COLOR_YELLOW,
        MACH_COLOR_BLUE, MACH_COLOR_MAGENTA, MACH_COLOR_CYAN,
    };

    Mach_R2D_Region sprites[SPRITE_KINDS];
    static u32 px[SPRITE_PX * SPRITE_PX];
    for (i32 i = 0; i < SPRITE_KINDS; i++) {
        make_sprite(px, SPRITE_PX, palette[i], i);
        Mach_Image img = {(u8 *)px, SPRITE_PX, SPRITE_PX, 4};
        sprites[i] = mach_r2d_atlas_add(&m.r2d, &atlas, img);
    }

    b32 batch_fills = MACH_TRUE; // SPACE toggles
    i32 count = START_COUNT;

    while (mach_running(&m)) {
        mach_frame_begin(&m);

        Mach_Input *in = &m.input;
        if (in->key_pressed[RGFW_keySpace])
            batch_fills = !batch_fills;
        if (in->key_down[RGFW_keyUp])
            count += 10;
        if (in->key_down[RGFW_keyDown])
            count -= 10;
        count = (i32)mach_clamp((f32)count, 0.0f, (f32)MAX_COUNT);

        f32 t = (f32)mach_ticks_ms() / 1000.0f;
        f32 w = (f32)m.r2d.width;
        f32 h = (f32)m.r2d.height;

        // The trick: while drawing world sprites, make untextured fills sample the
        // *atlas's* white block. Now a fill_rect between two sprites samples the same
        // texture they do, and the batch never breaks. Set it back to the font's block
        // and every fill costs two extra draw calls -- one to flush the sprites, one to
        // flush back.
        m.r2d.white = batch_fills ? atlas.white : m.r2d.font->white;

        for (i32 i = 0; i < count; i++) {
            f32 fi = (f32)i;
            f32 x = 60.0f + fmodf(fi * 37.0f, w - 160.0f);
            f32 y = 120.0f + fmodf(fi * 61.0f, h - 220.0f);
            y += 14.0f * sinf(t * 1.5f + fi * 0.05f);

            mach_r2d_region(&m.r2d, sprites[i % SPRITE_KINDS], x, y, 2.0f, MACH_COLOR_WHITE);

            // An untextured draw interleaved with every 8th sprite: the selection box,
            // the debug bound, the ghost placement. This is what thrashes the batch if
            // white lives in a different texture from the sprites.
            if (i % 8 == 0)
                mach_r2d_fill_rect(&m.r2d, x - 3.0f, y - 3.0f, 3.0f, 3.0f,
                                   mach_color_alpha(MACH_COLOR_FG_DIM, 0.5f));
        }

        // The HUD. Text lives in the font atlas, so put white back before drawing it.
        m.r2d.white = m.r2d.font->white;

        mach_r2d_fill_rect(&m.r2d, 0, 0, w, 104.0f, MACH_COLOR_BG_DIM);

        char line[128];
        snprintf(line, sizeof(line), "sprites: %d   draw calls: %d", count, m.r2d.draw_calls);
        mach_r2d_text(&m.r2d, 20, 20, 2, line, MACH_COLOR_FG_MAIN);

        snprintf(line, sizeof(line), "fills batch with sprites: %s   [SPACE] to toggle",
                 batch_fills ? "yes" : "no");
        mach_r2d_text(&m.r2d, 20, 44, 1, line,
                      batch_fills ? MACH_COLOR_GREEN : MACH_COLOR_RED);

        snprintf(line, sizeof(line), "%.2f ms  %d fps   [UP/DOWN] sprite count", m.frame_ms, m.fps);
        mach_r2d_text(&m.r2d, 20, 60, 1, line, MACH_COLOR_FG_DIM);

        mach_r2d_text(&m.r2d, 20, 80, 1,
                      "draw calls stay flat as the sprite count grows -- that's the batch.",
                      MACH_COLOR_FG_ALT);

        mach_frame_end(&m);
    }

    mach_r2d_atlas_destroy(&m.r2d, &atlas);
    mach_shutdown(&m);
    return 0;
}
