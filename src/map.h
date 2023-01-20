/* RecklessPillager/src/map.h */
#ifndef MAP_SENTRY
#define MAP_SENTRY

#include <stdio.h>

enum { map_size_x = 65, map_size_y = 19 };

typedef enum tag_cell_type {
    floor, wall, water, fire, abyss, escape
} cell_type;

typedef struct tag_map_cell {
    unsigned char glyph;
    cell_type type;
} map_cell;

typedef struct tag_map {
    map_cell cells[map_size_y][map_size_x];
    int entrance_x, entrance_y;
} map;

map_cell *map_cell_at(map *m, int x, int y);

map *create_map();
void destroy_map(map *m);
int read_map_from_file(map *m, FILE *f);

void clamp_map_xy(int *x, int *y);

void draw_cell(const map *m, int x, int y);
void draw_map(const map *m, int term_row, int term_col);

#endif
