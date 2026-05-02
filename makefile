BINARY = nupython
CC = gcc
CFLAGS = -std=c11 -g -Wall -pedantic -Werror -Wno-psabi -Wno-unused-variable -Wno-unused-function
LDFLAGS = -lm
SUPPORT_OBJECTS = ram.o nupython.o

.PHONY: all run valgrind clean

all:
	rm -f ./$(BINARY)
	$(CC) $(CFLAGS) main.c execute.c $(SUPPORT_OBJECTS) $(LDFLAGS) -o $(BINARY)

run: all
	./$(BINARY)

valgrind: all
	valgrind --tool=memcheck --leak-check=no --track-origins=yes ./$(BINARY) "$(file)"

clean:
	rm -f ./$(BINARY)
