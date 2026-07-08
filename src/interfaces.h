// =============================================================================
// mem: arena allocator
// =============================================================================

// Mach_Arena allocator: linear bump allocation over a linked list of malloc'd regions.
//
// Modeled on Tsoding's arena.h (https://github.com/tsoding/arena). An allocation
// bumps a cursor inside the current region; when a region fills, a larger one is
// chained on. Individual allocations are never freed on their own — you reset the
// arena (keep the memory, reuse it) or free it whole. That trades fine-grained
// frees for near-zero bookkeeping and no fragmentation, which fits allocations
// that share a lifetime: a world, a level, per-frame scratch.


typedef struct Mach_Arena_Region Mach_Arena_Region;

// A region's storage is measured in words (uintptr_t), not bytes, so the bump
// cursor stays word-aligned and every allocation comes back aligned for free.
struct Mach_Arena_Region {
    Mach_Arena_Region *next;
    usize count;        // words handed out
    usize capacity;     // words available
    uintptr_t data[];   // region storage (flexible array member)
};

// Zero-initialize to create an empty arena: Mach_Arena a = {0};
typedef struct {
    Mach_Arena_Region *begin;
    Mach_Arena_Region *end;
} Mach_Arena;

// Hand back `size` bytes from the arena, chaining on a new region if the current
// one is full. The result is uintptr_t-aligned. Memory is NOT zeroed. Returns
// NULL only if the backing malloc fails.
void *mach_arena_alloc(Mach_Arena *a, usize size);

// Mark every region empty so its memory is reused by later allocations, without
// returning anything to the OS. Pointers from before the reset become garbage.
void mach_arena_reset(Mach_Arena *a);

// Return every region to the OS and leave the arena empty (safe to reuse).
void mach_arena_free(Mach_Arena *a);

// =============================================================================
// math: 2D vectors and scalar helpers
// =============================================================================

// Math: 2D vectors and scalar helpers.
//
// The engine is 2D, so this is a 2D math library. Mach_Vec4 exists mainly as the
// backing of Mach_Color (render/color.h), which is what code should usually say.
// (3D vector/matrix math was removed with the SDL_GPU renderer; it returns if
// and when 3D does.)


typedef struct {
    f32 x, y;
} Mach_Vec2;

typedef struct {
    f32 x, y, z, w;
} Mach_Vec4;  // also used as RGBA color

// Scalar
f32 mach_min(f32 a, f32 b);
f32 mach_max(f32 a, f32 b);
f32 mach_clamp(f32 v, f32 lo, f32 hi);
f32 mach_lerp(f32 a, f32 b, f32 t);

// Mach_Vec2
Mach_Vec2 mach_vec2_add(Mach_Vec2 a, Mach_Vec2 b);
Mach_Vec2 mach_vec2_sub(Mach_Vec2 a, Mach_Vec2 b);
Mach_Vec2 mach_vec2_scale(Mach_Vec2 v, f32 s);
f32  mach_vec2_dot(Mach_Vec2 a, Mach_Vec2 b);
f32  mach_vec2_length(Mach_Vec2 v);
Mach_Vec2 mach_vec2_normalize(Mach_Vec2 v);
Mach_Vec2 mach_vec2_lerp(Mach_Vec2 a, Mach_Vec2 b, f32 t);

// =============================================================================
// color: Mach_Color type, helpers, stock palette (modus-vivendi)
// =============================================================================

// Mach_Color type and the stock palette.
//
// Mach_Color is Mach_Vec4 RGBA in [0,1] — the exact type every r2d call takes — under the
// name that says what it is. The palette is modus-vivendi (Protesilaos Stavrou's
// Emacs theme, https://protesilaos.com/emacs/modus-themes): WCAG-AAA-contrast
// colors designed for a black background, which is exactly what a game HUD wants.
// The names mirror the theme's own (bg-dim, red-warmer, ...), so its docs apply.
//
// A game can lean on these everywhere, or define its own colors with MACH_COLOR_HEX
// and ignore the palette entirely. The engine itself has no opinion.


typedef Mach_Vec4 Mach_Color;

// A Mach_Color from 0xRRGGBB, alpha 1. Every palette entry below is one of these, so
// the hex value stays visible and greppable.
#define MACH_COLOR_HEX(h) ((Mach_Color){ \
    (f32)(((h) >> 16) & 0xFF) / 255.0f, \
    (f32)(((h) >>  8) & 0xFF) / 255.0f, \
    (f32)(((h)      ) & 0xFF) / 255.0f, \
    1.0f })

