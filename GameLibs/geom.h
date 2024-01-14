/* PlatformLayers/GameLibs/geom.h */
#ifndef GEOM_SENTRY
#define GEOM_SENTRY

#include "../defs.h"
#include "utils.h"

#include <math.h>

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

inline vec2f_t vec2f_lit(f32 x, f32 y)
{
    vec2f_t res = { x, y };
    return res;
}

inline vec2f_t vec2f_add(vec2f_t v1, vec2f_t v2)
{
    vec2f_t res = { v1.x + v2.x, v1.y + v2.y };
    return res;
}

inline vec2f_t vec2f_sub(vec2f_t v1, vec2f_t v2)
{
    vec2f_t res = { v1.x - v2.x, v1.y - v2.y };
    return res;
}

inline vec2f_t vec2f_mul(vec2f_t v1, vec2f_t v2)
{
    vec2f_t res = { v1.x * v2.x, v1.y * v2.y };
    return res;
}

inline vec2f_t vec2f_div(vec2f_t v1, vec2f_t v2)
{
    vec2f_t res = { v1.x / v2.x, v1.y / v2.y };
    return res;
}

inline vec2f_t vec2f_scale(vec2f_t v, f32 scale)
{
    vec2f_t res = { v.x * scale, v.y * scale };
    return res;
}

inline vec2f_t vec2f_neg(vec2f_t v)
{
    vec2f_t res = { -v.x, -v.y };
    return res;
}

inline void vec2f_translate(vec2f_t *v, vec2f_t amt)
{
    v->x += amt.x;
    v->y += amt.y;
}

inline vec2f_t vec2f_reflect(vec2f_t v, vec2f_t norm)
{
    vec2f_t proj = vec2f_scale(norm, v.x*norm.x + v.y*norm.y);
    return vec2f_sub(vec2f_scale(proj, 2.f), v);
}

inline f32 vec2f_length(vec2f_t v)
{
    return sqrtf(v.x*v.x + v.y*v.y);
}

inline vec2f_t vec2f_normalized(vec2f_t v)
{
    return vec2f_scale(v, 1.f/vec2f_length(v));
}

inline bool rects_intersect(rect_t *r1, rect_t *r2)
{
    return (r1->x <= r2->x + r2->width && r2->x <= r1->x + r1->width) &&
           (r1->y <= r2->y + r2->height && r2->y <= r1->y + r1->height);
}

inline bool rect_and_circle_intersect(rect_t *r, circle_t *c)
{
    f32 half_w = r->width*0.5f;
    f32 half_h = r->height*0.5f;
    f32 dx = MAX(ABS(c->x - (r->x + half_w)) - half_w, 0.f); 
    f32 dy = MAX(ABS(c->y - (r->y + half_h)) - half_h, 0.f); 
    return dx*dx + dy*dy <= c->rad*c->rad;
}

bool intersect_ray_with_rect(ray_t *ray, rect_t *rect, f32 *tmin_out, f32 *tmax_out);
bool intersect_ray_with_circle(ray_t *ray, circle_t *circ, f32 *tmin_out, f32 *tmax_out);
bool intersect_ray_with_circular_rect(ray_t *ray, rect_t *rect, f32 rad, f32 *tmin_out, f32 *tmax_out);

#endif
