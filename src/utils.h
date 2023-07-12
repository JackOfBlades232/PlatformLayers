/* RecklessPillager/src/utils.h */
#ifndef UTILS_SENTRY
#define UTILS_SENTRY

#include <stdio.h>

int int_max(int a, int b);
int int_min(int a, int b);

int int_clamp(int n, int min, int max);

FILE *fopen_r(const char *path);

#endif
