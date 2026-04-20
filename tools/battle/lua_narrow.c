/* tools/battle/lua_narrow.c
 * Link against the zcc-compiled liblua.a in place of lua.c.
 * Produces raw fd-2 markers that survive any stdio/Lua state breakage.
 *
 * Build:
 *   zcc -I<lua-src> -o lua_narrow lua_narrow.c <lua-objs...> -lm -ldl
 *
 * Purpose (Step 1 of Lua stabilization trajectory):
 *   Mirror luaL_openlibs's loadedlibs[] exactly but bracket each call
 *   with direct write(2) syscalls so we know the last library entered
 *   before the crash, even if printf/stdio is what's broken.
 */
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <unistd.h>
#include <string.h>

static void mark(const char *s) {
    /* Direct syscall — bypass any broken libc/Lua state. */
    write(2, s, strlen(s));
    write(2, "\n", 1);
}

/* Mirror Lua's loadedlibs[] exactly, but call each one under our own watch. */
static const luaL_Reg narrow_libs[] = {
    {LUA_GNAME,        luaopen_base},
    {LUA_LOADLIBNAME,  luaopen_package},
    {LUA_COLIBNAME,    luaopen_coroutine},
    {LUA_TABLIBNAME,   luaopen_table},
    {LUA_IOLIBNAME,    luaopen_io},
    {LUA_OSLIBNAME,    luaopen_os},
    {LUA_STRLIBNAME,   luaopen_string},
    {LUA_MATHLIBNAME,  luaopen_math},
    {LUA_UTF8LIBNAME,  luaopen_utf8},
    {LUA_DBLIBNAME,    luaopen_debug},
    {NULL, NULL}
};

int main(void) {
    mark(">> creating state");
    lua_State *L = luaL_newstate();
    if (!L) { mark("!! luaL_newstate returned NULL"); return 1; }
    mark(">> state ok");

    const luaL_Reg *lib;
    for (lib = narrow_libs; lib->func; lib++) {
        mark(">> enter");
        mark(lib->name);
        luaL_requiref(L, lib->name, lib->func, 1);
        mark(">> exit");
        mark(lib->name);
        lua_pop(L, 1);
    }

    mark(">> all libs loaded");
    lua_close(L);
    mark(">> closed clean");
    return 0;
}
