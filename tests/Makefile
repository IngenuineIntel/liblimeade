#!/usr/bin/make
# quick-in-dirty test runners

test1:
	gcc test1.c -lz -llimeade -o test1.out
	valgrind --leak-check=full ./test1.out

test2:
	gcc test2.c -lz -llimeade -o test2.out
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

clean:
	-@rm test*.out *.csv *.txt *.png *.svg

all: test1 test2 test3 test4
