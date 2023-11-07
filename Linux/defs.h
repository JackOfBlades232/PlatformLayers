/* RecklessPillager/Linux/defs.h */
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

#if defined(USE_ASSERTIONS) && USE_ASSERTIONS == 1

  #include <stdio.h>
  #include <errno.h>

  #define _LOG_STRERR_GEN(_func, _errno, _fmt, ...) fprintf(stderr, "[ERR] (%s:%d: errno: %s) " _fmt "\n", \
          __FILE__, __LINE__, (errno == 0 ? "None" : _func(_errno)), ##__VA_ARGS__)
  #define _LOG_STRERR(_func, _fmt, ...) _LOG_STRERR_GEN(_func, errno, _fmt, ##__VA_ARGS__)
  #define _LOG_ERR(_fmt, ...) _LOG_STRERR(strerror, _fmt, ##__VA_ARGS__)
  
  #define ASSERT(_e) if(!(_e)) { fprintf(stderr, "Assertion (" #_e ") failed at %s:%d\n", __FILE__, __LINE__); exit(1); }
  #define ASSERTF(_e, _fmt, ...) if(!(_e)) { fprintf(stderr, "(%s:%d) " _fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); exit(1); }
  #define ASSERT_ERR(_e) if(!(_e)) { _LOG_ERR("Assertion (" #_e ") failed at %s:%d\n", __FILE__, __LINE__); exit(1); }
  #define ASSERTF_ERR(_e, _fmt, ...) if(!(_e)) { _LOG_ERR(_fmt, ##__VA_ARGS__); exit(1); }
  #define ASSERT_LIBERR(_e, _libfunc) if(!(_e)) { _LOG_STRERR(_libfunc, "Assertion (" #_e ") failed at %s:%d", __FILE__, __LINE__); exit(1); }
  #define ASSERTF_LIBERR(_e, _libfunc, _fmt, ...) if(!(_e)) { _LOG_STRERR(_libfunc, _fmt, ##__VA_ARGS__); exit(1); }
  #define ASSERT_LIBERR_GEN(_e, _libfunc, _errno) if(!(_e)) { _LOG_STRERR_GEN(_libfunc, _errno, "Assertion (" #_e ") failed at %s:%d", __FILE__, __LINE__); exit(1); }
  #define ASSERTF_LIBERR_GEN(_e, _libfunc, _errno, _fmt, ...) if(!(_e)) { _LOG_STRERR_GEN(_libfunc, _errno, _fmt, ##__VA_ARGS__); exit(1); }
  
  #define __ASSERT_GLUE(_a, _b) _a ## _b
  #define _ASSERT_GLUE(_a, _b) __ASSERT_GLUE(_a, _b)
  #define STATIC_ASSERT(_e) enum { _ASSERT_GLUE(s_assert_fail_, __LINE__) = 1 / (int)(!!(_e)) }

#else

  #define ASSERT(_e)
  #define ASSERTF(_e, _fmt, ...)
  #define ASSERT_ERR(_e)
  #define ASSERTF_ERR(_e, _fmt, ...)
  #define ASSERT_LIBERR(_e, _libfunc)
  #define ASSERTF_LIBERR(_e, _libfunc, _fmt, ...)
  #define ASSERT_LIBERR_GEN(_e, _libfunc, _error)
  #define ASSERTF_LIBERR_GEN(_e, _libfunc, _error, _fmt, ...)
  #define STATIC_ASSERT(_e)

#endif
