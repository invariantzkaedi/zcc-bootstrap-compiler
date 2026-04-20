typedef struct lua_State lua_State;
typedef unsigned long size_t;
extern const char *(luaL_checklstring)(lua_State *L, int arg, size_t *l);
extern const char *(luaL_optlstring)(lua_State *L, int arg, size_t *l);

extern char *getenv(const char *);
extern void lua_pushstring(lua_State *L, const char *s);

static int os_getenv(lua_State *L) {
  lua_pushstring(L, getenv((luaL_checklstring(L, ( 1), ((void*)0)))));
  return 1;
}
