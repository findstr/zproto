.PHONY:all clean

all:test zprotoparser.so

zprotoparser.so:zproto.c lzproto.c
	gcc -Ilua53/ -g -fPIC -shared -o $@ $^

test:zproto.c main.c
	gcc -Ilua53/ -g -o test zproto.c main.c lua53/liblua.a -lrt -lm -ldl -Wl,-E

clean:
	-rm zprotoparser.so
	-rm test

