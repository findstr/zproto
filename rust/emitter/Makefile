CXX=g++
CC?=cc
CXXFLAGS=-Wall -Wextra -O2 -g -std=c++0x -I../../
CFLAGS=-Wall -O2 -g -I../../

zproto-gen-rust: main.cpp zproto.o
	$(CXX) $(CXXFLAGS) -o zproto-gen-rust main.cpp zproto.o

zproto.o: ../../zproto.c
	$(CC) $(CFLAGS) -c ../../zproto.c -o zproto.o

clean:
	-rm zproto-gen-rust zproto.o