// --- Basic values -------------------------------------------------------------

#define MACH_COLOR_BG_MAIN      MACH_COLOR_HEX(0x000000)
#define MACH_COLOR_BG_DIM       MACH_COLOR_HEX(0x1e1e1e)
#define MACH_COLOR_FG_MAIN      MACH_COLOR_HEX(0xffffff)
#define MACH_COLOR_FG_DIM       MACH_COLOR_HEX(0x989898)
#define MACH_COLOR_FG_ALT       MACH_COLOR_HEX(0xc6daff)
#define MACH_COLOR_BG_ACTIVE    MACH_COLOR_HEX(0x535353)
#define MACH_COLOR_BG_INACTIVE  MACH_COLOR_HEX(0x303030)
#define MACH_COLOR_BORDER       MACH_COLOR_HEX(0x646464)

#define MACH_COLOR_BLACK        MACH_COLOR_BG_MAIN
#define MACH_COLOR_WHITE        MACH_COLOR_FG_MAIN

// --- Common accents -----------------------------------------------------------

#define MACH_COLOR_RED              MACH_COLOR_HEX(0xff5f59)
#define MACH_COLOR_RED_WARMER       MACH_COLOR_HEX(0xff6b55)
#define MACH_COLOR_RED_COOLER       MACH_COLOR_HEX(0xff7f86)
#define MACH_COLOR_RED_FAINT        MACH_COLOR_HEX(0xff9580)
#define MACH_COLOR_RED_INTENSE      MACH_COLOR_HEX(0xff5f5f)

#define MACH_COLOR_GREEN            MACH_COLOR_HEX(0x44bc44)
#define MACH_COLOR_GREEN_WARMER     MACH_COLOR_HEX(0x70b900)
#define MACH_COLOR_GREEN_COOLER     MACH_COLOR_HEX(0x00c06f)
#define MACH_COLOR_GREEN_FAINT      MACH_COLOR_HEX(0x88ca9f)
#define MACH_COLOR_GREEN_INTENSE    MACH_COLOR_HEX(0x44df44)

#define MACH_COLOR_YELLOW           MACH_COLOR_HEX(0xd0bc00)
#define MACH_COLOR_YELLOW_WARMER    MACH_COLOR_HEX(0xfec43f)
#define MACH_COLOR_YELLOW_COOLER    MACH_COLOR_HEX(0xdfaf7a)
#define MACH_COLOR_YELLOW_FAINT     MACH_COLOR_HEX(0xd2b580)
#define MACH_COLOR_YELLOW_INTENSE   MACH_COLOR_HEX(0xefef00)

#define MACH_COLOR_BLUE             MACH_COLOR_HEX(0x2fafff)
#define MACH_COLOR_BLUE_WARMER      MACH_COLOR_HEX(0x79a8ff)
#define MACH_COLOR_BLUE_COOLER      MACH_COLOR_HEX(0x00bcff)
#define MACH_COLOR_BLUE_FAINT       MACH_COLOR_HEX(0x82b0ec)
#define MACH_COLOR_BLUE_INTENSE     MACH_COLOR_HEX(0x338fff)

#define MACH_COLOR_MAGENTA          MACH_COLOR_HEX(0xfeacd0)
#define MACH_COLOR_MAGENTA_WARMER   MACH_COLOR_HEX(0xf78fe7)
#define MACH_COLOR_MAGENTA_COOLER   MACH_COLOR_HEX(0xb6a0ff)
#define MACH_COLOR_MAGENTA_FAINT    MACH_COLOR_HEX(0xcaa6df)
#define MACH_COLOR_MAGENTA_INTENSE  MACH_COLOR_HEX(0xff66ff)

#define MACH_COLOR_CYAN             MACH_COLOR_HEX(0x00d3d0)
#define MACH_COLOR_CYAN_WARMER      MACH_COLOR_HEX(0x4ae2f0)
#define MACH_COLOR_CYAN_COOLER      MACH_COLOR_HEX(0x6ae4b9)
#define MACH_COLOR_CYAN_FAINT       MACH_COLOR_HEX(0x9ac8e0)
#define MACH_COLOR_CYAN_INTENSE     MACH_COLOR_HEX(0x00eff0)

// --- Uncommon accents ---------------------------------------------------------

