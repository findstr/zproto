CC=g++
CCFLAG=-Wall -Wextra -O2 -g -std=c++0x -I../

zproto:zproto.o header.cpp body.cpp
	$(CC) $(CCFLAG) -o zproto proto.cpp header.cpp body.cpp zproto.o

# Runtime link is zproto.c-free: encoder/decoder/pack/unpack live in zprotowire.o.
# zproto.o stays linked into the generator (zproto) and the oracle (test_codec).
test:zprotowire.o test.cpp
	$(CC) $(CCFLAG) -o test test.cpp hello_world.cc zprotowire.o -lpthread

test_codec:zprotowire.o zproto.o test_codec.cpp
	$(CC) $(CCFLAG) -o test_codec test_codec.cpp zprotowire.o zproto.o -lpthread

libs: zprotowire.o
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


