CC=g++
CCFLAG=-Wall -Wextra -g -std=c++0x -I../

zproto:zproto.o header.cpp body.cpp
	$(CC) $(CCFLAG) -o zproto proto.cpp header.cpp body.cpp zproto.o

test:zproto.o zprotowire.o
	$(CC) $(CCFLAG) -o test test.cpp hello_world.cc zprotowire.o zproto.o

libs: zprotowire.o zproto.o
	ar rcs libzproto.a $^



zprotowire.o:zprotowire.cpp
	$(CC) $(CCFLAG) -c $<

zproto.o:../zproto.c
	gcc -Wall -g -c ../zproto.c



clean:
	-rm zproto
	-rm test
	-rm *.o
	-rm *.cc
	-rm *.hpp


