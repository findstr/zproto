.PHONY:all clean

all:test zproto.so

zproto.so:zproto.c lzproto.c
	gcc -Ilua53/ -g -fPIC -shared -o $@ $^

test:zproto.c main.c
	gcc -Ilua53/ -g -o test zproto.c main.c lua53/liblua.a -lrt -lm -ldl -Wl,-E

clean:
	-rm zproto.so
	-rm test

