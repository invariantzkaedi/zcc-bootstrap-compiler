# CG-PARSE-001: GCC-style `__asm__("name")` Symbol Rename Clause

**Status:** RESOLVED — B1 LOCKED (parse-and-discard) · **B2 COMPLETE (honor-rename)**  
**Validation:** `_sweep_output/B2_VALIDATION.md`

---

## 1. Discovery Context

ZCC failed to compile programs including glibc headers when functions like `fscanf`
use the GCC extension `extern int fscanf(...) __asm__("" "__isoc99_fscanf")`. ZCC
mis-classified the declaration as a function definition missing braces, then fed the
`__asm__("...")` token to `parse_stmt` which parsed it as inline assembly, emitting
the raw string `__isoc99_fscanf` as an x86 opcode — causing assembler rejection.

**Error:** `a.out.s: Error: no such instruction: '__isoc99_fscanf'`

---

## 2. Root Cause

`parse_func_def`'s prototype-vs-definition decision: *if `;`, prototype; else, definition.*
When `TK_ASM` appears between `)` and `;`, this condition misfires.

Additionally, `parse_program` hand-rolls identifier parsing for primary global variables
and never calls `parse_declarator` — so a general `parse_declarator`-level fix would
miss those paths.

---

## 3. Blast Radius (Sweep S3)

All major glibc headers contain `__REDIRECT` aliases: `<stdio.h>`, `<wchar.h>`,
`<unistd.h>`, `<sys/stat.h>`, `<fcntl.h>`, `<string.h>`, `<dirent.h>`, `<sys/socket.h>`,
`<signal.h>`, `<time.h>`, `<bits/*>` — 45+ rename triggers across 11 headers.

---

## 4. Phase B1 Patch — Three Insertion Sites

`parse_declarator` wrapper was insufficient alone. Three separate grammar paths each
route through distinct code paths that hand-roll identifier parsing without going through
`parse_declarator`.

| Site | Location | Grammar Path |
|------|----------|-------------|
| **#1** | `parse_declarator` wrapper | Multi-declarator lists; local `extern` inside function bodies |
| **#2** | `parse_func_def` after `expect(cc, TK_RPAREN)` | Function prototypes and definitions |
| **#3** | `parse_program` after `after_name:` label | Primary global variables; typedefs |

Each site consumes `TK_ASM ( <string>... )` and discards — no rename stored. B2 will
promote this to capture-and-honor.

### Patch pattern (all three sites identical):
```c
/* CG-PARSE-001: Consume optional GCC __asm__("name") rename.
 * Phase B1: parse and discard. */
if (cc->tk == TK_ASM) {
    next_token(cc);
    expect(cc, TK_LPAREN);
    while (cc->tk == TK_STR) next_token(cc);
    expect(cc, TK_RPAREN);
}
```

---

## 5. Amendment A — `__attribute__` at parser layer

`part0_pp.c:583–631` strips `__attribute__((…))` at preprocessor level. `TK_ATTRIBUTE`
never reaches the parser. The narrow `TK_ASM`-only fix is correctly scoped.

---

## 6. Validation Matrix (Amendment D, 9 probes × 4 configs)

Reference: `/_sweep_output/B1_VALIDATION.md`

| Probe | Description | ALL-3 |
|-------|-------------|-------|
| S2.1 | `extern int foo(int) __asm__("bar")` | ✓ PARSE-OK |
| S2.2 | `extern int foo __asm__("bar")` | ✓ PARSE-OK |
| S2.3 | `typedef int foo __asm__("bar")` | ✓ PARSE-OK |
| S2.4 | `int foo(int) __asm__("bar") { }` | ✓ PARSE-OK |
| S2.6 | GLIBC `__REDIRECT` pattern | ✓ PARSE-OK |
| S2.7 | `int a, b __asm__("b_renamed")` | ✓ PASS(exit=3) |
| S2.8 | local `extern int foo __asm__("bar")` | ✓ PASS(exit=1) |
| S2.9 | `void (*fp __asm__("fp_renamed"))(void)` | ✗ FAIL-PARSE → CG-PARSE-002 |

**Site-to-probe ownership** (confirmed by single-site isolation runs):

| Site | Exclusively Resolves |
|------|---------------------|
| Site #1 | S2.7 (multi-decl), S2.8 (local extern) |
| Site #2 | S2.1 (func proto), S2.4 (func def) |
| Site #3 | S2.2 (extern var), S2.3 (typedef) |

Self-host: ✓ `zcc2.s == zcc3.s` across all 4 configurations (ALL-3, each site in isolation).

---

## 7. Phase B2 Scope

**Goal:** Honor the rename — `foo __asm__("bar")` → emit label `bar:` not `foo:`.

**Storage:** `Symbol.asm_name[MAX_IDENT]` field already exists (`part1.c:189`).  
`cc_alloc` zeroes via arena — `asm_name[0] == 0` is the "no rename" sentinel.  
There is already a consumer at `part3.c:927–928` that copies `sym->asm_name` into
`node->name` if set. However, `part4.c` does not reference `asm_name` at all.

**Required B2 changes:**
1. `part3.c` Site #1, #2, #3: capture string instead of discarding
2. `part4.c`: For `.globl`, label emission (`func->func_def_name`, `gvar->name`), 
   prefer `asm_name` when non-empty

See `bug_corpus/CG-PARSE-002.md` for deferred S2.9 case.
