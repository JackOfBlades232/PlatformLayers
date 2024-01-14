/* PlatformLayers/GameLibs/utils.h */
#ifndef UTILS_SENTRY
#define UTILS_SENTRY

#define INCLUDE_TYPE(_type, _name) union { _type; _type _name; };

#define MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))
#define MIN(_a, _b) ((_a) < (_b) ? (_a) : (_b))
#define CLAMP(_x, _min, _max) MIN(_max, MAX(_min, _x))

#define ABS(_a) ((_a) < 0 ? -(_a) : (_a))
#define SGN(_a) ((_a) < 0 ? -1 : ((_a) > 0 ? 1 : 0))

#endif
