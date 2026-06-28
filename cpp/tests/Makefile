CC=g++
CFLAG=-Wall -Wextra -O2 -g -std=c++11 -I../runtime

GEN=../emitter/zproto
SCHEMA=hello.world.zproto
GENERATED=hello_world.hpp hello_world.message.hpp

# default: build generator, generate code, build both test binaries
all: test test_codec

$(GEN):
	$(MAKE) -C ../emitter

$(GENERATED): $(GEN) $(SCHEMA)
	./$(GEN) $(SCHEMA)

# functional round-trip: test.cpp + runtime + generated headers
test: $(GENERATED) test.cpp ../runtime/zproto.cpp
	$(CC) $(CFLAG) -o test test.cpp ../runtime/zproto.cpp -lpthread

# oracle: links the C reference (../../zproto.c) for byte-compat
test_codec: $(GENERATED) test_codec.cpp ../runtime/zproto.cpp
	$(CC) $(CFLAG) -o test_codec test_codec.cpp ../runtime/zproto.cpp \
		../../zproto.c -lpthread

run: all
	./test && ./test_codec

clean:
	-rm test test_codec $(GENERATED)
