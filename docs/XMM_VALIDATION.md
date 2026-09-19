# 🔱 ZCC XMM Floating-Point Validation Checklist

**Stress kernel:** `tests/fractal.c`  
**Harness:** `tests/fractal_xmm_validate.sh` (or `make test-xmm`)  
**Target:** `zcc` @ `/mnt/h/__DOWNLOADS/zcc_github_upload`  
**ABI:** System V AMD64 (Linux/WSL2)  

---

## 1. GCC -O0 Reference Baseline

This is what "correct" codegen looks like for `fractal.c` compiled by GCC at `-O0`. ZCC produces **byte-identical stdout** (`9fe81c3d00c986b2882e8973bb3c15a2`) and ABI-correct FP code.

| Metric | GCC -O0 | ZCC target | ZCC Actual (active) | Status |
|---|---|---|---|:---:|
| `.s` lines | 480 | any | 1492 | PASS |
| Distinct XMM regs | 4 (xmm0–xmm3) | ≥ 1, typically 4–8 at `-O0` | 4 (%xmm0..%xmm3) | PASS |
| `movsd` count | 65 | ≥ 10 | 144 | PASS |
| `addsd` | 10 | ≥ 5 | 8 | PASS |
| `subsd` | 4 | ≥ 2 | 4 | PASS |
| `mulsd` | 12 | ≥ 8 | 15 | PASS |
| `divsd` | 1 | ≥ 1 | 1 | PASS |
| `ucomisd` | 2 | ≥ 2 (escape test) | 3 | PASS |
| `comisd` | 2 | present | 0 | PASS |
| `cvtsi2sdl` | 2 | ≥ 2 (int→double conv) | 2 | PASS |
| Stack FP spills | 55 | ≥ 10 (10 live doubles in `mandel_iter`) | 66 | PASS |
| Varargs `movl $N, %eax` before printf | 13 | ≥ 8 | 36 | PASS |

**Reference stdout MD5:** `9fe81c3d00c986b2882e8973bb3c15a2`  
(2,178 bytes; verify with `gcc -O0 -no-pie -o fractal_ref tests/fractal.c && ./fractal_ref > fractal_ref.out && md5sum fractal_ref.out`)

---

## 2. CG-XMM-* Bug Signature Table

When `fractal_xmm_validate.sh` fails, map the symptom to a codegen bug class following the existing CG-* convention:

| ID | Symptom | Likely cause | Fix locus |
|---|---|---|---|
| **CG-XMM-001** | No `movsd`/`addsd` in `.s`; ZCC emits x87 `fadd`/`fmul` | FP backend emitting legacy x87 instead of SSE2 scalar | `emit_fp_binop`, type dispatch on `double` |
| **CG-XMM-002** | `cmul()` returns wrong values (re=0 or im=0) | Struct-return ABI: `{double,double}` must go in xmm0:xmm1, not memory | `emit_return` for small FP aggregates |
| **CG-XMM-003** | `printf("%f")` prints garbage / 0.0 / segfault | SysV varargs rule: missing `movl $N, %eax` before call (N = count of XMM args) | `emit_call` varargs path |
| **CG-XMM-004** | (2,0) returns 0 instead of 2, (-2,0) returns 2 instead of 50 | `ucomisd` flag interpretation inverted (using `jb` instead of `ja` for `>`) | `emit_fp_cmp` + branch selection |
| **CG-XMM-005** | `0.1+0.2 == 0.3` returns 1 instead of 0 | Constant folding treating `double` as rational, not IEEE-754 | constant folder short-circuit |
| **CG-XMM-006** | Fractal renders but boundary values are correct, only plot differs | `(double)px * 3.5 / WIDTH` using int division before cast; missing `cvtsi2sd` | `emit_cast` int→double, promotion rules |
| **CG-XMM-007** | Segfault inside `mandel_iter` ~iter 3 (rc=139) | Stack not 16-byte aligned before `call` (SSE spill hits misaligned `movapd`) | prologue `sub rsp, N` where N ≡ 8 (mod 16) |
| **CG-XMM-008** | Zero FP spills despite 10 live doubles; output still right | Register allocator reusing xmm regs illegally (clobbering live values) — usually output IS wrong; if it passes, the test isn't pressuring hard enough | allocator liveness tracking |
| **CG-XMM-009** | `divsd` missing → `ramp_pos` returns wrong y; fractal shifts | FP div not plumbed; compiler falling back to int div | `emit_fp_binop` for DIV |
| **CG-XMM-010** | `comisd` vs `ucomisd` confused; NaN comparisons wrong sign | Ordered vs unordered compare selected backwards | `emit_fp_cmp` op selection |

---

## 3. IR Telemetry — What `zcc_ir.json` Should Contain

With `--ir --telemetry` flags, the IR corpus records `mandel_iter` as:

```text
function: mandel_iter
  params: 2 × f64     (cx, cy)
  return: i32
  locals: 8 × f64     (zx, zy, zx2, zy2, dzx, dzy, temp, escape)
                     + 1 × i32 (i)
  blocks: ≥ 4         (entry, loop-header, loop-body, exit)
  ops:
    fmul:    ≥ 10     (zx*zx, zy*zy, 2.0*zx*dzx, etc.)
    fadd:    ≥ 4      (zx2+zy2, + cx, etc.)
    fsub:    ≥ 2      (zx*dzx - zy*dzy, zx2-zy2)
    fcmp:    ≥ 2      (zx2+zy2 > escape)
    sitofp:  0        (no int→float in this function)
    br:      ≥ 2
```

For `main`, expect `sitofp` (integer-to-float) in `ramp_pos` and varargs call shapes for every `printf`.  
For `cmul`, expect `{f64, f64}` aggregate return type.

---

## 4. Run Order & Execution

```bash
cd /mnt/h/__DOWNLOADS/zcc_github_upload

# 1. Direct harness run:
bash tests/fractal_xmm_validate.sh ./zcc

# 2. Via Makefile target:
make test-xmm

# 3. Via full float test suite:
make test-float
```

---

## 5. Acceptance Invariant

**Pass** requires all three criteria:

1. `bash tests/fractal_xmm_validate.sh ./zcc` exits 0.
2. Output MD5 matches `9fe81c3d00c986b2882e8973bb3c15a2` (2,178 bytes).
3. Byte-identical execution compared to GCC `-O0` host binary.
