#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#define MAX     (8 * 1024)
#define MAXSEG  (64)
#define MIN(a, b)  (a > b ? b : a)

static int
randzero(char *buff, int sz)
{
        int zero = rand() % MAXSEG;
        sz = MIN(sz, zero);
        memset(buff, 0, sz);
        return sz;
}

static int
randdata(char *buff, int sz)
{
        int i;
        int zero = rand() % MAXSEG;
        sz = MIN(sz, zero);
        for (i = 0; i < sz; i++)
                buff[i] = rand();
        return sz;
}

static int
lrand(lua_State *L)
{
        int i = 0;
        int max = 0;
        while (max == 0)
                max = rand() % MAX;
        int ret = max;
        char buff[max];
        while (max) {
                int n;
                n = randzero(&buff[i], max);
                i += n;
                max -= n;
                n = randdata(&buff[i], max);
                i += n;
                max -= n;
        }
        lua_pushlstring(L, buff, ret);
        return 1;
}

int
luaopen_rand(lua_State *L)
{
        luaL_Reg tbl[] = {
                {"rand", lrand},
                {NULL, NULL},
        };
        srand(time(NULL));
        luaL_checkversion(L);
        luaL_newlib(L, tbl);
        return 1;
}



