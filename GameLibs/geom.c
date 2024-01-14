/* PlatformLayers/GameLibs/geom.c */
#include "geom.h"

bool intersect_ray_with_rect(ray_t ray, rect_t rect, f32 *tmin_out, f32 *tmax_out)
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

bool intersect_ray_with_circle(ray_t ray, circle_t circ, f32 *tmin_out, f32 *tmax_out)
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

bool intersect_ray_with_circular_rect(ray_t ray, rect_t rect, f32 rad, f32 *tmin_out, f32 *tmax_out)
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
