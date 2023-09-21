/* RecklessPillager/Windows/win32_rp.c */
#include "defs.h"

#include <windows.h>

#define COBJMACROS
#include <mmdeviceapi.h>
#include <audioclient.h>

// @NOTE: rdtsc only works with x86
#include <intrin.h>

#include <math.h>

/* @TODO:
 *  Sound test
 *  Asserts and error checking
 *  Abstract out
 */

typedef struct offscreen_buffer_tag {
    u32 *bitmap_mem;
    u32 width;
    u32 height;
    u32 bytes_per_pixel;
    u32 byte_size;
    u32 pitch;
} offscreen_buffer_t;

enum input_key_mask_tag {
    INPUT_KEY_LEFT  = 1,
    INPUT_KEY_RIGHT = 1 << 1,
    INPUT_KEY_UP    = 1 << 2,
    INPUT_KEY_DOWN  = 1 << 3,
    INPUT_KEY_ESC   = 1 << 4
};

typedef struct input_state_tag {
    u32 pressed_key_flags;
    bool quit;
} input_state_t;

typedef struct wasapi_state_tag {
    IAudioClient        *client;
    IAudioRenderClient  *render;
    IMMDevice           *dev;
    IMMDeviceEnumerator *enumerator;

    u32 samples_per_sec;
    u32 channels;
    u32 bytes_per_sample;
    
    // @TEST
    bool started_playback;
    u32 buf_frames;
} wasapi_state_t;

static const LPCWSTR app_name = L"Reckless Pillager";

// Global state
static offscreen_buffer_t backbuffer = { 0 };
static input_state_t input_state     = { 0 };

static HWND window_handle            = NULL;
static HBITMAP bitmap_handle         = NULL;
static BITMAPINFO bmi                = { 0 };

static wasapi_state_t wasapi_state   = { 0 };

// @TEST Controls
typedef struct movement_input_tag {
    f32 x, y;
} movement_input_t;

movement_input_t get_movement_input(input_state_t *input)
{
    movement_input_t movement = { 0 };
    if (input->pressed_key_flags & INPUT_KEY_LEFT)  movement.x -= 1;
    if (input->pressed_key_flags & INPUT_KEY_RIGHT) movement.x += 1;
    if (input->pressed_key_flags & INPUT_KEY_UP)    movement.y -= 1;
    if (input->pressed_key_flags & INPUT_KEY_DOWN)  movement.y += 1;

    return movement;
}

// @TEST Graphics
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

static f32 x_offset  = 0;
static f32 y_offset  = 0;
static f32 dx        = 0;
static f32 dy        = 0;

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

/// Win32 ///

void win32_resize_dib_section()
{
    if (backbuffer.bitmap_mem)
        VirtualFree(backbuffer.bitmap_mem, backbuffer.byte_size, MEM_RELEASE);

    RECT client_rect;
    GetClientRect(window_handle, &client_rect);

    backbuffer.width = client_rect.right - client_rect.left;
    backbuffer.height  = client_rect.bottom - client_rect.top;
    backbuffer.bytes_per_pixel = 4;
    backbuffer.pitch = backbuffer.width * backbuffer.bytes_per_pixel;
    backbuffer.byte_size = backbuffer.pitch * backbuffer.height;

    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = backbuffer.width;
    bmi.bmiHeader.biHeight = -(s32)backbuffer.height; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 8*backbuffer.bytes_per_pixel;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    backbuffer.bitmap_mem = VirtualAlloc(NULL, backbuffer.byte_size,
                                         MEM_RESERVE|MEM_COMMIT, 
                                         PAGE_READWRITE);

    // @TEST Graphics
    render_gradient(&backbuffer, x_offset, y_offset);
}

win32_update_dib_section()
{
    RECT client_rect;
    GetClientRect(window_handle, &client_rect);
    int window_width = client_rect.right - client_rect.left;
    int window_height = client_rect.bottom - client_rect.top;

    HDC hdc = GetDC(window_handle);
    StretchDIBits(hdc,
                  0, 0, window_width, window_height,
                  0, 0, backbuffer.width, backbuffer.height,
                  backbuffer.bitmap_mem,
                  &bmi,
                  DIB_RGB_COLORS,
                  SRCCOPY); 
    ReleaseDC(window_handle, hdc);
}

