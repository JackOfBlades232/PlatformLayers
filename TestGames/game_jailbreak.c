/* PlatformLayers/TestGames/test_game.c */
#include "../os.h"

#include <math.h>
#include <string.h>

#define MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))
#define MIN(_a, _b) ((_a) < (_b) ? (_a) : (_b))
#define CLAMP(_x, _min, _max) MIN(_max, MAX(_min, _x))

#define ABS(_a) ((_a) < 0 ? -(_a) : (_a))
#define SGN(_a) ((_a) < 0 ? -1 : ((_a) > 0 ? 1 : 0))

#define EPS 0.0001f

#define PHYSICS_UPDATE_INTERVAL 1.f/30.f

#define NOTEXTURE_DEBUG_COL 0xFFFF00FF

/* @TODO:
 *  Implement texture loading (object textures, background)
    * Refactor
    * Pull out libs
    * Optimize (6fps AAA)
 *  Implement ttf bitmap and font rendering (score)
 *  Implement wav loading and mixer (music & sounds)
 */

/* @BUG-s:
 *  Fix alpha overwrites in circle drawing (strips to the same pix)
 *  Fix flickering (check against bouncing box)
 */

// @TODO: check out ball flickering

// @TODO: pass bigger structs as pointers?

// @TODO: pull out texture reading to lib
// @TODO: add run-length encoded versions
typedef struct mapped_file_crawler_tag {
    mapped_file_t *file;
    const u8 *pos;
} mapped_file_crawler_t;

inline u64 crawler_bytes_read(mapped_file_crawler_t *crawler)
{
    // @TODO: check if this is valid
    ASSERT(crawler->pos >= crawler->file->mem);
    return crawler->pos - crawler->file->mem;
}

inline u64 crawler_bytes_left(mapped_file_crawler_t *crawler)
{
    ASSERT(crawler->file->byte_size >= crawler_bytes_read(crawler));
    return crawler->file->byte_size - crawler_bytes_read(crawler);
}

// @TODO: u32?
bool consume_file_chunk(mapped_file_crawler_t *crawler,
                        void *out_mem, u32 chunk_size)
{
    if (chunk_size == 0)
        return true;

    if (crawler_bytes_left(crawler) < chunk_size)
        return false;
    
    if (out_mem)
        memcpy(out_mem, crawler->pos, chunk_size);

    crawler->pos += chunk_size;
    return true;
}

#define CONSUME(_crawler, _out_struct) consume_file_chunk(_crawler, _out_struct, sizeof(*_out_struct))
#define DISCARD(_crawler, _size) consume_file_chunk(_crawler, NULL, _size)

mapped_file_crawler_t make_crawler(mapped_file_t *file)
{
    mapped_file_crawler_t crawler = { 0 };
    crawler.file = file;
    crawler.pos = file->mem;
    return crawler;
}

typedef enum texture_type_tag {
    textype_truecolor,
    textype_grayscale
} texture_type_t;

typedef struct texture_tag {
    INCLUDE_TYPE(allocated_mem_t, alloc)

    u32 width;
    u32 height;
    u32 bytes_per_pixel;

    texture_type_t type;
    bool loaded; // @HACK ?
} texture_t;

// @TODO: assert struct correctness (with sizes)
// @TODO: make this cross-compiler
#pragma pack(push, 1) // @HACK: disable padding
typedef struct tga_texture_header_tag {
    u8 id_length;
    u8 color_map_type;
    u8 image_type;

    struct {
        u16 first_entry_idx;
        u16 color_map_len;
        u8 color_map_entry_size;
    } color_map_spec;

    struct {
        u16 x_orig;
        u16 y_orig;
        u16 width;
        u16 height;
        u8 pix_depth;
        u8 image_descr;
    } image_spec;
} tga_texture_header_t;
#pragma pack(pop)

typedef enum tga_horiz_order_tag {
    tga_hord_left_to_right = 0, // Default
    tga_hord_right_to_left
} tga_horiz_order_t;

typedef enum tga_vert_order_tag {
    tga_vord_bottom_to_top = 0, // Default
    tga_vord_top_to_bottom
} tga_vert_order_t;

typedef struct tga_channel_mask_tag {
    u32 rm, rsh;
    u32 gm, gsh;
    u32 bm, bsh;
    u32 am, ash;
} tga_channel_mask_t;

typedef enum tga_image_type_tag {
    tga_imt_none = 0,
    // uncompressed formats
    tga_imt_cmap = 1,
    tga_imt_truecolor = 2,
    tga_imt_grayscale = 3,
    // rle = run length encoded
    tga_imt_cmap_rle = 9,
    tga_imt_truecolor_rle = 10,
    tga_imt_grayscale_rle = 11
} tga_image_type_t;

