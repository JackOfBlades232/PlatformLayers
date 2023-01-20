/* RecklessPillager/src/character.c */
#include "character.h"
#include "map.h"

#include <curses.h>

/* test impl */
void init_character(character *c, map *m)
{
    c->cur_x = 1;
    c->cur_y = 1;
    c->cur_cell = map_cell_at(m, c->cur_x, c->cur_y);
}

void draw_character(character *c)
{
    move(c->cur_y, c->cur_x);
    addch(character_glyph);
}

void hide_character(character *c)
{
    move(c->cur_y, c->cur_x);
    addch(c->cur_cell->glyph);
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
            hide_character(c);

            c->cur_x = new_x;
            c->cur_y = new_y;
            c->cur_cell = new_cell;

            draw_character(c);
            return none;
        case wall:
            return none;
        case water:
            return none;
        case fire:
            return died;
        case escape:
            hide_character(c);
            return escaped;
        case abyss:
            hide_character(c);
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
