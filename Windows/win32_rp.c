/* RecklessPillager/Windows/win32_rp.c */
#include "defs.h"

#include <windows.h>

// @TEST
#include <string.h>

typedef struct offscreen_buffer_tag {
    u32 *bitmap_mem;
    u32 width;
    u32 height;
    u32 bytes_per_pixel;
    u32 pitch;
} offscreen_buffer_t;

static const LPCWSTR app_name = L"Reckless Pillager";

// Global state
static offscreen_buffer_t backbuffer = { 0 };

static HWND window = { 0 };
static BITMAPINFO bmi = { 0 };

LRESULT CALLBACK WindowProc(HWND   hwnd, 
                            UINT   u_msg, 
                            WPARAM w_param, 
                            LPARAM l_param)
{
    switch (u_msg) {
        // Resize
        case WM_SIZE:
            {
                // @TODO: do the resize
            } break;

        // (re)drawing of the window
        case WM_PAINT:
            {
                PAINTSTRUCT paint;
                HDC hdc = BeginPaint(window, &paint);
                int x = paint.rcPaint.left;
                int y = paint.rcPaint.top;
                int width = paint.rcPaint.right - x;
                int height = paint.rcPaint.bottom - y;
                StretchDIBits(hdc,
                              x, y, width, height,
                              x, y, width, height,
                              backbuffer.bitmap_mem,
                              &bmi,
                              DIB_RGB_COLORS,
                              SRCCOPY); 
                EndPaint(hwnd, &paint);    
            } break;

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
    backbuffer.width = 800;
    backbuffer.height = 600;
    backbuffer.bytes_per_pixel = 4;
    backbuffer.pitch = backbuffer.width * backbuffer.bytes_per_pixel;
    // @TODO: allocate my own buffer

    // Create window class for window, will be used by class name
    const LPCWSTR class_name = L"GameWindow";

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = h_instance;
    wc.lpszClassName = class_name;
    RegisterClass(&wc);

    window = CreateWindowEx(0, class_name, app_name, WS_OVERLAPPEDWINDOW,
                            0, 0, backbuffer.width, backbuffer.height, 
                            NULL, NULL, h_instance, NULL);
    ASSERT(window);
    ShowWindow(window, n_cmd_show); // n_cmd_show -- directive to min/max the win

    HDC hdc = GetDC(window);

    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = backbuffer.width;
    bmi.bmiHeader.biHeight = -((s32)backbuffer.height); // top down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 8*backbuffer.bytes_per_pixel;
    bmi.bmiHeader.biCompression = BI_RGB;

    HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS,
                                   &backbuffer.bitmap_mem,
                                   NULL, 0);
    ReleaseDC(window, hdc);

    // @TEST
    // @TODO: add gradient test
    memset(backbuffer.bitmap_mem, 0, backbuffer.pitch*backbuffer.height);

    MSG msg = { 0 };

    LARGE_INTEGER perf_count_freq_res;
    QueryPerformanceFrequency(&perf_count_freq_res);
    u64 perf_count_freq = perf_count_freq_res.QuadPart;

    LARGE_INTEGER last_counter;
    LARGE_INTEGER end_counter;
    QueryPerformanceCounter(&last_counter);

    /* @TODO:
     *  redraw buffer every frame (animate)
     *  add __rdtsc logging
     */

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
            last_counter = end_counter;

            odprintf("%f ms/frame, %d fps\n", dt * 1e3, (u32)(1.0f / dt));
        }
    }

    // @TODO: deinit everything? Or just drop it for the OS
    return (int)msg.wParam;
}
