SRCMODULES = $(shell find src -type f -name '*.c')
OBJMODULES = $(SRCMODULES:.c=.o)
CC = gcc
CFLAGS = -g -Wall -O0
DEFS = -DUSE_ASSERTIONS
LFLAGS = -lX11 -lpulse -lm

%.o: %.c %.h
	$(CC) $(CFLAGS) $(DEFS) -c $< -o $@

game: linux_main.c $(OBJMODULES)
	$(CC) $(CFLAGS) $(DEFS) $^ $(LFLAGS) -o $@

ifneq (clean, $(MAKECMDGOALS))
-include deps.mk
endif

deps.mk: $(SRCMODULES)
	$(CC) -MM $^ > $@

clean:
	rm -rf *.o src/*.o game
