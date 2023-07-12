/* RecklessPillager/src/utils.c */
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

int int_max(int a, int b)
{
    return a > b ? a : b;
}

int int_min(int a, int b)
{
    return a < b ? a : b;
}

int int_clamp(int n, int min, int max)
{
    if (min > max) {
        fprintf(stderr, "Trying to clamp int with min=%d > max=%d\n",
                min, max);
        exit(11);
    }

    return n < min ? min : (n > max ? max : n);
}

FILE *fopen_r(const char *path)
{
    FILE *f;
    f = fopen(path, "r");
    return f;
}
