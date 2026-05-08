#include <lua.h>
#include <lauxlib.h>
const char *test() { return luaL_optstring(0, 0, 0); }