#define MACH_COLOR_RUST    MACH_COLOR_HEX(0xdb7b5f)
#define MACH_COLOR_GOLD    MACH_COLOR_HEX(0xc0965b)
#define MACH_COLOR_OLIVE   MACH_COLOR_HEX(0x9cbd6f)
#define MACH_COLOR_SLATE   MACH_COLOR_HEX(0x76afbf)
#define MACH_COLOR_INDIGO  MACH_COLOR_HEX(0x9099d9)
#define MACH_COLOR_MAROON  MACH_COLOR_HEX(0xcf7fa7)
#define MACH_COLOR_PINK    MACH_COLOR_HEX(0xd09dc0)

// --- Accent backgrounds (intense > subtle > nuanced) --------------------------

#define MACH_COLOR_BG_RED_INTENSE      MACH_COLOR_HEX(0x9d1f1f)
#define MACH_COLOR_BG_GREEN_INTENSE    MACH_COLOR_HEX(0x2f822f)
#define MACH_COLOR_BG_YELLOW_INTENSE   MACH_COLOR_HEX(0x7a6100)
#define MACH_COLOR_BG_BLUE_INTENSE     MACH_COLOR_HEX(0x1640b0)
#define MACH_COLOR_BG_MAGENTA_INTENSE  MACH_COLOR_HEX(0x7030af)
#define MACH_COLOR_BG_CYAN_INTENSE     MACH_COLOR_HEX(0x2266ae)

#define MACH_COLOR_BG_RED_SUBTLE       MACH_COLOR_HEX(0x620f2a)
#define MACH_COLOR_BG_GREEN_SUBTLE     MACH_COLOR_HEX(0x00422a)
#define MACH_COLOR_BG_YELLOW_SUBTLE    MACH_COLOR_HEX(0x4a4000)
#define MACH_COLOR_BG_BLUE_SUBTLE      MACH_COLOR_HEX(0x242679)
#define MACH_COLOR_BG_MAGENTA_SUBTLE   MACH_COLOR_HEX(0x552f5f)
#define MACH_COLOR_BG_CYAN_SUBTLE      MACH_COLOR_HEX(0x004065)

#define MACH_COLOR_BG_RED_NUANCED      MACH_COLOR_HEX(0x3a0c14)
#define MACH_COLOR_BG_GREEN_NUANCED    MACH_COLOR_HEX(0x092f1f)
#define MACH_COLOR_BG_YELLOW_NUANCED   MACH_COLOR_HEX(0x381d0f)
#define MACH_COLOR_BG_BLUE_NUANCED     MACH_COLOR_HEX(0x12154a)
#define MACH_COLOR_BG_MAGENTA_NUANCED  MACH_COLOR_HEX(0x2f0c3f)
#define MACH_COLOR_BG_CYAN_NUANCED     MACH_COLOR_HEX(0x042837)

// --- Special purpose ----------------------------------------------------------

#define MACH_COLOR_BG_POPUP    MACH_COLOR_HEX(0x0c0c0c)
#define MACH_COLOR_BG_HOVER    MACH_COLOR_HEX(0x45605e)
#define MACH_COLOR_BG_HL_LINE  MACH_COLOR_HEX(0x2f3849)
#define MACH_COLOR_BG_REGION   MACH_COLOR_HEX(0x5a5a5a)

// --- Helpers ------------------------------------------------------------------

// Same color, different alpha.
static inline Mach_Color mach_color_alpha(Mach_Color c, f32 a) { c.w = a; return c; }

// Multiply RGB by f (keeps alpha): cheap directional shading.
static inline Mach_Color mach_color_shade(Mach_Color c, f32 f) {
    return (Mach_Color){c.x * f, c.y * f, c.z * f, c.w};
}

// Move RGB toward white by t in [0,1] (keeps alpha).
static inline Mach_Color mach_color_lighten(Mach_Color c, f32 t) {
    return (Mach_Color){c.x + (1.0f - c.x) * t, c.y + (1.0f - c.y) * t,
                   c.z + (1.0f - c.z) * t, c.w};
}

