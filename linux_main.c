/* RecklessPillager/linux_main.c */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>
// @TODO: add multiple kb layout support

#define LOG_ERR(_fmt, ...) fprintf(stderr, "[ERR] (%s:%d: errno: %s) " _fmt "\n", \
        __FILE__, __LINE__, (errno == 0 ? "None" : strerror(errno)), ##__VA_ARGS__)

#define ASSERT(_e) if(!(_e)) { fprintf(stderr, "Assertion failed at %s:%d\n", __FILE__, __LINE__); exit(1); }
#define ASSERTF(_e, _fmt, ...) if(!(_e)) { fprintf(stderr, _fmt, ##__VA_ARGS__); exit(1); }
#define ASSERT_ERR(_e) if(!(_e)) { LOG_ERR("Assertion failed at %s:%d\n", __FILE__, __LINE__); exit(1); }
#define ASSERTF_ERR(_e, _fmt, ...) if(!(_e)) { LOG_ERR(_fmt, ##__VA_ARGS__); exit(1); }
// @TODO: my own static assert

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

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
#define XStatus   Status

typedef struct offscreen_buffer_tag {
    u32 *bitmap_mem;
    u32 width;
    u32 height;
    u32 bytes_per_pixel;
    u32 pitch;
    // @TODO: use visual's RGB mask values to populate pixel
} offscreen_buffer_t;

enum input_key_mask_tag {
    LEFT_KEY      = 1,
    RIGHT_KEY     = 1 << 1,
    UP_KEY        = 1 << 2,
    DOWN_KEY      = 1 << 3,
    ESC_KEY       = 1 << 4
};

typedef struct game_input_state_tag {
    u32 pressed_key_flags;
    bool quit;
} game_input_state_t;

typedef struct x11_state_tag {
    Display *display;
    Window w;
    GC gc;
    XImage *image;

    Atom wm_delete_window;
} x11_state_t;

// @TODO: find a way to resize properly (or fix resolution)
// @TODO: find a way to check if x11 wants != 32 bits / pixel, and error thus
// @TODO: Fix screen tearing when moving horizontally (double buffer?)

// @TEST
#define GAME_VIEW_WIDTH   1920
#define GAME_VIEW_HEIGHT  1080

// Global state
static offscreen_buffer_t global_backbuffer  = {0};
static game_input_state_t global_input_state = {0};
static f32 delta_time                        = 0.;

static x11_state_t x11_state                 = {0};

// @TEST Graphics
#define OFFSET_PER_S 648

static s32 x_offset  = 0;
static s32 y_offset  = 0;
static s32 dx        = 0;
static s32 dy        = 0;

s32 s32_wrap(s32 val, s32 min, s32 max)
{
    u32 spread = max - min + 1;
    while (val < min)
        val += spread;
    while (val > max)
        val -= spread;
    return val;
}

// @TODO: make offscreen buffer a passable param
void render_gradient(offscreen_buffer_t *buffer, s32 x_offset, s32 y_offset)
{
    u32 width = buffer->width;
    u32 height = buffer->height;
    u32 pitch = buffer->pitch;

    u8 *row = (u8 *) buffer->bitmap_mem;
    for (u32 y = 0; y < height; y++) {
        u32 *pix = (u32 *) row;
        for (u32 x = 0; x < width; x++) {
            u8 blue = x + x_offset;
            u8 green = y + y_offset;
            *(pix++) = (green << 8) | blue;
        }

        row += pitch;
    }
}

void update_gardient(offscreen_buffer_t *buffer, 
                     game_input_state_t *input,
                     s32 *x_offset, s32 *y_offset)
{
    dx = 0;
    dy = 0;
    if (input->pressed_key_flags & UP_KEY) dy -= OFFSET_PER_S;
    if (input->pressed_key_flags & DOWN_KEY) dy += OFFSET_PER_S;
    if (input->pressed_key_flags & RIGHT_KEY) dx += OFFSET_PER_S;
    if (input->pressed_key_flags & LEFT_KEY) dx -= OFFSET_PER_S;

    // @BUG: on -O3 this seems to be optimized out to 0
    s32 sdx = dx * delta_time;
    s32 sdy = dy * delta_time;
    *x_offset += sdx;
    *y_offset += sdy;

    render_gradient(buffer, *x_offset, *y_offset);
}

// @TEST END

