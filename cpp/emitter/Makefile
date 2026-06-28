CC=g++
CFLAG=-Wall -Wextra -O2 -g -std=c++11 -I../../

# the generator: main + header + body, linked against the C parser (../../zproto.c)
zproto: zproto_c.o main.cpp header.cpp body.cpp
	$(CC) $(CFLAG) -o zproto main.cpp header.cpp body.cpp zproto_c.o

zproto_c.o: ../../zproto.c
	gcc -Wall -g -O2 -c ../../zproto.c -o zproto_c.o

clean:
	-rm zproto zproto_c.o
