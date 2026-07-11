// One-player pong: keep the ball alive against the back wall. A little more
// game than hello.c — input, collision, a serve, a game over, a restart — but
// still one file and the same three-call loop. Build it with nob (see README).
//
//   W / Up      paddle up
//   S / Down    paddle down
//   Space       serve
//   R           restart after game over

#define MACH_IMPLEMENTATION
#include "mach.h"

#include <stdio.h>

#define PADDLE_W 14.0f
#define PADDLE_H 110.0f
#define PADDLE_MARGIN 40.0f // gap between the paddle and the right edge
#define PADDLE_SPEED 520.0f

#define BALL_SIZE 14.0f
#define BALL_SPEED 340.0f   // serve speed; every paddle hit adds a little
#define BALL_SPEED_UP 18.0f // px/s gained per hit
#define BALL_SPEED_MAX 900.0f
#define BALL_MAX_BOUNCE_ANGLE 1.0f // radians off horizontal at the paddle's tip

#define START_LIVES 3

typedef enum {
    STATE_SERVE, // ball parked on the paddle, waiting for Space
    STATE_PLAY,
    STATE_OVER,
} Game_State;

typedef struct {
    Game_State state;
    f32 paddle_y; // top of the paddle
    Mach_Vec2 ball;
    Mach_Vec2 ball_vel;
    f32 ball_speed;
    i32 score;
    i32 best;
    i32 lives;
} Game;

// Back to the serve: the ball rides the paddle (STATE_SERVE parks it there
// every frame) and leaves it straight and slow again.
static void serve(Game *g) {
    g->state = STATE_SERVE;
    g->ball_speed = BALL_SPEED;
    g->ball_vel = (Mach_Vec2){-BALL_SPEED, 0.0f};
}

static void reset(Game *g, f32 field_h) {
    g->paddle_y = field_h * 0.5f - PADDLE_H * 0.5f;
    g->score = 0;
    g->lives = START_LIVES;
    serve(g);
}

// Center a string of the built-in bitmap font on x, at the given scale.
static f32 text_centered_x(const Mach_Renderer *r, const char *s, f32 scale, f32 center_x) {
    f32 width = (f32)strlen(s) * (f32)r->font->advance * scale;
    return center_x - width * 0.5f;
}

