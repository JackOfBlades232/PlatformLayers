/* PlatformLayers/GameLibs/texture.c */
#include "texture.h"

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
