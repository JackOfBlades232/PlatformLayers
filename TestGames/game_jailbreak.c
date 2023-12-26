/* PlatformLayers/Windows/test_game.c */
#include "os.h"

#include <math.h>
#include <string.h>

#define MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))
#define MIN(_a, _b) ((_a) < (_b) ? (_a) : (_b))

#define ABS(_a) ((_a) < 0 ? -(_a) : (_a))
#define SGN(_a) ((_a) < 0 ? -1 : ((_a) > 0 ? 1 : 0))

#define EPS 0.0001f

#define PHYSICS_UPDATE_INTERVAL 1.f/30.f

/* @TODO:
 *  Implement texture loading (object textures, background)
 *  Implement ttf bitmap and font rendering (score)
 *  Implement wav loading and mixer (music & sounds)
 */

/* @BUG s:
 *  Fix flickering (check against bouncing box)
 */

// @TODO: check out ball flickering

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

#define INCLUDE_TYPE(_type, _name) union { _type; _type _name; };

typedef struct vec2f_tag {
    f32 x, y;
} vec2f_t;

typedef struct ray_tag {
    vec2f_t orig;
    vec2f_t dir;
} ray_t;

typedef struct rect_tag {
    INCLUDE_TYPE(vec2f_t, pos)
    union {
        vec2f_t size;
        struct { f32 width, height; };
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
    f32 ty2 = (rect.y + rect.height - ray.orig.y) / ray.dir.y;

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

typedef struct static_body_tag {
    INCLUDE_TYPE(rect_t, r)
    u32 col;
} static_body_t;

typedef struct body_tag {
    static_body_t;
    vec2f_t vel;
} body_t;

typedef struct brick_tag {
    static_body_t;
    bool is_alive;
} brick_t;

static body_t player = { 0 };
static body_t ball   = { 0 };

#define BRICK_GRID_X 5
#define BRICK_GRID_Y 5
static brick_t bricks[BRICK_GRID_Y][BRICK_GRID_X] = { 0 };

static f32 fixed_dt  = 0;

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

static void draw_circle_strip(offscreen_buffer_t *backbuffer, 
                              s32 base_x, s32 min_y, s32 max_y, u32 color)
{
    if (base_x < 0 || base_x >= backbuffer->width)
        return;

    min_y = MAX(min_y, 0);
    max_y = MIN(max_y, backbuffer->height);

    u32 *pixel = backbuffer->bitmap_mem + min_y*backbuffer->width + base_x;
    for (u32 y = min_y; y < max_y; y++) {
        *pixel = color;
        pixel += backbuffer->width;
    }
}

static void draw_filled_sircle(offscreen_buffer_t *backbuffer, vec2f_t center, 
                               f32 rad, u32 color)
{
    s32 cx = center.x;
    s32 cy = center.y;
    s32 x = -rad;
    s32 y = 0;
    s32 sdf = x*x + y*y - rad*rad;
    s32 dx = 2*x + 1;
    s32 dy = 2*y + 1;

    while (-x >= y) {
        draw_circle_strip(backbuffer, cx + x, cy - y, cy + y + 1, color);
        draw_circle_strip(backbuffer, cx - x, cy - y, cy + y + 1, color);
        draw_circle_strip(backbuffer, cx - y, cy + x, cy - x + 1, color);
        draw_circle_strip(backbuffer, cx + y, cy + x, cy - x + 1, color);

        if (sdf <= 0) {
            sdf += dy;
            dy += 2;
            y++;
            if (ABS(sdf + dx) < ABS(sdf)) {
                sdf += dx;
                dx += 2;
                x++;
            }
        } else {
            sdf += dx;
            dx += 2;
            x++;
            if (ABS(sdf + dy) < ABS(sdf)) {
                sdf += dy;
                dy += 2;
                y++;
            }
        }
    }
}

static inline void draw_body(offscreen_buffer_t *backbuffer, static_body_t *body)
{
    draw_rect(backbuffer, body->r, body->col);
}

static inline void update_body(body_t *body, f32 dt)
{
    vec2f_translate(&body->pos, vec2f_scale(body->vel, dt));
}

static inline vec2f_t body_center(body_t *body)
{
    return vec2f_add(body->pos, vec2f_scale(body->size, 0.5f));
}

static void clamp_body(body_t* body, 
                       f32 min_x, f32 min_y, f32 max_x, f32 max_y, 
                       bool flip_vel_x, bool flip_vel_y)
{
    if (body->x < min_x + EPS) {
        body->x = min_x + EPS;
        if (body->vel.x < -EPS)
            body->vel.x = -body->vel.x;
        else if (body->vel.x < EPS)
            body->vel.x = 1.f;
    } else if (body->x > max_x - body->width - EPS) {
        body->x = max_x - body->width - EPS;
        if (body->vel.x > EPS) {
            if (flip_vel_x)
                body->vel.x = -body->vel.x;
            else
                body->vel.x = 0.f;
        } else if (body->vel.x > -EPS)
            body->vel.x = -1.f;
    }
    if (body->y < min_y + EPS) {
        body->y = min_y + EPS;
        if (body->vel.y < -EPS)
            body->vel.y = -body->vel.y;
        else if (body->vel.y < EPS)
            body->vel.y = 1.f;
    } else if (body->y > max_y - body->height - EPS) {
        body->y = max_y - body->height - EPS;
        if (body->vel.y > EPS) {
            if (flip_vel_y)
                body->vel.y = -body->vel.y;
            else
                body->vel.y = 0.f;
        } else if (body->vel.y > -EPS)
            body->vel.y = -1.f;
    }
}

static void reset_game_entities(offscreen_buffer_t *backbuffer)
{
    player.width  = backbuffer->width/10;
    player.height = backbuffer->height/30;
    player.x      = backbuffer->width/2 - player.width/2;
    player.y      = 5*backbuffer->height/6 - player.height/2;
    ball.width    = backbuffer->width/120;
    ball.height   = ball.width;
    ball.x        = player.x + player.width/2 - ball.width/2;
    ball.y        = player.y - ball.height - EPS;
    ball.vel.x    = backbuffer->width/6;
    ball.vel.y    = backbuffer->height/3;
    const float brick_w = player.width;
    const float brick_h = player.height;
    const float brick_space_w = brick_w / 2;
    const float brick_space_h = brick_h;
    const float brick_padding_x = (backbuffer->width - brick_w * BRICK_GRID_X - brick_space_w * (BRICK_GRID_X - 1)) / 2;
    const float brick_padding_y = backbuffer->height/2 - brick_h*BRICK_GRID_Y - brick_space_h*(BRICK_GRID_Y-1);
    for (u32 y = 0; y < BRICK_GRID_Y; y++)
        for (u32 x = 0; x < BRICK_GRID_X; x++) {
            brick_t *brick = &bricks[y][x];
            brick->x = brick_padding_x + x*(brick_w + brick_space_w);
            brick->y = brick_padding_y + y*(brick_h + brick_space_h);
            brick->width = brick_w;
            brick->height = brick_h;
        }
}

static void resolve_player_to_ball_collision()
{
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

    
    if (ball.x <= player.x - ball.width + EPS || 
        ball.x >= player.x + player.width - EPS)
    {
        // @TODO: maybe use momentum balance?
        ball.vel.x = -ball.vel.x;
        if (SGN(ball.vel.x) == SGN(player.vel.x) &&
            ABS(ball.vel.x) < ABS(player.vel.x) + EPS)
        {
            ball.vel.x = SGN(ball.vel.x) * (ABS(player.vel.x) + EPS);
        }
    }
    if (ball.y <= player.y - ball.height + EPS ||
        ball.y >= player.y + player.height - EPS)
    {
        ball.vel.y = -ball.vel.y;
    }
}

void game_init(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound)
{
    player.col = 0xFFFF0000;
    ball.col   = 0xFFFFFF00;
    for (u32 y = 0; y < BRICK_GRID_Y; y++)
        for (u32 x = 0; x < BRICK_GRID_X; x++) {
            brick_t *brick = &bricks[y][x];

            f32 coeff = (f32)y/(BRICK_GRID_Y-1);
            brick->col = 0xFF000000 |
                         (u32)(coeff*0xFF) << 16 |
                         (u32)((1.f-coeff)*0xFF) << 8;

            brick->is_alive = true;
        }

    reset_game_entities(backbuffer);
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
        player.vel.x -= player.width * 4 / 3;
    if (input_char_is_down(input, 'D'))
        player.vel.x += player.width * 4 / 3;

    fixed_dt += dt;

    if (fixed_dt >= PHYSICS_UPDATE_INTERVAL) {
        update_body(&player, fixed_dt);
        update_body(&ball, fixed_dt);

        clamp_body(&player, ball.width, ball.height,
                   backbuffer->width - ball.width, backbuffer->height - ball.height, 
                   false, false);

        if (rects_intersect(player.r, ball.r))
            resolve_player_to_ball_collision();

		for (u32 y = 0; y < BRICK_GRID_Y; y++)
            for (u32 x = 0; x < BRICK_GRID_X; x++) {
                brick_t* brick = &bricks[y][x];
                if (brick->is_alive && rects_intersect(ball.r, brick->r))
                    brick->is_alive = false;
            }

        clamp_body(&ball, 0, 0, backbuffer->width, backbuffer->height, true, true);

        fixed_dt = 0;
    }

    for (u32 y = 0; y < BRICK_GRID_Y; y++)
        for (u32 x = 0; x < BRICK_GRID_X; x++) {
            brick_t* brick = &bricks[y][x];
            if (brick->is_alive)
                draw_body(backbuffer, brick);
        }
    draw_body(backbuffer, &player);

    // @TODO: impl correct circular body with collisions
    //draw_body(backbuffer, &ball);
    draw_filled_sircle(backbuffer, body_center(&ball), ball.size.y * 0.5f, ball.col);
}

void game_redraw(offscreen_buffer_t *backbuffer)
{
    reset_game_entities(backbuffer);

    memset(backbuffer->bitmap_mem, 0, backbuffer->byte_size);
    
    draw_body(backbuffer, &player);
    draw_body(backbuffer, &ball);
}
