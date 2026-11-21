CC = gcc
CFLAGS = -std=gnu11 -Wall -Werror

.PHONY: all clean
all: myfind

myfind: myfind.c
	$(CC) $(CFLAGS)  -o myfind myfind.c
clean:
	rm -f myfind
