zproto:zproto.o header.cpp body.cpp
	g++ -Wall -g -std=c++0x -I../ -g -o zproto proto.cpp header.cpp body.cpp zproto.o

test:zproto.o
	g++ -Wall -g -std=c++0x -I../ -g -o test test.cpp test_zproto.cc zprotowire.cpp zproto.o

zproto.o:../zproto.c
	gcc -Wall -g -c ../zproto.c

clean:
	-rm zproto
	-rm test
	-rm zproto.o
	-rm *.cc
	-rm *.hpp


