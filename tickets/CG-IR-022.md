# CG-IR-022 — ret_type emits as "ptr" for all functions

**Status:** ✅ **RESOLVED** 2026-05-03
**Severity:** Cosmetic for x86 codegen · LOAD-BEARING for IR dataset consumers
**Filed:** 2026-05-03
**Closed:** 2026-05-03 (same session, single-paste verification)
**Discovered during:** IR_SCHEMA.md v1.0.0 lock session

---

## Symptom

The `; func <name> -> <type>` header in text IR (and the `ret_type` field in
the normalized JSON) always emitted `ptr`, regardless of the function's
actual return type. Per-instruction types (`IR_RET` carrying `IR_TY_I32`)
were correct.

## Evidence (pre-fix, captured 2026-05-03)

Probe input: `int add(int,int)`, `int loop(int)`, `int main(int,char**)` —
all `int`-returning.

```
; func add  -> ptr             <-- WRONG (was i32)
; func loop -> ptr             <-- WRONG (was i32)
; func main -> ptr             <-- WRONG (was i32)
  ret  i32  -  %t5  -  -       <-- per-instr types CORRECT
```

JSON: `"ret_type": "ptr"` × 3.

## Root cause

`ir_bridge.h:133` called `ir_map_type(func->func_type)`. The argument was
the function's *own* type, kind `TY_FUNC`. `ir_map_type` correctly maps
`TY_FUNC → IR_TY_PTR` per its "pointers/arrays/function-pointers/structs"
branch, so every function emitted `ptr`. The intent was the **return type**.

`struct Type` (part1.c:181-201) has the field as `Type *ret;` — the
function-kind variant uses `ret`, `params`, `num_params`, `is_variadic`.
Confirmed by `parse_func_def` (part3.c:3148) which calls
`type_func(cc, ret_type)` to build the function type from its return.

## Fix (commit pending)

```c
// ir_bridge.h:133
- ZCC_IR_FUNC_BEGIN(func->func_def_name, ir_map_type(func->func_type),
+ ZCC_IR_FUNC_BEGIN(func->func_def_name, ir_map_type(func->func_type->ret),
                   func->num_params);
```

Single-character semantic change (`->ret` appended). Diff: 1 line in 1 file.

## Verification

| Gate | Result |
|---|---|
| Pre-fix bootstrap (`cmp zcc2.s zcc3.s`) | ✅ GREEN |
| Match count for surgical replace | ✅ 1 (unambiguous) |
| Post-fix rebuild | ✅ 23s clean build |
| Post-fix bootstrap (`cmp zcc2.s zcc3.s`) | ✅ GREEN |
| Peephole elisions (sanity, expect identical) | ✅ 8,381 (same as pre-fix) |
| Probe text IR `; func * -> i32` | ✅ 3 / 3 |
| Probe JSON `"ret_type": "i32"` | ✅ 3 / 3 |

## Impact summary

- x86 codegen: zero change (proven by bootstrap-equivalence GREEN).
- IR text format: function headers now report correct return type.
- IR JSON normalization: `ret_type` field now correct.
- Dataset implication: any model trained on `ret_type` from corpora
  generated **before this fix** has been training on a constant. Models
  that didn't use the field as a feature are unaffected. Future emissions
  carry correct type information.

## Files touched

- `ir_bridge.h` (1 line)

## Closure criteria — all met

- [x] Pre-fix and post-fix baseline gates both GREEN
- [x] Probe shows `i32` for all three int-returning functions in text and JSON
- [x] Bootstrap-equivalence preserved
- [x] Single-line surgical edit, no collateral changes
- [ ] `IR_SCHEMA.md` §4 CG-IR-022 warning marker removed (pending commit)
- [ ] Commit and tag with SHA appended below

## Resolution

(commit SHA pending push)
