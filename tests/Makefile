#!/usr/bin/make
# quick-in-dirty test runners

LIB_DIR ?= ..

test1:
	gcc test1.c -L.. -Wl,-rpath,'$$ORIGIN/..' -l:liblimeade-0.1.so -lz -o test1.out
	valgrind --leak-check=full ./test1.out

test2:
	gcc test2.c -L.. -Wl,-rpath,'$$ORIGIN/..' -l:liblimeade-0.1.so -lz -o test2.out
	valgrind --leak-check=full ./test2.out

test3_graph:
	python3 -m venv .env
	./.env/bin/pip install pandas matplotlib
	./.env/bin/python3 test_visualizer.py test3.csv

test4_graph:
	./.env/bin/python3 test_visualizer.py test4.csv

test3:
	gcc test3.c -lz -llimeade -o test3.out -O2
	./test3.out
	#-@cat test3.csv
	$(MAKE) test3_graph

test4:
	gcc test4.c -lz -llimeade -o test4.out -O2
	./test4.out
	#-@cat test4.csv
	$(MAKE) test4_graph

test5:
	gcc -I../include test_ack_concurrency.c -L$(LIB_DIR) \
	  -Wl,-rpath,$(abspath $(LIB_DIR)) -l:liblimeade-0.1.so -lz -pthread \
	  -o test_ack_concurrency.out
	./test_ack_concurrency.out

clean:
	-@rm test*.out *.csv *.txt *.png *.svg

all: test1 test2 test3 test4 test5