u32 win32_key_mask(u32 vk_code)
{
    // @TODO: move mapping out of os layer
    if (vk_code == VK_ESCAPE)
        return INPUT_KEY_ESC;
    else if (vk_code == 'W' || vk_code == VK_UP)
        return INPUT_KEY_UP;
    else if (vk_code == 'S' || vk_code == VK_DOWN)
        return INPUT_KEY_DOWN;
    else if (vk_code == 'D' || vk_code == VK_RIGHT)
        return INPUT_KEY_RIGHT;
    else if (vk_code == 'A' || vk_code == VK_LEFT)
        return INPUT_KEY_LEFT;

    return 0;
}

LRESULT CALLBACK win32_window_handle_proc(HWND   hwnd, 
                                          UINT   u_msg, 
                                          WPARAM w_param, 
                                          LPARAM l_param)
{
    switch (u_msg) {
        // Resize
        case WM_SIZE:
            win32_resize_dib_section();
            break;

        // (re)drawing of the window
        case WM_PAINT:
            win32_update_dib_section();
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_KEYDOWN:
            input_state.pressed_key_flags |= win32_key_mask(w_param);
            return 0;

        case WM_KEYUP:
            input_state.pressed_key_flags &= ~win32_key_mask(w_param);
            return 0;

        default:
            break;
    }

    return DefWindowProc(hwnd, u_msg, w_param, l_param);
}

/// WASAPI Audio ///

// @TEST Sound
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
    u32 wave_period = wasapi_state.samples_per_sec/wave_freq;

    // Smooth tone switch
    if (wave_period != prev_wave_period && prev_wave_period != 0)
        wave_counter = (u32)((f32)wave_period * ((f32)wave_counter/prev_wave_period));

    prev_wave_period = wave_period;

    // @TODO: account for the possibility of not 16-bit channels and not stereo
    s32 *sample_out = buf;
    for (size_t i = 0; i < nbytes/bytes_per_sample; i++) {
        if (wave_counter == 0)
            wave_counter = wave_period;

        s32 sample_val = 0;
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

void wasapi_init()
{
    // Shit for generic-like functions for COM library
    // @NOTE: sadly, no useful intrinsics for ids of interfaces, and this
    //  might not be very good
    const CLSID CLSID_MMDeviceEnumerator = {0xbcde0395, 0xe52f, 0x467c, {0x8e,0x3d, 0xc4,0x57,0x92,0x91,0x69,0x2e}};
    const IID IID_IMMDeviceEnumerator    = {0xa95664d2, 0x9614, 0x4f35, {0xa7,0x46, 0xde,0x8d,0xb6,0x36,0x17,0xe6}};
    const IID IID_IAudioClient           = {0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1,0x78, 0xc2,0xf5,0x68,0xa7,0x03,0xb2}};
    const IID IID_IAudioRenderClient     = {0xf294acfc, 0x3146, 0x4483, {0xa7,0xbf, 0xad,0xdc,0xa7,0xc2,0x60,0xe2}};

    // Init COM library
    CoInitializeEx(NULL, 0);

    // Create device enumerator
    CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, 
                     &IID_IMMDeviceEnumerator, 
                     (void **) &wasapi_state.enumerator);

    // Init the default device with the enumerator
    // eRender = playback, eConsole -- role indication (for games)
    IMMDeviceEnumerator_GetDefaultAudioEndpoint(wasapi_state.enumerator, 
                                                eRender, eConsole, 
                                                &wasapi_state.dev);

    // Create a buffer
    IMMDevice_Activate(wasapi_state.dev, &IID_IAudioClient, CLSCTX_ALL,
                       NULL, (void **) &wasapi_state.client);

    WAVEFORMATEX *wf;
	IAudioClient_GetMixFormat(wasapi_state.client, &wf);

    wasapi_state.samples_per_sec = wf->nSamplesPerSec;
    wasapi_state.channels = wf->nChannels;
    wasapi_state.bytes_per_sample = wf->wBitsPerSample/8;

    // Set up what we can -- mode and 
    u32 buffer_length_msec = 66;
	REFERENCE_TIME dur = buffer_length_msec * 1000 * 10; // in 100ns pieces
	u32 mode = AUDCLNT_SHAREMODE_SHARED;
    // Set up buffer itself
	IAudioClient_Initialize(wasapi_state.client, mode, 0,
                            dur, dur, (void *)wf, NULL);
    IAudioClient_GetBufferSize(wasapi_state.client, &wasapi_state.buf_frames);

    IAudioClient_GetService(wasapi_state.client, &IID_IAudioRenderClient, 
                            (void **) &wasapi_state.render);

    wasapi_state.started_playback = false;

    CoTaskMemFree(wf);
}

