typedef struct lua_State lua_State;
typedef unsigned long size_t;
#define LUA_TSTRING 4
void tag_error(lua_State *L, int arg, int tag);
const char *lua_tolstring(lua_State *L, int arg, size_t *len);
const char *luaL_checklstring(lua_State *L, int arg, size_t *len) {
  const char *s = lua_tolstring(L, arg, len);
  if (!s) tag_error(L, arg, LUA_TSTRING);
  return s;
}
int main() { return 0; }
