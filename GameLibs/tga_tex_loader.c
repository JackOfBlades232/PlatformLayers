/* PlatformLayers/GameLibs/tga_tex_loader.c */
#include "tga_tex_loader.h"
#include "file_io.h"
#include "color.h"

#include "../os.h"

// @TODO: add run-length encoded versions

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
STATIC_ASSERT(sizeof(tga_texture_header_t) == 18);

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

// @TODO: add logging
// @TODO: handle endianness. TGA is always little endian, so it may not work
// on other architectures.
void tga_load_texture(const char *path, texture_t *out_tex)
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
    // @HACK Will be orred into the pixel, a little hacky
    u32 alpha_override = 0x0;
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
            alpha_override = 0xFF;
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
    texture_alloc_mem(out_tex);

    // @TODO: test weird tga formats

    for (u32 y = 0; y < out_tex->height; y++)
        for (u32 x = 0; x < out_tex->width; x++) {
            u32 pix_data = 0;
            if (!consume_file_chunk(&crawler, &pix_data, in_bytes_per_pixel))
                goto cleanup;
            // @HACK: if read less bytes, shift them to less significant @TODO: allow big endian arch
            if (in_bytes_per_pixel < 4)
                pix_data >>= 8 * (4 - in_bytes_per_pixel);

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
                                               extract_channel_bits(pix_data, cmask.am, cmask.ash) | alpha_override);
                ((u32 *)out_tex->mem)[dest_offset] = abgr;

                if (abgr >> 24 != 0xFF)
                    out_tex->has_transparency = true;
            }
        }

    out_tex->loaded = true;

cleanup:
    if (!out_tex->loaded && out_tex->mem)
        texture_free_mem(out_tex);
    if (file.mem)
        os_unmap_file(&file);
}
