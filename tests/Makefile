#!/usr/bin/make
# quick-in-dirty test runners

POC_CC ?= clang
POC_CFLAGS := -std=gnu23 -g -O1 -fno-omit-frame-pointer -fsanitize=address \
			  -Wall -Wextra -Wpedantic -Wno-gnu-pointer-arith \
			  -Wno-gnu-binary-literal -I../include
POC_LIBS := -lz -pthread
MEMORY_SAFETY_BINS := pocs/poc_parser_cleanup.asan \
					  pocs/poc_truncated_numeric.asan \
					  pocs/poc_zero_died.asan \
					  pocs/poc_string_boundary.asan \
					  pocs/poc_populate_failure.asan \
					  pocs/poc_ack_size.ubsan

pocs/%.asan: pocs/%.c $(wildcard ../src/*.c) pocs/poc_parser_packet.h pocs/poc_sender_context.h
	$(POC_CC) $(POC_CFLAGS) $< ../src/*.c $(POC_LIBS) -o $@

pocs/poc_ack_size.ubsan: pocs/poc_ack_size.c $(wildcard ../src/*.c)
	$(POC_CC) $(POC_CFLAGS) -fsanitize=address,undefined $< ../src/*.c $(POC_LIBS) -o $@

liblimeade:
	$(MAKE) -C .. so

test1: liblimeade
	gcc test1.c -L.. -Wl,-rpath,'$$ORIGIN/..' -l:liblimeade-0.1.so -lz -o test1.out
	valgrind --leak-check=full ./test1.out

test2: liblimeade
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

test5: $(MEMORY_SAFETY_BINS)
	sh pocs/check_memory_safety.sh

test6:
	$(POC_CC) $(POC_CFLAGS) test_ssh_framing.c ../src/th_recv_helpers.c $(POC_LIBS) -o test_ssh_framing.out
	./test_ssh_framing.out
	rm -f test_ssh_framing.out

clean:
	-@rm test*.out *.csv *.txt *.png *.svg
	-@rm test*.out *.csv *.txt *.png *.svg
	-@rm -f pocs/*.asan

.PHONY: liblimeade test_ssh_framing

all: test1 test2 test3 test4 test5 test6
