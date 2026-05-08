#include <lua.h>
#include <lauxlib.h>
void tag_error(lua_State *L, int arg, int tag);
const char *luaL_checklstring(lua_State *L, int arg, size_t *len) {
  const char *s = lua_tolstring(L, arg, len);
  if (!s) tag_error(L, arg, LUA_TSTRING);
  return s;
}
