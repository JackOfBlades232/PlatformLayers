/* RecklessPillager/linux_main.c */
#include "defs.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#define XK_MISCELLANY
#define XK_LATIN
#include <X11/keysymdef.h>

#include <pulse/pulseaudio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>

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

typedef struct pulse_state_tag {
    pa_threaded_mainloop  *mloop;
    pa_context            *ctx;
    pa_stream             *stream;

    u32 samples_per_sec;
    u32 channels;
    u32 bytes_per_sample;
} pulse_state_t;

static offscreen_buffer_t backbuffer = { 0 };
static input_state_t input_state     = { 0 };

static x11_state_t x11_state         = { 0 };
static pulse_state_t pulse_state     = { 0 };

// @TEST Controls
typedef struct movement_input_tag {
    f32 x, y;
} movement_input_t;

movement_input_t get_movement_input(input_state_t *input)
{
    movement_input_t movement = { 0 };
    if (input->pressed_key_flags & VK_LEFT)  movement.x -= 1;
    if (input->pressed_key_flags & VK_RIGHT) movement.x += 1;
    if (input->pressed_key_flags & VK_UP)    movement.y -= 1;
    if (input->pressed_key_flags & VK_DOWN)  movement.y += 1;

    return movement;
}

// @TEST Graphics
#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720

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
    const s32 offset_per_s = 648;

    movement_input_t movement = get_movement_input(input);
    dx = offset_per_s * movement.x;
    dy = offset_per_s * movement.y;

    *x_offset += dx*dt;
    *y_offset += dy*dt;
}

/// X11 ///

