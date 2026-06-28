#include <stdio.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

int main(int argc, char *argv[])
{
	const char *script = argc > 1 ? argv[1] : "main.lua";
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	if (luaL_loadfile(L, script)) {
		fprintf(stderr, "load %s fail, %s\n", script, lua_tostring(L, -1));
		lua_close(L);
		return -1;
	}
	/* pass remaining argv as Lua function arguments */
	int nargs = argc - 2;
	for (int i = 2; i < argc; i++)
		lua_pushstring(L, argv[i]);
	if (lua_pcall(L, nargs, 0, 0)) {
		fprintf(stderr, "call %s fail, %s\n", script, lua_tostring(L, -1));
		lua_close(L);
		return -1;
	}

	lua_close(L);

	return 0;
}

