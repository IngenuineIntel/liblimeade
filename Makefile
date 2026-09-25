#!/usr/bin/make

CC     := clang
CFLAGS := -fPIC -O1 -Wall -Iinclude -std=gnu11
SRCS   := $(wildcard src/*.c)
OBJS   := $(SRCS:.c=.o)

objs: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

so: objs
	$(CC) -shared -o liblimeade-0.1.so $(OBJS) -lz

clean:
	rm -f $(OBJS) liblimeade-0.1.so

install: so uninstall
	sudo cp liblimeade-0.1.so /usr/local/lib/liblimeade.so.0.1
	sudo chmod 775 /usr/local/lib/liblimeade.so.0.1
	sudo ln -s /usr/local/lib/liblimeade.so.0.1 /usr/local/lib/liblimeade.so.0
	sudo ln -s /usr/local/lib/liblimeade.so.0 /usr/local/lib/liblimeade.so
	sudo cp -r include/liblimeade/ /usr/local/include/liblimeade
	sudo cp liblimeade.conf /etc/ld.so.conf.d/
	sudo ldconfig
	sudo ldconfig -p | grep liblimeade

uninstall:
	-sudo rm /usr/local/lib/liblimeade.so*
	-sudo rm -r /usr/local/include/liblimeade
	-sudo rm /etc/ld.so.conf.d/liblimeade.conf
	sudo ldconfig

