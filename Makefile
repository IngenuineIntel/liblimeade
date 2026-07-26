#!/usr/bin/make

CC := clang
CFLAGS := -fPIC -O2 -Wall -Iinclude -std=c11
SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)

.PHONY: objs so clean build_tests help

objs: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

so: objs
	$(CC) -shared -o liblimeade.so $(OBJS)

build_tests:
	$(MAKE) -C tests

clean:
	rm -f src/*.o liblimeade.so

help:
	@echo "Available targets:"
	@echo "  objs        - compile source files into object files"
	@echo "  so          - build shared library liblimeade.so"
	@echo "  clean       - remove object files and library"
	@echo "  build_tests - invoke Makefile in tests/ directory"
	@echo "  help        - show this message"
