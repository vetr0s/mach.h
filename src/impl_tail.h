
Mach_Image mach_image_load(const char *path) {
    Mach_Image img = {0};
    if (!path)
        return img;

    int w, h, channels;
    u8 *data = stbi_load(path, &w, &h, &channels, 4); // Force RGBA
    if (!data)
        return img;

    img.data = data;
    img.width = w;
    img.height = h;
    img.channels = 4;
    return img;
}

Mach_Image mach_image_load_from_memory(const void *data, i32 size) {
    Mach_Image img = {0};
    if (!data || size <= 0)
        return img;

    int w, h, channels;
    u8 *pixels = stbi_load_from_memory((const stbi_uc *)data, (int)size, &w, &h, &channels,
                                       4); // Force RGBA
    if (!pixels)
        return img;

    img.data = pixels;
    img.width = w;
    img.height = h;
    img.channels = 4;
    return img;
}

void mach_image_free(Mach_Image *img) {
    if (!img || !img->data)
        return;
    stbi_image_free(img->data);
    img->data = NULL;
    img->width = 0;
    img->height = 0;
    img->channels = 0;
}

// =============================================================================
// font
// =============================================================================

// Mach_Font implementation: 8x8 glyphs baked into a GL texture atlas (included into
// mach.c).
//
// Glyphs are stored as a static bit table, then expanded into an RGBA atlas
// texture at startup (white pixels with alpha; transparent elsewhere). Text is
// drawn by batching glyph quads with a per-vertex color for tint.

#include <stdlib.h>
#include <string.h>

#define MACH_FONT_FIRST_CHAR 32
#define MACH_FONT_LAST_CHAR 126
#define MACH_FONT_GLYPH_COUNT (MACH_FONT_LAST_CHAR - MACH_FONT_FIRST_CHAR + 1) // 95

#define MACH_FONT_CELL 8
#define MACH_FONT_SHEET_COLS 16
#define MACH_FONT_SHEET_ROWS 6
#define MACH_FONT_SHEET_W (MACH_FONT_SHEET_COLS * MACH_FONT_CELL) // 128
#define MACH_FONT_SHEET_H (MACH_FONT_SHEET_ROWS * MACH_FONT_CELL) // 48

