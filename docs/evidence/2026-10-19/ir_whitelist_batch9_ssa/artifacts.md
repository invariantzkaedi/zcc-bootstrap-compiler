# Artifacts Manifest: IR-WHITELIST-BATCH9-SSA-PROMOTION

**Date:** 2026-10-19  
**Subsystem:** Codegen / SSA-IR Lowering  
**Target:** Batch 9 Core Compiler & Type System SSA-IR Lowering Promotion  

---

## 1. Verified Key Artifacts

| Artifact | Path | Size (Bytes) | MD5 Checksum | SHA256 Checksum |
|---|---|---|---|---|
| Stage 2 Assembly | `zcc2.s` | 9,582,450 | `b13b1207c7cee38741746b83a12deba9` | `139294a163463c776510ea98095100e67e2a2b57e9f845f6af65117e5931d67c` |
| Stage 3 Assembly | `zcc3.s` | 9,582,450 | `b13b1207c7cee38741746b83a12deba9` | `139294a163463c776510ea98095100e67e2a2b57e9f845f6af65117e5931d67c` |
| Compiler Binary | `zcc` | 1,771,784 | `2a884bfdbb42491a6d9534c0fb35ec05` | `e181a7fc0a744aa7e6cfd0c8cff8fb16fdfa1b17065cba124f20d573a15849c9` |
| QuickJS Harness | `/tmp/test_quickjs_runner` | 2,302,088 | `6f38ad9b302c40c8152e7ce3fa7a4991` | `f22655605a57d8ac299539f3007e807fa14f9385274143fb0737dce2f5ca104d` |

---

## 2. Source Modifications

- **Target File:** `part4.c`
- **Diff:**
  - Added 14 core routines to `ir_whitelisted()`:
    `"symbol_alignment"`, `"alignment_log2"`, `"node_stmts"`, `"node_is_float"`,
    `"node_is_char_or_void_ptr_cast"`, `"is_complex_type"`, `"is_compatible_type"`,
    `"is_unsigned_cmp"`, `"abi_join"`, `"is_nullable_call"`, `"is_null_check"`,
    `"is_rax_deref"`, `"is_rax_moved"`, `"type_to_str"`.
  - Guarded `is_ir_eligible()`:
    - Null node & body check: `if (!func || !func->func_def_name || !func->body) return 0;`
    - System V 6-arg window: `if (func->num_params > 6) return 0;`
    - Variadic check: `if (func->func_type && func->func_type->is_variadic) return 0;`
    - Bounded return size: `if (type_size(ret_type) > 8) return 0;` (prevents ABI register splitting conflicts on multi-register aggregate returns / E-LEARN-016 `%rdx` truncation).
    - Parameter scalar size check: `if (type_size(ptype) > 8 || ptype->kind == TY_STRUCT || ptype->kind == TY_UNION) return 0;` (protects parameter slot alignment).
- **Line Count Impact:**
  - `part4.c`: +14 lines, -3 lines (net +11 lines, strictly < 50-line limit).
