/* RecklessPillager/src/character.c */
#include "character.h"
#include "map.h"

#include <curses.h>

/* test impl */
void init_character(character *c, map *m)
{
    c->cur_x = m->entrance_x;
    c->cur_y = m->entrance_y;
    c->cur_cell = map_cell_at(m, c->cur_x, c->cur_y);
}

static void dir_to_dxdy(move_dir dir, int *dx, int *dy)
{
    switch (dir) {
        case up:
            *dx = 0;
            *dy = -1;
            break;
        case down:
            *dx = 0;
            *dy = 1;
            break;
        case left:
            *dx = -1;
            *dy = 0;
            break;
        case right:
            *dx = 1;
            *dy = 0;
    }
}

static move_result handle_new_cell(character *c, 
        map_cell *new_cell, int new_x, int new_y)
{
    switch (new_cell->type) {
        case floor: 
            c->cur_x = new_x;
            c->cur_y = new_y;
            c->cur_cell = new_cell;
            return none;
        case wall:
            return none;
        case water:
            return none;
        case fire:
            return died;
        case escape:
            return escaped;
        case abyss:
            return died;
        default:
            return none;
    }
}

/* test impl */
move_result move_character(character *c, move_dir dir, map *m)
{
    int dx, dy, new_x, new_y;
    map_cell *new_cell;

    dir_to_dxdy(dir, &dx, &dy);

    new_x = c->cur_x + dx;
    new_y = c->cur_y + dy;
    clamp_map_xy(&new_x, &new_y);
    new_cell = map_cell_at(m, new_x, new_y);

    if (new_cell != c->cur_cell)
        return handle_new_cell(c, new_cell, new_x, new_y);
    else
        return none;
}
