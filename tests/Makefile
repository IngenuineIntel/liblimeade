#!/usr/bin/make
# quick-in-dirty test runners

test1:
	gcc test1.c -lz -llimeade -o test1.out
	valgrind --leak-check=full ./test1.out

test2:
	gcc test2.c -lz -llimeade -o test2.out
	valgrind --leak-check=full ./test2.out

test3_graph:
	python3 -m venv env
	env/bin/pip install pandas matplotlib
	env/bin/python3 test3.py

test3:
	gcc test3.c -lz -llimeade -o test3.out
	./test3.out
	#-@cat test3.csv
	$(MAKE) test3_graph

clean:
	-@rm test*.out *.csv *.txt *.png *.svg

all: test1 test2 test3
