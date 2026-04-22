# HYGIENE-LUA-TESTES-MATH-CRASH

**Parent:** HYGIENE-LUA-TESTES
**Priority:** High
**Status:** Open

## Description
The Lua 5.4.6 test suite (`testes/`) fails to execute under the ZCC-compiled `lua` runtime at `HEAD` (post-16d2af4). 

*Note on earlier reports: `basic.lua` does not exist in the Lua 5.4.6 test suite directory; the execution harness simply failed to find it. The first actual test script to trigger a crash is `math.lua`.*

Execution of `math.lua` (and sequentially `strings.lua`, `goto.lua`, `constructs.lua`, `bitwise.lua`, `closure.lua`, `calls.lua`, `sort.lua`) results in an immediate segmentation fault (core dumped). 

## Backtrace Isolation

A preliminary `gdb` trace on `math.lua` maps the crash to `getjump()` propagating through `patchlistaux()` in `lcode.c` during AST generation logic.

```
Program received signal SIGSEGV, Segmentation fault.
0x00000000004127fe in getjump ()
#0  0x00000000004127fe in getjump ()
#1  0x0000000000413277 in patchlistaux ()
#2  0x00000000004161fc in exp2reg ()
#3  0x000000000041631f in luaK_exp2nextreg ()
#4  0x00000000004468ee in funcargs ()
#5  0x0000000000446ce1 in suffixedexp ()
#6  0x0000000000449ee1 in exprstat ()
#7  0x000000000044a755 in statement ()
#8  0x00000000004456b8 in statlist ()
#9  0x000000000044a964 in mainfunc ()
#10 0x000000000044ad72 in luaY_parser ()
...
#27 0x00000000004621e4 in main ()
```

## Next Steps
Minimization, reproducer isolation, and root-cause diagnosis of the `getjump()` path are required before HYGIENE-LUA-TESTES can make further progress.
