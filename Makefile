.PHONY:all clean

#---------compiler
CC := gcc -std=gnu99
LD := gcc

#---------

BUILD_PATH ?= .
TARGET ?= silly

#-----------platform
PLATS=linux macosx
platform:
	@echo "'make PLATFORM' where PLATFORM is one of these:"
	@echo "$(PLATS)"
CCFLAG = -g -Wall
LDFLAG := -lm -ldl

linux:CCFLAG += -D__linux__
macosx:CCFLAG += -D__macosx__

linux:LDFLAG += -Wl,-E -lrt
macosx:LDFLAG += -Wl,-no_compact_unwind 
linux macosx:LDFLAG += -lpthread

linux:SHARED:=--share -fPIC
macosx:SHARED=-dynamiclib -fPIC -Wl,-undefined,dynamic_lookup

linux macosx:test zproto.so rand.so

zproto.so:zproto.c lzproto.c
	$(CC) -Ilua53/ $(CCFLAG) $(SHARED) -o $@ $^
rand.so:lrand.c
	$(CC) -Ilua53/ $(CCFLAG) $(SHARED) -o $@ $^
test:zproto.c main.c
	$(CC) -Ilua53/ $(CCFLAG) -o test zproto.c main.c lua53/liblua.a $(LDFLAG)

clean:
	-rm zproto.so
	-rm test

