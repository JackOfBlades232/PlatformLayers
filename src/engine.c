/* RecklessPilager/src/engine.c */
#include "map.h"
#include "character.h"

#include <curses.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

enum { key_escape = 27 };

typedef struct tag_term_state {
    int row, col;
    int key;
} term_state;

static term_state t_state;

static int init_curses(term_state *t_state)
{
    initscr();
    if (!has_colors()) {
        endwin();
        fprintf(stderr, "Can't show colors on a BW screen\n");
        return -1;
    }
    cbreak();
    keypad(stdscr, 1);
    noecho();
    curs_set(0);
    start_color();

    getmaxyx(stdscr, t_state->row, t_state->col);
    return 0;
}

static int init_game(map **mp, character *c)
{
    int status;

    status = init_curses(&t_state);
    if (status != 0)
        return 1;

    *mp = create_map();
    status = read_map_from_file(*mp, NULL);
    if (status != 0)
        return 2;

    init_character(c, *mp);

    return 0;
}

static void show_endgame_screen(const char *message)
{
    erase();
    move(t_state.row/2, (t_state.col-strlen(message))/2);
    addstr(message);
    refresh();
    usleep(1000000);
}

/* test impl */
int run_game()
{
    map *m = NULL;
    character c;
    init_game(&m, &c);

    draw_map(m, t_state.row, t_state.col);
    draw_character(&c);
    refresh();

    while ((t_state.key = getch()) != key_escape) {
        move_result move_res;

        switch (t_state.key) {
            case KEY_UP:
                move_res = move_character(&c, up, m);
                break;
            case KEY_DOWN:
                move_res = move_character(&c, down, m);
                break;
            case KEY_LEFT:
                move_res = move_character(&c, left, m);
                break;
            case KEY_RIGHT:
                move_res = move_character(&c, right, m);
                break;
            case KEY_RESIZE:
                goto deinitialization;
        }

        switch (move_res) {
            case none:
                break;
            case died:
                show_endgame_screen("You died");
                goto deinitialization;
            case escaped:
                show_endgame_screen("You escaped!");
                goto deinitialization;
        }

        usleep(25000);
    }

deinitialization:
    endwin();
    destroy_map(m);
    return 0;
}
