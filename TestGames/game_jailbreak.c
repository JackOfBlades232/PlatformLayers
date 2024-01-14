/* PlatformLayers/TestGames/game_jalbreak.c */
#include "../os.h"
#include "../GameLibs/geom.h"
#include "../GameLibs/color.h"
#include "../GameLibs/texture.h"
#include "../GameLibs/tga_tex_loader.h"
#include "../GameLibs/utils.h"

#define EPS 0.0001f
#define PHYSICS_UPDATE_INTERVAL 1.f/30.f
#define NOTEXTURE_DEBUG_COL 0xFFFF00FF
#define BG_NOTEXTURE_DEBUG_COL 0xFF000000

/* @TODO:
 *  Implement texture loading (object textures, background)
    * Optimize (6fps AAA)
    * Refactor
 *  Implement ttf bitmap and font rendering (score)
 *  Implement wav loading and mixer (music & sounds)
 */

/* @BUG-s:
 *  Fix alpha overwrites in circle drawing (strips to the same pix)
 *  Fix flickering (check against bouncing box)
 */

// @TODO: check out ball flickering

// @TODO: pass bigger structs as pointers?

typedef struct material_tag {
    texture_t *albedo;
    bool has_albedo_tex;
    u32 col;
} material_t;

typedef struct static_body_tag {
    // @TODO: unhack
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
    
    INCLUDE_TYPE(material_t, mat)
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

static material_t bg_mat = { 0 };

// @TODO: make a more universal cache
static texture_t ground_tex = { 0 };
static texture_t bricks_tex = { 0 };
static texture_t sun_tex    = { 0 };
static texture_t bg_tex     = { 0 };

static f32 fixed_dt  = 0;

static inline u32 get_pixel_color(material_t *mat, vec2f_t dst_coord, vec2f_t dst_dim)
{
    if (mat->has_albedo_tex) {
        u32 tex_col = texture_get_pixel(mat->albedo, dst_coord, dst_dim);
        return scale_color(tex_col, mat->col);
    } else
        return mat->col;
}

static void write_pixel(u32 *dst, u32 col)
{
    u32 col_alpha = col >> 24;

    if (col_alpha == 0xFF)
        *dst = col;
    else if (col_alpha != 0) {
        u32 dst_alpha = *dst >> 24;
        if (dst_alpha + col_alpha > 0xFF)
            dst_alpha = 0xFF - col_alpha;
        
        u32 dst_alpha_mask = color_from_channel(dst_alpha);
        u32 col_alpha_mask = color_from_channel(col_alpha);

        u32 rgb = add_colors(scale_color(*dst, dst_alpha_mask), scale_color(col, col_alpha_mask));
        *dst = color_from_channels(rgb & 0xFF, (rgb >> 8) & 0xFF, (rgb >> 16) & 0xFF, dst_alpha + col_alpha);
    }
}

static void draw_rect(offscreen_buffer_t *backbuffer, rect_t r, material_t *mat)
{
    f32 xmin = MAX(r.x, 0);
    f32 xmax = MAX(MIN(r.x + r.width, backbuffer->width), 0);
    f32 ymin = MAX(r.y, 0);
    f32 ymax = MAX(MIN(r.y + r.height, backbuffer->height), 0);

    u32 drawing_pitch = backbuffer->width - (u32)xmax + (u32)xmin;
    u32 *pixel = backbuffer->bitmap_mem + (u32)ymin*backbuffer->width + (u32)xmin;

    // @TODO: fix texture drawing bug (waves) by keeping float coords here
    for (f32 y = ymin; y < ymax; y += 1.f) {
        for (f32 x = xmin; x < xmax; x += 1.f) {
            vec2f_t r_coord = { x - r.x, y - r.y };
            u32 col = get_pixel_color(mat, r_coord, r.size);
            write_pixel(pixel, col);
            pixel++;
        }
        pixel += drawing_pitch;
    }
}

static void draw_circle_strip(offscreen_buffer_t *backbuffer, 
                              f32 min_x, f32 max_x, f32 base_y,
                              rect_t circle_r, material_t *mat)
{
    if (base_y < 0 || base_y >= backbuffer->height)
        return;

    min_x = MAX(min_x, 0);
    max_x = MIN(max_x, backbuffer->width);

    u32 *pixel = backbuffer->bitmap_mem + (u32)base_y*backbuffer->width + (u32)min_x;
    for (u32 x = min_x; x < max_x; x++) {
        vec2f_t r_coord = { x - circle_r.x, base_y - circle_r.y };
        // @TODO: pull out
        u32 col = get_pixel_color(mat, r_coord, circle_r.size);
        write_pixel(pixel++, col);
    }
}

static void draw_filled_sircle(offscreen_buffer_t *backbuffer, 
                               circle_t circle, material_t *mat)
{
    f32 cx = circle.center.x;
    f32 cy = circle.center.y;
    f32 x = 0.f;
    f32 y = -circle.rad;
    f32 sdf = x*x + y*y - circle.rad*circle.rad;
    f32 dx = 2.f*x + 1.f;
    f32 dy = 2.f*y + 1.f;

    rect_t circle_r = {circle.center.x-circle.rad, circle.center.y-circle.rad,
                        2.f*circle.rad, 2.f*circle.rad};

    while (-y >= x) {
        if (sdf <= 0.f) {
            if (ABS(x) > EPS) {
                draw_circle_strip(backbuffer, cx + y, cx - y + 1, cy - x, circle_r, mat);
                draw_circle_strip(backbuffer, cx + y, cx - y + 1, cy + x, circle_r, mat);
            } else
                draw_circle_strip(backbuffer, cx + y, cx - y + 1, cy, circle_r, mat);
            sdf += dx;
            dx += 2.f;
            x += 1.f;
            if (ABS(sdf + dy) < ABS(sdf)) {
                if (-y > x) {
                    draw_circle_strip(backbuffer, cx - x, cx + x + 1, cy + y, circle_r, mat);
                    draw_circle_strip(backbuffer, cx - x, cx + x + 1, cy - y, circle_r, mat);
                }
                sdf += dy;
                dy += 2.f;
                y += 1.f;
            }
        }
        else {
            if (-y > x) {
                draw_circle_strip(backbuffer, cx - x, cx + x + 1, cy + y, circle_r, mat);
                draw_circle_strip(backbuffer, cx - x, cx + x + 1, cy - y, circle_r, mat);
            }
            sdf += dy;
            dy += 2.f;
            y += 1.f;
            if (ABS(sdf + dx) < ABS(sdf)) {
                if (ABS(x) > EPS) {
                    draw_circle_strip(backbuffer, cx + y, cx - y + 1, cy - x, circle_r, mat);
                    draw_circle_strip(backbuffer, cx + y, cx - y + 1, cy + x, circle_r, mat);
                } else
                    draw_circle_strip(backbuffer, cx + y, cx - y + 1, cy, circle_r, mat);
                sdf += dx;
                dx += 2.f;
                x += 1.f;
            }
        }
    }
}

static inline void draw_body(offscreen_buffer_t *backbuffer, static_body_t *body)
{
    draw_rect(backbuffer, body->r, &body->mat);
}

static inline void draw_circular_body(offscreen_buffer_t *backbuffer, static_body_t *body)
{
    draw_filled_sircle(backbuffer, body->c, &body->mat);
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
    ball.rad      = backbuffer->width/120;
    ball.x        = player.x + player.width/2;
    ball.y        = player.y - ball.rad/2 - EPS;
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
    // @TODO: refac
    rect_t bg_rect = { 0, 0, backbuffer->width, backbuffer->height };
    draw_rect(backbuffer, bg_rect, &bg_mat);

    for (u32 y = 0; y < BRICK_GRID_Y; y++)
        for (u32 x = 0; x < BRICK_GRID_X; x++) {
            brick_t* brick = &bricks[y][x];
            if (brick->is_alive)
                draw_body(backbuffer, brick);
        }

    draw_body(backbuffer, &player);
    draw_circular_body(backbuffer, &ball);
}

void set_material(material_t *mat, u32 col, u32 dbg_col, texture_t *tex)
{
    if (!tex) {
        mat->has_albedo_tex = false;
        mat->col = col;
    } else if (tex->loaded) {
        mat->albedo = tex;
        mat->has_albedo_tex = true;
        mat->col = col;
    } else {
        mat->has_albedo_tex = false;
        mat->col = dbg_col;
    }
}

void game_init(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound)
{
    //tga_load_texture("../Assets/ground.tga", &ground_tex);
    //tga_load_texture("../Assets/sun.tga", &sun_tex);
    //tga_load_texture("../Assets/bg.tga", &bg_tex);
    //tga_load_texture("../Assets/bricks.tga", &bricks_tex);

    // @TEST
    const u32 obj_alpha_mask = 0x5FFFFFFF;

    set_material(&player.mat, obj_alpha_mask & 0xFFFFFFFF, NOTEXTURE_DEBUG_COL, &ground_tex);
    //set_material(&ball.mat, obj_alpha_mask & 0xFFFFFF00, NOTEXTURE_DEBUG_COL, &sun_tex);
    set_material(&ball.mat, obj_alpha_mask & 0xFFFFFF00, NOTEXTURE_DEBUG_COL, NULL);
    set_material(&bg_mat, 0xFFFFFFFF, BG_NOTEXTURE_DEBUG_COL, &bg_tex);

    for (u32 y = 0; y < BRICK_GRID_Y; y++)
        for (u32 x = 0; x < BRICK_GRID_X; x++) {
            brick_t *brick = &bricks[y][x];

            f32 coeff = (f32)y/(BRICK_GRID_Y-1);
            set_material(&brick->mat, 
                         (obj_alpha_mask & 0xFF000000) | (u32)(coeff*0xFF) << 16 | (u32)((1.f-coeff)*0xFF) << 8, 
                         NOTEXTURE_DEBUG_COL,
                         &bricks_tex);

            brick->is_alive = true;
        }

    reset_game_entities(backbuffer);
}

void game_deinit(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound)
{
    texture_free_mem(&ground_tex);
    texture_free_mem(&bricks_tex);
    texture_free_mem(&sun_tex);
    texture_free_mem(&bg_tex);
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

        clamp_body(&player, 2.f*ball.rad + EPS, 2.f*ball.rad + EPS,
                   backbuffer->width - player.width - 2.f*ball.rad - EPS,
                   backbuffer->height - player.height - 2.f*ball.rad - EPS, 
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
