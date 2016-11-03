#include <stdio.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

int main()
{
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	if (luaL_loadfile(L, "main.lua") || lua_pcall(L, 0, 0, 0)) {
		fprintf(stderr, "call main.lua fail, %s\n", lua_tostring(L, -1));
		lua_close(L);
		return -1;
	}

	lua_close(L);

	return 0;
}

