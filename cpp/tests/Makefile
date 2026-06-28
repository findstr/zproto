CC=g++
CFLAG=-Wall -Wextra -O2 -g -std=c++11 -I../runtime -I../..

GEN=../emitter/zproto
SCHEMA=hello.world.zproto
GENERATED=hello_world.hpp hello_world.message.hpp

# default: build generator, generate code, build both test binaries
all: test test_codec

# track emitter sources (and the C parser) so editing them forces an emitter
# rebuild -- otherwise a stale generator silently emits outdated code.
EMITTER_SRCS=../emitter/main.cpp ../emitter/header.cpp ../emitter/body.cpp \
	     ../emitter/header.h ../emitter/body.h ../../zproto.c
$(GEN): $(EMITTER_SRCS)
	$(MAKE) -C ../emitter

$(GENERATED): $(GEN) $(SCHEMA)
	./$(GEN) $(SCHEMA)

# functional round-trip: test.cpp + runtime + generated headers
test: $(GENERATED) test.cpp ../runtime/zproto.cpp
	$(CC) $(CFLAG) -o test test.cpp ../runtime/zproto.cpp -lpthread

# oracle: links the C reference (../../zproto.c, compiled as C) for byte-compat.
# zproto.c must be compiled with gcc (it uses `new` as an identifier and other C
# constructs that are illegal/error under C++).
zproto_c.o: ../../zproto.c
	gcc -Wall -g -O2 -c ../../zproto.c -o zproto_c.o

# test_codec.cpp lives in `namespace zproto` (the C++ runtime) and so CANNOT
# include zproto.h directly: the C reference declares an opaque `struct zproto`
# at global scope, which collides with the runtime's `namespace zproto`. oracle
# .{hpp,cpp} is the single TU that includes zproto.h and bridges the C reference
# to the C++ POD types (hello::world::packet).
test_codec: $(GENERATED) test_codec.cpp oracle.cpp ../runtime/zproto.cpp zproto_c.o
	$(CC) $(CFLAG) -o test_codec test_codec.cpp oracle.cpp ../runtime/zproto.cpp \
		zproto_c.o -lpthread

run: all
	./test && ./test_codec

clean:
	-rm test test_codec zproto_c.o $(GENERATED)
