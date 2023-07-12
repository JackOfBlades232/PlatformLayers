engine.o: src/engine.c src/engine.h src/map.h src/character.h \
 src/game_view.h src/utils.h
map.o: src/map.c src/map.h src/utils.h
character.o: src/character.c src/character.h src/map.h
game_view.o: src/game_view.c src/game_view.h src/map.h src/character.h \
 src/utils.h
utils.o: src/utils.c src/utils.h
