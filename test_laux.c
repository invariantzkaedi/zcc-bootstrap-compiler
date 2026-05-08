#include <lua.h>
#include <lauxlib.h>
void test(lua_State *L, int arg, size_t *len) { const char *s = lua_tolstring(L, arg, len); }
