# P0.6 — Static-Local Name Mangling Pipeline Trace

**Date:** 2026-04-19  
**Probe:** `/mnt/g/suno/static_probe.c`  
**Verdict: ZERO**

---

## Section A — Emission Pattern

### Labels at definition

| C source | Emitted label | Section |
|----------|--------------|---------|
| `static int x = 0;` inside `counter()` | `__x__100:` | `.data` |
| `static int x = 100;` inside `other()` | `__x__101:` | `.data` |

No plain `x:` label appears anywhere in the output. Both definitions use the
fully mangled name. Two distinct statics with the same C identifier `x` in
different scopes produce two distinct mangled labels — no collision.

### References at use sites

| Function | Reference in .s |
|----------|----------------|
| `counter()` | `leaq __x__100(%rip), %rax` |
| `other()` | `leaq __x__101(%rip), %rax` |

References use the mangled name, not `x`. No plain `x(%rip)` anywhere.

### Execution result

```
./static_probe; echo "Exit code: $?"
Exit code: 104
```

Expected: `1 + 2 + 101 = 104`. Correct.

---

## Section B — Classification

### VERDICT ZERO

Both definition labels and all references use the mangled name.  
The pipeline is **complete and correct end-to-end**.

**Propagation path verified:**

1. `parse_stmt` (line 2267): writes `foo__x__N` into `sym->asm_name`
2. `parse_stmt` (line 2270): writes `foo__x__N` into `gvar->name` — this is what the global var node emits in the `.data` section label
3. `parse_primary` (line 927–928): when a `ND_VAR` reference to `x` is resolved, `sym->asm_name[0]` is set, so `n->name` is overwritten with `foo__x__N`
4. `codegen_expr` / `ND_VAR` path (part4.c:432): emits `leaq %s(%%rip)` using `node->name` — which is already the mangled name by step 3

**Part4.c is never consulted for `sym->asm_name` directly** — the substitution happens at the AST node level in `parse_primary`, and `part4.c` just emits whatever `node->name` contains.

---

## Section C — B2 Codegen Work Inheritance

Because VERDICT ZERO was reached, the static-local mangling pipeline extends
directly to the GCC `__asm__` rename case with **zero additional part4.c changes**,
provided B2 is implemented correctly at the parser layer.

Specifically:

- **For variable references (ND_VAR nodes):** Once `sym->asm_name` is populated
  by the B2 parser sites, `parse_primary` at line 927–928 automatically substitutes
  the alias into `node->name`. `part4.c` emits `node->name` unchanged. Zero new
  part4.c code needed for variable reference sites.

- **For global variable label definitions:** The static-local pattern works because
  `gvar->name` is set to the mangled name directly (line 2270). For B2 to work with
  `extern int foo __asm__("bar")`, the symbol's `asm_name` must be written at Site #3
  AND the `gvar->name` must be set from it as well. Specifically, line 3062:
  `strncpy(gvar->name, name, MAX_IDENT - 1)` must instead use the alias if present.
  This is **one line in part4.c** — actually it is in part3.c (the ND_GLOBAL_VAR
  node construction), not in the codegen.

- **For function label definitions (part4.c:3489, 3491):** `func->func_def_name`
  on the Node is set during `parse_func_def` and is not routed through `parse_primary`.
  The 927–928 path does not cover functions. **These two sites in part4.c still require
  B2 changes** — look up symbol by `func->func_def_name`, check `sym->asm_name`,
  substitute. This was already identified in P0.2 Group A.

---

## Updated B2 Part4.c Patch Surface

| Site | File | Change Needed? |
|------|------|---------------|
| Variable reference (`leaq %s(%%rip)`) | part4.c:432 | **NO** — handled by parse_primary 927–928 |
| Variable reference (`gname` for call) | part4.c:435 | **NO** — same path |
| Global var `.globl` | part4.c:3811 | **NO** — handled by gvar->name (set in part3.c) |
| Global var label `%s:` | part4.c:3813 | **NO** — same |
| Global var `.local` / `.comm` | part4.c:3966–3967 | **NO** — same |
| Function `.globl` | part4.c:3489 | **YES** — `func->func_def_name` bypasses 927–928 |
| Function label `%s:` | part4.c:3491 | **YES** — same |

**Total new part4.c lines: 2 sites, ~8 lines** (scope_find + asm_name check + emit).

**Total new part3.c lines: 3 parser sites + gvar->name assignment** (capture string, write-back).

---

## B2 Scope — Final Shape

Based on P0.1 through P0.6:

### Parser changes (part3.c)

| Site | Change |
|------|--------|
| Site #1 `parse_declarator` | Clear `cc->pending_asm_name` on entry; populate if `TK_ASM` seen |
| Site #2 `parse_func_def` | Capture string into `char buf[]`; `strncpy(fsym->asm_name, buf, ...)` |
| Site #3 `parse_program` | Capture string into `char asm_alias[]`; write into `sym->asm_name` after each `scope_add`; also set `gvar->name` from alias if set |
| Callers of `parse_declarator` | After each `scope_add`/`scope_add_local`, pickup `cc->pending_asm_name` if set, write into `sym->asm_name`, clear `cc->pending_asm_name` |

### Struct change (part1.c or part2.c where Compiler is defined)

Add `char pending_asm_name[MAX_IDENT]` to `Compiler` struct for the Site #1 stash.

### Codegen changes (part4.c)

Site `3489` (function `.globl`) and `3491` (function label):

```c
/* B2: prefer asm alias for function label if symbol declares one */
{
    Symbol *fsym = scope_find(cc, func->func_def_name);
    const char *emit_name = (fsym && fsym->asm_name[0])
                            ? fsym->asm_name
                            : func->func_def_name;
    fprintf(cc->out, "    .globl %s\n", emit_name);
    fprintf(cc->out, "%s:\n", emit_name);
}
```

### Regression gate

S2.1 link test: `extern int foo(int) __asm__("bar")` + provider `int bar(int x) { return x; }` → `./probe1; echo $?` must print `42`.

Full Amendment D probe matrix must still pass after B2.
