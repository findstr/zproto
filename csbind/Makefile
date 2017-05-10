.PHONY: all

all:zproto zproto.dll

zproto:proto.cpp body.cpp body.h zproto.o
	g++ -g -std=c++0x -I../ -o $@ $^

zproto.o:../zproto.c
	gcc -Wall -g -c ../zproto.c

zproto.dll:cszproto.c ../zproto.c
	gcc -g -I../ -shared -o $@ $^

test:zprotowire.cs zprotodll.cs test_zproto.cs main.cs
	csc /out:$@ $^

clean:
	-rm test
	-rm zproto
	-rm *.exe
	-rm *.o
	-rm zproto.dll
	-rm test_zproto.cs
