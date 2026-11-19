/*
 * ZCC Standalone Lua 5.4.6 WASI Driver (src/wasm_lua_driver.c)
 * ============================================================
 * Hosts upstream Lua 5.4.6 (`tests/embedded_lua/*.c`) atop ZCC's freestanding
 * WASI Preview1 runtime (`src/wasm_wasi_rt.c`). Supports:
 *   lua.wasm -e "<script>"
 *   lua.wasm <script.lua>
 *   lua.wasm   (reads script from stdin)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static char *_zcc_lua_read_stream(FILE *fp, size_t *out_len) {
    size_t cap = 4096U;
    size_t len = 0U;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (len + 1024U >= cap) {
            size_t ncap = cap * 2U;
            char *nb = (char *)realloc(buf, ncap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
            cap = ncap;
        }
        size_t nr = fread(buf + len, 1U, 1024U, fp);
        len += nr;
        if (nr < 1024U) break;
    }
    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}

static int _zcc_lua_panic(lua_State *L) {
    const char *msg = lua_tostring(L, -1);
    fprintf(stderr, "LUA_PANIC: %s\n", msg ? msg : "(no message)");
    fflush(stderr);
    return 0;
}

int main(int argc, char **argv) {
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "LUA_ERROR: luaL_newstate returned NULL\n");
        return 1;
    }
    lua_atpanic(L, _zcc_lua_panic);
    luaL_requiref(L, "_G", luaopen_base, 1);
    lua_pop(L, 1);

    const char *chunk_name = "=stdin";
    const char *src = NULL;
    size_t src_len = 0U;
    char *heap_buf = NULL;

    if (argc >= 3 && strcmp(argv[1], "-e") == 0) {
        chunk_name = "=(command line)";
        src = argv[2];
        src_len = strlen(src);
    } else if (argc >= 2 && strcmp(argv[1], "-") != 0) {
        FILE *fp = fopen(argv[1], "rb");
        if (!fp) {
            fprintf(stderr, "LUA_ERROR: cannot open script '%s'\n", argv[1]);
            lua_close(L);
            return 1;
        }
        heap_buf = _zcc_lua_read_stream(fp, &src_len);
        fclose(fp);
        if (!heap_buf) {
            fprintf(stderr, "LUA_ERROR: out of memory reading '%s'\n", argv[1]);
            lua_close(L);
            return 1;
        }
        src = heap_buf;
        chunk_name = argv[1];
    } else {
        heap_buf = _zcc_lua_read_stream(stdin, &src_len);
        if (!heap_buf) {
            fprintf(stderr, "LUA_ERROR: out of memory reading stdin\n");
            lua_close(L);
            return 1;
        }
        src = heap_buf;
    }

    int status = luaL_loadbufferx(L, src, src_len, chunk_name, NULL);
    if (heap_buf) { free(heap_buf); heap_buf = NULL; }
    if (status != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        fprintf(stderr, "LUA_SYNTAX_ERROR: %s\n", err ? err : "(unknown)");
        lua_close(L);
        return 2;
    }

    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        fprintf(stderr, "LUA_RUNTIME_ERROR: %s\n", err ? err : "(unknown)");
        lua_close(L);
        return 3;
    }

    fflush(stdout);
    lua_close(L);
    return 0;
}
