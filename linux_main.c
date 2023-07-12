/* RecklessPillager/linux_main.c */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#define ASSERT(_e, ...) if (!(_e)) { fprintf(stderr, __VA_ARGS__); exit(1); }

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

int main(int argc, char **argv)
{
    // Connect to the X Server, get connection handle
    Display *dpy = XOpenDisplay(NULL);
    ASSERT(dpy, "XOpenDisplay failed\n");

    // Get the integer format of the color for this display/screen
    // @HUH What is DefaultScreen?
    // @HUH Can I use precise byte types here?
    int black_color = BlackPixel(dpy, DefaultScreen(dpy));
    int white_color = WhitePixel(dpy, DefaultScreen(dpy));

    // Create window and get handle, with given root window (@HUH?)
    // 0-0 as top-left corner pos, 200-100 as size, 0 as native border size
    // black-black as border/bg color
    Window w = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
                                   200, 100, 0, black_color, black_color);

    // @TODO: finish simple X window
}
