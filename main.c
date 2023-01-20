/* RecklessPillager/prog.c */
#include "src/engine.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Provide map file\n");
        return 1;
    }

    return run_game(argv[1]);
}
