#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#define MAX     (64 * 1024)
#define MAXSEG  (128)
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
rand2(lua_State *L)
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

static int
rand0(lua_State *L)
{
        int i = 0;
        int max = 0;
        while (max == 0)
                max = rand() % MAX;
        char buff[max];
        for (i = 0; i < max; i++) {
                buff[i] = rand();
        }
        lua_pushlstring(L, buff, max);
        return 1;
}

static int
rand1(lua_State *L)
{
        int max = 0;
        while (max == 0)
                max = rand() % MAX;
        char buff[max];
        memset(buff, max, 0);
        lua_pushlstring(L, buff, max);
        return 1;
}

static int
lrand(lua_State *L)
{
        int type = luaL_checkinteger(L, 1);
        switch (type) {
        case 0:
                return rand0(L);
        case 1:
                return rand1(L);
        case 2:
                return rand2(L);
        default:
                return 0;
        }
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



