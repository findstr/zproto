CC=g++
CCFLAG=-Wall -Wextra -O2 -g -std=c++0x -I../

zproto:zproto.o header.cpp body.cpp
	$(CC) $(CCFLAG) -o zproto proto.cpp header.cpp body.cpp zproto.o

test:zproto.o zprotowire.o test.cpp
	$(CC) $(CCFLAG) -o test test.cpp hello_world.cc zprotowire.o zproto.o -lpthread

libs: zprotowire.o zproto.o
	ar rcs libzproto.a $^



zprotowire.o:zprotowire.cpp
	$(CC) $(CCFLAG) -c $<

zproto.o:../zproto.c
	gcc -Wall -g -O2 -c ../zproto.c



clean:
	-rm zproto
	-rm test
	-rm *.o
	-rm *.cc
	-rm *.hpp
	-rm *.a


