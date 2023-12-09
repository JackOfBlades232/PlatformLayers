/* PlatformLayers/Windows/test_game.c */
#include "os.h"

#include <math.h>
#include <string.h>

#define MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))
#define MIN(_a, _b) ((_a) < (_b) ? (_a) : (_b))

#define ABS(_a) ((_a) < 0 ? -(_a) : (_a))
#define SGN(_a) ((_a) < 0 ? -1 : ((_a) > 0 ? 1 : 0))

#define EPS 0.0001f

// @TODO: check out fps drops

static inline bool input_key_is_down(input_state_t *input, u32 key)
{
    if (key >= INPUT_KEY_MAX)
        return false;
    return input->pressed_keys[key].is_down;
}

// @TODO: I dont particularly like this separation
static inline bool input_char_is_down(input_state_t *input, u32 c)
{
    return input_key_is_down(input, char_to_input_key(c));
}

typedef struct vec2f_tag {
    f32 x, y;
} vec2f_t;

typedef struct ray_tag {
    vec2f_t orig;
    vec2f_t dir;
} ray_t;

typedef union rect_tag {
    struct {
        vec2f_t pos;
        vec2f_t size;
    };
    struct {
        f32 x, y;
        f32 width, height;
    };
} rect_t;

static inline vec2f_t vec2f_lit(f32 x, f32 y)
{
    vec2f_t res = { x, y };
    return res;
}

static inline vec2f_t vec2f_add(vec2f_t v1, vec2f_t v2)
{
    vec2f_t res = { v1.x + v2.x, v1.y + v2.y };
    return res;
}

static inline vec2f_t vec2f_scale(vec2f_t v, f32 scale)
{
    vec2f_t res = { v.x * scale, v.y * scale };
    return res;
}

static inline vec2f_t vec2f_neg(vec2f_t v)
{
    vec2f_t res = { -v.x, -v.y };
    return res;
}

static inline vec2f_t vec2f_normalized(vec2f_t v)
{
    return vec2f_scale(v, 1.f / sqrt(v.x*v.x + v.y*v.y));
}

static inline void vec2f_translate(vec2f_t *v, vec2f_t amt)
{
    v->x += amt.x;
    v->y += amt.y;
}

static inline bool rects_intersect(rect_t r1, rect_t r2)
{
    return (r1.x <= r2.x + r2.width && r2.x <= r1.x + r1.width) &&
           (r1.y <= r2.y + r2.height && r2.y <= r1.y + r1.height);
}

static bool intersect_ray_with_rect(ray_t ray, rect_t rect, f32 *tmin_out, f32 *tmax_out)
{
    f32 tx1 = (rect.x - ray.orig.x) / ray.dir.x;
    f32 tx2 = (rect.x + rect.width - ray.orig.x) / ray.dir.x;
    f32 ty1 = (rect.y - ray.orig.y) / ray.dir.y;
    f32 ty2 = (rect.y + rect.width - ray.orig.y) / ray.dir.y;

    f32 tx_min = MIN(tx1, tx2);
    f32 ty_min = MIN(ty1, ty2);
    f32 tx_max = MAX(tx1, tx2);
    f32 ty_max = MAX(ty1, ty2);

    if (tx_max < ty_min || ty_max < tx_min)
        return false;
    else {
        if (tmin_out) *tmin_out = MAX(tx_min, ty_min);
        if (tmax_out) *tmax_out = MIN(tx_max, ty_max);
        return true;
    }
}

#define INCLUDE_TYPE(_type, _name) union { _type; _type _name; };

typedef struct body_tag {
    INCLUDE_TYPE(rect_t, r)
    vec2f_t vel;
    u32 col;
} body_t;

static body_t player = { 0 };
static body_t ball   = { 0 };

static void draw_rect(offscreen_buffer_t *backbuffer, rect_t r, u32 color)
{
    u32 xmin = MAX(r.x, 0);
    u32 xmax = MAX(MIN(r.x + r.width, backbuffer->width), 0);
    u32 ymin = MAX(r.y, 0);
    u32 ymax = MAX(MIN(r.y + r.height, backbuffer->height), 0);

    u32 drawing_pitch = backbuffer->width - xmax + xmin;
    u32 *pixel = backbuffer->bitmap_mem + ymin*backbuffer->width + xmin;

    for (u32 y = ymin; y < ymax; y++) {
        for (u32 x = xmin; x < xmax; x++)
            *(pixel++) = color;
        pixel += drawing_pitch;
    }
}

static inline void draw_body(offscreen_buffer_t *backbuffer, body_t *body)
{
    draw_rect(backbuffer, body->r, body->col);
}

static inline void update_body(body_t *body, f32 dt)
{
    vec2f_translate(&body->pos, vec2f_scale(body->vel, dt));
}

