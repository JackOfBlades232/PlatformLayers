/* RecklessPillager/linux_main.c */
#include "defs.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#define XK_MISCELLANY
#define XK_LATIN
#include <X11/keysymdef.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>

// @TEST
#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720
static char app_name[] = "Reckless Pillager";

typedef struct offscreen_buffer_tag {
    u32 *bitmap_mem;
    u32 width;
    u32 height;
    u32 bytes_per_pixel;
    u32 pitch;
} offscreen_buffer_t;

enum input_key_mask_tag {
    VK_LEFT  = 1,
    VK_RIGHT = 1 << 1,
    VK_UP    = 1 << 2,
    VK_DOWN  = 1 << 3,
    VK_ESC   = 1 << 4
};

typedef struct input_state_tag {
    u32 pressed_key_flags;
    bool quit;
} input_state_t;

typedef struct x11_state_tag {
    Display *display;
    Window window;
    Visual *visual;
    GC gc;
    int screen;
    Pixmap pixmap;
    XImage *image;

    XClassHint *classhint;
    XWMHints *wmhints;
    XSizeHints *sizehints;
    Atom wm_delete_window;
} x11_state_t;

static offscreen_buffer_t backbuffer = { 0 };
static input_state_t input_state     = { 0 };

static x11_state_t x11_state         = { 0 };

// @TEST Graphics
#define OFFSET_PER_S 648

static f32 x_offset  = 0;
static f32 y_offset  = 0;
static f32 dx        = 0;
static f32 dy        = 0;

s32 s32_wrap(s32 val, s32 min, s32 max)
{
    u32 spread = max - min + 1;
    while (val < min)
        val += spread;
    while (val > max)
        val -= spread;
    return val;
}

void render_gradient(offscreen_buffer_t *buffer, f32 x_offset, f32 y_offset)
{
    u32 width = buffer->width;
    u32 height = buffer->height;
    u32 pitch = buffer->pitch;

    u8 *row = (u8 *)buffer->bitmap_mem;
    for (u32 y = 0; y < height; y++) {
        u32 *pix = (u32 *)row;
        for (u32 x = 0; x < width; x++) {
            u8 blue = x + x_offset;
            u8 green = y + y_offset;
            *(pix++) = (green << 8) | blue;
        }

        row += pitch;
    }
}

void update_gardient(input_state_t *input, f32 *x_offset, f32 *y_offset, f32 dt)
{
    dx = 0;
    dy = 0;
    if (input->pressed_key_flags & VK_UP)    dy -= OFFSET_PER_S;
    if (input->pressed_key_flags & VK_DOWN)  dy += OFFSET_PER_S;
    if (input->pressed_key_flags & VK_RIGHT) dx += OFFSET_PER_S;
    if (input->pressed_key_flags & VK_LEFT)  dx -= OFFSET_PER_S;

    *x_offset += dx*dt;
    *y_offset += dy*dt;
}

// @IDEA: look at Casey's style and introduce local variables for less text
void x11_init()
{
    // @TODO: factor out the 24-s and 32-s?
    x11_state.display = XOpenDisplay(getenv("DISPLAY"));
    ASSERT_ERR(x11_state.display);

    x11_state.screen = XDefaultScreen(x11_state.display);
    x11_state.visual = DefaultVisual(x11_state.display, x11_state.screen);
    x11_state.gc = DefaultGC(x11_state.display, x11_state.screen);
    x11_state.window = XCreateWindow(x11_state.display, 
                                     DefaultRootWindow(x11_state.display),
                                     0, 0, backbuffer.width, backbuffer.height,
                                     1, 24, InputOutput, CopyFromParent, 0, 0);
    ASSERT_ERR(x11_state.window);

    x11_state.classhint = XAllocClassHint();
    x11_state.classhint->res_name = app_name;
    x11_state.classhint->res_class = app_name;

    x11_state.wmhints = XAllocWMHints();
    x11_state.wmhints->input = true;
    x11_state.wmhints->flags = InputHint;

    x11_state.sizehints = XAllocSizeHints();
    x11_state.sizehints->flags = PMaxSize | PMinSize;
    x11_state.sizehints->min_width = backbuffer.width;
    x11_state.sizehints->max_width = backbuffer.width;
    x11_state.sizehints->min_height = backbuffer.height;
    x11_state.sizehints->max_height = backbuffer.height;

    XSetWMProperties(x11_state.display, x11_state.window, NULL, NULL, NULL, 0,
                     x11_state.sizehints, x11_state.wmhints, x11_state.classhint);

    x11_state.wm_delete_window = XInternAtom(x11_state.display, "WM_DELETE_WINDOW", false);
    XSetWMProtocols(x11_state.display, x11_state.window, &x11_state.wm_delete_window, 1);

    XSelectInput(x11_state.display, x11_state.window,
                 ExposureMask | KeyPressMask | KeyReleaseMask);
    XMapWindow(x11_state.display, x11_state.window);

    x11_state.pixmap = XCreatePixmap(x11_state.display, x11_state.window, 
                                     backbuffer.width, backbuffer.height, 24);
    ASSERT_ERR(x11_state.pixmap);

    x11_state.image = XCreateImage(x11_state.display, x11_state.visual, 
                                   24, ZPixmap, 0, (char *)backbuffer.bitmap_mem, 
                                   backbuffer.width, backbuffer.height, 32, 0);
    ASSERT_ERR(x11_state.image);

    XFlush(x11_state.display);
}