int main(void) {
    Mach m = {0};
    // No target_fps: vsync paces the loop at the display's rate. Everything below
    // moves in px/s against m.dt, so the game plays the same at 60 or 144.
    if (!mach_init(&m, (Mach_Config){
                           .title = "pong - mach",
                           .width = 960,
                           .height = 640,
                           .escape_quits = 1,
                       }))
        return 1;

    Game g = {0};
    reset(&g, (f32)m.r2d.height);

    while (mach_running(&m)) {
        mach_frame_begin(&m);

        // The field is the window, so a resize just makes for a bigger court.
        f32 fw = (f32)m.r2d.width, fh = (f32)m.r2d.height;
        f32 paddle_x = fw - PADDLE_MARGIN - PADDLE_W;
        const Mach_Input *in = &m.input;

        // --- update ---------------------------------------------------------
        f32 move = 0.0f;
        if (in->key_down[RGFW_keyW] || in->key_down[RGFW_keyUp])
            move -= 1.0f;
        if (in->key_down[RGFW_keyS] || in->key_down[RGFW_keyDown])
            move += 1.0f;
        g.paddle_y = mach_clamp(g.paddle_y + move * PADDLE_SPEED * m.dt, 0.0f, fh - PADDLE_H);

        switch (g.state) {
        case STATE_SERVE:
            // Ride the paddle until the serve.
            g.ball.x = paddle_x - BALL_SIZE;
            g.ball.y = g.paddle_y + PADDLE_H * 0.5f - BALL_SIZE * 0.5f;
            if (in->key_pressed[RGFW_keySpace])
                g.state = STATE_PLAY;
            break;

        case STATE_PLAY: {
            g.ball.x += g.ball_vel.x * m.dt;
            g.ball.y += g.ball_vel.y * m.dt;

            // Walls: top, bottom, and the far left. Clamp on the bounce so the
            // ball can't stick to a wall it overshot.
            if (g.ball.y < 0.0f) {
                g.ball.y = 0.0f;
                g.ball_vel.y = -g.ball_vel.y;
            } else if (g.ball.y + BALL_SIZE > fh) {
                g.ball.y = fh - BALL_SIZE;
                g.ball_vel.y = -g.ball_vel.y;
            }
            if (g.ball.x < 0.0f) {
                g.ball.x = 0.0f;
                g.ball_vel.x = -g.ball_vel.x;
            }

            // Paddle: only while moving right, so a ball caught inside the
            // paddle can't be batted back and forth on consecutive frames. The
            // hit offset (-1 at the top tip, +1 at the bottom) picks the angle,
            // which is what makes the paddle a control surface and not a wall.
            b32 overlaps = g.ball.x + BALL_SIZE >= paddle_x && g.ball.x <= paddle_x + PADDLE_W &&
                           g.ball.y + BALL_SIZE >= g.paddle_y && g.ball.y <= g.paddle_y + PADDLE_H;
            if (overlaps && g.ball_vel.x > 0.0f) {
                f32 ball_mid = g.ball.y + BALL_SIZE * 0.5f;
                f32 paddle_mid = g.paddle_y + PADDLE_H * 0.5f;
                f32 offset = mach_clamp((ball_mid - paddle_mid) / (PADDLE_H * 0.5f), -1.0f, 1.0f);
                f32 angle = offset * BALL_MAX_BOUNCE_ANGLE;

                g.ball_speed = mach_min(g.ball_speed + BALL_SPEED_UP, BALL_SPEED_MAX);
                g.ball_vel.x = -g.ball_speed * cosf(angle);
                g.ball_vel.y = g.ball_speed * sinf(angle);
                g.ball.x = paddle_x - BALL_SIZE;

                g.score++;
                if (g.score > g.best)
                    g.best = g.score;
            }

            // Past the paddle: a life, and back to the serve.
            if (g.ball.x > fw) {
                g.lives--;
                if (g.lives <= 0) {
                    g.state = STATE_OVER;
                } else {
                    serve(&g);
                }
            }
        } break;

        case STATE_OVER:
            if (in->key_pressed[RGFW_keyR])
                reset(&g, fh);
            break;
        }

        // --- draw -----------------------------------------------------------
        // Dashed center line, drawn as a column of short rects.
        for (f32 y = 8.0f; y < fh; y += 32.0f) {
            mach_r2d_fill_rect(&m.r2d, fw * 0.5f - 2.0f, y, 4.0f, 16.0f, MACH_COLOR_BG_ACTIVE);
        }

        mach_r2d_fill_rect(&m.r2d, paddle_x, g.paddle_y, PADDLE_W, PADDLE_H, MACH_COLOR_FG_MAIN);
        if (g.state != STATE_OVER) {
            mach_r2d_fill_rect(&m.r2d, g.ball.x, g.ball.y, BALL_SIZE, BALL_SIZE,
                               MACH_COLOR_YELLOW_INTENSE);
        }

        char buf[64];
        snprintf(buf, sizeof buf, "score %d", g.score);
        mach_r2d_text(&m.r2d, 20, 20, 2, buf, MACH_COLOR_FG_MAIN);
        // fps sits at the cap, so it says nothing about headroom; frame_ms is
        // the frame's real cost and peak is the worst frame of the last second.
        snprintf(buf, sizeof buf, "best %d   lives %d   fps %d   %.1fms (peak %.1f)", g.best,
                 g.lives, m.fps, (f64)m.frame_ms, (f64)m.frame_ms_peak);
        mach_r2d_text(&m.r2d, 20, 48, 1, buf, MACH_COLOR_FG_DIM);

        if (g.state == STATE_SERVE) {
            const char *msg = "press space to serve";
            mach_r2d_text(&m.r2d, text_centered_x(&m.r2d, msg, 2, fw * 0.5f), fh * 0.5f - 8.0f, 2,
                          msg, MACH_COLOR_FG_ALT);
        } else if (g.state == STATE_OVER) {
            const char *msg = "game over";
            mach_r2d_text(&m.r2d, text_centered_x(&m.r2d, msg, 4, fw * 0.5f), fh * 0.5f - 40.0f, 4,
                          msg, MACH_COLOR_RED);
            snprintf(buf, sizeof buf, "you rallied %d - press r to play again", g.score);
            mach_r2d_text(&m.r2d, text_centered_x(&m.r2d, buf, 1, fw * 0.5f), fh * 0.5f + 8.0f, 1,
                          buf, MACH_COLOR_FG_DIM);
        }

        mach_frame_end(&m);
    }

    mach_shutdown(&m);
    return 0;
}
