/* RecklessPillager/src/utils.c */
#include "utils.h"

int int_max(int a, int b)
{
    return a > b ? a : b;
}

int int_min(int a, int b)
{
    return a < b ? a : b;
}

FILE *fopen_r(const char *path)
{
    FILE *f;
    f = fopen(path, "r");
    return f;
}
