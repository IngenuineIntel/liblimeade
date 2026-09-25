#!/usr/bin/make

CC := clang
CFLAGS := -fPIC -O1 -Wall -Iinclude -std=gnu11 -Werror
SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)

objs: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

so: objs
	$(CC) -shared -o liblimeade-0.1.so $(OBJS)

clean:
	rm -f $(OBJS) liblimeade-0.1.so

