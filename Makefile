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
	$(CC) -shared -o liblimeade-0.1.so $(OBJS)

clean:
	rm -f src/*.o liblimeade-0.1.so

install: so
	cp liblimeade-0.1.so /usr/local/lib/liblimeade.so.0.1
	chmod 775 /usr/local/lib/liblimeade.so.0.1
	ln -s /usr/local/lib/liblimeade.so.0.1 /usr/local/lib/liblimeade.so.0
	ln -s /usr/local/lib/liblimeade.so.0 /usr/local/lib/liblimeade.so
	cp -r include/liblimeade /usr/local/include/liblimeade
	cp liblimeade.conf /etc/ld.so.conf.d/
	ldconfig
	ldconfig -p | grep liblimeade

uninstall:
	-rm /usr/local/lib/liblimeade.so.0.1
	-rm /usr/local/lib/liblimeade.so.0
	-rm /usr/local/lib/liblimeade.so
	-rm -r /usr/local/include/liblimeade
	-rm /etc/ld.so.conf.d/liblimeade.conf
	ldconfig

help:
	@echo "Available targets:"
	@echo "  objs"
	@echo "  so"
	@echo "  clean"
	@echo "  install"
	@echo "  uninstall"
	@echo "  help"
