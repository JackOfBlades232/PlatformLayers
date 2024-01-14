/* PlatformLayers/GameLibs/profiling.h */
#ifndef PROFOLING_SENTRY
#define PROFOLING_SENTRY

void profiling_start_timed_section(const char *section_name);
void profiling_end_and_log_timed_section();

#endif
