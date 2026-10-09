#!/usr/bin/make
# quick-in-dirty test runners

test1:
	gcc test1.c -lz -llimeade -o test1.out
	valgrind --leak-check=full ./test1.out
	@echo \n\n\n

test2:
	gcc test2.c -lz -llimeade -o test2.out
	valgrind --leak-check=full ./test2.out

test3:
	gcc test3.c -lz -llimeade -o test3.out
	valgrind --leak-check=full ./test3.out

clean:
	rm test*.out

all: test1 test2 test3