// 8x8 glyphs indexed by (ascii - MACH_FONT_FIRST_CHAR). MSB = leftmost pixel.
static const u8 GLYPHS[MACH_FONT_GLYPH_COUNT][MACH_FONT_CELL] = {
    [' ' - MACH_FONT_FIRST_CHAR] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['$' - MACH_FONT_FIRST_CHAR] = {0x18, 0x3E, 0x58, 0x3C, 0x1A, 0x7C, 0x18, 0x00},
    ['(' - MACH_FONT_FIRST_CHAR] = {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00},
    [')' - MACH_FONT_FIRST_CHAR] = {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00},
    ['+' - MACH_FONT_FIRST_CHAR] = {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
    [',' - MACH_FONT_FIRST_CHAR] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30},
    ['-' - MACH_FONT_FIRST_CHAR] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
    ['.' - MACH_FONT_FIRST_CHAR] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['%' - MACH_FONT_FIRST_CHAR] = {0x62, 0x66, 0x0C, 0x18, 0x30, 0x66, 0x46, 0x00},
    ['/' - MACH_FONT_FIRST_CHAR] = {0x06, 0x0C, 0x18, 0x18, 0x30, 0x60, 0xC0, 0x00},
    ['!' - MACH_FONT_FIRST_CHAR] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00},
    ['"' - MACH_FONT_FIRST_CHAR] = {0x6C, 0x6C, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['#' - MACH_FONT_FIRST_CHAR] = {0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00},
    ['&' - MACH_FONT_FIRST_CHAR] = {0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00},
    ['\'' - MACH_FONT_FIRST_CHAR] = {0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['*' - MACH_FONT_FIRST_CHAR] = {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},
    [';' - MACH_FONT_FIRST_CHAR] = {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30},
    ['<' - MACH_FONT_FIRST_CHAR] = {0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C, 0x00},
    ['=' - MACH_FONT_FIRST_CHAR] = {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00},
    ['>' - MACH_FONT_FIRST_CHAR] = {0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00},
    ['?' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00},
    ['@' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x62, 0x3C, 0x00},
    ['[' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00},
    ['\\' - MACH_FONT_FIRST_CHAR] = {0xC0, 0x60, 0x30, 0x30, 0x18, 0x0C, 0x06, 0x00},
    [']' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00},
    ['^' - MACH_FONT_FIRST_CHAR] = {0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['_' - MACH_FONT_FIRST_CHAR] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE},
    ['`' - MACH_FONT_FIRST_CHAR] = {0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['{' - MACH_FONT_FIRST_CHAR] = {0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00},
    ['|' - MACH_FONT_FIRST_CHAR] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['}' - MACH_FONT_FIRST_CHAR] = {0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00},
    ['~' - MACH_FONT_FIRST_CHAR] = {0x00, 0x00, 0x76, 0xDC, 0x00, 0x00, 0x00, 0x00},
    ['0' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['1' - MACH_FONT_FIRST_CHAR] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['2' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00},
    ['3' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    ['4' - MACH_FONT_FIRST_CHAR] = {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00},
    ['5' - MACH_FONT_FIRST_CHAR] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
    ['6' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
    ['7' - MACH_FONT_FIRST_CHAR] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x60, 0x00},
    ['8' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
    ['9' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00},
    [':' - MACH_FONT_FIRST_CHAR] = {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00},
    ['A' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['B' - MACH_FONT_FIRST_CHAR] = {0x7C, 0x66, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
    ['C' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
    ['D' - MACH_FONT_FIRST_CHAR] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
    ['E' - MACH_FONT_FIRST_CHAR] = {0x7E, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['F' - MACH_FONT_FIRST_CHAR] = {0x7E, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x60, 0x00},
    ['G' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00},
    ['H' - MACH_FONT_FIRST_CHAR] = {0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['I' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['J' - MACH_FONT_FIRST_CHAR] = {0x0E, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x00},
    ['K' - MACH_FONT_FIRST_CHAR] = {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
    ['L' - MACH_FONT_FIRST_CHAR] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['M' - MACH_FONT_FIRST_CHAR] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
    ['N' - MACH_FONT_FIRST_CHAR] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
    ['O' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['P' - MACH_FONT_FIRST_CHAR] = {0x7C, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x60, 0x00},
    ['Q' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x66, 0x66, 0x6E, 0x7C, 0x06, 0x00},
    ['R' - MACH_FONT_FIRST_CHAR] = {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00},
    ['S' - MACH_FONT_FIRST_CHAR] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
    ['T' - MACH_FONT_FIRST_CHAR] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U' - MACH_FONT_FIRST_CHAR] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['V' - MACH_FONT_FIRST_CHAR] = {0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x00},
    ['W' - MACH_FONT_FIRST_CHAR] = {0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x63, 0x00},
    ['X' - MACH_FONT_FIRST_CHAR] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['Y' - MACH_FONT_FIRST_CHAR] = {0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['Z' - MACH_FONT_FIRST_CHAR] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
    ['a' - MACH_FONT_FIRST_CHAR] = {0x00, 0x3C, 0x06, 0x3E, 0x66, 0x66, 0x3C, 0x00},
    ['b' - MACH_FONT_FIRST_CHAR] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
    ['c' - MACH_FONT_FIRST_CHAR] = {0x00, 0x3C, 0x60, 0x60, 0x60, 0x60, 0x3C, 0x00},
    ['d' - MACH_FONT_FIRST_CHAR] = {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00},
    ['e' - MACH_FONT_FIRST_CHAR] = {0x00, 0x3C, 0x66, 0x7E, 0x60, 0x66, 0x3C, 0x00},
    ['f' - MACH_FONT_FIRST_CHAR] = {0x1C, 0x30, 0x7E, 0x30, 0x30, 0x30, 0x30, 0x00},
    ['g' - MACH_FONT_FIRST_CHAR] = {0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C, 0x00},
    ['h' - MACH_FONT_FIRST_CHAR] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['i' - MACH_FONT_FIRST_CHAR] = {0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['j' - MACH_FONT_FIRST_CHAR] = {0x0C, 0x00, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00},
    ['k' - MACH_FONT_FIRST_CHAR] = {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00},
    ['l' - MACH_FONT_FIRST_CHAR] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['m' - MACH_FONT_FIRST_CHAR] = {0x00, 0x7C, 0xA6, 0x92, 0x92, 0x82, 0x82, 0x00},
    ['n' - MACH_FONT_FIRST_CHAR] = {0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['o' - MACH_FONT_FIRST_CHAR] = {0x00, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['p' - MACH_FONT_FIRST_CHAR] = {0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x00},
    ['q' - MACH_FONT_FIRST_CHAR] = {0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x00},
    ['r' - MACH_FONT_FIRST_CHAR] = {0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x60, 0x00},
    ['s' - MACH_FONT_FIRST_CHAR] = {0x00, 0x3C, 0x60, 0x3C, 0x06, 0x06, 0x3C, 0x00},
    ['t' - MACH_FONT_FIRST_CHAR] = {0x30, 0x7E, 0x30, 0x30, 0x30, 0x30, 0x1C, 0x00},
    ['u' - MACH_FONT_FIRST_CHAR] = {0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00},
    ['v' - MACH_FONT_FIRST_CHAR] = {0x00, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x00},
    ['w' - MACH_FONT_FIRST_CHAR] = {0x00, 0x63, 0x63, 0x6B, 0x7F, 0x37, 0x63, 0x00},
    ['x' - MACH_FONT_FIRST_CHAR] = {0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['y' - MACH_FONT_FIRST_CHAR] = {0x00, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x3C, 0x00},
    ['z' - MACH_FONT_FIRST_CHAR] = {0x00, 0x7E, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
};

// The sheet has 96 cells and 95 glyphs, so cell 95 is spare. It becomes a block of
// opaque white that untextured draws sample, which is what puts text and solid fills
// on the same texture: a HUD of rectangles and labels then costs one draw call
// instead of one per alternation.
#define MACH_FONT_WHITE_CELL MACH_FONT_GLYPH_COUNT

// Expand the bit table into an RGBA pixel buffer (caller frees). Set bits become
// opaque white; everything else is transparent.
static u32 *mach_font_build_pixels(void) {
    u32 *px = (u32 *)MACH_CALLOC(MACH_FONT_SHEET_W * MACH_FONT_SHEET_H, sizeof(u32));
    if (!px)
        return NULL;

    for (i32 idx = 0; idx < MACH_FONT_GLYPH_COUNT; idx++) {
        i32 cx = (idx % MACH_FONT_SHEET_COLS) * MACH_FONT_CELL;
        i32 cy = (idx / MACH_FONT_SHEET_COLS) * MACH_FONT_CELL;
        for (i32 r = 0; r < MACH_FONT_CELL; r++) {
            u8 bits = GLYPHS[idx][r];
            for (i32 c = 0; c < MACH_FONT_CELL; c++) {
                if (bits & (0x80 >> c))
                    px[(cy + r) * MACH_FONT_SHEET_W + (cx + c)] = 0xFFFFFFFFu;
            }
        }
    }

    // Fill the spare cell with opaque white.
    i32 wx = (MACH_FONT_WHITE_CELL % MACH_FONT_SHEET_COLS) * MACH_FONT_CELL;
    i32 wy = (MACH_FONT_WHITE_CELL / MACH_FONT_SHEET_COLS) * MACH_FONT_CELL;
    for (i32 r = 0; r < MACH_FONT_CELL; r++) {
        for (i32 c = 0; c < MACH_FONT_CELL; c++) {
            px[(wy + r) * MACH_FONT_SHEET_W + (wx + c)] = 0xFFFFFFFFu;
        }
    }
    return px;
}

// The white block's UVs, all four collapsed onto its center.
//
// This is deliberate and load-bearing. A quad interpolates u0,v0 -> u1,v1 across its
// face, so if white spanned the cell, the samples at the quad's edges would land on
// the texel boundary and, with any floating-point slop, round into the transparent
// glyph next door: every fill_rect would wear a 1px fringe. Collapsing the rect to a
// point means every sample, at every size, lands dead center of an opaque white texel.
// The block is 8x8 rather than 1x1 for the same reason -- room to be wrong in.
static Mach_R2D_Region mach_font_white_region(Mach_R2D_Texture atlas) {
    f32 cx = (f32)((MACH_FONT_WHITE_CELL % MACH_FONT_SHEET_COLS) * MACH_FONT_CELL) +
             (f32)MACH_FONT_CELL * 0.5f;
    f32 cy = (f32)((MACH_FONT_WHITE_CELL / MACH_FONT_SHEET_COLS) * MACH_FONT_CELL) +
             (f32)MACH_FONT_CELL * 0.5f;
    f32 u = cx / (f32)MACH_FONT_SHEET_W;
    f32 v = cy / (f32)MACH_FONT_SHEET_H;

    Mach_R2D_Region white;
    white.tex = atlas.id;
    white.u0 = u;
    white.v0 = v;
    white.u1 = u;
    white.v1 = v;
    white.w = (f32)MACH_FONT_CELL;
    white.h = (f32)MACH_FONT_CELL;
    return white;
}

Mach_Font *mach_font_create(struct Mach_Renderer *r) {
    Mach_Font *font = (Mach_Font *)MACH_CALLOC(1, sizeof(Mach_Font));
    if (!font)
        return NULL;

    font->glyph_w = MACH_FONT_CELL;
    font->glyph_h = MACH_FONT_CELL;
    font->advance = MACH_FONT_CELL + 1;

    u32 *px = mach_font_build_pixels();
    if (!px) {
        MACH_FREE(font);
        return NULL;
    }

    font->atlas = mach_r2d_texture_from_pixels((Mach_Renderer *)r, px, MACH_FONT_SHEET_W,
                                               MACH_FONT_SHEET_H, MACH_TRUE);
    MACH_FREE(px);
    if (!font->atlas.id) {
        MACH_LOG_ERROR("mach_font_create: atlas texture creation failed");
        MACH_FREE(font);
        return NULL;
    }
    font->white = mach_font_white_region(font->atlas);

    MACH_LOG_INFO("font atlas created (%dx%d RGBA, %d glyphs + white)", MACH_FONT_SHEET_W,
                  MACH_FONT_SHEET_H, MACH_FONT_GLYPH_COUNT);
    return font;
}

void mach_font_destroy(struct Mach_Renderer *r, Mach_Font *font) {
    if (!font)
        return;
    mach_r2d_destroy_texture((Mach_Renderer *)r, &font->atlas);
    MACH_FREE(font);
}

b32 mach_font_glyph_uv(const Mach_Font *font, char ch, f32 *u0, f32 *v0, f32 *u1, f32 *v1) {
    (void)font;
    u8 c = (u8)ch;
    if (c < MACH_FONT_FIRST_CHAR || c > MACH_FONT_LAST_CHAR)
        return MACH_FALSE;
    i32 idx = c - MACH_FONT_FIRST_CHAR;
    f32 x = (f32)((idx % MACH_FONT_SHEET_COLS) * MACH_FONT_CELL);
    f32 y = (f32)((idx / MACH_FONT_SHEET_COLS) * MACH_FONT_CELL);
    *u0 = x / (f32)MACH_FONT_SHEET_W;
    *v0 = y / (f32)MACH_FONT_SHEET_H;
    *u1 = (x + (f32)MACH_FONT_CELL) / (f32)MACH_FONT_SHEET_W;
    *v1 = (y + (f32)MACH_FONT_CELL) / (f32)MACH_FONT_SHEET_H;
    return MACH_TRUE;
}

// =============================================================================
// render2d
// =============================================================================

// 2D renderer implementation over OpenGL 3.3 core (MACH_IMPLEMENTATION).
//
// Everything draws through one shader as batched, textured, vertex-colored
// triangles. The draw calls append to a CPU-side batch that flushes to a single
// glDrawElements whenever the texture or scissor changes, the batch fills, or
// the frame presents.

// --- Shader -------------------------------------------------------------------

// Positions arrive in window points; u_screen maps them to clip space (y down).
static const char *R2D_VERT_SRC = "#version 330 core\n"
                                  "layout(location = 0) in vec2 a_pos;\n"
                                  "layout(location = 1) in vec2 a_uv;\n"
                                  "layout(location = 2) in vec4 a_color;\n"
                                  "uniform vec2 u_screen;\n"
                                  "out vec2 v_uv;\n"
                                  "out vec4 v_color;\n"
                                  "void main() {\n"
                                  "    vec2 ndc = vec2(a_pos.x * 2.0 / u_screen.x - 1.0,\n"
                                  "                    1.0 - a_pos.y * 2.0 / u_screen.y);\n"
                                  "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
                                  "    v_uv = a_uv;\n"
                                  "    v_color = a_color;\n"
                                  "}\n";

static const char *R2D_FRAG_SRC = "#version 330 core\n"
                                  "in vec2 v_uv;\n"
                                  "in vec4 v_color;\n"
                                  "uniform sampler2D u_tex;\n"
                                  "out vec4 frag;\n"
                                  "void main() { frag = texture(u_tex, v_uv) * v_color; }\n";

// --- GL loading ---------------------------------------------------------------

static b32 mach_r2d_gl_load(Mach_GLApi *gl) {
    b32 ok = MACH_TRUE;
#define MACH_GL_LOAD(name)                                                                         \
    do {                                                                                           \
        *(void **)(&gl->name) = (void *)RGFW_getProcAddress_OpenGL("gl" #name);                    \
        if (!gl->name) {                                                                           \
            MACH_LOG_ERROR("gl load: missing gl%s", #name);                                        \
            ok = MACH_FALSE;                                                                       \
        }                                                                                          \
    } while (0)
    MACH_GL_LOAD(ActiveTexture);
    MACH_GL_LOAD(AttachShader);
    MACH_GL_LOAD(BindBuffer);
    MACH_GL_LOAD(BindTexture);
    MACH_GL_LOAD(BindVertexArray);
    MACH_GL_LOAD(BlendFunc);
    MACH_GL_LOAD(BufferData);
    MACH_GL_LOAD(BufferSubData);
    MACH_GL_LOAD(Clear);
    MACH_GL_LOAD(ClearColor);
    MACH_GL_LOAD(CompileShader);
    MACH_GL_LOAD(CreateProgram);
    MACH_GL_LOAD(CreateShader);
    MACH_GL_LOAD(DeleteBuffers);
    MACH_GL_LOAD(DeleteProgram);
    MACH_GL_LOAD(DeleteShader);
    MACH_GL_LOAD(DeleteTextures);
    MACH_GL_LOAD(DeleteVertexArrays);
    MACH_GL_LOAD(Disable);
    MACH_GL_LOAD(DrawElements);
    MACH_GL_LOAD(Enable);
    MACH_GL_LOAD(EnableVertexAttribArray);
    MACH_GL_LOAD(GenBuffers);
    MACH_GL_LOAD(GenTextures);
    MACH_GL_LOAD(GenVertexArrays);
    MACH_GL_LOAD(GetProgramInfoLog);
    MACH_GL_LOAD(GetProgramiv);
    MACH_GL_LOAD(GetShaderInfoLog);
    MACH_GL_LOAD(GetShaderiv);
    MACH_GL_LOAD(GetString);
    MACH_GL_LOAD(GetUniformLocation);
    MACH_GL_LOAD(LinkProgram);
    MACH_GL_LOAD(PixelStorei);
    MACH_GL_LOAD(Scissor);
    MACH_GL_LOAD(ShaderSource);
    MACH_GL_LOAD(TexImage2D);
    MACH_GL_LOAD(TexParameteri);
    MACH_GL_LOAD(TexSubImage2D);
    MACH_GL_LOAD(Uniform1i);
    MACH_GL_LOAD(Uniform2f);
    MACH_GL_LOAD(UseProgram);
    MACH_GL_LOAD(VertexAttribPointer);
    MACH_GL_LOAD(Viewport);
#undef MACH_GL_LOAD
    return ok;
}

static u32 mach_r2d_compile_shader(const Mach_GLApi *gl, u32 type, const char *src) {
    u32 shader = gl->CreateShader(type);
    gl->ShaderSource(shader, 1, &src, NULL);
    gl->CompileShader(shader);
    i32 status = 0;
    gl->GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        gl->GetShaderInfoLog(shader, sizeof(log), NULL, log);
        MACH_LOG_ERROR("shader compile failed: %s", log);
        gl->DeleteShader(shader);
        return 0;
    }
    return shader;
}

// --- Lifecycle ------------------------------------------------------------------

// Cache the window size (points) and framebuffer size (pixels), and point the
// viewport at the full framebuffer. Render and mouse coordinates share the
// window-point space; GL scales to the HiDPI framebuffer via the viewport.
static void mach_r2d_apply_window_size(Mach_Renderer *r) {
    RGFW_window_getSize(r->window, &r->width, &r->height);
    r->fb_width = r->width;
    r->fb_height = r->height;
    RGFW_window_getSizeInPixels(r->window, &r->fb_width, &r->fb_height);
    r->gl.Viewport(0, 0, r->fb_width, r->fb_height);
}

// Bring up the renderer against an existing GL context. Every failure after the
// first allocation unwinds through mach_r2d_shutdown, which null-checks each handle
// and so is safe on a half-built renderer: a failed init leaves nothing behind, and
// the caller only has to close the window.
b32 mach_r2d_init(Mach_Renderer *r, RGFW_window *window) {
    r->window = window;

    if (!mach_r2d_gl_load(&r->gl))
        return MACH_FALSE;
    const Mach_GLApi *gl = &r->gl;

    // Both shaders are compiled before either is checked, so one can succeed while
    // the other fails. Drop whichever survived: returning here would otherwise leak
    // it, and a shader outlives the function that made it.
    u32 vs = mach_r2d_compile_shader(gl, GL_VERTEX_SHADER, R2D_VERT_SRC);
    u32 fs = mach_r2d_compile_shader(gl, GL_FRAGMENT_SHADER, R2D_FRAG_SRC);
    if (!vs || !fs) {
        if (vs)
            gl->DeleteShader(vs);
        if (fs)
            gl->DeleteShader(fs);
        return MACH_FALSE;
    }
    r->program = gl->CreateProgram();
    gl->AttachShader(r->program, vs);
    gl->AttachShader(r->program, fs);
    gl->LinkProgram(r->program);
    gl->DeleteShader(vs);
    gl->DeleteShader(fs);
    i32 status = 0;
    gl->GetProgramiv(r->program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[512];
        gl->GetProgramInfoLog(r->program, sizeof(log), NULL, log);
        MACH_LOG_ERROR("program link failed: %s", log);
        mach_r2d_shutdown(r);
        return MACH_FALSE;
    }
    r->u_screen = gl->GetUniformLocation(r->program, "u_screen");
    gl->UseProgram(r->program);
    gl->Uniform1i(gl->GetUniformLocation(r->program, "u_tex"), 0);

    // One VAO/VBO/IBO for the batch, allocated to capacity once and refilled
    // per flush with BufferSubData.
    gl->GenVertexArrays(1, &r->vao);
    gl->BindVertexArray(r->vao);
    gl->GenBuffers(1, &r->vbo);
    gl->BindBuffer(GL_ARRAY_BUFFER, r->vbo);
    gl->BufferData(GL_ARRAY_BUFFER, (isize)sizeof(r->verts), NULL, GL_STREAM_DRAW);
    gl->GenBuffers(1, &r->ibo);
    gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ibo);
    gl->BufferData(GL_ELEMENT_ARRAY_BUFFER, (isize)sizeof(r->indices), NULL, GL_STREAM_DRAW);
    gl->EnableVertexAttribArray(0);
    gl->VertexAttribPointer(0, 2, GL_FLOAT, 0, (i32)sizeof(Mach_R2D_Vertex), (void *)0);
    gl->EnableVertexAttribArray(1);
    gl->VertexAttribPointer(1, 2, GL_FLOAT, 0, (i32)sizeof(Mach_R2D_Vertex),
                            (void *)(2 * sizeof(f32)));
    gl->EnableVertexAttribArray(2);
    gl->VertexAttribPointer(2, 4, GL_FLOAT, 0, (i32)sizeof(Mach_R2D_Vertex),
                            (void *)(4 * sizeof(f32)));

    gl->Enable(GL_BLEND);
    gl->BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl->ActiveTexture(GL_TEXTURE0);

    mach_r2d_apply_window_size(r);

    // The font has to exist before white does: white is a block inside the font's
    // sheet, not a texture of its own. That is the whole batching trick -- fills and
    // text sample the same texture, so they never break the batch apart.
    r->font = mach_font_create(r);
    if (!r->font) {
        mach_r2d_shutdown(r);
        return MACH_FALSE;
    }
    r->white = r->font->white;
    r->batch_tex = r->white.tex;

    MACH_LOG_INFO("2D renderer ready (%dx%d points, %dx%d px, GL %s)", r->width, r->height,
                  r->fb_width, r->fb_height, (const char *)gl->GetString(GL_VERSION));
    return MACH_TRUE;
}

void mach_r2d_shutdown(Mach_Renderer *r) {
    const Mach_GLApi *gl = &r->gl;
    // `white` is a region of the font's sheet, not a texture we own, so destroying the
    // font destroys it. Deleting it separately here would be a double free.
    if (r->font) {
        mach_font_destroy(r, r->font);
        r->font = NULL;
    }
    r->white = (Mach_R2D_Region){0};
    if (r->program) {
        gl->DeleteProgram(r->program);
        r->program = 0;
    }
    if (r->vbo) {
        gl->DeleteBuffers(1, &r->vbo);
        r->vbo = 0;
    }
    if (r->ibo) {
        gl->DeleteBuffers(1, &r->ibo);
        r->ibo = 0;
    }
    if (r->vao) {
        gl->DeleteVertexArrays(1, &r->vao);
        r->vao = 0;
    }
    MACH_LOG_INFO("2D renderer shut down");
}

void mach_r2d_resized(Mach_Renderer *r) {
    mach_r2d_apply_window_size(r);
    MACH_LOG_DEBUG("window resized to %dx%d", r->width, r->height);
}

// --- Batch ----------------------------------------------------------------------

static void mach_r2d_flush(Mach_Renderer *r) {
    if (r->index_count == 0)
        return;
    const Mach_GLApi *gl = &r->gl;
    gl->BufferSubData(GL_ARRAY_BUFFER, 0, (isize)((usize)r->vert_count * sizeof(Mach_R2D_Vertex)),
                      r->verts);
    gl->BufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (isize)((usize)r->index_count * sizeof(u16)),
                      r->indices);
    gl->BindTexture(GL_TEXTURE_2D, r->batch_tex);
    gl->DrawElements(GL_TRIANGLES, r->index_count, GL_UNSIGNED_SHORT, 0);
    r->draw_calls_acc++;
    r->vert_count = 0;
    r->index_count = 0;
}

// Reserve batch space for nverts/nindices drawing with `tex`, flushing first if
// the texture changes or the batch would overflow. Returns the base vertex
// index; the caller writes verts at verts[base + i] and absolute indices.
static i32 mach_r2d_reserve(Mach_Renderer *r, u32 tex, i32 nverts, i32 nindices) {
    // A single draw larger than the whole batch cannot be flushed into existence; it
    // would just overrun. Nothing today asks for more than 16 verts, but a future
    // batched helper might, and it should find out here rather than in the vertex array.
    MACH_DEBUG_ASSERT(nverts <= MACH_R2D_MAX_VERTS && nindices <= MACH_R2D_MAX_INDICES);
    if (tex != r->batch_tex || r->vert_count + nverts > MACH_R2D_MAX_VERTS ||
        r->index_count + nindices > MACH_R2D_MAX_INDICES) {
        mach_r2d_flush(r);
        r->batch_tex = tex;
    }
    return r->vert_count;
}

static Mach_R2D_Vertex mach_r2d_vertex(f32 x, f32 y, f32 u, f32 v, Mach_Color c) {
    Mach_R2D_Vertex vert;
    vert.x = x;
    vert.y = y;
    vert.u = u;
    vert.v = v;
    vert.color = c;
    return vert;
}

// Append one textured axis-aligned quad.
static void mach_r2d_quad(Mach_Renderer *r, u32 tex, f32 x, f32 y, f32 w, f32 h, f32 u0, f32 v0,
                          f32 u1, f32 v1, Mach_Color c) {
    i32 base = mach_r2d_reserve(r, tex, 4, 6);
    r->verts[base + 0] = mach_r2d_vertex(x, y, u0, v0, c);
    r->verts[base + 1] = mach_r2d_vertex(x + w, y, u1, v0, c);
    r->verts[base + 2] = mach_r2d_vertex(x + w, y + h, u1, v1, c);
    r->verts[base + 3] = mach_r2d_vertex(x, y + h, u0, v1, c);
    u16 b = (u16)base;
    u16 *ix = r->indices + r->index_count;
    ix[0] = b;
    ix[1] = (u16)(b + 1);
    ix[2] = (u16)(b + 2);
    ix[3] = b;
    ix[4] = (u16)(b + 2);
    ix[5] = (u16)(b + 3);
    r->vert_count += 4;
    r->index_count += 6;
}

// --- Frame ------------------------------------------------------------------

void mach_r2d_begin(Mach_Renderer *r, Mach_Color clear) {
    const Mach_GLApi *gl = &r->gl;
    gl->ClearColor(clear.x, clear.y, clear.z, clear.w);
    gl->Clear(GL_COLOR_BUFFER_BIT);
    gl->UseProgram(r->program);
    gl->Uniform2f(r->u_screen, (f32)r->width, (f32)r->height);
    gl->BindVertexArray(r->vao);
    gl->BindBuffer(GL_ARRAY_BUFFER, r->vbo);
    r->vert_count = 0;
    r->index_count = 0;
    // Note this does not reset r->white: the consumer owns which white block is in
    // force, and silently repointing it every frame would be a nasty surprise.
    r->batch_tex = r->white.tex;

    // The frame that just ended is fully flushed by now (mach_r2d_submit ran in
    // mach_frame_end), so its total is final: publish it and start counting again.
    r->draw_calls = r->draw_calls_acc;
    r->draw_calls_acc = 0;

    // A frame that forgot a clip_end must not clip the next one.
    r->clip_depth = 0;
    gl->Disable(GL_SCISSOR_TEST);
}

void mach_r2d_submit(Mach_Renderer *r) {
    mach_r2d_flush(r);
}

void mach_r2d_present(Mach_Renderer *r) {
    mach_r2d_submit(r);
    RGFW_window_swapBuffers_OpenGL(r->window);
}

// --- Primitives -------------------------------------------------------------

void mach_r2d_fill_rect(Mach_Renderer *r, f32 x, f32 y, f32 w, f32 h, Mach_Color color) {
    const Mach_R2D_Region *w2 = &r->white;
    mach_r2d_quad(r, w2->tex, x, y, w, h, w2->u0, w2->v0, w2->u1, w2->v1, color);
}

// Fill a convex polygon as a triangle fan.
void mach_r2d_fill_poly(Mach_Renderer *r, const Mach_Vec2 *pts, i32 n, Mach_Color color) {
    if (n < 3 || n > 16)
        return;
    i32 base = mach_r2d_reserve(r, r->white.tex, n, (n - 2) * 3);
    for (i32 i = 0; i < n; i++) {
        r->verts[base + i] = mach_r2d_vertex(pts[i].x, pts[i].y, r->white.u0, r->white.v0, color);
    }
    u16 *ix = r->indices + r->index_count;
    for (i32 i = 1; i < n - 1; i++) {
        *ix++ = (u16)base;
        *ix++ = (u16)(base + i);
        *ix++ = (u16)(base + i + 1);
    }
    r->vert_count += n;
    r->index_count += (n - 2) * 3;
}

// Stroke a closed polygon with 1-point-thick edge quads (core GL has no
// reliable line width, so edges are geometry like everything else).
void mach_r2d_poly_outline(Mach_Renderer *r, const Mach_Vec2 *pts, i32 n, Mach_Color color) {
    if (n < 2 || n > 16)
        return;
    for (i32 i = 0; i < n; i++) {
        Mach_Vec2 p = pts[i];
        Mach_Vec2 q = pts[(i + 1) % n];
        Mach_Vec2 d = mach_vec2_sub(q, p);
        f32 len = mach_vec2_length(d);
        if (len < 0.0001f)
            continue;
        // Half-thickness normal on each side of the edge.
        f32 nx = -d.y / len * 0.5f;
        f32 ny = d.x / len * 0.5f;
        f32 wu = r->white.u0, wv = r->white.v0;
        i32 base = mach_r2d_reserve(r, r->white.tex, 4, 6);
        r->verts[base + 0] = mach_r2d_vertex(p.x + nx, p.y + ny, wu, wv, color);
        r->verts[base + 1] = mach_r2d_vertex(q.x + nx, q.y + ny, wu, wv, color);
        r->verts[base + 2] = mach_r2d_vertex(q.x - nx, q.y - ny, wu, wv, color);
        r->verts[base + 3] = mach_r2d_vertex(p.x - nx, p.y - ny, wu, wv, color);
        u16 b = (u16)base;
        u16 *ix = r->indices + r->index_count;
        ix[0] = b;
        ix[1] = (u16)(b + 1);
        ix[2] = (u16)(b + 2);
        ix[3] = b;
        ix[4] = (u16)(b + 2);
        ix[5] = (u16)(b + 3);
        r->vert_count += 4;
        r->index_count += 6;
    }
}

void mach_r2d_text(Mach_Renderer *r, f32 x, f32 y, f32 scale, const char *text, Mach_Color color) {
    if (!text)
        return;
    Mach_Font *font = r->font;
    f32 gw = (f32)font->glyph_w * scale;
    f32 gh = (f32)font->glyph_h * scale;
    f32 adv = (f32)font->advance * scale;
    f32 cur_x = x;
    for (const char *c = text; *c; c++) {
        f32 u0, v0, u1, v1;
        if (mach_font_glyph_uv(font, *c, &u0, &v0, &u1, &v1)) {
            mach_r2d_quad(r, font->atlas.id, cur_x, y, gw, gh, u0, v0, u1, v1, color);
        }
        cur_x += adv;
    }
}

// --- Clip ---------------------------------------------------------------------

// glScissor works in framebuffer pixels with a bottom-left origin, so convert
// from window points (y down) and scale for HiDPI.
// Put the top of the clip stack into GL, or turn the scissor off if the stack is
// empty. Rects are stored in window points and converted here, because the
// intersection is easier to reason about in the same space the caller used.
static void mach_r2d_clip_apply(Mach_Renderer *r) {
    if (r->clip_depth <= 0) {
        r->gl.Disable(GL_SCISSOR_TEST);
        return;
    }

    i32 top = r->clip_depth - 1;
    if (top >= MACH_R2D_MAX_CLIPS)
        top = MACH_R2D_MAX_CLIPS - 1; // overflowed: hold the deepest rect we kept

    f32 x = r->clips[top][0];
    f32 y = r->clips[top][1];
    f32 w = r->clips[top][2];
    f32 h = r->clips[top][3];

    f32 sx = (f32)r->fb_width / (f32)r->width;
    f32 sy = (f32)r->fb_height / (f32)r->height;
    i32 px = (i32)(x * sx);
    i32 py = r->fb_height - (i32)((y + h) * sy);
    i32 pw = (i32)(w * sx);
    i32 ph = (i32)(h * sy);
    if (pw < 0)
        pw = 0;
    if (ph < 0)
        ph = 0;

    r->gl.Enable(GL_SCISSOR_TEST);
    r->gl.Scissor(px, py, pw, ph);
}

void mach_r2d_clip_begin(Mach_Renderer *r, f32 x, f32 y, f32 w, f32 h) {
    mach_r2d_flush(r);

    // Intersect with the enclosing rect. A nested clip can only ever shrink its
    // parent's window: without this, an inner panel would happily draw outside the
    // scrolling container that owns it.
    if (r->clip_depth > 0 && r->clip_depth <= MACH_R2D_MAX_CLIPS) {
        const f32 *p = r->clips[r->clip_depth - 1];
        f32 x0 = mach_max(x, p[0]);
        f32 y0 = mach_max(y, p[1]);
        f32 x1 = mach_min(x + w, p[0] + p[2]);
        f32 y1 = mach_min(y + h, p[1] + p[3]);
        x = x0;
        y = y0;
        w = x1 > x0 ? x1 - x0 : 0.0f;
        h = y1 > y0 ? y1 - y0 : 0.0f;
    }

    if (r->clip_depth < MACH_R2D_MAX_CLIPS) {
        r->clips[r->clip_depth][0] = x;
        r->clips[r->clip_depth][1] = y;
        r->clips[r->clip_depth][2] = w;
        r->clips[r->clip_depth][3] = h;
    } else {
        MACH_LOG_ERROR("clip stack overflow (max %d deep); rect ignored", MACH_R2D_MAX_CLIPS);
    }

    // Count the push either way, so an overflowing begin still has a matching end
    // and the stack does not desynchronize.
    r->clip_depth++;
    mach_r2d_clip_apply(r);
}

void mach_r2d_clip_end(Mach_Renderer *r) {
    if (r->clip_depth <= 0) {
        MACH_LOG_ERROR("mach_r2d_clip_end without a matching mach_r2d_clip_begin");
        return;
    }
    mach_r2d_flush(r);
    r->clip_depth--;
    mach_r2d_clip_apply(r);
}

// --- Textures and sprites -----------------------------------------------------

Mach_R2D_Texture mach_r2d_texture_from_pixels(Mach_Renderer *r, const void *rgba, i32 w, i32 h,
                                              b32 nearest) {
    const Mach_GLApi *gl = &r->gl;
    Mach_R2D_Texture tex = {0};
    gl->GenTextures(1, &tex.id);
    if (!tex.id) {
        MACH_LOG_ERROR("mach_r2d_texture_from_pixels: glGenTextures failed");
        return tex;
    }
    tex.w = (f32)w;
    tex.h = (f32)h;
    i32 filter = nearest ? GL_NEAREST : GL_LINEAR;
    gl->BindTexture(GL_TEXTURE_2D, tex.id);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

Mach_R2D_Texture mach_r2d_load_texture(Mach_Renderer *r, const char *path) {
    Mach_R2D_Texture tex = {0};
    Mach_Image img = mach_image_load(path);
    if (!img.data) {
        MACH_LOG_ERROR("mach_r2d_load_texture: failed to load %s", path);
        return tex;
    }
    tex = mach_r2d_texture_from_pixels(r, img.data, img.width, img.height, MACH_FALSE);
    mach_image_free(&img);
    return tex;
}

Mach_R2D_Texture mach_r2d_texture_from_memory(Mach_Renderer *r, const void *data, i32 size,
                                              b32 nearest) {
    Mach_R2D_Texture tex = {0};
    Mach_Image img = mach_image_load_from_memory(data, size);
    if (!img.data) {
        MACH_LOG_ERROR("mach_r2d_texture_from_memory: failed to decode %d bytes", size);
        return tex;
    }
    tex = mach_r2d_texture_from_pixels(r, img.data, img.width, img.height, nearest);
    mach_image_free(&img);
    return tex;
}

void mach_r2d_destroy_texture(Mach_Renderer *r, Mach_R2D_Texture *tex) {
    if (!tex->id)
        return;
    // The pending batch may still be naming this texture. Draw it out before the id
    // dies, or the next flush binds a deleted name.
    if (tex->id == r->batch_tex)
        mach_r2d_flush(r);
    r->gl.DeleteTextures(1, &tex->id);
    tex->id = 0;
    tex->w = 0.0f;
    tex->h = 0.0f;
}

void mach_r2d_sprite(Mach_Renderer *r, Mach_R2D_Texture tex, f32 x, f32 y, f32 scale,
                     Mach_Color tint) {
    if (!tex.id)
        return;
    mach_r2d_quad(r, tex.id, x, y, tex.w * scale, tex.h * scale, 0.0f, 0.0f, 1.0f, 1.0f, tint);
}

// --- Atlas and regions --------------------------------------------------------
//
// The draws here are three lines each because mach_r2d_quad already took UVs: the
// primitive an atlas needs was always in the renderer, it just was not reachable.

Mach_R2D_Region mach_r2d_region_of(Mach_R2D_Texture tex, f32 x, f32 y, f32 w, f32 h) {
    Mach_R2D_Region s = {0};
    if (!tex.id || tex.w <= 0.0f || tex.h <= 0.0f)
        return s;
    s.tex = tex.id;
    s.u0 = x / tex.w;
    s.v0 = y / tex.h;
    s.u1 = (x + w) / tex.w;
    s.v1 = (y + h) / tex.h;
    s.w = w;
    s.h = h;
    return s;
}

void mach_r2d_region(Mach_Renderer *r, Mach_R2D_Region s, f32 x, f32 y, f32 scale,
                     Mach_Color tint) {
    if (!s.tex)
        return;
    mach_r2d_quad(r, s.tex, x, y, s.w * scale, s.h * scale, s.u0, s.v0, s.u1, s.v1, tint);
}

void mach_r2d_region_rect(Mach_Renderer *r, Mach_R2D_Region s, f32 x, f32 y, f32 w, f32 h,
                          Mach_Color tint) {
    if (!s.tex)
        return;
    mach_r2d_quad(r, s.tex, x, y, w, h, s.u0, s.v0, s.u1, s.v1, tint);
}

b32 mach_r2d_atlas_create(Mach_Renderer *r, Mach_R2D_Atlas *a, i32 w, i32 h) {
    if (!a || w <= 0 || h <= 0) {
        MACH_LOG_ERROR("mach_r2d_atlas_create: bad size %dx%d", w, h);
        return MACH_FALSE;
    }
    *a = (Mach_R2D_Atlas){0};

    // The texture must start zeroed, not merely allocated: glTexImage2D with NULL
    // leaves the contents undefined, and the transparent gutter between regions is the
    // thing that stops neighbours bleeding into each other. One temporary buffer at
    // load time, freed immediately, is a fine price for that.
    u32 *zeros = (u32 *)MACH_CALLOC((usize)w * (usize)h, sizeof(u32));
    if (!zeros) {
        MACH_LOG_ERROR("mach_r2d_atlas_create: out of memory (%dx%d)", w, h);
        return MACH_FALSE;
    }
    a->tex = mach_r2d_texture_from_pixels(r, zeros, w, h, MACH_TRUE);
    MACH_FREE(zeros);
    if (!a->tex.id)
        return MACH_FALSE;

    a->w = w;
    a->h = h;

    // Reserve white first, so it sits at (0,0) and can never be crowded out. It is a
    // block rather than a texel for the same reason the font's is: the UVs collapse to
    // its center, and the surrounding pixels are the margin for floating-point error.
    u32 white_px[MACH_FONT_CELL * MACH_FONT_CELL];
    for (usize i = 0; i < MACH_ARRAY_COUNT(white_px); i++)
        white_px[i] = 0xFFFFFFFFu;

    Mach_Image white_img;
    white_img.data = (u8 *)white_px;
    white_img.width = MACH_FONT_CELL;
    white_img.height = MACH_FONT_CELL;
    white_img.channels = 4;

    Mach_R2D_Region block = mach_r2d_atlas_add(r, a, white_img);
    if (!block.tex) {
        mach_r2d_atlas_destroy(r, a);
        return MACH_FALSE;
    }

    // Collapse to the block's center: see mach_font_white_region for why.
    a->white = block;
    a->white.u0 = a->white.u1 = (block.u0 + block.u1) * 0.5f;
    a->white.v0 = a->white.v1 = (block.v0 + block.v1) * 0.5f;

    MACH_LOG_INFO("atlas created (%dx%d RGBA)", w, h);
    return MACH_TRUE;
}

Mach_R2D_Region mach_r2d_atlas_add(Mach_Renderer *r, Mach_R2D_Atlas *a, Mach_Image img) {
    Mach_R2D_Region s = {0};
    if (!a || !a->tex.id || !img.data || img.width <= 0 || img.height <= 0)
        return s;

    // Shelf packing: run along the current row, and when it fills, drop to a new row
    // below the tallest thing placed so far.
    i32 pw = img.width + MACH_R2D_ATLAS_PAD;
    i32 ph = img.height + MACH_R2D_ATLAS_PAD;

    if (a->shelf_x + pw > a->w) {
        a->shelf_y += a->shelf_h;
        a->shelf_x = 0;
        a->shelf_h = 0;
    }
    if (a->shelf_y + ph > a->h) {
        MACH_LOG_ERROR("mach_r2d_atlas_add: %dx%d does not fit in the %dx%d atlas (%d packed)",
                       img.width, img.height, a->w, a->h, a->count);
        return s;
    }

    i32 px = a->shelf_x;
    i32 py = a->shelf_y;
    a->shelf_x += pw;
    if (ph > a->shelf_h)
        a->shelf_h = ph;

    const Mach_GLApi *gl = &r->gl;
    gl->BindTexture(GL_TEXTURE_2D, a->tex.id);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->TexSubImage2D(GL_TEXTURE_2D, 0, px, py, img.width, img.height, GL_RGBA, GL_UNSIGNED_BYTE,
                      img.data);
    // The upload rebound the texture behind the batch's back; put it back.
    gl->BindTexture(GL_TEXTURE_2D, r->batch_tex);

    a->count++;
    return mach_r2d_region_of(a->tex, (f32)px, (f32)py, (f32)img.width, (f32)img.height);
}

Mach_R2D_Region mach_r2d_atlas_add_file(Mach_Renderer *r, Mach_R2D_Atlas *a, const char *path) {
    Mach_Image img = mach_image_load(path);
    if (!img.data)
        return (Mach_R2D_Region){0};
    Mach_R2D_Region s = mach_r2d_atlas_add(r, a, img);
    mach_image_free(&img);
    return s;
}

Mach_R2D_Region mach_r2d_atlas_add_memory(Mach_Renderer *r, Mach_R2D_Atlas *a, const void *data,
                                          i32 size) {
    Mach_Image img = mach_image_load_from_memory(data, size);
    if (!img.data)
        return (Mach_R2D_Region){0};
    Mach_R2D_Region s = mach_r2d_atlas_add(r, a, img);
    mach_image_free(&img);
    return s;
}

void mach_r2d_atlas_destroy(Mach_Renderer *r, Mach_R2D_Atlas *a) {
    if (!a)
        return;
    mach_r2d_destroy_texture(r, &a->tex);
    *a = (Mach_R2D_Atlas){0};
}

// --- Isometric projection ---------------------------------------------------

Mach_Vec2 mach_iso_to_screen(const Mach_Camera2D *cam, f32 screen_w, f32 screen_h, f32 grid_x,
                             f32 grid_y, f32 elev) {
    f32 iso_x = (grid_x - grid_y) * (MACH_ISO_TILE_W * 0.5f);
    f32 iso_y = (grid_x + grid_y) * (MACH_ISO_TILE_H * 0.5f) - elev * MACH_ISO_ELEV;
    return (Mach_Vec2){
        (iso_x - cam->pan.x) * cam->zoom + screen_w * 0.5f,
        (iso_y - cam->pan.y) * cam->zoom + screen_h * 0.5f,
    };
}

Mach_Vec2 mach_screen_to_iso(const Mach_Camera2D *cam, f32 screen_w, f32 screen_h, f32 screen_x,
                             f32 screen_y) {
    f32 iso_x = (screen_x - screen_w * 0.5f) / cam->zoom + cam->pan.x;
    f32 iso_y = (screen_y - screen_h * 0.5f) / cam->zoom + cam->pan.y;
    // Invert the elev-0 projection: a = gx-gy, b = gx+gy.
    f32 a = iso_x / (MACH_ISO_TILE_W * 0.5f);
    f32 b = iso_y / (MACH_ISO_TILE_H * 0.5f);
    return (Mach_Vec2){(a + b) * 0.5f, (b - a) * 0.5f};
}

// =============================================================================
// clay_ui (Clay implementation lives here)
// =============================================================================

// Clay UI backend (included once into the unity build). This is the single home of
// Clay's implementation, plus the text-measure hook and the render-command -> r2d
// translation. See clay_ui.h for the hot-reload notes.

// Clay's implementation lives here. It's third-party single-header code, so silence
// the warnings it trips under -Wall -Wextra rather than let them bury ours.
// (Clay's implementation is embedded up top; it compiled on this pass.)

#include <stdlib.h>

static Mach_Color mach_clay_color(Clay_Color c) {
    return (Mach_Color){c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
}

// Clay measures text through this; our bitmap font is a fixed 8x8 glyph advanced by
// font->advance, scaled uniformly. fontSize is treated as the target pixel height.
static Clay_Dimensions mach_clay_measure_text(Clay_StringSlice text, Clay_TextElementConfig *config,
                                              void *userData) {
    Mach_Renderer *r = (Mach_Renderer *)userData;
    f32 scale = (f32)config->fontSize / (f32)r->font->glyph_h;
    return (Clay_Dimensions){
        .width = (f32)text.length * (f32)r->font->advance * scale,
        .height = (f32)r->font->glyph_h * scale,
    };
}

static void mach_clay_on_error(Clay_ErrorData e) {
    MACH_LOG_ERROR("clay: %.*s", (int)e.errorText.length, e.errorText.chars);
}

b32 mach_clay_ui_init(Mach_ClayUI *ui, Mach_Renderer *r) {
    if (!ui || !r)
        return MACH_FALSE;
    uint32_t need = Clay_MinMemorySize();
    ui->memory = MACH_MALLOC(need);
    if (!ui->memory) {
        MACH_LOG_ERROR("mach_clay_ui_init: failed to allocate %u bytes", need);
        return MACH_FALSE;
    }
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(need, ui->memory);
    Clay_Dimensions dims = {(f32)r->width, (f32)r->height};
    ui->ctx = Clay_Initialize(arena, dims, (Clay_ErrorHandler){mach_clay_on_error, NULL});
    Clay_SetMeasureTextFunction(mach_clay_measure_text, r);
    ui->ready = MACH_TRUE;
    MACH_LOG_INFO("clay ui initialized (%u bytes)", need);
    return MACH_TRUE;
}

void mach_clay_ui_shutdown(Mach_ClayUI *ui) {
    if (!ui)
        return;
    MACH_FREE(ui->memory);
    ui->memory = NULL;
    ui->ctx = NULL;
    ui->ready = MACH_FALSE;
}

void mach_clay_ui_begin(Mach_ClayUI *ui, Mach_Renderer *r, Clay_Vector2 mouse, b32 mouse_down) {
    if (!ui || !ui->ready)
        return;
    // Re-point Clay's globals every frame: after a hot reload the new library's copy
    // of Clay starts with an empty context and a dangling measure-fn pointer.
    Clay_SetCurrentContext(ui->ctx);
    Clay_SetMeasureTextFunction(mach_clay_measure_text, r);
    Clay_SetLayoutDimensions((Clay_Dimensions){(f32)r->width, (f32)r->height});
    Clay_SetPointerState(mouse, mouse_down);
    Clay_BeginLayout();
}

void mach_clay_ui_render(Mach_ClayUI *ui, Mach_Renderer *r) {
    if (!ui || !ui->ready)
        return;
    // deltaTime is only used by Clay's animation/transition features, which we don't
    // use yet, so 0 is fine.
    Clay_RenderCommandArray cmds = Clay_EndLayout(0.0f);
    for (i32 i = 0; i < cmds.length; i++) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&cmds, i);
        Clay_BoundingBox b = cmd->boundingBox;
        switch (cmd->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            mach_r2d_fill_rect(r, b.x, b.y, b.width, b.height,
                               mach_clay_color(cmd->renderData.rectangle.backgroundColor));
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *t = &cmd->renderData.text;
            char buf[256];
            i32 n = t->stringContents.length;
            if (n > (i32)sizeof(buf) - 1)
                n = (i32)sizeof(buf) - 1;
            memcpy(buf, t->stringContents.chars, (size_t)n);
            buf[n] = '\0';
            f32 scale = (f32)t->fontSize / (f32)r->font->glyph_h;
            mach_r2d_text(r, b.x, b.y, scale, buf, mach_clay_color(t->textColor));
        } break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData *bd = &cmd->renderData.border;
            Mach_Vec4 col = mach_clay_color(bd->color);
            if (bd->width.top)
                mach_r2d_fill_rect(r, b.x, b.y, b.width, (f32)bd->width.top, col);
            if (bd->width.bottom)
                mach_r2d_fill_rect(r, b.x, b.y + b.height - (f32)bd->width.bottom, b.width,
                                   (f32)bd->width.bottom, col);
            if (bd->width.left)
                mach_r2d_fill_rect(r, b.x, b.y, (f32)bd->width.left, b.height, col);
            if (bd->width.right)
                mach_r2d_fill_rect(r, b.x + b.width - (f32)bd->width.right, b.y,
                                   (f32)bd->width.right, b.height, col);
        } break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            mach_r2d_clip_begin(r, b.x, b.y, b.width, b.height);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            mach_r2d_clip_end(r);
            break;
        default:
            break; // images, custom, overlays: unused by our HUD for now
        }
    }
}

// =============================================================================
// core (RGFW implementation lives here)
// =============================================================================

// Core implementation (MACH_IMPLEMENTATION).
//
// The engine exposes the frame loop as discrete steps; the game owns the loop in
// main() and calls them. The engine keeps window lifecycle and frame timing.
//
// RGFW's implementation compiles here; this file is the single home of the
// windowing layer, the way clay_ui.c owns Clay and image.c owns stb_image.
// Everything else includes engine/rgfw.h for declarations only.

#include <stdio.h>
#include <time.h>
#if !defined(_WIN32)
#include <sched.h> // sched_yield, for the frame cap's spin tail
#endif

// RGFW is third-party single-header code, so silence the warnings it trips
// under -Wall -Wextra rather than let them bury ours.
// (RGFW's implementation is embedded up top; it compiled on this pass.)

// Clamp dt to prevent large simulation jumps after a stall.
#define MACH_MAX_DT 0.1f

// (npt): The Win32 branches lean on RGFW's implementation include above already
// having pulled in windows.h; QPC/Sleep are core kernel32 so WIN32_LEAN_AND_MEAN
// doesn't hide them. RGFW also calls timeBeginPeriod(1), which is what keeps
// Sleep's granularity at 1ms rather than the default ~15.6ms -- the frame cap
// spins out the last millisecond itself.
u32 mach_ticks_ms(void) {
#if defined(_WIN32)
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (u32)(count.QuadPart * 1000 / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u32)((u64)ts.tv_sec * 1000u + (u64)ts.tv_nsec / 1000000u);
#endif
}

// The frame loop times itself in nanoseconds, not milliseconds: the cap period
// for a target rate is 1e9 / fps, and rates whose period isn't a whole
// millisecond (144 fps = 6.944 ms, 30 fps = 33.333 ms) can't be represented in
// integer ms without biasing the rate. The underlying clocks are already sub-ms
// (QPC, CLOCK_MONOTONIC), so this just stops truncating their resolution away.
static u64 mach_ticks_ns(void) {
#if defined(_WIN32)
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (u64)count.QuadPart * 1000000000ull / (u64)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
#endif
}

// Sleeping is only ever approximate: nanosleep and Sleep guarantee *at least*
// the requested time and routinely overshoot by a millisecond or more (Sleep is
// 1ms-granular even with RGFW's timeBeginPeriod(1)). Overshooting the frame
// deadline is what makes a naive cap miss its target -- ask for 60 and get 58,
// unevenly. So the wait sleeps to a millisecond short of the deadline and spins
// out the remainder, trading a sliver of CPU for landing on the mark.
#define MACH_SPIN_MARGIN_NS 1000000ull // 1ms: the slop we assume a sleep can overshoot by

static void mach_wait_until_ns(u64 deadline) {
    u64 now = mach_ticks_ns();
    if (now >= deadline)
        return;

    u64 remaining = deadline - now;
    if (remaining > MACH_SPIN_MARGIN_NS) {
        u64 ns = remaining - MACH_SPIN_MARGIN_NS;
#if defined(_WIN32)
        Sleep((DWORD)(ns / 1000000ull)); // round *down*: never sleep past the deadline
#else
        struct timespec ts;
        ts.tv_sec = (time_t)(ns / 1000000000ull);
        ts.tv_nsec = (long)(ns % 1000000000ull);
        nanosleep(&ts, NULL);
#endif
    }

    // The last millisecond, by hand. Yield inside the spin so we don't starve
    // another runnable thread on a single-core machine.
    while (mach_ticks_ns() < deadline) {
#if defined(_WIN32)
        Sleep(0);
#else
        sched_yield();
#endif
    }
}

// Initialize RGFW, create the window from the config (zeroed fields defaulted)
// with a GL 3.3 core context, bring up the 2D renderer, and start the clocks.
b32 mach_init(Mach *m, Mach_Config cfg) {
    MACH_LOG_INFO("mach v%d.%d.%d starting up", MACH_VERSION_MAJOR, MACH_VERSION_MINOR,
                  MACH_VERSION_PATCH);

    if (!cfg.title)
        cfg.title = "mach";
    if (cfg.width <= 0)
        cfg.width = 1280;
    if (cfg.height <= 0)
        cfg.height = 720;
    if (cfg.clear_color.w == 0.0f)
        cfg.clear_color = (Mach_Color){0, 0, 0, 1};

    if (RGFW_init("mach", RGFW_initOpenGL) < 0) {
        MACH_LOG_ERROR("RGFW_init failed");
        return MACH_FALSE;
    }

    // RGFW keeps the pointer, so the hints have to outlive this call: they live in the
    // Mach the caller owns, not in a static.
    m->gl_hints = *RGFW_getGlobalHints_OpenGL();
    m->gl_hints.major = 3;
    m->gl_hints.minor = 3;
    m->gl_hints.profile = RGFW_glCore;
    RGFW_setGlobalHints_OpenGL(&m->gl_hints);

    RGFW_windowFlags flags = RGFW_windowCenter | RGFW_windowOpenGL;
    if (cfg.fullscreen)
        flags |= RGFW_windowFullscreen;
    if (cfg.fixed_size)
        flags |= RGFW_windowNoResize;
    m->window = RGFW_createWindow(cfg.title, 0, 0, cfg.width, cfg.height, flags);
    if (!m->window) {
        MACH_LOG_ERROR("RGFW_createWindow failed");
        RGFW_deinit();
        return MACH_FALSE;
    }

    // Pacing. Vsync is the default: the display is already a clock, and letting
    // it pace the loop costs no CPU and can't tear. A target_fps means the game
    // wants a rate the display isn't offering, so vsync comes off and the
    // deadline cap in mach_frame_end governs instead -- the two would otherwise
    // fight, each waiting on the other's schedule.
    b32 vsync = !cfg.vsync_off && cfg.target_fps <= 0;
    RGFW_window_swapInterval_OpenGL(m->window, vsync ? 1 : 0);

    if (!mach_r2d_init(&m->r2d, m->window)) {
        RGFW_window_close(m->window);
        m->window = NULL;
        RGFW_deinit();
        return MACH_FALSE;
    }

    m->clear_color = cfg.clear_color;
    m->escape_quits = cfg.escape_quits;
    m->frame_cap_ns = cfg.target_fps > 0 ? 1000000000ull / (u64)cfg.target_fps : 0;

    if (vsync)
        MACH_LOG_INFO("pacing: vsync (display rate)");
    else if (m->frame_cap_ns)
        MACH_LOG_INFO("pacing: %d fps cap, vsync off", cfg.target_fps);
    else
        MACH_LOG_INFO("pacing: uncapped, vsync off");

    u64 now = mach_ticks_ns();
    m->running = MACH_TRUE;
    m->dt = 0.0f;
    m->fps = 0;
    m->frame_ms = 0.0f;
    m->frame_ms_peak = 0.0f;
    m->frame_ms_peak_acc = 0.0f;
    m->frame_start = now;
    m->frame_deadline = now; // the first frame_end advances this by one period
    m->last_frame_time = now;
    m->fps_timer = now;
    m->frame_count = 0;
    return MACH_TRUE;
}

// Clean up renderer resources and close the window.
void mach_shutdown(Mach *m) {
    mach_arena_free(&m->frame_arena);
    mach_r2d_shutdown(&m->r2d);
    if (m->window) {
        RGFW_window_close(m->window);
        m->window = NULL;
    }
    RGFW_deinit();
    MACH_LOG_INFO("shutdown complete");
}

b32 mach_running(const Mach *m) {
    return m->running;
}

// Start a frame: compute m->dt since the previous frame (clamped), reset the
// frame arena, drain the event queue, and clear the screen. Window lifecycle
// events (quit, Escape, resize) are consumed here; everything else folds into
// m->input for the game to read.
void mach_frame_begin(Mach *m) {
    m->frame_start = mach_ticks_ns();
    f32 dt = (f32)(m->frame_start - m->last_frame_time) / 1000000000.0f;
    if (dt > MACH_MAX_DT)
        dt = MACH_MAX_DT;
    m->dt = dt;
    m->last_frame_time = m->frame_start;

    mach_arena_reset(&m->frame_arena);
    mach_input_frame_begin(&m->input);
    RGFW_event ev;
    while (RGFW_window_checkEvent(m->window, &ev)) {
        // Every event lands in the snapshot first, then we act on the ones the core
        // cares about. Folding these into an else-chain would let the special cases
        // eat the event: escape_quits used to swallow the Escape keypress outright,
        // so a game could never see it.
        mach_input_handle_event(&m->input, &ev);

        if (ev.type == RGFW_windowClose) {
            MACH_LOG_INFO("quit requested");
            m->running = MACH_FALSE;
        } else if (m->escape_quits && ev.type == RGFW_keyPressed &&
                   ev.key.value == RGFW_keyEscape) {
            MACH_LOG_INFO("escape pressed, exiting");
            m->running = MACH_FALSE;
        } else if (ev.type == RGFW_windowResized) {
            mach_r2d_resized(&m->r2d);
        }
    }

    mach_r2d_begin(&m->r2d, m->clear_color);
}

// Finish a frame: hand the draws to the driver, sample how long the frame actually
// cost, show it, update the 1s FPS window, and wait out the frame cap.
void mach_frame_end(Mach *m) {
    // Submit, measure, and only then swap. The swap is where vsync blocks, so a
    // sample taken after it would be reading the wait, not the work: with vsync on
    // (the default) frame_ms would read a flat ~16.7ms whether the frame cost 2ms or
    // 15ms, which is exactly the question frame_ms exists to answer.
    mach_r2d_submit(&m->r2d);

    // The work this frame took, measured before any pacing wait -- this is the
    // number that says whether there is headroom, and the only one that keeps
    // saying it once a cap or vsync pins fps to a flat 60. It covers the update and
    // the draws' submission; it is CPU cost, not GPU time, and the GPU may still be
    // chewing on the batch when this is sampled.
    u64 work_ns = mach_ticks_ns() - m->frame_start;
    m->frame_ms = (f32)work_ns / 1000000.0f;
    if (m->frame_ms > m->frame_ms_peak_acc)
        m->frame_ms_peak_acc = m->frame_ms;

    // Now show the frame. Under vsync this is the wait; fps below is sampled after
    // it, so fps keeps measuring real elapsed time while frame_ms measures work.
    RGFW_window_swapBuffers_OpenGL(m->window);

    // FPS and the frame-time peak are both reported over the last completed 1s
    // window: an average hides the one 12ms frame that hitches, the peak is
    // what catches it.
    m->frame_count++;
    u64 now = mach_ticks_ns();
    if (now - m->fps_timer >= 1000000000ull) {
        m->fps = m->frame_count;
        m->frame_ms_peak = m->frame_ms_peak_acc;
        m->frame_count = 0;
        m->frame_ms_peak_acc = 0.0f;
        m->fps_timer = now;
    }

    // Pace to an absolute deadline rather than "sleep the remainder of this
    // frame": a sleep that overshoots by a millisecond would otherwise push the
    // next frame's deadline out by a millisecond too, and the error would walk.
    // Anchoring to the deadline lets a long frame be absorbed by the next short
    // one, so the rate holds. A frame that blows the budget outright (a stall, a
    // hitch) would leave the deadline in the past and hand us a burst of
    // zero-length frames to "catch up"; resetting to now when we fall behind
    // gives up the lost time instead of sprinting after it.
    if (m->frame_cap_ns) {
        m->frame_deadline += m->frame_cap_ns;
        if (now > m->frame_deadline)
            m->frame_deadline = now;
        else
            mach_wait_until_ns(m->frame_deadline);
    }
}

// =============================================================================
// input
// =============================================================================

// Mach_Input snapshot implementation (MACH_IMPLEMENTATION).

// Map an RGFW button to Mach_Mouse_Button, or -1 for buttons we don't model.
static i32 mach_mouse_button_index(u8 rgfw_button) {
    switch (rgfw_button) {
    case RGFW_mouseLeft:
        return MACH_MOUSE_LEFT;
    case RGFW_mouseRight:
        return MACH_MOUSE_RIGHT;
    case RGFW_mouseMiddle:
        return MACH_MOUSE_MIDDLE;
    default:
        return -1;
    }
}

void mach_input_frame_begin(Mach_Input *in) {
    memset(in->key_pressed, 0, sizeof(in->key_pressed));
    memset(in->key_released, 0, sizeof(in->key_released));
    memset(in->mouse_pressed, 0, sizeof(in->mouse_pressed));
    memset(in->mouse_released, 0, sizeof(in->mouse_released));
    in->mouse_delta = (Mach_Vec2){0.0f, 0.0f};
    in->wheel = 0.0f;
}

void mach_input_handle_event(Mach_Input *in, const RGFW_event *ev) {
    switch (ev->type) {
    case RGFW_keyPressed:
        if (!ev->key.repeat)
            in->key_pressed[ev->key.value] = 1;
        in->key_down[ev->key.value] = 1;
        break;
    case RGFW_keyReleased:
        in->key_down[ev->key.value] = 0;
        in->key_released[ev->key.value] = 1;
        break;
    case RGFW_mouseMotion: {
        // RGFW reports absolute positions; the delta is ours to accumulate.
        Mach_Vec2 m = {(f32)ev->mouse.x, (f32)ev->mouse.y};
        if (in->mouse_seen) {
            in->mouse_delta.x += m.x - in->mouse.x;
            in->mouse_delta.y += m.y - in->mouse.y;
        }
        in->mouse = m;
        in->mouse_seen = 1;
    } break;
    case RGFW_mouseButtonPressed:
    case RGFW_mouseButtonReleased: {
        i32 b = mach_mouse_button_index(ev->button.value);
        if (b < 0)
            break;
        if (ev->type == RGFW_mouseButtonPressed) {
            in->mouse_down[b] = 1;
            in->mouse_pressed[b] = 1;
        } else {
            in->mouse_down[b] = 0;
            in->mouse_released[b] = 1;
        }
    } break;
    case RGFW_mouseScroll:
        in->wheel += ev->delta.y;
        break;
    default:
        break;
    }
}

#endif // MACH_IMPLEMENTATION (once)

/*
zlib License

Copyright (c) 2026 Nathan Tebbs

This software is provided 'as-is', without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use
of this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject to
the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
   an acknowledgment in the product documentation would be appreciated but is
   not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
