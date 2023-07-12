/* RecklessPillager/src/map.c */
#include "map.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <curses.h>

map_cell *map_cell_at(map *m, int x, int y)
{
    return m->cells[y] + x;
}

map *create_map()
{
    return malloc(sizeof(map));
}

void destroy_map(map *m)
{
    if (m)
        free(m);
}

/* test impl */
static int init_cell(map *m, int x, int y, int glyph)
{
    map_cell *cell = map_cell_at(m, x, y);
    cell->glyph = glyph;
    switch (glyph) {
        case '.':
            cell->type = floor;
            break;
        case '#':
            cell->type = wall;
            break;
        case '~':
            cell->type = water;
            break;
        case '*':
            cell->type = fire;
            break;
        case ' ':
            cell->type = abyss;
            break;
        case '>':
            cell->type = escape;
            break;
        case '<':
            cell->type = floor;
            break;
        default:
            return 1;
    }

    return 0;
}

int read_map_from_file(map *m, FILE *f)
{
    int y, x;
    int ch;
    int init_res;

    y = 0;
    x = 0;
    while ((ch = getc(f)) != EOF && y < map_size_y) {
        if (ch == '\n') {
            if (x < map_size_x)
                return 1;
            else {
                x = 0;
                y++;
            }
        } else {
            init_res = init_cell(m, x, y, ch);
            if (init_res != 0)
                return 2;
            if (ch == '<') {
                m->entrance_x = x;
                m->entrance_y = y;
            }
            x++;
        }
    }

    if (y == map_size_y && x == 0)
        return 0;
    else
        return 1;
}

void clamp_map_xy(int *x, int *y)
{
    if (*x < 0)
        *x = 0;
    if (*y < 0)
        *y = 0;
    if (*x >= map_size_x)
        *x = map_size_x - 1;
    if (*y >= map_size_y)
        *y = map_size_y - 1;
}
