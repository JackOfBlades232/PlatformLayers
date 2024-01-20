/* PlatformLayers/GameLibs/texture.h */
#ifndef TEXTURE_SENTRY
#define TEXTURE_SENTRY

#include "../os.h"
#include "geom.h"

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
    bool loaded; // @TODO: is this a @HACK?
} texture_t;

// this only allocates/frees mem, loading funcs should be used
// for filling the texture and setting the loaded flag
void texture_alloc_mem(texture_t *tex);
void texture_free_mem(texture_t *tex);

inline u32 texture_get_pixel(texture_t *tex, vec2f_t dst_coord, vec2f_t dst_dim)
{
    vec2f_t uv = vec2f_div(dst_coord, dst_dim);
    vec2f_t tex_dim = { tex->width, tex->height };
    vec2f_t texcoord = vec2f_mul(uv, tex_dim);

    // @TODO: fast floor for correct reads
    u32 x = CLAMP(texcoord.x, 0, tex->width-1);
    u32 y = CLAMP(texcoord.y, 0, tex->height-1);

    if (tex->type == textype_grayscale) {
        u8 scale = ((u8 *)tex->mem)[y*tex->width + x];
        return color_from_channel(scale) | 0xFF000000;
    } else
        return ((u32 *)tex->mem)[y*tex->width + x];
}

#endif
