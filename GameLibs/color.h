/* PlatformLayers/GameLibs/color.h */
#ifndef COLOR_SENTRY
#define COLOR_SENTRY

#include "../defs.h"

// @TODO: try float colors

// @TODO: automate in build
#define USE_FAST_COLOR_FUNCTIONS 1

// @TODO: add more functions
#if defined(USE_FAST_COLOR_FUNCTIONS) && USE_FAST_COLOR_FUNCTIONS
  #define SCALE_COLOR(_in, _scale) scale_color_fast(_in, _scale)
#else
  #define SCALE_COLOR(_in, _scale) scale_color(_in, _scale)
#endif

inline u8 scale_color_channel(u8 in, u8 scale)
{
    return scale == 0 ? 0 : (u16)in * scale / 0xFF;
}

inline u8 scale_color_channel_fast(u8 in, u8 scale)
{
    // @SPEED @NOTE: this is faster, but not entirely correct: this is /256, but 255->255
    // @NOTE: i could do another incorrect version: ((u16)in * scale) >> 8 + 1 without branch.
    //        However, it is not a big gain in debug, and in release current is actually faster
    return scale == 0xFF ? in : ((u16)in * scale) >> 8;
}

inline u8 add_color_channels(u8 c1, u8 c2)
{
    return ((u16)c1 + c2) & 0xFF; 
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

inline u32 scale_color_fast(u32 in, u32 scale)
{
    // @TODO: as this the fast version, may make itself faster
    return color_from_channels(scale_color_channel_fast(in & 0xFF, scale & 0xFF),
                               scale_color_channel_fast((in >> 8) & 0xFF, (scale >> 8) & 0xFF),
                               scale_color_channel_fast((in >> 16) & 0xFF, (scale >> 16) & 0xFF),
                               scale_color_channel_fast(in >> 24, scale >> 24));
}

inline u32 add_colors(u32 c1, u32 c2)
{
    return color_from_channels(add_color_channels(c1 & 0xFF, c2 & 0xFF),
                               add_color_channels((c1 >> 8) & 0xFF, (c2 >> 8) & 0xFF),
                               add_color_channels((c1 >> 16) & 0xFF, (c2 >> 16) & 0xFF),
                               add_color_channels(c1 >> 24, c2 >> 24));
}

inline u8 extract_channel_bits(u32 pix_data, u32 cm, u32 csh)
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

#endif