inline u8 scale_color_channel(u8 in, u8 scale)
{
    return scale == 0 ? 0 : (u32)in * scale / 0xFF;
}

inline u8 add_color_channels(u8 c1, u8 c2)
{
    return MIN((u32)c1 + c2, 0xFF); 
}

// @TODO: endianness?
inline u32 color_from_channels(u8 r, u8 g, u8 b, u8 a)
{
    return r | (g << 8) | (b << 16) | (a << 24);
}

inline u32 color_from_channel(u8 ch)
{
    return color_from_channels(ch, ch, ch, ch);
}

inline u32 scale_color(u32 in, u32 scale)
{
    return color_from_channels(scale_color_channel(in & 0xFF, scale & 0xFF),
                               scale_color_channel((in >> 8) & 0xFF, (scale >> 8) & 0xFF),
                               scale_color_channel((in >> 16) & 0xFF, (scale >> 16) & 0xFF),
                               scale_color_channel(in >> 24, scale >> 24));
}

inline u32 add_colors(u32 c1, u32 c2)
{
    return color_from_channels(add_color_channels(c1 & 0xFF, c2 & 0xFF),
                               add_color_channels((c1 >> 8) & 0xFF, (c2 >> 8) & 0xFF),
                               add_color_channels((c1 >> 16) & 0xFF, (c2 >> 16) & 0xFF),
                               add_color_channels(c1 >> 24, c2 >> 24));
}

u8 extract_channel_bits(u32 pix_data, u32 cm, u32 csh)
{
    ASSERT(cm >> csh <= 0xFF); // at most 8 bits per ch (@TODO: huh?)
    if (cm == 0)
        return 0;
    else if (cm >> csh == 0xFF)
        return (pix_data & cm) >> csh;
    else {
        u32 bit_val = (pix_data & cm) >> csh;
        u32 max_val = ((u32)(-1) & cm) >> csh;
        return bit_val * 0xFF / max_val;
    }
}

