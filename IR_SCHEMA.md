# ZCC IR Schema

**Version:** `1.0.0` — LOCKED 2026-05-03
**Baseline:** `selforglinux/` tree as of Apr 14 2026 (`ir_emit_dispatch.h`)
**Lock evidence:** `/tmp/ir_probe.c → /tmp/ir_emit.txt` (60 lines, 3 funcs) +
                   `/tmp/ir_emit.json` (7222 bytes, list of 3)
**Source-of-truth files:** `ir.h:116-130`, `ir_bridge.h:64-115`,
                           `ir_emit_dispatch.h:36-52`, `part5.c:419-440`,
                           `ir.c:97-113` (`ir_type_name`), `ir.c:318` (text emit)

**PARTS concat order** (from Makefile): `part1.c part0_pp.c part2.c part2b_layout.c part3.c ir.h ir_emit_dispatch.h ir_bridge.h part4.c part5.c part6_arm.c ir.c ir_to_x86.c ir_pass_manager.c regalloc.c ir_telemetry_stub.c`. Macros defined in `ir_bridge.h` are visible in `ir.c` without #include.

---

## 0. Purpose & scope

This schema governs the **text-IR persistence format** that ZCC emits via
`ir_module_emit_text` (`ir.c:276`). Two distinct artifacts use the IR
infrastructure; only the first is governed here:

| Artifact | Trigger | Format | Schema |
|---|---|---|---|
| **Text IR module** | `ZCC_EMIT_IR=1` | flat text to stdout | **THIS DOC** |
| Pass telemetry | `--ir --telemetry` | JSONL events to stdout | (separate, `IR_CORPUS_README.md`) |
| In-memory IR | always (when `g_emit_ir=1`) | `ir_node_t` linked list | not persisted, no schema |

The HF dataset `zkaedi/zcc-ir-prime-v1` is a **source corpus** (uploads
`part*.c`, `ir.c`, `compiler_passes.c`, etc.) — it does not directly contain
text IR. Consumers regenerate IR from sources using this schema.

---

## 1. Versioning policy

Semver. Bump rules:

- **PATCH** — additive optional fields (e.g. new `extra` keys), no removal.
- **MINOR** — new opcodes, new types in `ir_type_t`, new optional sections.
- **MAJOR** — opcode rename or removal, type-encoding change, structural
  reorganization (e.g. wrapping the function list under a top-level dict).

The version constant lives in `ir_bridge.h`:

```c
/* IR persistence schema version. See IR_SCHEMA.md. */
#define ZCC_IR_VERSION_MAJOR 1
#define ZCC_IR_VERSION_MINOR 0
#define ZCC_IR_VERSION_PATCH 0
#define ZCC_IR_VERSION       "1.0.0"
```

The text-IR emitter prepends the version as the first IR-content line of
the module header so consumers can detect drift. Implemented in `ir.c`
(`ir_module_emit_text`):

```c
fprintf(fp, "; ZCC IR v%s\n", ZCC_IR_VERSION);
fprintf(fp, "; ZCC IR module  funcs=%d\n\n", mod->func_count);
```

Implemented 2026-05-03. The version macro lives in `ir_bridge.h`, which
precedes `ir.c` in the PARTS concat order, so no include is required.

---

## 2. Invocation (canonical)

```bash
ZCC_EMIT_IR=1 ./zcc <source.c> -o <out>.s > <out>.ir
```

- `<out>.s` receives x86-64 assembly (unchanged behavior).
- Stdout receives 5 phase banners followed by the IR module followed by
  Phase 5/6 banners. Consumers MUST filter banners by line prefix:
  - Banner lines start with `[Phase`, `[OK]`, or are blank-after-banner.
  - IR lines start with `;` (comments/headers), two spaces (instructions),
    or are blank between functions.

⚠️ **Skill correction needed.** `zkaedi-zcc-ir-bridge/SKILL.md v1.0.4` lines
53–58 document `ZCC_IR_BACKEND=1 ZCC_IR_FLUSH=1` as the IR-emission path.
Verified 2026-05-03: that combination produces NO IR text — backend mode
drains `g_ir_module` per-function (OOM mitigation). The `--ir` flag also
does NOT emit text — it sets `g_ir_primary=1` which gates OUT the FLUSH
call at part5.c:571. Only `ZCC_EMIT_IR=1` produces text IR.

---

## 3. Architectural invariants [VERIFIED]

- **3-address SSA-style.** Each `ir_node_t` has at most one destination,
  three source slots (`src1`, `src2`, `src3`), one immediate (`imm`).
- **Flat function list.** No nested scopes; functions emit sequentially.
- **No basic blocks in text form.** Control flow lives on `IR_label` /
  `IR_br` / `IR_br_if` opcodes referencing `.L<N>` labels. Block structure
  is reconstructed by consumers from those.
- **Named temporaries** `%t0`, `%t1`, … allocated by `ir_bridge_fresh_tmp()`,
  reset per function. `%t0` in `add` and `%t0` in `loop` are unrelated.
- **Stack references** as `%stack_<offset>` where `<offset>` is the negative
  byte offset from `%rbp` (e.g. `%stack_-8`).
