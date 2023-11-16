/* PlatformLayers/Windows/defs.h */
#ifndef DEFS_SENTRY
#define DEFS_SENTRY

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

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

#define M_PI 3.14159265359

// @TODO: move this stuff somewhere

// Limited to 4kb
inline void debug_printf(const char* format, ...)
{
    char buf[4096];
    char* p = buf;
    va_list args;
    int nchars;
    const int max_len = sizeof(buf) - 3; // \r\n\0

    va_start(args, format);
    nchars = _vsnprintf_s(buf, sizeof(buf), max_len, format, args);
    va_end(args);

    if (nchars > 0)
        p += nchars;

    *(p++) = '\r';
    *(p++) = '\n';
    *p = '\0';

    OutputDebugStringA(buf);
}

inline bool structs_are_equal(u8 *s1, u8 *s2, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        if (*s1 != *s2)
            return false;
        s1++;
        s2++;
    }
    
    return true;
}

#ifdef _DEBUG
  #define USE_ASSERTIONS
#endif

// @TODO: make assertions literally crash the program?
#ifdef USE_ASSERTIONS
  
  #define ASSERT(_e) if(!(_e)) { debug_printf("Assertion (" #_e ") failed at %s:%d\n", __FILE__, __LINE__); __debugbreak(); }
  #define ASSERTF(_e, _fmt, ...) if(!(_e)) { debug_printf("(%s:%d) " _fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); __debugbreak(); }
  
  #define __ASSERT_GLUE(_a, _b) _a ## _b
  #define _ASSERT_GLUE(_a, _b) __ASSERT_GLUE(_a, _b)
  #define STATIC_ASSERT(_e) enum { _ASSERT_GLUE(s_assert_fail_, __LINE__) = 1 / (int)(!!(_e)) }

#else

  #define ASSERT(_e)
  #define ASSERTF(_e, _fmt, ...)
  #define STATIC_ASSERT(_e)

#endif

#endif
