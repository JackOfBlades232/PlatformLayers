/* RecklessPillager/Windows/win32_rp.c */
#include "defs.h"

#include <windows.h>

#include <stdio.h>
#include <stdarg.h>

// Limited to 4kb
void odprintf(const char* format, ...)
{
    char buf[4096];
    char* p = buf;
    va_list args;
    int nchars;
    const int max_len = sizeof(buf) - 3; // \r\n\0

    va_start(args, format);
    nchars = _vsnprintf_s(buf, sizeof(buf), max_len, format, args);
    va_end(args);

    if (nchars > 0)
        p += nchars;

    *(p++) = '\r';
    *(p++) = '\n';
    *p = '\0';

    OutputDebugStringA(buf);
}

int APIENTRY wWinMain(_In_ HINSTANCE     h_instance,
                      _In_opt_ HINSTANCE h_prev_instance,
                      _In_ LPWSTR        lp_cmd_line,
                      _In_ int           n_cmd_show)
{
    MSG msg = { 0 };

    LARGE_INTEGER perf_count_freq_res;
    QueryPerformanceFrequency(&perf_count_freq_res);
    u64 perf_count_freq = perf_count_freq_res.QuadPart;

    LARGE_INTEGER last_counter;
    LARGE_INTEGER end_counter;
    QueryPerformanceCounter(&last_counter);

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

    return (int)msg.wParam;
}