void x11_deinit()
{
    XFreePixmap(x11_state.display, x11_state.pixmap);
    XFree(x11_state.classhint);
    XFree(x11_state.wmhints);
    XFree(x11_state.sizehints);
    XCloseDisplay(x11_state.display);
}

u32 x11_key_mask(XKeyEvent *key_event)
{
    KeySym ksym = XLookupKeysym(key_event, 0);

    // @HUH: should I move the WASD=arrows logic to upper layers?
    if (ksym == XK_Escape)
        return VK_ESC;
    else if (ksym == 'w' || ksym == XK_Up)
        return VK_UP;
    else if (ksym == 's' || ksym == XK_Down)
        return VK_DOWN;
    else if (ksym == 'd' || ksym == XK_Right)
        return VK_RIGHT;
    else if (ksym == 'a' || ksym == XK_Left)
        return VK_LEFT;

    return 0;
}

void x11_poll_events()
{
    while (XPending(x11_state.display) > 0) {
        XEvent event = { 0 };
        XNextEvent(x11_state.display, &event);

        switch (event.type) {
            case ClientMessage:
                // @HUH: weird. Why is it in long data, not msg type?
                if ((Atom) event.xclient.data.l[0] == x11_state.wm_delete_window)
                    input_state.quit = true;
                break;

            case KeyPress: 
                input_state.pressed_key_flags |= x11_key_mask(&event.xkey);
                break;

            case KeyRelease: 
                input_state.pressed_key_flags &= ~x11_key_mask(&event.xkey);
                break;
        }
    }
}

void x11_draw_buffer()
{
    XPutImage(x11_state.display, x11_state.pixmap, 
              x11_state.gc, x11_state.image,
              0, 0, 0, 0, x11_state.image->width, x11_state.image->height);
    XCopyArea(x11_state.display, x11_state.pixmap, 
              x11_state.window, x11_state.gc,
              0, 0, backbuffer.width, backbuffer.height, 0, 0);
    XFlush(x11_state.display);
}

u64 get_nsec()
{
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000 + (u64)ts.tv_nsec;
}

int main(int argc, char **argv)
{
    backbuffer.width = SCREEN_WIDTH;
    backbuffer.height = SCREEN_HEIGHT;
    backbuffer.bytes_per_pixel = 4;
    backbuffer.pitch = backbuffer.width * backbuffer.bytes_per_pixel;

    u32 bufsize = backbuffer.height * backbuffer.pitch;
    u32 pgs = getpagesize();
    u32 pgbufsize = ((bufsize-1) / pgs + 1) * pgs;
    backbuffer.bitmap_mem = mmap(NULL, pgbufsize, PROT_READ|PROT_WRITE,
                                 MAP_PRIVATE|MAP_ANON, -1, 0);

    x11_init();

    u64 prev_time = get_nsec();

    for (;;) {
        sched_yield();
        
        x11_poll_events();

        u64 cur_time = get_nsec();
        if (cur_time == prev_time)
            continue;

        f32 dt = (f32)((f64)(cur_time - prev_time) * 1e-9);
        // @TODO: why should I make dt = const if dt > const?

        // @TEST
        printf("%6.5fs per frame\n", dt);
        update_gardient(&input_state, &x_offset, &y_offset, dt);

        prev_time = cur_time;

        if (input_state.quit)
            break;

        // @TEST
        render_gradient(&backbuffer, x_offset, y_offset);

        x11_draw_buffer();
    }

    x11_deinit();
    munmap(backbuffer.bitmap_mem, pgbufsize);
    return 0;
}