void x11_init()
{
    x11_state.display = XOpenDisplay(NULL);
    ASSERT_ERR(x11_state.display);

    int black_color = BlackPixel(x11_state.display, DefaultScreen(x11_state.display));
    x11_state.w = XCreateSimpleWindow(x11_state.display, DefaultRootWindow(x11_state.display),
                                    0, 0, GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT,
                                    0, black_color, black_color);
    ASSERT_ERR(x11_state.w);

    XWindowAttributes wa = {0};
    XGetWindowAttributes(x11_state.display, x11_state.w, &wa);

    x11_state.image = XCreateImage(x11_state.display, wa.visual, wa.depth, ZPixmap, 0,
                                 (XPointer) global_backbuffer.bitmap_mem, 
                                 global_backbuffer.width, global_backbuffer.height,
                                 32, global_backbuffer.width * sizeof(*global_backbuffer.bitmap_mem));
    ASSERT_ERR(x11_state.image);

    x11_state.gc = XCreateGC(x11_state.display, x11_state.w, 0, NULL);
    ASSERT_ERR(x11_state.gc);

    XSelectInput(x11_state.display, x11_state.w, KeyPressMask | KeyReleaseMask);

    // Intercept window manager window close event (since it is on the
    // level of the window manager, not the X system))
    x11_state.wm_delete_window = XInternAtom(x11_state.display, "WM_DELETE_WINDOW", XFalse);
    XStatus sr = XSetWMProtocols(x11_state.display, x11_state.w, &x11_state.wm_delete_window, 1);
    ASSERT_ERR(sr);

    XMapWindow(x11_state.display, x11_state.w);
}

void x11_deinit()
{
    XCloseDisplay(x11_state.display);
}

void x11_redraw_buffer()
{
    XPutImage(x11_state.display, x11_state.w, x11_state.gc, x11_state.image,
              0, 0, 0, 0, GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT);
    XSync(x11_state.display, XFalse);
}

u32 x11_key_mask(XKeyEvent *key_event)
{
    KeySym ksym = XLookupKeysym(key_event, 0);

    if (ksym == XK_Escape)
        return ESC_KEY;
    else if (ksym == 'w' || ksym == XK_Up)
        return UP_KEY;
    else if (ksym == 's' || ksym == XK_Down)
        return DOWN_KEY;
    else if (ksym == 'd' || ksym == XK_Right)
        return RIGHT_KEY;
    else if (ksym == 'a' || ksym == XK_Left)
        return LEFT_KEY;

    return 0;
}

int x11_key_was_actually_released(XEvent *event)
{
    ASSERTF(event->type == KeyRelease,
            "x11_key_was_actually_released was called on a non-release event\n");

    if (XPending(x11_state.display) == 0)
        return 1;

    XEvent next_ev;
    XPeekEvent(x11_state.display, &next_ev);
    if ( 
            next_ev.type == KeyPress && 
            next_ev.xkey.time == event->xkey.time &&
            next_ev.xkey.keycode == event->xkey.keycode
       ) {
        XNextEvent(x11_state.display, &next_ev);
        return 0;
    }

    return 1;
}

void x11_poll_events()
{
    while (XPending(x11_state.display) > 0) {
        XEvent event = {0};
        XNextEvent(x11_state.display, &event);

        switch (event.type) {
            case ClientMessage:
                {
                    // @HUH: weird. Why is it in long data, not msg type?
                    if ((Atom) event.xclient.data.l[0] == x11_state.wm_delete_window)
                        global_input_state.quit = true;
                }
                break;

            case KeyPress: 
                {
                    global_input_state.pressed_key_flags |= 
                        x11_key_mask(&event.xkey);
                }
                break;

            case KeyRelease: 
                {
                    if (x11_key_was_actually_released(&event)) {
                        global_input_state.pressed_key_flags &= 
                            ~x11_key_mask(&event.xkey);
                    }
                }
                break;
        }
    }
}

int main(int argc, char **argv)
{
    // @TEST
    // @TODO: init struct according to visual and window dims
    // @TODO: Move to another func
    global_backbuffer.width = GAME_VIEW_WIDTH;
    global_backbuffer.height = GAME_VIEW_HEIGHT;
    global_backbuffer.bytes_per_pixel = 4;
    global_backbuffer.pitch = global_backbuffer.width * global_backbuffer.bytes_per_pixel;

    u32 bufsize = global_backbuffer.height * global_backbuffer.pitch;
    u32 pgs = getpagesize();
    u32 pgbufsize = ((bufsize-1) / pgs + 1) * pgs;
    global_backbuffer.bitmap_mem = mmap(NULL, pgbufsize, PROT_READ|PROT_WRITE,
                                        MAP_PRIVATE|MAP_ANON, -1, 0);
    ASSERT_ERR(global_backbuffer.bitmap_mem);

    x11_init();

    render_gradient(&global_backbuffer, x_offset, y_offset); // @TEST
    x11_redraw_buffer();

    for (;;) {
        clock_t fstart = clock();

        x11_poll_events();
        if (global_input_state.pressed_key_flags & ESC_KEY)
            global_input_state.quit = true;

        if (global_input_state.quit) {
            printf("Shutting down\n");
            break;
        }

        update_gardient(&global_backbuffer, &global_input_state,
                        &x_offset, &y_offset); // @TEST
        x11_redraw_buffer();

        delta_time = ((f32) clock() - fstart) / CLOCKS_PER_SEC;
        printf("%6.3f ms per frame\n", delta_time * 1000);
    }

    x11_deinit();
    // @TODO: Move to another func
    munmap(global_backbuffer.bitmap_mem, pgbufsize);
    return 0;
}
