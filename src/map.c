/* RecklessPillager/src/map.c */
#include "map.h"
#include "utils.h"

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
    free(m);
}

/* test impl */
int read_map_from_file(map *m, FILE *f)
{
    int y, x;

    for (y = 0; y < map_size_y; y++)
        for (x = 0; x < map_size_x; x++) {
            map_cell *cell = map_cell_at(m, x, y);
            if (x == map_size_x-1 && y == map_size_y-2) {
                cell->glyph = '>';
                cell->type = escape;
            } else if (x == 0 || y == 0 ||
                    x == map_size_x-1 || y == map_size_y-1) {
                cell->glyph = '#';
                cell->type = wall;
            } else  {
                cell->glyph = '.';
                cell->type = floor;
            }
        }

    return 0;
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

/* test impl */
void draw_cell(const map *m, int x, int y)
{
    move(y, x);
    addch(m->cells[y][x].glyph);
}

/* test impl */
void draw_map(const map *m, int term_row, int term_col)
{
    int y, x;
    int img_w, img_h;

    img_w = int_min(map_size_x, term_col);
    img_h = int_min(map_size_y, term_row);

    for (y = 0; y < img_h; y++)
        for (x = 0; x < img_w; x++)
             draw_cell(m, x, y);
}
