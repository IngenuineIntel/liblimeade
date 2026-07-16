#!/usr/bin/make

objs: # compiles into .o files
	-mkdir ./bin
	gcc -fPIC -c ./src/encode.c -o ./bin/encode.o -Iinclude
	gcc -fPIC -c ./src/decode.c -o ./bin/decode.o -Iinclude
	gcc -fPIC -c ./src/errors.c -o ./bin/errors.o -Iinclude
	gcc -fPIC -c ./src/ssh-compat.c -o ./bin/ssh-compat.o -Iinclude
	gcc -fPIC -c ./src/versioning.c -o ./bin/versioning.o -Iinclude

so: objs # compiles into .so file
	# TODO * here is kinda bad?
	gcc -shared -o liblimeade.so ./bin/*.o
