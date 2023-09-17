/* RecklessPillager/Windows/win32_rp.c */
#include "defs.h"

#include <windows.h>

// @NOTE: rdtsc only works with x86
#include <intrin.h>

/* @TODO:
 *  Keyboard input test
 *  Sound test
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

static const LPCWSTR app_name = L"Reckless Pillager";

// Global state
static offscreen_buffer_t backbuffer = { 0 };

static HWND window_handle = NULL;
static HBITMAP bitmap_handle = NULL;
static BITMAPINFO bmi = { 0 };

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

// @TODO: insert controls
void update_gardient(f32 *x_offset, f32 *y_offset, f32 dt)
{
    const s32 offset_per_s = 648;

    dx = offset_per_s * 1.0f;
    dy = offset_per_s * 0.5f;

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
                                         MEM_COMMIT, PAGE_READWRITE);

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

        default:
            break;
    }

    return DefWindowProc(hwnd, u_msg, w_param, l_param);
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

    // @TODO: add gradient test

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
            update_gardient(&x_offset, &y_offset, dt);
            render_gradient(&backbuffer, x_offset, y_offset);

            win32_update_dib_section();
        }
    }

    // @TODO: deinit everything? Or just drop it for the OS
    return (int)msg.wParam;
}
