/* RecklessPillager/Windows/win32_rp.c */
#include "defs.h"

#include <windows.h>

// @NOTE: rdtsc only works with x86
#include <intrin.h>

/* @TODO:
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

static const LPCWSTR app_name = L"Reckless Pillager";

// Global state
static offscreen_buffer_t backbuffer = { 0 };
static input_state_t input_state     = { 0 };

static HWND window_handle = NULL;
static HBITMAP bitmap_handle = NULL;
static BITMAPINFO bmi = { 0 };

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

            win32_update_dib_section();
        }
    }

    // @TODO: deinit everything? Or just drop it for the OS
    return (int)msg.wParam;
}
