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
// @TODO: add multiple kb layout support
#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>

#include <pulse/pulseaudio.h>

#define LOG_STRERR_GEN(_func, _errno, _fmt, ...) fprintf(stderr, "[ERR] (%s:%d: errno: %s) " _fmt "\n", \
        __FILE__, __LINE__, (errno == 0 ? "None" : _func(_errno)), ##__VA_ARGS__)
#define LOG_STRERR(_func, _fmt, ...) LOG_STRERR_GEN(_func, errno, _fmt, ##__VA_ARGS__)
#define LOG_ERR(_fmt, ...) LOG_STRERR(strerror, _fmt, ##__VA_ARGS__)

#define ASSERT(_e) if(!(_e)) { fprintf(stderr, "Assertion (" #_e ") failed at %s:%d\n", __FILE__, __LINE__); exit(1); }
#define ASSERTF(_e, _fmt, ...) if(!(_e)) { fprintf(stderr, _fmt, ##__VA_ARGS__); exit(1); }
#define ASSERT_ERR(_e) if(!(_e)) { LOG_ERR("Assertion (" #_e ") failed at %s:%d\n", __FILE__, __LINE__); exit(1); }
#define ASSERTF_ERR(_e, _fmt, ...) if(!(_e)) { LOG_ERR(_fmt, ##__VA_ARGS__); exit(1); }
#define ASSERT_LIBERR(_e, _libfunc) if(!(_e)) { LOG_STRERR(_libfunc, "Assertion (" #_e ") failed at %s:%d", __FILE__, __LINE__); exit(1); }
#define ASSERTF_LIBERR(_e, _libfunc, _fmt, ...) if(!(_e)) { LOG_STRERR(_libfunc, _fmt, ##__VA_ARGS__); exit(1); }
#define ASSERT_LIBERR_GEN(_e, _libfunc, _error) if(!(_e)) { LOG_STRERR_GEN(_libfunc, _error "Assertion (" #_e ") failed at %s:%d", __FILE__, __LINE__); exit(1); }
#define ASSERTF_LIBERR_GEN(_e, _libfunc, _error, _fmt, ...) if(!(_e)) { LOG_STRERR_GEN(_libfunc, _error, _fmt, ##__VA_ARGS__); exit(1); }
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

// @TODO: add logging at required places + todos everywhere it is required
// @TODO: find a way to resize properly (or fix resolution)
// @TODO: find a way to check if x11 wants != 32 bits / pixel, and error thus
// @TODO: Fix screen tearing when moving horizontally (double buffer?)

// @HUH: I might be using asserts too much (in their basic form
//      maybe just log somewhere?

static const char app_name[] = "RecklessPillager";

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

// @TEST
static pa_threaded_mainloop *pulse_mloop;
static pa_mainloop_api *pulse_mlapi;
static pa_context *pulse_ctx;
static pa_stream *pulse_stream;

// @TODO: aggregate sound constants

#define BUFFER_LEN_MS      500

// @TEST @TEST
//static char pulse_device_id[256];
#define SQUARE_WAVE_PERIOD 48000/128
#define SQUARE_WAVE_VOLUME 16000 

// @TEST @TEST
void pulse_fill_sound_buffer()
{
    static u32 square_wave_counter = 0;

    pa_threaded_mainloop_lock(pulse_mloop);
    {
        size_t n;
        if ((n = pa_stream_writable_size(pulse_stream)) == 0)
            pa_threaded_mainloop_wait(pulse_mloop);
        // @TODO: try and set another callback? now just catch SIGABRT
        // @TODO: learn what is draining
        // @TODO: not ok to do this on every frame

        void *buf;
        pa_stream_begin_write(pulse_stream, &buf, &n);
        // @TODO: handle errors

        s16 *sample_out = buf;
        for (size_t i = 0; i < n/2; i++) { // @TODO: magic number BytesPerSample
            if (square_wave_counter == 0)
                square_wave_counter = SQUARE_WAVE_PERIOD;

            s16 sample_val = square_wave_counter > SQUARE_WAVE_PERIOD/2 ? SQUARE_WAVE_VOLUME : -SQUARE_WAVE_VOLUME;
            *(sample_out++) = sample_val;
            *(sample_out++) = sample_val;
        }

        pa_stream_write(pulse_stream, buf, n, NULL, 0, PA_SEEK_RELATIVE);
        // @TODO: handle errors
    }
    pa_threaded_mainloop_unlock(pulse_mloop);
}

void pulse_on_state_change(pa_context *c, void *udata)
{
    pa_threaded_mainloop_signal(pulse_mloop, 0);
}