// @TODO: add logging
// @TODO: handle endianness. TGA is always little endian, so it may not work
// on other architectures.
void tga_try_map_texture(const char *path, texture_t *out_tex)
{
    ASSERT(!out_tex->loaded && out_tex->mem == NULL);
    memset(out_tex, 0, sizeof(*out_tex));

    mapped_file_t file = { 0 };
    if (!os_map_file(path, &file))
        goto cleanup;

    tga_texture_header_t header = { 0 };
    mapped_file_crawler_t crawler = make_crawler(&file);

    if (!CONSUME(&crawler, &header))
        goto cleanup;

    // We only support truecolor and grayscale
    if (header.color_map_type)
        goto cleanup;

    switch (header.image_type) {
    case tga_imt_truecolor:
        out_tex->type = textype_truecolor;
        break;

    case tga_imt_grayscale:
        out_tex->type = textype_grayscale;
        break;

        // @TODO: impl rle

    default:
        goto cleanup;
    }

    // We do not support colormap, thus this section must be 0
    if (header.color_map_spec.first_entry_idx != 0 ||
        header.color_map_spec.color_map_len != 0 ||
        header.color_map_spec.color_map_entry_size != 0)
    {
        goto cleanup;
    }

    // Parse width/height
    // @NOTE: ignore orig coords, we will position the texture where we want
    if (header.image_spec.width == 0 || header.image_spec.height == 0)
        goto cleanup;
    out_tex->width = header.image_spec.width;
    out_tex->height = header.image_spec.height;

    u32 in_bytes_per_pixel;
    tga_channel_mask_t cmask = { 0 };

    // Parse pixel depth and layout
    if (out_tex->type == textype_grayscale) {
        if (header.image_spec.pix_depth != 8)
            goto cleanup;

        in_bytes_per_pixel = 1;
        out_tex->bytes_per_pixel = 1;
    } else {
        if (header.image_spec.pix_depth == 16) {
            if (header.image_spec.image_descr & 0xF != 1) // Alpha depth == 1 bit
                goto cleanup;
            // 5 bits per channel
            cmask.rsh = 0;
            cmask.gsh = 5;
            cmask.bsh = 10;
            cmask.ash = 15;
            cmask.rm  = 0x1F << cmask.rsh;
            cmask.gm  = 0x1F << cmask.gsh;
            cmask.bm  = 0x1F << cmask.bsh;
            cmask.am  = 0x1  << cmask.ash;
        } else if (header.image_spec.pix_depth == 24) {
            if (header.image_spec.image_descr & 0xF != 0) // Alpha depth == 0 bit
                goto cleanup;

            // 8 bits per channel
            cmask.rsh = 0;
            cmask.gsh = 8;
            cmask.bsh = 16;
            cmask.rm  = 0xFF << cmask.rsh;
            cmask.gm  = 0xFF << cmask.gsh;
            cmask.bm  = 0xFF << cmask.bsh;

            // No alpha
            cmask.ash = 0;
            cmask.am  = 0x0;
        } else if (header.image_spec.pix_depth == 32) {
            if (header.image_spec.image_descr & 0xF != 8) // Alpha depth == 8 bit
                goto cleanup;

            // 8 bits per channel
            cmask.rsh = 0;
            cmask.gsh = 8;
            cmask.bsh = 16;
            cmask.ash = 24;
            cmask.rm  = 0xFF << cmask.rsh;
            cmask.gm  = 0xFF << cmask.gsh;
            cmask.bm  = 0xFF << cmask.bsh;
            cmask.am  = 0xFF << cmask.ash;
        } else
            goto cleanup; // @NOTE: we ignore 15bit pix depth because it is not aligned

        in_bytes_per_pixel = header.image_spec.pix_depth / 8;
        out_tex->bytes_per_pixel = BYTES_PER_PIXEL;
    }

    // Parse image layout
    tga_horiz_order_t horder = header.image_spec.image_descr & (0x1 << 4) ?
                               tga_hord_right_to_left : 
                               tga_hord_left_to_right;
    tga_vert_order_t vorder  = header.image_spec.image_descr & (0x1 << 5) ?
                               tga_vord_top_to_bottom : 
                               tga_vord_bottom_to_top;

    // Discard image id (and colormap data, that must be empty)
    if (!DISCARD(&crawler, header.id_length))
        goto cleanup;

    // Parse and copy image data
    out_tex->alloc = 
        os_allocate_mem(out_tex->width * out_tex->height * out_tex->bytes_per_pixel);


    // @TODO: test weird tga formats

    for (u32 y = 0; y < out_tex->height; y++)
        for (u32 x = 0; x < out_tex->width; x++) {
            u32 pix_data = 0;
            if (!consume_file_chunk(&crawler, &pix_data, in_bytes_per_pixel))
                goto cleanup;

            // @TODO: here also need to handle big endian

            // @NOTE: if in_bytes_per_pixel < 4, the order will be little end, 
            //        but we will be writing to top order bytes, so we need to
            //        shift down
            pix_data >>= 8 * (sizeof(pix_data) - in_bytes_per_pixel);

            u32 dest_x = horder == tga_hord_left_to_right ? x : out_tex->width - x - 1;
            u32 dest_y = vorder == tga_vord_top_to_bottom ? y : out_tex->height - y - 1;
            u32 dest_offset = dest_y*out_tex->width + dest_x;

            if (out_tex->type == textype_grayscale)
                ((u8 *)out_tex->mem)[dest_offset] = pix_data; // Just 8-bit grayscale
            else {
                // u32 ABGR color
                u32 abgr = color_from_channels(extract_channel_bits(pix_data, cmask.rm, cmask.rsh),
                                               extract_channel_bits(pix_data, cmask.gm, cmask.gsh),
                                               extract_channel_bits(pix_data, cmask.bm, cmask.bsh),
                                               extract_channel_bits(pix_data, cmask.am, cmask.ash));
                ((u32 *)out_tex->mem)[dest_offset] = abgr;
            }
        }

    out_tex->loaded = true;

cleanup:
    if (!out_tex->loaded && out_tex->mem)
        os_free_mem(&out_tex->alloc);
    if (file.mem)
        os_unmap_file(&file);
}

void tga_unmap_texture(texture_t *tex)
{
    ASSERT(tex);

    if (!tex->loaded)
        return;

    ASSERT(tex->mem);
    os_free_mem(&tex->alloc);
    tex->loaded = false;
}

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

