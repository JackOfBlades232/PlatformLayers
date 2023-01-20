/* RecklessPillager/src/character.h */
#ifndef CHARACTER_SENTRY
#define CHARACTER_SENTRY

#include "map.h"

enum { character_glyph = '@' };

typedef enum tag_move_dir { up, down, left, right } move_dir;

typedef enum tag_move_result { none, died, escaped } move_result;

typedef struct tag_character {
    int cur_x, cur_y;
    map_cell *cur_cell;
} character;

void init_character(character *c, map *m);
void draw_character(character *c);
void hide_character(character *c);
move_result move_character(character *c, move_dir dir, map *m);

#endif