- **LP64 ABI.** All slots 8 bytes (CG-IR-010). Pointers, `long`, `long long`
  all map to `IR_TY_I64` or `IR_TY_PTR`.

---

## 4. Type system [LOCKED, ir.h:116-130]

```c
typedef enum {
    IR_TY_VOID = 0,
    IR_TY_I8,     IR_TY_I16,    IR_TY_I32,    IR_TY_I64,
    IR_TY_U8,     IR_TY_U16,    IR_TY_U32,    IR_TY_U64,
    IR_TY_PTR,
    IR_TY_F32,    IR_TY_F64,    /* RESERVED, not emitted in P1 */
    IR_TY_COUNT
} ir_type_t;
```

### Text-form type names [LOCKED, ir.c `ir_type_name`]

| Enum | Text |
|---|---|
| `IR_TY_VOID` | `-` (or omitted for control ops) |
| `IR_TY_I8/I16/I32/I64` | `i8/i16/i32/i64` |
| `IR_TY_U8/U16/U32/U64` | `u8/u16/u32/u64` |
| `IR_TY_PTR` | `ptr` |
| `IR_TY_F32/F64` | `f32/f64` (reserved) |

### `ir_map_type` C-type → IR-type mapping [LOCKED]

| C type | IR type |
|---|---|
| `void` | `IR_TY_VOID` |
| `char`, `unsigned char` | `IR_TY_I8`, `IR_TY_U8` |
| `short`, `unsigned short` | `IR_TY_I16`, `IR_TY_U16` |
| `int`, `unsigned int` | `IR_TY_I32`, `IR_TY_U32` |
| `long`, `long long` (signed) | `IR_TY_I64` |
| `unsigned long`, `unsigned long long` | `IR_TY_U64` |
| pointer, array, function-pointer, struct | `IR_TY_PTR` |
| enum | `IR_TY_I32` |
| (null Type, unknown kind) | `IR_TY_I64` (safety fallback) |

