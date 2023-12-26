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

typedef struct circle_tag {
    INCLUDE_TYPE(vec2f_t, center)
    f32 rad;
} circle_t;

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

static inline vec2f_t vec2f_sub(vec2f_t v1, vec2f_t v2)
{
    vec2f_t res = { v1.x - v2.x, v1.y - v2.y };
    return res;
}

static inline vec2f_t vec2f_mul(vec2f_t v1, vec2f_t v2)
{
    vec2f_t res = { v1.x * v2.x, v1.y * v2.y };
    return res;
}

static inline vec2f_t vec2f_div(vec2f_t v1, vec2f_t v2)
{
    vec2f_t res = { v1.x / v2.x, v1.y / v2.y };
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

static inline void vec2f_translate(vec2f_t *v, vec2f_t amt)
{
    v->x += amt.x;
    v->y += amt.y;
}

static inline vec2f_t vec2f_reflect(vec2f_t v, vec2f_t norm)
{
    vec2f_t proj = vec2f_scale(norm, v.x*norm.x + v.y*norm.y);
    return vec2f_sub(vec2f_scale(proj, 2.f), v);
}

static inline f32 vec2f_length(vec2f_t v)
{
    return sqrtf(v.x*v.x + v.y*v.y);
}

static inline vec2f_t vec2f_normalized(vec2f_t v)
{
    return vec2f_scale(v, 1.f/vec2f_length(v));
}

static inline bool rects_intersect(rect_t r1, rect_t r2)
{
    return (r1.x <= r2.x + r2.width && r2.x <= r1.x + r1.width) &&
           (r1.y <= r2.y + r2.height && r2.y <= r1.y + r1.height);
}

static inline bool rect_and_circle_intersect(rect_t r, circle_t c)
{
    f32 half_w = r.width*0.5f;
    f32 half_h = r.height*0.5f;
    f32 dx = MAX(ABS(c.x - (r.x + half_w)) - half_w, 0.f); 
    f32 dy = MAX(ABS(c.y - (r.y + half_h)) - half_h, 0.f); 
    return dx*dx + dy*dy <= c.rad*c.rad;
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

static bool intersect_ray_with_circle(ray_t ray, circle_t circ, f32 *tmin_out, f32 *tmax_out)
{
    vec2f_t dv = vec2f_sub(ray.orig, circ.center);

    // using the even b formula
    f32 a = ray.dir.x*ray.dir.x + ray.dir.y*ray.dir.y;
    f32 b = ray.dir.x*dv.x + ray.dir.y*dv.y;
    f32 c = dv.x*dv.x + dv.y*dv.y - circ.rad*circ.rad;

    f32 disc = b*b - a*c;
    if (disc < 0)
        return false;
    else {
        *tmin_out = (-b-sqrtf(disc))/a;
        *tmax_out = (-b+sqrtf(disc))/a;
        return true;
    }
}

static bool intersect_ray_with_circular_rect(ray_t ray, rect_t rect, f32 rad, f32 *tmin_out, f32 *tmax_out)
{
    f32 tmin, tmax;
    rect_t ext_r = { rect.x - rad, rect.y - rad,
                     rect.width + 2.f*rad, rect.height + 2.f*rad };

    if (!intersect_ray_with_rect(ray, ext_r, &tmin, &tmax))
        return false;

    // @TODO: refac
    vec2f_t near_point = vec2f_add(ray.orig, vec2f_scale(ray.dir, tmin));
    vec2f_t far_point  = vec2f_add(ray.orig, vec2f_scale(ray.dir, tmax));
    
    circle_t ulc = { rect.x, rect.y, rad };
    circle_t urc = { rect.x + rect.width, rect.y, rad };
    circle_t llc = { rect.x, rect.y + rect.height, rad };
    circle_t lrc = { rect.x + rect.width, rect.y + rect.height, rad };
    f32 ul_tmin, ul_tmax;
    f32 ur_tmin, ur_tmax;
    f32 ll_tmin, ll_tmax;
    f32 lr_tmin, lr_tmax;
    bool ul_int, ur_int, ll_int, lr_int;
    
    ul_int = intersect_ray_with_circle(ray, ulc, &ul_tmin, &ul_tmax);
    ur_int = intersect_ray_with_circle(ray, urc, &ur_tmin, &ur_tmax);
    ll_int = intersect_ray_with_circle(ray, llc, &ll_tmin, &ll_tmax);
    lr_int = intersect_ray_with_circle(ray, lrc, &lr_tmin, &lr_tmax);

    if (near_point.x < rect.x && near_point.y < rect.y) {
        if (!ul_int)
            return false;
        tmin = ul_tmin;
    } else if (near_point.x > rect.x + rect.width && near_point.y < rect.y) {
        if (!ur_int)
            return false;
        tmin = ur_tmin;
    } else if (near_point.x < rect.x && near_point.y > rect.y + rect.height) {
        if (!ll_int)
            return false;
        tmin = ll_tmin;
    } else if (near_point.x > rect.x + rect.width && near_point.y > rect.y + rect.height) {
        if (!lr_int)
            return false;
        tmin = lr_tmin;
    }

    if (far_point.x < rect.x && far_point.y < rect.y) {
        if (!ul_int)
            return false;
        tmax = ul_tmax;
    } else if (far_point.x > rect.x + rect.width && far_point.y < rect.y) {
        if (!ur_int)
            return false;
        tmax = ur_tmax;
    } else if (far_point.x < rect.x && far_point.y > rect.y + rect.height) {
        if (!ll_int)
            return false;
        tmax = ll_tmax;
    } else if (far_point.x > rect.x + rect.width && far_point.y > rect.y + rect.height) {
        if (!lr_int)
            return false;
        tmax = lr_tmax;
    }

    if (tmin_out) *tmin_out = tmin;
    if (tmax_out) *tmax_out = tmax;
    return true;
}

typedef struct static_body_tag {
    union {
        INCLUDE_TYPE(rect_t, r)
            // @HACK: due to the data layout I can still use .x and .y for center haha
            union {
                circle_t c;
                // @HACK: and due to this, also .center and .rad
                struct {
                    vec2f_t center;
                    f32 rad;
            };
        };
    };
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

static void draw_filled_sircle(offscreen_buffer_t *backbuffer, 
                               circle_t circle, u32 color)
{
    s32 cx = circle.center.x;
    s32 cy = circle.center.y;
    s32 x = -circle.rad;
    s32 y = 0;
    s32 sdf = x*x + y*y - circle.rad*circle.rad;
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

static inline void draw_circular_body(offscreen_buffer_t *backbuffer, static_body_t *body)
{
    draw_filled_sircle(backbuffer, body->c, body->col);
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
    } else if (body->x > max_x - EPS) {
        body->x = max_x - EPS;
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
    } else if (body->y > max_y - EPS) {
        body->y = max_y - EPS;
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
    ball.rad      = backbuffer->width/180;
    ball.x        = player.x + player.width/2;
    ball.y        = player.y - ball.rad/2 - EPS;
    /*
    ball.width    = backbuffer->width/90;
    ball.height   = ball.width;
    ball.x        = player.x + player.width/2 - ball.width/2;
    ball.y        = player.y - ball.height - EPS;
    */
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
    ray_t vel_ray = { ball.center, vec2f_normalized(vec2f_add(ball.vel, vec2f_neg(player.vel))) };

    f32 tmin = 0.f;
    ASSERTF(intersect_ray_with_circular_rect(vel_ray, player.r, ball.rad, &tmin, NULL),
            "BUG: Velocity ray must intersect with extended rect\n");

    vec2f_translate(&ball.center, vec2f_scale(vel_ray.dir, tmin));
    
    bool reflecting_from_side = ball.x < player.x + EPS || ball.x > player.x + player.width - EPS;
    vec2f_t contact_point = 
    {
        ball.x < player.x + EPS ? player.x : (ball.x > player.x + player.width - EPS ? player.x + player.width : ball.x),
        ball.y < player.y + EPS ? player.y : (ball.y > player.y + player.height - EPS ? player.y + player.height : ball.y)
    };
    vec2f_t normal = vec2f_normalized(vec2f_sub(ball.center, contact_point));
    
    ball.vel = vec2f_reflect(vec2f_neg(ball.vel), normal);

    // @HACK
    if (reflecting_from_side &&
        SGN(player.vel.x) != 0 &&
        SGN(ball.vel.x) != -SGN(player.vel.x) &&
        ABS(ball.vel.x) < ABS(player.vel.x) + EPS)
    {
        ball.vel.x = SGN(player.vel.x) * ABS(player.vel.x) * 1.1f;
    }
}

static void draw(offscreen_buffer_t *backbuffer)
{
    memset(backbuffer->bitmap_mem, 0, backbuffer->byte_size);

    for (u32 y = 0; y < BRICK_GRID_Y; y++)
        for (u32 x = 0; x < BRICK_GRID_X; x++) {
            brick_t* brick = &bricks[y][x];
            if (brick->is_alive)
                draw_body(backbuffer, brick);
        }

    draw_body(backbuffer, &player);
    draw_circular_body(backbuffer, &ball);
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
    if (input_key_is_down(input, INPUT_KEY_ESC))
        input->quit = true;

    player.vel.x = 0.f;
    if (input_char_is_down(input, 'A'))
        player.vel.x -= player.width * 4 / 3;
    if (input_char_is_down(input, 'D'))
        player.vel.x += player.width * 4 / 3;

    fixed_dt += dt;

    if (fixed_dt >= PHYSICS_UPDATE_INTERVAL) {
        // @TEST
        if (input_key_is_down(input, INPUT_KEY_SPACE))
            fixed_dt *= 0.2f;

        update_body(&player, fixed_dt);
        update_body(&ball, fixed_dt);

        clamp_body(&player, 2.f*ball.rad, 2.f*ball.rad,
                   backbuffer->width - player.width - 2.f*ball.rad,
                   backbuffer->height - player.height - 2.f*ball.rad, 
                   false, false);

        if (rect_and_circle_intersect(player.r, ball.c))
            resolve_player_to_ball_collision();

		for (u32 y = 0; y < BRICK_GRID_Y; y++)
            for (u32 x = 0; x < BRICK_GRID_X; x++) {
                brick_t* brick = &bricks[y][x];
                if (brick->is_alive && rect_and_circle_intersect(brick->r, ball.c))
                    brick->is_alive = false;
            }

        clamp_body(&ball, ball.rad, ball.rad, 
                   backbuffer->width - ball.rad, backbuffer->height - ball.rad,
                   true, true);

        fixed_dt = 0;
    }

    draw(backbuffer);
}

void game_redraw(offscreen_buffer_t *backbuffer)
{
    reset_game_entities(backbuffer);
    draw(backbuffer);
}