void wasapi_deinit()
{
    // @NOTE: do we need to drain output buffer?
    if (!wasapi_state.started_playback) {
		IAudioClient_Start(wasapi_state.client);
		wasapi_state.started_playback = true;
	}

    u32 filled;
    do {
        IAudioClient_GetCurrentPadding(wasapi_state.client, &filled);
    } while (filled > 0);

    IAudioRenderClient_Release(wasapi_state.render);
    IAudioClient_Release(wasapi_state.client);
    IMMDevice_Release(wasapi_state.dev);
    IMMDeviceEnumerator_Release(wasapi_state.enumerator);
    CoUninitialize();
}

void wasapi_write_to_stream()
{
    u32 filled;
	IAudioClient_GetCurrentPadding(wasapi_state.client, &filled);
	u32 n_free_frames = wasapi_state.buf_frames - filled;
    if (n_free_frames == 0)
        return;

    u8 *data;
    IAudioRenderClient_GetBuffer(wasapi_state.render, n_free_frames, &data);
    {
        // @TODO: what are frames? Seems like bytes*bps*channels
        fill_audio_buffer(data, n_free_frames*wasapi_state.channels*wasapi_state.bytes_per_sample,
                          wasapi_state.bytes_per_sample);
    }
	IAudioRenderClient_ReleaseBuffer(wasapi_state.render, n_free_frames, 0);

    if (!wasapi_state.started_playback) {
		IAudioClient_Start(wasapi_state.client);
		wasapi_state.started_playback = true;
	}
}

int APIENTRY wWinMain(_In_ HINSTANCE     h_instance,
                      _In_opt_ HINSTANCE h_prev_instance,
                      _In_ LPWSTR        lp_cmd_line,
                      _In_ int           n_cmd_show)
{
    // Create window class for window, will be used by class name
    const LPCWSTR class_name = L"GameWindow";

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = win32_window_handle_proc;
    wc.hInstance = h_instance;
    wc.lpszClassName = class_name;
    RegisterClass(&wc);

    window_handle = CreateWindowEx(0, class_name, app_name, WS_OVERLAPPEDWINDOW,
                                   0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 
                                   NULL, NULL, h_instance, NULL);
    ASSERT(window_handle);
    // n_cmd_show -- directive to min/max the win
    ShowWindow(window_handle, n_cmd_show);

    wasapi_init();

    MSG msg = { 0 };

    LARGE_INTEGER perf_count_freq_res;
    QueryPerformanceFrequency(&perf_count_freq_res);
    u64 perf_count_freq = perf_count_freq_res.QuadPart;

    LARGE_INTEGER last_counter;
    LARGE_INTEGER end_counter;
    QueryPerformanceCounter(&last_counter);
    
    u64 prev_clocks = __rdtsc();

    while (msg.message != (WM_QUIT | WM_CLOSE))
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            QueryPerformanceCounter(&end_counter);
            u64 dcounts = end_counter.QuadPart - last_counter.QuadPart;
            f32 dt = (f32)dcounts / perf_count_freq;

            u64 cur_clocks = __rdtsc();
            u64 dclocks = cur_clocks - prev_clocks;

            last_counter = end_counter;
            prev_clocks = cur_clocks;

            odprintf("%.2f ms/frame, %d fps, %lu clocks/frame\n",
                     dt * 1e3, (u32)(1.0f/dt), dclocks);

            // @TEST Graphics
            update_gardient(&input_state, &x_offset, &y_offset, dt);
            render_gradient(&backbuffer, x_offset, y_offset);

            wasapi_write_to_stream();
            win32_update_dib_section();
        }
    }

    wasapi_deinit();

    // @TODO: deinit everything? Or just drop it for the OS
    return (int)msg.wParam;
}
