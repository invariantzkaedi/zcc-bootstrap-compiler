typedef struct lua_State lua_State;
typedef unsigned long size_t;
const char *lua_tolstring(lua_State *L, int arg, size_t *len);
void test(lua_State *L, int arg, size_t *len) { const char *s = lua_tolstring(L, arg, len); }
int main() { return 0; }
