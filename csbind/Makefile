.PHONY: win, macosx

win: zproto zproto.dll

macosx:zproto zproto.so

zproto:proto.cpp body.cpp zproto.o
	g++ -g -std=c++0x -I../ -o $@ $^

zproto.o:../zproto.c
	gcc -Wall -g -c ../zproto.c

zproto.dll:cszproto.c ../zproto.c
	gcc -DWIN32 -g -I../ -shared -o $@ $^

zproto.so:cszproto.c ../zproto.c
	gcc -m32 -g -I../  -Wl,-undefined,dynamic_lookup -o $@ $^

test:zprotowire.cs zprotodll.cs hello_world.cs main.cs
	csc /out:$@ $^

clean:
	-rm test
	-rm zproto
	-rm *.exe
	-rm *.o
	-rm *.so
	-rm zproto.dll
	-rm hello_world.cs
