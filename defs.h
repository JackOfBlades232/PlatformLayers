/* PlatformLayers/defs.h */
#ifndef DEFS_SENTRY
#define DEFS_SENTRY

#include <stdint.h>

typedef float     f32;
typedef double    f64;
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;

typedef int       bool;
#define true      1
#define false     0

// @TODO: make tweakable?
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// @TODO: huh?
#define BYTES_PER_PIXEL 4

#endif
