#!/usr/bin/make

CC := clang
CFLAGS := -fPIC -O2 -Wall -Iinclude -std=gnu11
SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)

.PHONY: objs so clean build_tests help

objs: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

so: objs
	$(CC) -shared -o liblimeade.so $(OBJS)

clean:
	rm -f src/*.o liblimeade.so

help:
	@echo "Available targets:"
	@echo "  objs"
	@echo "  so"
	@echo "  clean"
	@echo "  build_tests"
	@echo "  help"
