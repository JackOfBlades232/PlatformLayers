/* RecklessPillager/linux_main.c */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <X11/Xlib.h>
#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>
// @TODO: add multiple kb layout support

#define LOG_ERR(_fmt, ...) fprintf(stderr, "[ERR] (%s:%d: errno: %s) " _fmt "\n", \
        __FILE__, __LINE__, (errno == 0 ? "None" : strerror(errno)), ##__VA_ARGS__)

#define ASSERT(_e) if(!(_e)) { fprintf(stderr, "Assertion failed at %s:%d\n", __FILE__, __LINE__); exit(1); }
#define ASSERTF(_e, _fmt, ...) if(!(_e)) { fprintf(stderr, _fmt, __VA_ARGS__); exit(1); }
#define ASSERT_ERR(_e) if(!(_e)) { LOG_ERR("Assertion failed at %s:%d\n", __FILE__, __LINE__); exit(1); }
#define ASSERTF_ERR(_e, _fmt, ...) if(!(_e)) { LOG_ERR(_fmt, ##__VA_ARGS__); exit(1); }
// @TODO: my own static assert

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

typedef struct XState_tag {
    Display *display;
    Window w;
    GC gc;
    XImage *image;

    Atom wm_delete_window;
} XState;

// @TODO: find a way to resize properly (or fix resolution)
// @TODO: find a way to check if x11 wants != 32 bits / pixel, and error thus
// @TODO: use visual's RGB mask values to populate pixel
// @TODO: Fix screen tearing when moving horizontally (double buffer?)

#define GAME_VIEW_WIDTH   1920
#define GAME_VIEW_HEIGHT  1080

// Global state
static XState x_state = {0};

// @TODO: make offscreen buffer a struct and allocate memory dynamically
static u32 back_buffer[GAME_VIEW_WIDTH*GAME_VIEW_HEIGHT];

static game_input_state_t input_state  = {0};
static f32 delta_time                  = 0.;

// @TEST Graphics
#define OFFSET_PER_S 432

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
void fill_back_buffer()
{
    const u32 square_size = 120;

    for (u32 y = 0; y < GAME_VIEW_HEIGHT; y++)
        for (u32 x = 0; x < GAME_VIEW_WIDTH; x++) {
            u32 *pix = back_buffer + y*GAME_VIEW_WIDTH + x;
            u32 x_mod = s32_wrap(x + x_offset, 0, GAME_VIEW_WIDTH-1) % square_size;
            u32 y_mod = s32_wrap(y + y_offset, 0, GAME_VIEW_HEIGHT-1) % square_size;
            *pix = ((u32) 256.0*x_mod / square_size << 8) +
                   ((u32) 256.0*y_mod / square_size);
        }
}

void update_back_buffer()
{
    dx = 0;
    dy = 0;
    if (input_state.pressed_key_flags & UP_KEY) dy -= OFFSET_PER_S;
    if (input_state.pressed_key_flags & DOWN_KEY) dy += OFFSET_PER_S;
    if (input_state.pressed_key_flags & RIGHT_KEY) dx += OFFSET_PER_S;
    if (input_state.pressed_key_flags & LEFT_KEY) dx -= OFFSET_PER_S;

    x_offset += dx * delta_time;
    y_offset += dy * delta_time;
    fill_back_buffer();
}

// @TEST END

void XInit()
{
    x_state.display = XOpenDisplay(NULL);
    ASSERT_ERR(x_state.display);

    int black_color = BlackPixel(x_state.display, DefaultScreen(x_state.display));
    x_state.w = XCreateSimpleWindow(x_state.display, DefaultRootWindow(x_state.display),
                                    0, 0, GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT,
                                    0, black_color, black_color);
    ASSERT_ERR(x_state.w);

    XWindowAttributes wa = {0};
    XGetWindowAttributes(x_state.display, x_state.w, &wa);

    x_state.image = XCreateImage(x_state.display, wa.visual, wa.depth, ZPixmap, 0,
                                 (XPointer) back_buffer, 
                                 GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT,
                                 32, GAME_VIEW_WIDTH * sizeof(*back_buffer));
    ASSERT_ERR(x_state.image);

    x_state.gc = XCreateGC(x_state.display, x_state.w, 0, NULL);
    ASSERT_ERR(x_state.gc);

    XSelectInput(x_state.display, x_state.w, KeyPressMask | KeyReleaseMask);

    // Intercept window manager window close event (since it is on the
    // level of the window manager, not the X system))
    x_state.wm_delete_window = XInternAtom(x_state.display, "WM_DELETE_WINDOW", XFalse);
    XStatus sr = XSetWMProtocols(x_state.display, x_state.w, &x_state.wm_delete_window, 1);
    ASSERT_ERR(sr);

    XMapWindow(x_state.display, x_state.w);
}

void XDeinit()
{
    XCloseDisplay(x_state.display);
}

void XRedrawBuffer()
{
    XPutImage(x_state.display, x_state.w, x_state.gc, x_state.image,
              0, 0, 0, 0, GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT);
    XSync(x_state.display, XFalse);
}

u32 XPressedKeyMask(XKeyEvent *key_event)
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

int XKeyWasTrulyReleased(XEvent *event)
{
    // @SPEED: should I remove this?
    if (event->type != KeyRelease)
        return 0;

    if (XPending(x_state.display) == 0)
        return 1;

    // @SPEED: maybe I should remove the keypress too (if it is the repeated one)
    XEvent next_ev;
    XPeekEvent(x_state.display, &next_ev);
    return !(
            next_ev.type == KeyPress && 
            next_ev.xkey.time == event->xkey.time &&
            next_ev.xkey.keycode == event->xkey.keycode
            );
}

void XPollEvents()
{
    while (XPending(x_state.display) > 0) {
        XEvent event = {0};
        XNextEvent(x_state.display, &event);

        switch (event.type) {
            case ClientMessage:
                {
                    // @HUH: weird. Why is it in long data, not msg type?
                    if ((Atom) event.xclient.data.l[0] == x_state.wm_delete_window)
                        input_state.quit = true;
                }
                break;

            case KeyPress: 
                {
                    input_state.pressed_key_flags |= 
                        XPressedKeyMask(&event.xkey);
                }
                break;

            case KeyRelease: 
                {
                    if (XKeyWasTrulyReleased(&event)) {
                        input_state.pressed_key_flags &= 
                            ~XPressedKeyMask(&event.xkey);
                    }
                }
                break;
        }

        if (input_state.pressed_key_flags & ESC_KEY)
            input_state.quit = true;
    }
}

int main(int argc, char **argv)
{
    XInit();

    fill_back_buffer(); // @TEST
    XRedrawBuffer();

    for (;;) {
        clock_t fstart = clock();

        XPollEvents();
        if (input_state.quit) {
            printf("Shutting down\n");
            break;
        }

        update_back_buffer(); // @TEST
        XRedrawBuffer();

        delta_time = ((f32) clock() - fstart) / CLOCKS_PER_SEC;
        printf("%6.3f ms per frame\n", delta_time * 1000);
    }

    XDeinit();
    return 0;
}
