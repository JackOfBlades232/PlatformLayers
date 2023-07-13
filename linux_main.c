/* RecklessPillager/linux_main.c */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

#include <X11/Xlib.h>

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

typedef int       bool;
#define true      1
#define false     0

#define XTrue     True
#define XFalse    False

// @TODO: handle errors
// @TODO: factor rendering init, event loop and tests apart
// @TODO: find a way to resize properly (or fix resolution)

#define GAME_VIEW_WIDTH   1920
#define GAME_VIEW_HEIGHT  1080

// The main game buffer to blit to the screen
static u32 back_buffer[GAME_VIEW_WIDTH*GAME_VIEW_HEIGHT] = {0};
static f32 delta_time = 0.;

// @TEST
#define OFFSET_PER_S 432
static s32 x_offset = 0;
static s32 y_offset = 0;
static s32 dx = 0;
static s32 dy = 0;

s32 s32_wrap(s32 val, s32 min, s32 max)
{
    u32 spread = max - min + 1;
    while (val < min)
        val += spread;
    while (val > max)
        val -= spread;
    return val;
}

void fill_back_buffer()
{
    const u32 square_size = 432;

    for (u32 y = 0; y < GAME_VIEW_HEIGHT; y++)
        for (u32 x = 0; x < GAME_VIEW_WIDTH; x++) {
            u32 *pix = back_buffer + y*GAME_VIEW_WIDTH + x;

            /*
             *pix = ((u32) (256.0*rand() / (RAND_MAX+1.0)) << 16) + 
                    ((u32) (256.0*rand() / (RAND_MAX+1.0)) << 8) +
                    ((u32) (256.0*rand() / (RAND_MAX+1.0)));
             */

            u32 x_mod = s32_wrap(x + x_offset, 0, GAME_VIEW_WIDTH-1);
            u32 y_mod = s32_wrap(y + y_offset, 0, GAME_VIEW_HEIGHT-1);
            u32 manh_from_corner = (x_mod + y_mod) % square_size;
            *pix = ((u32) (256.0*manh_from_corner) / square_size << 8) +
                   ((u32) (256.0*(square_size-manh_from_corner-1) / square_size));

            // @TODO: Implement gradient squares correctly
        }
}

int main(int argc, char **argv)
{
    Display *display = XOpenDisplay(NULL);
    ASSERT(display, "XOpenDisplay failed\n");

    // @HUH What is DefaultScreen?
    // @HUH Can I use precise byte types here?
    int black_color = BlackPixel(display, DefaultScreen(display));
    int white_color = WhitePixel(display, DefaultScreen(display));

    Window w = XCreateSimpleWindow(display, DefaultRootWindow(display),
                                   0, 0, 1920, 1080,
                                   0, black_color, black_color);

    XWindowAttributes wa = {0};
    XGetWindowAttributes(display, w, &wa);

    XImage *image = XCreateImage(display, wa.visual, wa.depth, ZPixmap, 0,
                                 (XPointer) back_buffer, 
                                 GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT,
                                 32, GAME_VIEW_WIDTH * sizeof(*back_buffer));

    GC gc = XCreateGC(display, w, 0, NULL);
    XSetForeground(display, gc, white_color);

    XSelectInput(display, w, KeyPressMask);

    // Intercept window manager window close event (since it is on the
    // level of the window manager, not the X system))
    Atom wm_delete_w = XInternAtom(display, "WM_DELETE_WINDOW", XFalse);
    XSetWMProtocols(display, w, &wm_delete_w, 1);

    XMapWindow(display, w);

    srand(time(NULL));
    fill_back_buffer();

    XPutImage(display, w, gc, image,
              0, 0, 0, 0, GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT);
    XSync(display, XFalse);

    for (;;) {
        clock_t fstart = clock();

        while (XPending(display) > 0) {
            XEvent event = {0};
            XNextEvent(display, &event);

            switch (event.type) {
                case ClientMessage:
                    {
                        // @HUH: weird. Why is it in long data, not msg type?
                        if ((Atom) event.xclient.data.l[0] == wm_delete_w) {
                            printf("Shutting down\n");
                            goto defer;
                        }
                    }
                    break;

                case KeyPress: 
                    {
                        switch (event.xkey.keycode) {
                            case 111:
                                dy = -OFFSET_PER_S;
                                dx = 0;
                                break;
                            case 116:
                                dy = OFFSET_PER_S;
                                dx = 0;
                                    break;
                            case 114:
                                dy = 0;
                                dx = OFFSET_PER_S;
                                break;
                            case 113:
                                dy = 0;
                                dx = -OFFSET_PER_S;
                                break;
                            default:
                                printf("Pressed key %d\n", event.xkey.keycode);
                                break;
                        }
                    }
                    break;
            }
        }

        x_offset += dx * delta_time;
        y_offset += dy * delta_time;
        x_offset = s32_wrap(x_offset, 0, GAME_VIEW_WIDTH-1);
        y_offset = s32_wrap(y_offset, 0, GAME_VIEW_HEIGHT-1);

        fill_back_buffer();
        XPutImage(display, w, gc, image,
                0, 0, 0, 0, GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT);
        XSync(display, XFalse);

        delta_time = ((float) clock() - fstart) / CLOCKS_PER_SEC;
        printf("%6.3f ms per frame\n", delta_time * 1000);
    }

defer:
    XCloseDisplay(display);
    return 0;
}
