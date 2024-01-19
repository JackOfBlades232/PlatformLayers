/* PlatformLayers/GameLibs/profiling.c */
#include "profiling.h"
#include "../os.h"
#include "../defs.h"

enum {
    TIMER_STACK_SIZE = 256
};

const char *profiling_log_prefix = "[PROFILING] ";
#define PROFILING_LOG(_fmt, ...) os_debug_printf("%s" _fmt, profiling_log_prefix, ##__VA_ARGS__)

typedef struct timer_entry_tag {
    const char *name;
    u64 start_clocks;
    f32 start_time;
} timer_entry_t;

static timer_entry_t timer_stack[TIMER_STACK_SIZE] = { 0 };
static s32 timer_stack_head                        = 0;

void profiling_start_timed_section(const char *section_name)
{
    ASSERT(timer_stack_head < TIMER_STACK_SIZE);
    timer_entry_t *entry = &timer_stack[timer_stack_head++];

    entry->name         = section_name;
    entry->start_time   = os_get_time_in_frame();
    entry->start_clocks = os_get_clocks_in_frame();
}

void profiling_end_and_log_timed_section()
{
    ASSERT(timer_stack_head >= 0);
    timer_entry_t *entry = &timer_stack[--timer_stack_head];

    u64 dclocks = os_get_clocks_in_frame() - entry->start_clocks;
    f32 dtime   = os_get_time_in_frame() - entry->start_time;
    PROFILING_LOG("%s: %.4fms/%luclocks", entry->name, dtime * 1e3, dclocks);
}