/*
void pulse_on_dev_sink(pa_context *c, const pa_sink_info *info, int eol, void *udata)
{
    if (eol != 0) {
        pa_threaded_mainloop_signal(pulse_mloop, 0);
        return;
    }

    // @TEST @TEST
    strncpy(pulse_device_id, info->name, sizeof(pulse_device_id-1));
}
*/

void pulse_on_io_complete(pa_stream *s, size_t nbytes, void *udata)
{
    pa_threaded_mainloop_signal(pulse_mloop, 0);
}

void pulse_init()
{ 
    u32 cd;

    // @TODO: remake asserts to pulse's errno
    pulse_mloop = pa_threaded_mainloop_new();
    ASSERT(pulse_mloop);

	cd = pa_threaded_mainloop_start(pulse_mloop);
    ASSERT(cd == 0);

    pa_threaded_mainloop_lock(pulse_mloop);
    {
        // Context connection
        pulse_mlapi = pa_threaded_mainloop_get_api(pulse_mloop);
        ASSERT(pulse_mlapi);

        pulse_ctx = pa_context_new_with_proplist(pulse_mlapi, app_name, NULL);
        ASSERT(pulse_ctx);

        void *udata = NULL;
        pa_context_set_state_callback(pulse_ctx, pulse_on_state_change, udata);

        cd = pa_context_connect(pulse_ctx, NULL, 0, NULL);
        ASSERT(cd == 0);

        // @HUH: how exactly do the context, main loop thread and server interact?
        while (pa_context_get_state(pulse_ctx) != PA_CONTEXT_READY)
            pa_threaded_mainloop_wait(pulse_mloop);

        // Enumerating devices (for playback)
        /*
        pa_operation *op;
        op = pa_context_get_sink_info_list(pulse_ctx, pulse_on_dev_sink, udata);

        // @HUH: Why the hell do I even need to enumerate devices?
        u32 r;
        for (;;) {
            r = pa_operation_get_state(op);
            if (r == PA_OPERATION_DONE || r == PA_OPERATION_CANCELLED)
                break;
            pa_threaded_mainloop_wait(pulse_mloop);
        }
        pa_operation_unref(op);
        ASSERT(r == PA_OPERATION_DONE);

        // @TEST @TEST
        printf("=======[ Output Device ]=======\n");
        printf("ID: %s\n", pulse_device_id);
        printf("\n");
        */


        // Opening stream&buffer
        pa_sample_spec spec;
        spec.format = PA_SAMPLE_S16NE;
        spec.rate = 48000;
        spec.channels = 2;
        pulse_stream = pa_stream_new(pulse_ctx, app_name, &spec, NULL);
        ASSERT(pulse_stream);

        pa_buffer_attr buf_attr;
        memset(&buf_attr, 0xff, sizeof(buf_attr)); // all -1 == default
        buf_attr.tlength = spec.rate * 16/8 * spec.channels * BUFFER_LEN_MS / 1000; // @HUH Refac?

        pa_stream_set_write_callback(pulse_stream, pulse_on_io_complete, udata);
        // @HUH: Should I choose a device more wisely?
        cd = pa_stream_connect_playback(pulse_stream, NULL, &buf_attr, 0, NULL, NULL);
        ASSERT(cd == 0);

        pa_threaded_mainloop_wait(pulse_mloop);
        ASSERT(pa_stream_get_state(pulse_stream) == PA_STREAM_READY);
    }
    pa_threaded_mainloop_unlock(pulse_mloop);
}

void pulse_deinit()
{
    pa_threaded_mainloop_lock(pulse_mloop);
    {
        pa_stream_disconnect(pulse_stream);
        pa_stream_unref(pulse_stream);
        pa_context_disconnect(pulse_ctx);
        pa_context_unref(pulse_ctx);
    }
    pa_threaded_mainloop_unlock(pulse_mloop);
    
	pa_threaded_mainloop_stop(pulse_mloop);
	pa_threaded_mainloop_free(pulse_mloop);
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
    pulse_init();

    render_gradient(&global_backbuffer, x_offset, y_offset); // @TEST
    x11_redraw_buffer();
    pulse_fill_sound_buffer(); // @TEST

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
        pulse_fill_sound_buffer(); // @TEST

        delta_time = ((f32) clock() - fstart) / CLOCKS_PER_SEC;
        //printf("%6.3f ms per frame\n", delta_time * 1000);
    }

    pulse_deinit();
    x11_deinit();
    // @TODO: Move to another func
    munmap(global_backbuffer.bitmap_mem, pgbufsize);
    return 0;
}