typedef struct material_tag {
    bool has_albedo_tex;
    u32 col;
    // @TODO: make it just pointer and store textures sep
    texture_t *albedo;
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

static u32 texture_get_pixel(texture_t *tex, vec2f_t dst_coord, vec2f_t dst_dim)
{
    vec2f_t uv = { dst_coord.x / dst_dim.x, dst_coord.y / dst_dim.y };
    vec2f_t dim = { tex->width, tex->height };
    vec2f_t texcoord = vec2f_mul(uv, dim);

    u32 x = CLAMP(floor(texcoord.x), 0, tex->width-1);
    u32 y = CLAMP(floor(texcoord.y), 0, tex->height-1);

    if (tex->type == textype_grayscale) {
        u8 scale = ((u8 *)tex->mem)[y*tex->width + x];
        return color_from_channel(scale) | 0xFF000000;
    } else
        return ((u32 *)tex->mem)[y*tex->width + x];
}

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
                              f32 base_x, f32 min_y, f32 max_y,
                              rect_t circle_r, material_t *mat)
{
    if (base_x < 0 || base_x >= backbuffer->width)
        return;

    min_y = MAX(min_y, 0);
    max_y = MIN(max_y, backbuffer->height);

    // @TODO: do horiz strips instead for locality?
    u32 *pixel = backbuffer->bitmap_mem + (u32)min_y*backbuffer->width + (u32)base_x;
    for (u32 y = min_y; y < max_y; y++) {
        vec2f_t r_coord = { base_x - circle_r.x, y - circle_r.y };
        // @TODO: pull out
        u32 col = get_pixel_color(mat, r_coord, circle_r.size);
        write_pixel(pixel, col);
        pixel += backbuffer->width;
    }
}

static void draw_filled_sircle(offscreen_buffer_t *backbuffer, 
                               circle_t circle, material_t *mat)
{
    f32 cx = circle.center.x;
    f32 cy = circle.center.y;
    f32 x = -circle.rad;
    f32 y = 0.f;
    f32 sdf = x*x + y*y - circle.rad*circle.rad;
    f32 dx = 2.f*x + 1.f;
    f32 dy = 2.f*y + 1.f;

    rect_t circle_r = {circle.center.x-circle.rad, circle.center.y-circle.rad,
                        2.f*circle.rad, 2.f*circle.rad};

    while (-x >= y) {
        draw_circle_strip(backbuffer, cx + x, cy - y, cy + y + 1, circle_r, mat);
        draw_circle_strip(backbuffer, cx - x, cy - y, cy + y + 1, circle_r, mat);
        draw_circle_strip(backbuffer, cx - y, cy + x, cy - x + 1, circle_r, mat);
        draw_circle_strip(backbuffer, cx + y, cy + x, cy - x + 1, circle_r, mat);

        if (sdf <= 0.f) {
            sdf += dy;
            dy += 2.f;
            y += 1.f;
            if (ABS(sdf + dx) < ABS(sdf)) {
                sdf += dx;
                dx += 2.f;
                x += 1.f;
            }
        }
        else {
            sdf += dx;
            dx += 2.f;
            x += 1.f;
            if (ABS(sdf + dy) < ABS(sdf)) {
                sdf += dy;
                dy += 2.f;
                y += 1.f;
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

void set_material(material_t *mat, u32 col, texture_t *tex)
{
    if (tex->loaded) {
        mat->albedo = tex;
        mat->has_albedo_tex = true;
        mat->col = col;
    } else {
        mat->has_albedo_tex = false;
        mat->col = NOTEXTURE_DEBUG_COL;
    }
}

void game_init(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound)
{
    tga_try_map_texture("../Assets/ground.tga", &ground_tex);
    tga_try_map_texture("../Assets/sun.tga", &sun_tex);
    tga_try_map_texture("../Assets/bg.tga", &bg_tex);
    tga_try_map_texture("../Assets/bricks.tga", &bricks_tex);

    // @TEST
    const u32 obj_alpha_mask = 0x5FFFFFFF;

    set_material(&player.mat, obj_alpha_mask & 0xFFFFFFFF, &ground_tex);
    set_material(&ball.mat, obj_alpha_mask & 0xFFFFFF00, &sun_tex);
    set_material(&bg_mat, 0xFFFFFFFF, &bg_tex);

    for (u32 y = 0; y < BRICK_GRID_Y; y++)
        for (u32 x = 0; x < BRICK_GRID_X; x++) {
            brick_t *brick = &bricks[y][x];

            f32 coeff = (f32)y/(BRICK_GRID_Y-1);
            set_material(&brick->mat, 
                         (obj_alpha_mask & 0xFF000000) | (u32)(coeff*0xFF) << 16 | (u32)((1.f-coeff)*0xFF) << 8, 
                         &bricks_tex);

            brick->is_alive = true;
        }

    reset_game_entities(backbuffer);
}

void game_deinit(input_state_t *input, offscreen_buffer_t *backbuffer, sound_buffer_t *sound)
{
    tga_unmap_texture(&ground_tex);
    tga_unmap_texture(&bricks_tex);
    tga_unmap_texture(&sun_tex);
    tga_unmap_texture(&bg_tex);
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
