# Gate Evidence — Vector 6: ABI Calling Convention Oracle

**Milestone**: `feat(abi): calling convention oracle --trace-abi`
**Commit baseline**: `d42ec947`

## Phase 0 Verdict
```
BASELINE:              GREEN
SYMPTOM-IN-HISTORY:    NO
FORENSIC-LATEST-SHA:   a60a8b89
PROCEED:               YES
```

---

## Changes Implemented

### part5.c
- Added parser support for the `--trace-abi` CLI flag.
- Suppressed standard compiler progress diagnostics (`[Phase 1] ...`) when `--trace-abi` is specified to keep stdout clean.
- Hooked `run_abi_trace(cc, prog)` directly after parsing to capture low-level compiler ABI representations.

### src/codegen.c
- Intercepted compiler invocation to bypass linking and downstream generation when `--trace-abi` is present.

### src/zcc_oracle_substrate.c
- Coded AMD64 System V Calling Convention rules:
  - Recursive formatting of C-compatible type strings.
  - Returns and parameters classification into eightbytes.
  - Registers allocation (`rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9` / `xmm0`–`xmm7`).
  - Handling of `sret` returns shifting parameters to the right.
  - Stack placement of spilled registers and padding/offsets layout calculations.

### tools/abi_probe_clang.py
- Programmatic Sys V layout extraction tool using Clang.

### tools/abi_probe_gcc.py
- Programmatic Sys V layout extraction tool using GCC.

### tools/abi_diff.py
- JSON differences verifier checking layouts and lowers byte-for-byte.

---

## Gate 1 — Self-host byte-identical: `cmp zcc2.s zcc3.s`

Compiled with:
```bash
make selfhost
```
Output:
```
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (16105 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)"
SELF-HOST VERIFIED (assembly identical)
```

**Result: BYTE-IDENTICAL**

---

## Gate 2 — Calling Convention Parity Check

Verified by running differential validations:
```bash
./zcc2 --trace-abi cases.c > zcc.abi.json
python3 tools/abi_probe_clang.py cases.c > clang.abi.json
python3 tools/abi_probe_gcc.py cases.c > gcc.abi.json
python3 tools/abi_diff.py zcc.abi.json clang.abi.json gcc.abi.json
```
Output:
```
Comparing function 'bar'...
Comparing function 'foo'...
Comparing function 'mixed_spill'...
Comparing function 'return_pair'...
Comparing function 'spill_gp'...
Comparing function 'spill_sse'...

✅ ABI verification successful! ZCC matches Clang and GCC byte-for-byte in layout / registers lowering.
```

**Result: PASS**

---

## Bugs caught mid-gate
- **Clang/GCC layout extraction IndexError**: The regex header parser used index `1` instead of `2` to extract the dot-separated field identifier, causing `ValueError`. Fixed by pointing the parser directly at the correct split index.

## Hygiene / deferred
None.
