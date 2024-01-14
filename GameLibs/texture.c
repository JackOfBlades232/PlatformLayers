/* PlatformLayers/GameLibs/texture.c */
#include "texture.h"
#include "color.h"

void texture_alloc_mem(texture_t *tex)
{
    ASSERT(tex);
    tex->alloc = os_allocate_mem(tex->width * tex->height * tex->bytes_per_pixel);
}

void texture_free_mem(texture_t *tex)
{
    ASSERT(tex);
    if (tex->mem)
        os_free_mem(&tex->alloc);
    tex->loaded = false;
}

u32 texture_get_pixel(texture_t *tex, vec2f_t dst_coord, vec2f_t dst_dim)
{
    vec2f_t uv = vec2f_div(dst_coord, dst_dim);
    vec2f_t tex_dim = { tex->width, tex->height };
    vec2f_t texcoord = vec2f_mul(uv, tex_dim);

    u32 x = CLAMP(floor(texcoord.x), 0, tex->width-1);
    u32 y = CLAMP(floor(texcoord.y), 0, tex->height-1);

    if (tex->type == textype_grayscale) {
        u8 scale = ((u8 *)tex->mem)[y*tex->width + x];
        return color_from_channel(scale) | 0xFF000000;
    } else
        return ((u32 *)tex->mem)[y*tex->width + x];
}
