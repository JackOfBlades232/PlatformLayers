/* RecklessPillager/src/game_view.c */
#include "game_view.h"
#include "map.h"
#include "utils.h"

#include <curses.h>

enum { character_glyph = '@' };

static void draw_cell(const map *m, int x, int y)
{
    move(y, x);
    addch(m->cells[y][x].glyph);
}

static void draw_character(const character *c, int img_x, int img_y)
{
    move(img_y + c->cur_y, img_x + c->cur_x);
    addch(character_glyph);
}

/* test impl */
void draw_game_view(const map *m, const character *c, 
        int term_row, int term_col)
{
    int y, x;
    int img_w, img_h;
    int img_x, img_y;

    /* test */
    term_row = 15;
    term_col = 45;

    img_h = int_min(map_size_y, term_row);

    /* There may be a bug with last col/row not showing when w or h % 2 == 1 */
    if (map_size_x <= term_col) {
        img_w = map_size_x;
        img_x = 0;
    } else {
        int img_halfw;

        img_w = term_col;
        img_halfw = img_w/2;
        img_x = int_clamp(
                c->cur_x, 
                img_halfw, 
                map_size_x - img_halfw
            ) - img_halfw;
    }

    if (map_size_y <= term_col) {
        img_h = map_size_y;
        img_y = 0;
    } else {
        int img_halfh;

        img_h = term_col;
        img_halfh = img_h/2;
        img_y = int_clamp(
                c->cur_y, 
                img_halfh, 
                map_size_y - img_halfh
            ) - img_halfh;
    }

    move(term_row + 1, 0);
    wprintw(stdscr, "%d %d\n", img_x, img_y);

    for (y = img_y; y < img_y + img_h; y++)
        for (x = img_x; x < img_x + img_w; x++)
             draw_cell(m, x, y);

    draw_character(c, img_x, img_y);

    refresh();
}

