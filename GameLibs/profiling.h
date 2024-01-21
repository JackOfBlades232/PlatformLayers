/* PlatformLayers/GameLibs/profiling.h */
#ifndef PROFOLING_SENTRY
#define PROFOLING_SENTRY

// @TODO: add separate options for only __rdtsc() (fast) and only qpc (precise)

void profiling_start_timed_section(const char *section_name);
void profiling_end_and_log_timed_section();

#endif
