GAME = jailbreak
SRCMODULES = $(shell find src -type f -name '../Linux/*.c') $(shell find src -type f -name '../GameLibs/*.c') ../TestGames/game_$(GAME).c
OBJMODULES = $(SRCMODULES:.c=.o)
CC = gcc
CFLAGS = -g -Wall -O0
DEFS = -DUSE_ASSERTIONS
LFLAGS = -lX11 -lpulse -lm

%.o: %.c %.h
	$(CC) $(CFLAGS) $(DEFS) -c $< -o $@
	# TODO: move .o files to build dir

build: linux_main.c $(OBJMODULES)
	$(CC) $(CFLAGS) $(DEFS) $^ $(LFLAGS) -o linux_main.out

ifneq (clean, $(MAKECMDGOALS))
-include deps.mk
endif

deps.mk: $(SRCMODULES)
	$(CC) -MM $^ > $@

clean:
	rm -rf *.o ../Linux/*.o ../TestGames/game_$(GAME).o ../GameLibs/*.o linux_main.out
