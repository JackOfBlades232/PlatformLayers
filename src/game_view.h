/* RecklessPillager/src/game_view.h */
#ifndef GAME_VIEW_SENTRY
#define GAME_VIEW_SENTRY

#include "map.h"
#include "character.h"

void draw_game_view(const map *m, const character *c, 
        int term_row, int term_col);

#endif