**CG-IR-022 RESOLVED 2026-05-03.** Earlier emissions had a bug where the
function-level `ret_type` always emitted as `ptr` (caused by passing the
function's TY_FUNC type rather than its return type to `ir_map_type`).
Fixed at `ir_bridge.h:133` — now correctly uses `func->func_type->ret`.
Bootstrap-equivalence preserved. Pre-fix corpora may have constant
`ret_type=ptr` and should be regenerated if that field is load-bearing.

---

## 5. Opcode reference [LOCKED]

### 5.1 Core opcodes

| Text | JSON `op` | Operands | Notes |
|---|---|---|---|
| `addr`   | `IR_addr`  | `dst src1 - -`         | Take address of stack slot |
| `load`   | `IR_load`  | `dst src1 - -`         | 8-byte movq (CG-IR-010) |
| `store`  | `IR_store` | `dst src1 - -`         | 8-byte movq (CG-IR-010) |
| `const`  | `IR_const` | `dst - - - imm=N`      | Immediate load |
| `arg`    | `IR_arg`   | `- src1 - -`           | Set up call arg |
| `call`   | `IR_call`  | `dst - - <callee>`     | Function call; src3 = name |
| `ret`    | `IR_ret`   | `- src1 - -`           | Return; src1 = retval |
| `label`  | `IR_label` | `- - - - .L<N>`        | Branch target |
| `br`     | `IR_br`    | `- - - - .L<N>`        | Unconditional jump |
| `br_if`  | `IR_br_if` | `- src1 - .L<N>`       | Cond branch on src1 |

### 5.2 Binary ops [LOCKED — 16/16, ir_bridge.h:151–181]

`IR_add`, `IR_sub`, `IR_mul`, `IR_div`, `IR_mod`,
`IR_band`, `IR_bor`, `IR_bxor`,
`IR_shl`, `IR_shr`,
`IR_eq`, `IR_ne`, `IR_lt`, `IR_le`, `IR_gt`, `IR_ge`.

All emit `<op> <type> <dst> <src1> <src2> -`.
Mapped from AST `ND_*` kinds via `ir_map_binop` (`ir_bridge.h:149`).

### 5.3 Reserved / partial

- Float opcodes — `IR_TY_F32`/`F64` reserved; no float ops emitted in P1.
- Struct opcodes — structs map to `IR_TY_PTR` (address-pass); no
  `IR_struct_*` opcodes observed.
- PHI — `OP_PHI` exists in-memory (CG-IR-005) but did NOT appear in probe
  output. Text emission may collapse PHIs into edge-copy form. UNVERIFIED
  for full SSA programs; revisit if a probe with multi-block PHI emerges.

---

## 6. Text grammar [LOCKED]

```
module      ::= module_header  func+
module_header ::= "; ZCC IR v" version "\n"                  (mandatory since 2026-05-03)
                  "; ZCC IR module  funcs=" int "\n" "\n"

func        ::= func_header  instr*  func_footer
func_header ::= "; func "  name  " -> "  type_name  "\n"
func_footer ::= "; end "   name  " nodes=" int "\n" "\n"

instr       ::= "  " op "  " type "  " dst "  " src1 "  " src2 "  " src3 [ imm ] "  ; line " int "\n"

op          ::= "addr" | "load" | "store" | "const" | "arg" | "call"
              | "ret"  | "label" | "br" | "br_if"
              | "add"  | "sub"   | "mul" | "div" | "mod"
              | "band" | "bor"   | "bxor"
              | "shl"  | "shr"
              | "eq" | "ne" | "lt" | "le" | "gt" | "ge"

type        ::= "-" | "i8"|"i16"|"i32"|"i64" | "u8"|"u16"|"u32"|"u64"
              | "ptr" | "f32"|"f64"

dst, src1, src2, src3 ::= reg | stack_ref | label_ref | name | "-"
reg         ::= "%t"     int        (e.g. %t0, %t12)
stack_ref   ::= "%stack_-" int      (e.g. %stack_-8)
label_ref   ::= ".L"     int        (e.g. .L102)
imm         ::= "imm="   int        (IR_const only)
```

Field separator is 2 spaces. Trailing `; line N` is mandatory.

---

## 7. JSON normalization [LOCKED, `convert_ir_to_json.py`]

Top level is a JSON list. Each element:

```jsonc
{
  "func":      "<name>",
  "ret_type":  "<type_name>",        /* CG-IR-022 fixed — now reports actual return type */
  "instructions": [
    {
      "op":    "IR_<lowercase>",
      "type":  "IR_TY_<UPPER>" | "",  /* "" for control ops */
      "dst":   "<dst>" | "",
      "src1":  "<src1>" | "",
      "src2":  "<src2>" | "",
      "src3":  "<label_or_callee>",   /* OPTIONAL key — present iff non-empty */
      "extra": "imm=<n>",             /* OPTIONAL key — IR_const only */
      "line":  <int>
    },
    ...
  ]
}
```

Consumers MUST handle `src3` and `extra` as optional via `dict.get(...)`.

---

## 8. Determinism contract [DEFERRED — Gate IR-2]

For fixed input, the persisted IR MUST be byte-identical across:
(a) repeated runs in the same env, (b) `MALLOC_PERTURB_=42`, (c) different cwd.

Forbidden: addresses, PIDs, timestamps, absolute paths, hash-map iteration
order leaks. Verified by `gate_ir2.sh` (run with `ZCC_EMIT_IR=1`, not the
documented-but-broken flag combo).

---

## 9. Coverage contract [DEFERRED — Gate IR-1]

Every C construct ZCC's parser accepts MUST emit a non-empty function body,
unless the function name appears on `ir_blacklisted` (`part4.c:3458`),
which currently includes at least `"main"`. Silent IR drops are forbidden.

**Coverage corpus (locked):**
- `/mnt/d/_stego/zcc_recursive/experiments/*.c` — raytracer, voxel, audio,
  VR, physics (graphics workloads, ~65 functions per README_MEGA_CORPUS)
- `/mnt/h/__DOWNLOADS/selforglinux/zcc_pp.c` — selfhost source, 7338 lines
- `/mnt/d/_stego/zcc_recursive/gods_eye/corpus/zcc-compiler-bug-corpus/`
  — 12-bug ground-truth JSON

The empty stub dirs at `/mnt/d/_stego/zcc-level{1,2,3,4}`,
`/mnt/d/_stego/zcc_tests_lv1`, `/mnt/d/_stego/zcc-compiler-bug-corpus`
are NOT corpus (`total 0`, never populated) and MUST NOT be used.

---

## 10. Out of scope

- In-memory `ir_node_t` / `ir_func_t` / `ir_module_t`. May change freely.
- Optimizer pass implementation. Pass *output* must conform to this schema;
  pass internals are unconstrained.
- x86 codegen output — governed by bootstrap-equivalence (`zcc2.s == zcc3.s`).
- `--ir --telemetry` JSONL pass-events — separate format, separate schema.

---

## 11. Closure checklist

- [x] §4 type system locked from `ir.h`
- [x] §5.1 core opcodes locked from real emission
- [x] §5.2 16 binary ops locked from `ir_bridge.h`
- [x] §6 text grammar locked from probe output
- [x] §7 JSON shape locked from `convert_ir_to_json.py` output
- [x] `ZCC_IR_VERSION` macro added to `ir_bridge.h` and emitted by `ir_module_emit_text` (verified 2026-05-03: probe shows `; ZCC IR v1.0.0` as first IR-content line; bootstrap GREEN)
- [ ] Skill `zkaedi-zcc-ir-bridge/SKILL.md` lines 53–58 corrected to `ZCC_EMIT_IR=1`
- [x] CG-IR-022 ticket filed and RESOLVED (`ir_bridge.h:133`, 1-line fix, bootstrap GREEN both sides)
- [ ] Doc committed at repo root as `IR_SCHEMA.md`
- [ ] `ZCC_STATUS.md` updated

When all 5 unchecked items resolve, schema is `1.0.0` LOCKED-AND-MERGED.
Until then it's LOCKED-IN-DRAFT — the format is fixed, the bookkeeping is pending.
