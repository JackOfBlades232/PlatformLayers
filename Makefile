SRCMODULES = $(shell find src -type f -name '*.c')
OBJMODULES = $(SRCMODULES:.c=.o)
CC = gcc
CFLAGS = -g -Wall #-O3
LFLAGS = -lX11 -lasound

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

game: linux_main.c $(OBJMODULES)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@

ifneq (clean, $(MAKECMDGOALS))
-include deps.mk
endif

deps.mk: $(SRCMODULES)
	$(CC) -MM $^ > $@

clean:
	rm -rf *.o src/*.o game
