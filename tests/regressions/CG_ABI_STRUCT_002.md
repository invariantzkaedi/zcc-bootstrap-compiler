# CG-ABI-STRUCT-002
## Memory-Class Struct Parameter Offset Misclassification

| Field | Value |
|-------|-------|
| **Bug ID** | CG-ABI-STRUCT-002 |
| **Component** | `part4.c` — `codegen_func` / `adjust_sym` pre-pass |
| **Kind** | Wrong-code (incorrect output, no crash) |
| **Trigger** | Function that both **returns** and **receives** memory-class struct args (size > 16B, SysV AMD64) |
| **Discovery** | Compiler Olympics `abi_struct_val` with `--mutate-cold-generators` (80-seed run) |
| **Fixed in** | `part4.c` line 4990 |
| **Status** | ✅ FIXED, selfhost verified, Olympics GOLD |

---

## Trigger Conditions

All three must be true simultaneously:

1. The function **returns** a struct > 16 bytes (SysV memory-class → hidden `%rdi` pointer)
2. The function **receives** ≥ 2 struct parameters > 16 bytes each
3. Combined parameter size exceeds non-sret frame boundary

The bug **did not** fire for:
- Structs ≤ 16B (register-class, `shift = 0`, `adjust_sym` not invoked)
- Functions with only one memory-class struct parameter (first param always within boundary)
- Functions that return a scalar (no sret, `local_offset` starts at 0)

---

## Root Cause

The parser (`scope_add_local`) assigns stack offsets starting from `cc->local_offset = -8` (the sret hidden-pointer slot) when the return type is memory-class. For `D d_op(D x, D y)` where D = 24B:

```
sret:   local_offset = -8   →  no symbol (raw rdi spill)
x (D):  local_offset = -32  →  x.stack_offset = -32
y (D):  local_offset = -56  →  y.stack_offset = -56
```

In `codegen_func` (part4.c), the boundary used by `adjust_sym` to distinguish parameters from locals was:

```c
// BEFORE (bug):
int parser_param_limit = -parser_param_space;  // = -(24+24) = -48
```

`y.stack_offset = -56 < -48` → misclassified as a **local variable** → `adjust_sym` applied the wrong shift:

```
y.stack_offset = -56 - 8 = -64   ← wrong
```

Emitted assembly:
```asm
leaq -64(%rbp), %rax    ; BUG: y.d loaded from wrong address
movsd (%rax), %xmm0     ; reads garbage → mulsd produces 0.0
```

---

## The Fix

**File**: `part4.c`, function `codegen_func`, line 4990

```diff
- int parser_param_limit = -parser_param_space;
+ /* CG-ABI-STRUCT-002: include the sret slot (-8B) in the boundary so
+  * trailing memory-class struct params are not misclassified as locals. */
+ int parser_param_limit = -actual_param_space;
```

`actual_param_space = parser_param_space + sret_size = 48 + 8 = 56`

New boundary: `-56` — exactly where `y` lives → correctly enters the parameter-matching branch.

---

## Verification

### Struct Progression Test (`repro_struct_progression.c`)

| Struct | Size | GCC | ZCC before | ZCC after |
|--------|------|-----|------------|-----------|
| A `{double}` | 8B | 26.75 | ✅ | ✅ |
| B `{int, double}` | 16B | 30 / 26.75 | ✅ | ✅ |
| C `{double, int, int}` | 16B | 26.75 / 30 / 255 | ✅ | ✅ |
| D `{double, u, i, f, i}` single | **24B** | 99.41 | ❌ 0.00 | ✅ 99.41 |
| D chain `d_op(d_op(x,y),y)` | **24B** | 2215.92 | ❌ 0.00 | ✅ 2215.92 |

### Bootstrap

```
make selfhost
→ diff zcc2.s zcc3.s
→ SELF-HOST VERIFIED (assembly identical)
```

### Regression Suite

```
./zcc -I./zcc_sys_includes tests/regressions/repro_struct_progression.c -o /tmp/t.s
gcc /tmp/t.s -o /tmp/t && /tmp/t
→ PASS CG-ABI-STRUCT-002 regression

./zcc -I./zcc_sys_includes tests/regressions/repro_chain_min.c -o /tmp/t.s
gcc /tmp/t.s -o /tmp/t && /tmp/t
→ PASS CG-ABI-STRUCT-002 chain regression
```

### Olympics (80-seed, `abi_struct_val` in permanent mutate mode)

```
Wrong-code Mismatches...... 0   ← was 8 before fix
Codegen Compiler Crashes... 0
Overall Olympics Score..... 🏆 GOLD
✅ ALL GATES PASSED SUCCESSFULLY.
```

---

## Discovery Method

This bug was found by **coverage-guided fuzzing**:

1. Initial 10-seed Olympic run: `abi_struct_val` identified as a cold category (low new-line yield)
2. `--mutate-cold-generators` activated diversity knobs for cold categories
3. `abi_struct_val` mutated: 5-field structs with mixed int+float types, multi-function chains
4. 80-seed mutated run: 8 wrong-code mismatches surfaced, all `abi_struct_val`
5. Minimum reproducer constructed: `{double, u, i, f, i}` (24B), 2-function chain
6. Assembly inspection pinpointed `leaq -64(%rbp)` vs expected `leaq -56(%rbp)`
7. Root cause traced to `parser_param_limit` boundary in `adjust_sym`

**Key takeaway**: Coverage guidance did not just improve metrics — it found an ABI correctness bug.

---

## Nightly Olympics Configuration

`abi_struct_val` is permanently in mutated mode. In `compiler_olympics.py`:

```python
# Categories that are always mutated (have known cold-path coverage value)
ALWAYS_MUTATE = {'abi_struct_val'}
```

This ensures struct-by-value correctness is adversarially tested on every nightly run.

---

## Files

| File | Purpose |
|------|---------|
| `tests/regressions/repro_struct_progression.c` | Struct A/B/C/D single+chain progression, self-checking |
| `tests/regressions/repro_chain_min.c` | Minimal 24B struct chain reproducer, self-checking |
| `tests/regressions/CG_ABI_STRUCT_002.md` | This document |
| `part4.c` line 4990 | The fix |
