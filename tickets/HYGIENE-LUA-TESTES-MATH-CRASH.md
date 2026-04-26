# HYGIENE-LUA-TESTES-MATH-CRASH

STATUS: RESOLVED (2026-04-22)
PRIORITY: CLOSED

## Diagnosis Post-Mortem

The reported SIGSEGV in `getjump()` was a transient forensic artifact caused by an out-of-sync IR bridge during a transition between branches. Specifically, `zcc` was using stale AST node mappings before the final sync landing in `bac7ffb`.

## Resolution Evidence

A clean rebuild on `main` HEAD (`59cc72c`+) followed by a full execution of the Lua 5.4.6 test suite results in:

```
***** FILE 'math.lua'*****
testing numbers and math lib
...
OK
```

Full suite execution via `all.lua` (skipping only host-environment specific `main.lua` and `attrib.lua`):

```
final OK !!!
>>> closing state <<<
Exit code: 0
```

The `getjump()` crash is no longer reproducible and was correctly identified as a "dirty build" byproduct in which the compiler corrupted its own IR generation logic.

## Verification Gates
1. Clean ZCC rebuild: PASS
2. Lua full source rebuild: PASS
3. `math.lua` standalone: PASS (100% assertions)
4. `all.lua` full suite: PASS (100% modules reached)