// @TEMP
static void clamp_body(body_t* body, u32 world_w, u32 world_h, bool flip_vel_x, bool flip_vel_y)
{
    if (body->x < EPS) {
        body->x = EPS;
        if (body->vel.x < -EPS)
            body->vel.x = -body->vel.x;
        else if (body->vel.x < EPS)
            body->vel.x = 1.f;
    } else if (body->x > world_w - body->width - EPS) {
        body->x = world_w - body->width - EPS;
        if (body->vel.x > EPS) {
            if (flip_vel_x)
                body->vel.x = -body->vel.x;
            else
                body->vel.x = 0.f;
        } else if (body->vel.x > -EPS)
            body->vel.x = -1.f;
    }
    if (body->y < EPS) {
        body->y = EPS;
        if (body->vel.y < -EPS)
            body->vel.y = -body->vel.y;
        else if (body->vel.y < EPS)
            body->vel.y = 1.f;
    } else if (body->y > world_h - body->height - EPS) {
        body->y = world_h - body->height - EPS;
        if (body->vel.y > EPS) {
            if (flip_vel_y)
                body->vel.y = -body->vel.y;
            else
                body->vel.y = 0.f;
        } else if (body->vel.y > -EPS)
            body->vel.y = -1.f;
    }
}

void game_init(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound)
{
    player.width  = 150;
    player.height = 30;
    player.x      = backbuffer->width/2 - player.width/2;
    player.y      = 5*backbuffer->height/6 - player.height/2;
    player.col    = 0xFFFF0000;

    ball.width    = 15;
    ball.height   = 15;
    ball.x        = player.x + player.width/2 - ball.width/2;
    ball.y        = player.y - ball.height;
    ball.vel.y    = 300.0;
    ball.vel.x    = 300.0;
    ball.col      = 0xFFFFFF00;
}

void game_deinit(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound)
{
    
}

void game_update_and_render(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound, f32 dt)
{
    memset(backbuffer->bitmap_mem, 0, backbuffer->byte_size);

    if (input_key_is_down(input, INPUT_KEY_ESC))
        input->quit = true;
    
    player.vel.x = 0.f;
    if (input_char_is_down(input, 'A'))
        player.vel.x -= 200.f;
    if (input_char_is_down(input, 'D'))
        player.vel.x += 200.f;

    update_body(&player, dt);
    update_body(&ball, dt);

    // @TODO: make player bounds more strict, so that he cant press into the ball
    clamp_body(&player, backbuffer->width, backbuffer->height, false, false);

    // @TEST: noodling around, this is not correct
    if (rects_intersect(player.r, ball.r)) {
        rect_t ext_player_rect = { player.x - ball.width*0.5f,
                                   player.y - ball.height*0.5f,
                                   player.width + ball.width,
                                   player.height + ball.height };

        ray_t vel_ray = { vec2f_add(ball.pos, vec2f_scale(ball.size, 0.5f)),
                          vec2f_normalized(ball.vel) };

        f32 tmin = 0.f;
        ASSERTF(intersect_ray_with_rect(vel_ray, ext_player_rect, &tmin, NULL),
                "BUG: Velocity ray must intersect with extended rect\n");

        vec2f_translate(&ball.pos, vec2f_scale(vel_ray.dir, tmin));

        if (ball.x >= player.x - ball.width - EPS && 
            ball.x <= player.x + player.width + EPS)
        {
            ball.vel.y = -ball.vel.y;
        }
        if (ball.y >= player.y - ball.height - EPS && 
            ball.y <= player.y + player.height + EPS)
        {
            // @TODO: maybe use momentum balance?
            ball.vel.x = -ball.vel.x;
            if (SGN(ball.vel.x) == SGN(player.vel.x) &&
                ABS(ball.vel.x) < ABS(player.vel.x) + EPS)
            {
                ball.vel.x = SGN(ball.vel.x) * (ABS(player.vel.x) + EPS);
            }
        }

    }

    clamp_body(&ball, backbuffer->width, backbuffer->height, true, true);

    draw_body(backbuffer, &player);
    draw_body(backbuffer, &ball);
}

void game_redraw(offscreen_buffer_t *backbuffer)
{
    memset(backbuffer->bitmap_mem, 0, backbuffer->byte_size);

    // @TODO: temp solution, need prev screen size to reposition and resize player
    player.x     = backbuffer->width/2 - player.width/2;
    player.y     = 5*backbuffer->height/6 - player.height/2;
    player.vel.x = 0;
    ball.x       = player.x + player.width/2 - ball.width/2;
    ball.y       = player.y - ball.height;
    ball.vel.y   = 300.0;
    ball.vel.x   = 300.0;

    draw_body(backbuffer, &player);
    draw_body(backbuffer, &ball);
}
