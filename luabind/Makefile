.PHONY:all clean

#---------compiler
CC := gcc -std=gnu99
LD := gcc
#-----------platform
PLATS=linux macosx
platform:
	@echo "'make PLATFORM' where PLATFORM is one of these:"
	@echo "$(PLATS)"
CCFLAG = -I../ -Ilua53/ -g -Wall -Wextra
LDFLAG := -lm -ldl

linux:LDFLAG += -Wl,-E -lrt
macosx:LDFLAG += -Wl,-no_compact_unwind
linux macosx:LDFLAG += -lpthread

linux:SHARED:=--share -fPIC
macosx:SHARED=-dynamiclib -fPIC -Wl,-undefined,dynamic_lookup

linux macosx:test zproto.so rand.so

zproto.so:../zproto.c lzproto.c
	$(CC) $(CCFLAG) $(SHARED) -o $@ $^
rand.so:lrand.c
	$(CC) $(CCFLAG) $(SHARED) -o $@ $^
test:main.c
	$(CC) $(CCFLAG) -o test main.c lua53/liblua.a $(LDFLAG)

clean:
	-rm *.o
	-rm zproto.so
	-rm rand.so
	-rm test

