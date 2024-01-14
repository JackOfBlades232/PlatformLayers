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

u32 texture_get_pixel(texture_t *tex, vec2f_t dst_coord, vec2f_t dst_dim);

#endif