// Componentwise blend from a to b, alpha included.
static inline Mach_Color mach_color_lerp(Mach_Color a, Mach_Color b, f32 t) {
    return (Mach_Color){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                   a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

// =============================================================================
// gl: the hand-declared GL 3.3 core surface
// =============================================================================

// Minimal OpenGL 3.3 core surface for the 2D renderer.
//
// The renderer declares exactly the entry points and constants it uses instead
// of pulling in platform GL headers. mach_r2d_init fills the function table through
// RGFW's proc loader once the context exists; the table lives inside the
// Mach_Renderer struct — pointer-passed like all engine state — so a hot-reloaded
// game library draws through the pointers the host loaded.


// Constants the renderer uses (values fixed by the GL spec). Guarded as a block
// in case a platform GL header ends up in the same translation unit.
#ifndef GL_TRIANGLES
#define GL_TRIANGLES            0x0004
#define GL_UNSIGNED_BYTE        0x1401
#define GL_UNSIGNED_SHORT       0x1403
#define GL_FLOAT                0x1406
#define GL_RGBA                 0x1908
#define GL_RGBA8                0x8058
#define GL_TEXTURE_2D           0x0DE1
#define GL_TEXTURE0             0x84C0
#define GL_TEXTURE_MAG_FILTER   0x2800
#define GL_TEXTURE_MIN_FILTER   0x2801
#define GL_TEXTURE_WRAP_S       0x2802
#define GL_TEXTURE_WRAP_T       0x2803
#define GL_NEAREST              0x2600
#define GL_LINEAR               0x2601
#define GL_CLAMP_TO_EDGE        0x812F
#define GL_BLEND                0x0BE2
#define GL_SCISSOR_TEST         0x0C11
#define GL_SRC_ALPHA            0x0302
#define GL_ONE_MINUS_SRC_ALPHA  0x0303
#define GL_COLOR_BUFFER_BIT     0x00004000
#define GL_ARRAY_BUFFER         0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW          0x88E0
#define GL_VERTEX_SHADER        0x8B31
#define GL_FRAGMENT_SHADER      0x8B30
#define GL_COMPILE_STATUS       0x8B81
#define GL_LINK_STATUS          0x8B82
#define GL_UNPACK_ALIGNMENT     0x0CF5
#define GL_VERSION              0x1F02
#endif

// The loaded GL entry points, named without the gl prefix: gl->DrawElements(...).
typedef struct Mach_GLApi {
    void (*ActiveTexture)(u32 texture);
    void (*AttachShader)(u32 program, u32 shader);
    void (*BindBuffer)(u32 target, u32 buffer);
    void (*BindTexture)(u32 target, u32 texture);
    void (*BindVertexArray)(u32 array);
    void (*BlendFunc)(u32 sfactor, u32 dfactor);
    void (*BufferData)(u32 target, isize size, const void *data, u32 usage);
    void (*BufferSubData)(u32 target, isize offset, isize size, const void *data);
    void (*Clear)(u32 mask);
    void (*ClearColor)(f32 r, f32 g, f32 b, f32 a);
    void (*CompileShader)(u32 shader);
    u32  (*CreateProgram)(void);
    u32  (*CreateShader)(u32 type);
    void (*DeleteBuffers)(i32 n, const u32 *buffers);
    void (*DeleteProgram)(u32 program);
    void (*DeleteShader)(u32 shader);
    void (*DeleteTextures)(i32 n, const u32 *textures);
    void (*DeleteVertexArrays)(i32 n, const u32 *arrays);
    void (*Disable)(u32 cap);
    void (*DrawElements)(u32 mode, i32 count, u32 type, const void *indices);
    void (*Enable)(u32 cap);
    void (*EnableVertexAttribArray)(u32 index);
    void (*GenBuffers)(i32 n, u32 *buffers);
    void (*GenTextures)(i32 n, u32 *textures);
    void (*GenVertexArrays)(i32 n, u32 *arrays);
    void (*GetProgramInfoLog)(u32 program, i32 max_len, i32 *len, char *log);
    void (*GetProgramiv)(u32 program, u32 pname, i32 *params);
    void (*GetShaderInfoLog)(u32 shader, i32 max_len, i32 *len, char *log);
    void (*GetShaderiv)(u32 shader, u32 pname, i32 *params);
    const u8 *(*GetString)(u32 name);
    i32  (*GetUniformLocation)(u32 program, const char *name);
    void (*LinkProgram)(u32 program);
    void (*PixelStorei)(u32 pname, i32 param);
    void (*Scissor)(i32 x, i32 y, i32 w, i32 h);
    void (*ShaderSource)(u32 shader, i32 count, const char *const *string, const i32 *length);
    void (*TexImage2D)(u32 target, i32 level, i32 internal_format, i32 w, i32 h,
                       i32 border, u32 format, u32 type, const void *pixels);
    void (*TexParameteri)(u32 target, u32 pname, i32 param);
    void (*Uniform1i)(i32 location, i32 v0);
    void (*Uniform2f)(i32 location, f32 v0, f32 v1);
    void (*UseProgram)(u32 program);
    void (*VertexAttribPointer)(u32 index, i32 size, u32 type, u8 normalized,
                                i32 stride, const void *pointer);
    void (*Viewport)(i32 x, i32 y, i32 w, i32 h);
} Mach_GLApi;

// A GPU texture and its pixel size (for sprite sizing and atlas UVs).
typedef struct {
    u32 id;
    f32 w, h;
} Mach_R2D_Texture;

// =============================================================================
// font: 8x8 bitmap font
// =============================================================================

// Bitmap font: 8x8 glyphs baked into a GL texture atlas for 2D text.


struct Mach_Renderer;

typedef struct {
    Mach_R2D_Texture atlas;        // RGBA: white glyph pixels with alpha; tinted per-vertex
    i32 glyph_w, glyph_h;     // glyph cell size in pixels
    i32 advance;              // horizontal step per character
} Mach_Font;

Mach_Font *mach_font_create(struct Mach_Renderer *r);
void  mach_font_destroy(struct Mach_Renderer *r, Mach_Font *font);

// Normalized atlas UVs for an ASCII character. Returns false for glyphs outside
// the printable range.
b32 mach_font_glyph_uv(const Mach_Font *font, char ch, f32 *u0, f32 *v0, f32 *u1, f32 *v1);

// =============================================================================
// image: stb_image loading
// =============================================================================

// Mach_Image loading: PNG, BMP, JPG support via stb_image.


typedef struct {
    u8 *data;        // Raw pixel data (RGBA)
    i32 width;
    i32 height;
    i32 channels;    // Usually 4 (RGBA)
} Mach_Image;

// Load image from file. Returns image with allocated data, or zeroed struct on failure.
Mach_Image mach_image_load(const char *path);

// Free image data.
void mach_image_free(Mach_Image *img);

// =============================================================================
// render2d: the batch renderer
// =============================================================================

// 2D renderer over OpenGL 3.3 core: one batched stream of textured,
// vertex-colored triangles, plus primitives, text, sprites, and an isometric
// camera. See the README for the design.


// Isometric tile footprint in screen pixels at zoom 1 (classic 2:1 diamond), and
// the screen height of one unit of block elevation.
#define MACH_ISO_TILE_W 64.0f
#define MACH_ISO_TILE_H 32.0f
#define MACH_ISO_ELEV   28.0f

// 2D camera over the isometric plane: an iso-space pan point centered on screen,
// and a zoom (pixels-per-iso-unit multiplier).
typedef struct {
    Mach_Vec2 pan;
    f32  zoom;
} Mach_Camera2D;

// One vertex of the batch. Everything — fills, text, sprites — draws through
// the same shader; untextured shapes sample a 1x1 white texture.
typedef struct {
    f32   x, y;   // window points; the shader maps to clip space
    f32   u, v;
    Mach_Color color;
} Mach_R2D_Vertex;

// Batch capacity. A flush costs one glDrawElements; overflowing mid-frame just
// splits the frame into more draws, so these only need to cover the common case.
#define MACH_R2D_MAX_VERTS   8192
#define MACH_R2D_MAX_INDICES 16384

typedef struct Mach_Renderer {
    RGFW_window *window;
    Mach_GLApi gl;              // loaded GL entry points (see gl.h)

    i32 width, height;         // logical render size (window points)
    i32 fb_width, fb_height;   // framebuffer size in pixels (differs under HiDPI)
    Mach_Font *font;                // bitmap font as a GL texture atlas

    // GL objects, created once at init.
    u32 program;
    u32 vao, vbo, ibo;
    i32 u_screen;              // uniform: logical size, for point -> clip mapping
    Mach_R2D_Texture white;         // 1x1 white, sampled by untextured draws

    // The pending batch: appended by the draw calls, flushed on texture change,
    // scissor change, overflow, or present.
    u32        batch_tex;      // texture the pending vertices sample
    i32        vert_count, index_count;
    Mach_R2D_Vertex verts[MACH_R2D_MAX_VERTS];
    u16        indices[MACH_R2D_MAX_INDICES];
} Mach_Renderer;

// Lifecycle. The window must already hold a current GL 3.3 core context.
b32  mach_r2d_init(Mach_Renderer *r, RGFW_window *window);
void mach_r2d_shutdown(Mach_Renderer *r);

// Re-read the window and framebuffer size. Call after the window is resized so
// render and input coordinates track the new size.
void mach_r2d_resized(Mach_Renderer *r);

// Frame.
void mach_r2d_begin(Mach_Renderer *r, Mach_Color clear);
void mach_r2d_present(Mach_Renderer *r);

// Screen-space primitives. Colors are RGBA in [0,1]; see color.h for the palette.
void mach_r2d_fill_rect(Mach_Renderer *r, f32 x, f32 y, f32 w, f32 h, Mach_Color color);
void mach_r2d_fill_poly(Mach_Renderer *r, const Mach_Vec2 *pts, i32 n, Mach_Color color);  // convex, <=16 pts
void mach_r2d_poly_outline(Mach_Renderer *r, const Mach_Vec2 *pts, i32 n, Mach_Color color);  // closed loop, <=16 pts
void mach_r2d_text(Mach_Renderer *r, f32 x, f32 y, f32 scale, const char *text, Mach_Color color);

// Clip rect in window points (the UI's scissor). Draws between begin/end are
// clipped; nesting is not supported.
void mach_r2d_clip_begin(Mach_Renderer *r, f32 x, f32 y, f32 w, f32 h);
void mach_r2d_clip_end(Mach_Renderer *r);

// Textures and sprites (for real art later). Tint multiplies the texture; pass
// white for none. A zero id means the load failed.
Mach_R2D_Texture mach_r2d_texture_from_pixels(Mach_Renderer *r, const void *rgba, i32 w, i32 h, b32 nearest);
Mach_R2D_Texture mach_r2d_load_texture(Mach_Renderer *r, const char *path);
void mach_r2d_destroy_texture(Mach_Renderer *r, Mach_R2D_Texture *tex);
void mach_r2d_sprite(Mach_Renderer *r, Mach_R2D_Texture tex, f32 x, f32 y, f32 scale, Mach_Color tint);

// Isometric projection helpers (no Mach_Renderer needed). `elev` is block height in
// units; the inverse solves on the ground plane (elev 0).
Mach_Vec2 mach_iso_to_screen(const Mach_Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 grid_x, f32 grid_y, f32 elev);
Mach_Vec2 mach_screen_to_iso(const Mach_Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 screen_x, f32 screen_y);

// =============================================================================
// input: per-frame snapshot
// =============================================================================

// Per-frame input snapshot: keyboard and mouse state the game reads directly,
// instead of draining window events itself.
//
// The engine owns one of these (Mach.input) and fills it while draining the
// event queue in mach_frame_begin. "down" persists while a key or button is
// held; "pressed"/"released" mark this frame's edges and are cleared at the top
// of the next frame. Read fields directly: in->key_pressed[RGFW_key1].


typedef enum {
    MACH_MOUSE_LEFT = 0,
    MACH_MOUSE_RIGHT,
    MACH_MOUSE_MIDDLE,
    MACH_MOUSE_BUTTON_COUNT,
} Mach_Mouse_Button;

typedef struct {
    // Keyboard, indexed by RGFW_key. key_pressed excludes OS key repeats.
    u8 key_down[RGFW_keyLast];
    u8 key_pressed[RGFW_keyLast];

    // Mouse, in render coordinates (window points). wheel is this frame's scroll,
    // positive away from the user.
    Mach_Vec2 mouse;
    Mach_Vec2 mouse_delta;
    f32  wheel;
    u8   mouse_down[MACH_MOUSE_BUTTON_COUNT];
    u8   mouse_pressed[MACH_MOUSE_BUTTON_COUNT];
    u8   mouse_released[MACH_MOUSE_BUTTON_COUNT];

    u8 mouse_seen;  // internal: first motion event seeds mouse without a delta spike
} Mach_Input;

// Clear the per-frame edges (pressed/released/delta/wheel). Held state persists.
void mach_input_frame_begin(Mach_Input *in);

// Fold one RGFW event into the snapshot. Events the snapshot doesn't model are ignored.
void mach_input_handle_event(Mach_Input *in, const RGFW_event *ev);