// @IDEA: look at Casey's style and introduce local variables for less text
void x11_init()
{
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

/// Pulse Audio ///

// @TEST Sound
#define SOUND_SAMPLE_RATE       48000
#define SOUND_IS_STEREO         true
#define SOUND_BYTES_PER_CHANNEL 2

#define LATENCIES_PER_SEC       15
#define SOUND_BUFFER_LEN_MS     (1000/LATENCIES_PER_SEC)

void fill_audio_buffer(u8 *buf, u32 nbytes, u32 bytes_per_sample)
{
    enum wave_type_t { wt_square, wt_sine };

    //enum wave_type_t wtype = wt_square;
    const enum wave_type_t wtype = wt_sine;
    const s16 wave_volume = 2000;
    const u32 base_tone_hz = 384;
    const u32 offset_tone_hz = 128;

    static u32 wave_counter = 0;
    static u32 prev_wave_period = 0;

    movement_input_t movement = get_movement_input(&input_state);
    u32 wave_freq = base_tone_hz - offset_tone_hz*movement.y;
    u32 wave_period = pulse_state.samples_per_sec/wave_freq;

    // Smooth tone switch
    if (wave_period != prev_wave_period && prev_wave_period != 0)
        wave_counter = (u32)((f32)wave_period * ((f32)wave_counter/prev_wave_period));

    prev_wave_period = wave_period;

    s16 *sample_out = buf;
    for (size_t i = 0; i < nbytes/bytes_per_sample; i++) {
        if (wave_counter == 0)
            wave_counter = wave_period;

        s16 sample_val = 0;
        if (wtype == wt_square) {
            sample_val = wave_counter > wave_period/2 ?
                wave_volume : -wave_volume;
        } else if (wtype == wt_sine) {
            sample_val = wave_volume *
                sinf((f32)(wave_period-wave_counter)*2*M_PI/wave_period);
        }

        *(sample_out++) = sample_val;
        *(sample_out++) = sample_val;
        wave_counter--;
    }
}

#define ASSERT_PULSE(_cd) ASSERT_LIBERR_GEN(_cd == 0, pa_strerror, _cd)
#define ASSERT_PULSE_CTX(_e, _ctx) ASSERT_LIBERR_GEN(_e, pa_strerror, pa_context_errno(_ctx))

void pulse_on_state_change(pa_context *ctx, void *udata);
void pulse_on_io_complete(pa_stream *stream, size_t nbytes, void *udata);
void pulse_on_op_complete(pa_stream *stream, int success, void *udata);

void pulse_init()
{
    int pa_res;

    pulse_state.samples_per_sec = SOUND_SAMPLE_RATE;
    pulse_state.channels = SOUND_IS_STEREO ? 2 : 1;
    pulse_state.bytes_per_sample = SOUND_BYTES_PER_CHANNEL * pulse_state.channels;

    pulse_state.mloop = pa_threaded_mainloop_new();
    ASSERT_ERR(pulse_state.mloop);

    pa_res = pa_threaded_mainloop_start(pulse_state.mloop);
    ASSERT_PULSE(pa_res);

    pa_threaded_mainloop_lock(pulse_state.mloop);
    {
        // Context connection
        pulse_state.ctx = pa_context_new_with_proplist(
                pa_threaded_mainloop_get_api(pulse_state.mloop), 
                app_name, NULL);
        ASSERT_ERR(pulse_state.ctx);

        pa_context_set_state_callback(pulse_state.ctx, pulse_on_state_change, NULL);
        pa_res = pa_context_connect(pulse_state.ctx, NULL, 0, NULL);
        ASSERT_PULSE(pa_res);
        while (pa_context_get_state(pulse_state.ctx) != PA_CONTEXT_READY)
            pa_threaded_mainloop_wait(pulse_state.mloop);

        // Opening stream&buffer
        pa_sample_spec spec;
        spec.format = PA_SAMPLE_S16NE;
        spec.rate = pulse_state.samples_per_sec;
        spec.channels = 2;
        pulse_state.stream = pa_stream_new(pulse_state.ctx, app_name, &spec, NULL);
        ASSERT_PULSE_CTX(pulse_state.stream, pulse_state.ctx);

        pa_buffer_attr buf_attr;
        memset(&buf_attr, 0xff, sizeof(buf_attr)); // all -1 == default
        buf_attr.tlength = 
            spec.rate * pulse_state.bytes_per_sample * SOUND_BUFFER_LEN_MS / 1000;

        pa_stream_set_write_callback(pulse_state.stream, pulse_on_io_complete, NULL);
        pa_res = pa_stream_connect_playback(pulse_state.stream, NULL, &buf_attr, 0, NULL, NULL);
        ASSERT_PULSE(pa_res);
        for (;;) {
            int res = pa_stream_get_state(pulse_state.stream);
            ASSERTF(res != PA_STREAM_FAILED, "Failed to connect PA stream\n");
            if (res == PA_STREAM_READY)
                break;

            pa_threaded_mainloop_wait(pulse_state.mloop);
        }
    }
    pa_threaded_mainloop_unlock(pulse_state.mloop);
}

void pulse_deinit()
{
    pa_threaded_mainloop_lock(pulse_state.mloop);
    {
        pa_operation *op = pa_stream_drain(pulse_state.stream,
                                           pulse_on_op_complete, 
                                           NULL);

        for (;;) {
            int res = pa_operation_get_state(op);
            if (res == PA_OPERATION_DONE || res == PA_OPERATION_CANCELLED)
                break;
            pa_threaded_mainloop_wait(pulse_state.mloop);
        }
        pa_operation_unref(op);

        pa_stream_disconnect(pulse_state.stream);
        pa_stream_unref(pulse_state.stream);
        pa_context_disconnect(pulse_state.ctx);
        pa_context_unref(pulse_state.ctx);
    }
    pa_threaded_mainloop_unlock(pulse_state.mloop);

	pa_threaded_mainloop_stop(pulse_state.mloop);
	pa_threaded_mainloop_free(pulse_state.mloop);
}

void pulse_write_to_stream()
{
    size_t nbytes;
    if ((nbytes = pa_stream_writable_size(pulse_state.stream)) == 0)
        return;
    ASSERT_PULSE_CTX(nbytes != -1, pulse_state.ctx);

    pa_threaded_mainloop_lock(pulse_state.mloop);
    {
        int pa_res;
        u8 *buf;
        pa_res = pa_stream_begin_write(pulse_state.stream, &buf, &nbytes);
        ASSERT_PULSE_CTX(pa_res == 0 && buf, pulse_state.ctx);

        // @TEST Sound
        fill_audio_buffer(buf, nbytes, pulse_state.bytes_per_sample);

        pa_res = pa_stream_write(pulse_state.stream, buf, nbytes, NULL, 0, PA_SEEK_RELATIVE);
        ASSERT_PULSE(pa_res);
    }
    pa_threaded_mainloop_unlock(pulse_state.mloop);
}

void pulse_on_state_change(pa_context *ctx, void *udata)
{
    pa_threaded_mainloop_signal(pulse_state.mloop, 0);
}

void pulse_on_io_complete(pa_stream *stream, size_t nbytes, void *udata)
{
    pa_threaded_mainloop_signal(pulse_state.mloop, 0);
}

void pulse_on_op_complete(pa_stream *stream, int success, void *udata)
{
    pa_threaded_mainloop_signal(pulse_state.mloop, 0);
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
    pulse_init();

    // @TEST Graphics
    render_gradient(&backbuffer, x_offset, y_offset);

    u64 prev_time = get_nsec();

    for (;;) {
        sched_yield();
        
        x11_poll_events();

        u64 cur_time = get_nsec();
        if (cur_time == prev_time)
            continue;

        f32 dt = (f32)((f64)(cur_time - prev_time) * 1e-9);
        // @TODO: why should I make dt = const if dt > const?

        printf("%6.5fs per frame\n", dt);

        // @TEST Graphics
        update_gardient(&input_state, &x_offset, &y_offset, dt);

        prev_time = cur_time;

        if (input_state.quit)
            break;

        // @TEST Graphics
        render_gradient(&backbuffer, x_offset, y_offset);

        pulse_write_to_stream();
        x11_draw_buffer();
    }

    pulse_deinit();
    x11_deinit();
    munmap(backbuffer.bitmap_mem, pgbufsize);
    return 0;
}
