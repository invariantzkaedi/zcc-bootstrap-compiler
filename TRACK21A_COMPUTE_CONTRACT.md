# ZCC Track 21A — Canonical Computational Contract (`track21a.compute_contract.v1`)

**Status:** Normative (`ZCC-EF-0`, `ZCC-EF-7`)  
**ABI Version Constant:** `ZCC_COMPUTE_ABI_VERSION = 0x00010000` (`"1.0.0"`)  
**Receipt Schema Version:** `"track21a.receipt.v1"`  
**Scope:** Portable verified compute across `SCALAR_C99` (reference), `AVX2_FMA` (accelerated CPU), and `PTX_SM120` (ZCC `part6_ptx.c` -> CUDA Driver PTX JIT). Vulkan (`T21-G02`) and SPIR-V (`T21-G01`) remain explicitly **DEFERRED** to Track 21B.

---

## 1. Design Principles and Epistemic Boundaries

1. **No Silent Hardware Execution:** Accelerated CPU instructions (`AVX`, `AVX2`, `FMA`, `F16C`) MUST reside in separate translation units compiled with target flags (`-mavx2 -mfma`) and MUST NEVER execute unless live `CPUID` + `OSXSAVE` + `XGETBV(XCR0[2:1] == 0b11)` verification succeeds at runtime. Portable code (`cpu_capabilities.c`, `compute_dispatch.c`, `compute_scalar.c`, `compute_sha256.c`, `execution_receipt.c`, `compute_ptx.c`) MUST compile cleanly with `-mno-avx -mno-avx2 -mno-fma`.
2. **No Silent Backend Substitution:** When a caller specifies `ZCC_POLICY_EXACT_REQUIRED`, failure to initialize or satisfy the requested backend MUST fail closed with a specific non-zero `zcc_status_t` error code (`ZCC_STATUS_ERR_UNSUPPORTED_HW` or `ZCC_STATUS_ERR_BACKEND_UNAVAILABLE`). Silent fallback is permitted **ONLY** under `ZCC_POLICY_AUTO_FALLBACK` and MUST be explicitly recorded in `receipt.selection_reason` and `receipt.fallback_occurred`.
3. **Heterogeneous Numerical Honesty:** IEEE-754 single-precision (`binary32`) arithmetic with differing associativity, FMA contraction, `rsqrtf`/SFU approximation, or accumulation width is NOT bitwise identical across scalar C99, AVX2+FMA, and PTX. Each operation declares explicit `(abs_tol, rel_tol, ulp_tol)` bounds and determinism guarantees.
4. **Compatibility Preservation:** Existing `include/zcc_gpu_llm_api.h` C exports (`ZccLlmGenConfig`, `ZccLlmStats`, `zcc_llm_*`) and `sdk/zcc_client.py` / `sdk/zcc_client.mjs` HTTP/SSE interfaces remain binary- and API-compatible. Track 21A extends the SDK namespace non-destructively via additive structs/symbols (`zcc_compute_*`, `sdk/zcc_compute.py`, `sdk/zcc_compute.mjs`).

---

## 2. Canonical Tensor Descriptor (`zcc_tensor_desc_t`)

All tensor operands cross the runtime boundary via a canonical, fixed-size descriptor (`sizeof(zcc_tensor_desc_t) == 128` bytes on 64-bit targets, verified with `_Static_assert`):

```c
typedef enum zcc_dtype_e {
    ZCC_DTYPE_F32  = 1,  /* IEEE-754 binary32, 4 bytes/elem */
    ZCC_DTYPE_F16  = 2,  /* IEEE-754 binary16, 2 bytes/elem (LE uint16) */
    ZCC_DTYPE_BF16 = 3,  /* bfloat16 (upper 16 bits of binary32), 2 bytes/elem */
    ZCC_DTYPE_Q8_0 = 4,  /* 32-elem blocks, 34 bytes/block: u16 f16 d + 32x int8 */
    ZCC_DTYPE_Q4_0 = 5,  /* 32-elem blocks, 18 bytes/block: u16 f16 d + 16B packed nibbles */
    ZCC_DTYPE_Q4_K = 6,  /* 256-elem super-blocks, 144 bytes/block: u16 d + u16 dmin + 12B scales + 128B qs */
    ZCC_DTYPE_U8   = 7,  /* Raw unsigned byte view (SHA-256 / diagnostic tests) */
    ZCC_DTYPE_I32  = 8   /* Signed 32-bit integer */
} zcc_dtype_t;

typedef enum zcc_ownership_e {
    ZCC_OWNER_BORROWED_HOST = 0x01,  /* Caller owns host pointer; must remain valid across call */
    ZCC_OWNER_OWNED_HOST    = 0x02,  /* Runtime allocated/managed via zcc_tensor_alloc/free */
    ZCC_OWNER_READONLY      = 0x04,  /* Output writes to READONLY buffer MUST fail closed */
    ZCC_OWNER_DEVICE_PINNED = 0x08   /* Reserved / optional explicit device residency handle */
} zcc_ownership_t;
```

### 2.1 Shape, Rank, Strides, Alignment, and Bounds Rules

| Field | Contract |
| --- | --- |
| `rank` | `1 <= rank <= 4`. Rank `0` or `> 4` returns `ZCC_STATUS_ERR_INVALID_SHAPE`. |
| `shape[0..3]` | Positive element counts (`shape[d] > 0` for `d < rank`; trailing dimensions unused `<= 1` or `0`). Zero-element shapes fail closed unless an op explicitly permits empty (Track 21A requires `>= 1`). |
| `strides[0..3]` | **In bytes** per logical element along dense dims, or **0 for contiguous row-major canonical strides** on dense types (`ZCC_DTYPE_F32`, `F16`, `BF16`, `U8`, `I32`). Negative strides (`< 0`), zero stride on non-broadcast inputs, or overlapping non-monotonic strides where `stride[d] < min_element_bytes` fail closed with `ZCC_STATUS_ERR_INVALID_STRIDE`. Quantized formats (`Q8_0`, `Q4_0`, `Q4_K`) MUST use contiguous packed row-major innermost dimension (`K % block_elems == 0`, `rank == 2` for matrix weights `[N, K]`, or `rank == 1` `[K]`). |
| `alignment` | Required alignment field (`1` for arbitrary byte/unaligned host pointers; recommended `32` for aligned fast-paths). Backends MUST inspect buffer pointer address `(uintptr_t)data % 32` at dispatch and support unaligned host buffers (`align >= 1`, e.g. 4-byte float aligned `+4`/`+12` offsets) safely without crashing (`_mm256_loadu_ps`). Declared `alignment` must be power-of-2; if non-zero & `(uintptr_t)data % alignment != 0`, return `ZCC_STATUS_ERR_MISALIGNED`. |
| `data` & `nbytes` | `data != NULL` (or `ZCC_STATUS_ERR_NULL_PTR`). `nbytes` MUST satisfy `nbytes >= max_byte_offset_accessed` using overflow-safe 64-bit arithmetic (`ZCC_STATUS_ERR_OOB`). |
| Output aliasing | Output tensor aliasing an input tensor in ops requiring out-of-place semantics (`GEMM`, `RMSNORM`, `SOFTMAX`, `DOT`, `MATVEC_*`) is rejected with `ZCC_STATUS_ERR_ALIASING`. Elementwise `VEC_*` permits exact identical `out == a` only when `strides` match and no partial overlap occurs; partial byte overlap is rejected (`ZCC_STATUS_ERR_ALIASING`). |

---

## 3. Quantization Formats (GGUF Byte-Level Parity)

Quantization formulas match existing ZCC GGUF GPU kernels (`src/win64_gpu_llm_kernels.c:421-567`):

1. **`Q8_0` (block = 32 elements, 34 bytes):**
   - `bytes[0..1]`: little-endian IEEE-754 `f16` scale $d$
   - `bytes[2..33]`: 32 `int8_t` signed values $q_i \in [-128, 127]$
   - Dequantization: $x_i = d \cdot q_i$ (converting $d$ from IEEE-754 `f16` to `f32`; non-finite `f16` produces IEEE NaN/Inf cleanly without UB)

2. **`Q4_0` (block = 32 elements, 18 bytes):**
   - `bytes[0..1]`: little-endian IEEE-754 `f16` scale $d$
   - `bytes[2..17]`: 16 packed bytes holding 32 unsigned nibbles, **split-half layout**:
     - For $i \in [0, 15]$: $q_i = (\text{bytes}[2 + i] \mathbin{\&} \text{0x0F}) - 8$
     - For $i \in [16, 31]$: $q_i = (\text{bytes}[2 + (i - 16)] \gg 4) - 8$
   - Dequantization: $x_i = d \cdot q_i$

3. **`Q4_K` (super-block = 256 elements, 144 bytes):**
   - `bytes[0..1]`: little-endian `f16` super-scale $d$
   - `bytes[2..3]`: little-endian `f16` super-min $d_{\min}$
   - `bytes[4..15]`: 12 bytes encoding 8 `(sc_j, mn_j)` 6-bit pairs (`j in 0..7`):
     - If $j < 4$: `sc = scales[j] & 63`, `mn = scales[j + 4] & 63`
     - If $j \ge 4$: `sc = (scales[j + 4] & 15) | ((scales[j - 4] >> 6) << 4)`, `mn = (scales[j + 4] >> 4) | ((scales[j] >> 6) << 4)`
   - `bytes[16..143]`: 128 bytes across 4 chunks $c \in \{0,1,2,3\}$ of 64 elements each (`32` bytes per chunk):
     - Low sub-block (`sub = 0`, elements $64c + l$, $l \in [0, 31]$): nibble `q = qs[32c + l] & 15`, `j = 2c`
     - High sub-block (`sub = 1`, elements $64c + 32 + l$, $l \in [0, 31]$): nibble `q = qs[32c + l] >> 4`, `j = 2c + 1`
   - Dequantization: $x = (d \cdot sc_j) \cdot q - (d_{\min} \cdot mn_j)$

All three require inner dimension $K$ to be an exact positive multiple of `32` (`Q8_0`/`Q4_0`) or `256` (`Q4_K`); non-divisible $K$ returns `ZCC_STATUS_ERR_INVALID_SHAPE`.

---

## 4. Operations Semantics & Rounding / Accumulation / Determinism Policies

### 4.1 Operation Set (`zcc_op_t`)

| Op ID | Enum | Signature / Mathematical Definition |
| --- | --- | --- |
| 1 | `ZCC_OP_VEC_ADD_F32` | $C_i = A_i + B_i$ for $i \in [0, N)$ |
| 2 | `ZCC_OP_VEC_MUL_F32` | $C_i = A_i \cdot B_i$ for $i \in [0, N)$ |
| 3 | `ZCC_OP_VEC_FMA_F32` | $D_i = A_i \cdot B_i + C_i$ (scalar C99 `fmaf` when `ACCUM_FMA`, separate mul+add when `ACCUM_STRICT_FP32`) |
| 4 | `ZCC_OP_DOT_F32` | Scalar result $r = \sum_{i=0}^{K-1} A_i B_i$ (`out` shape `[1]`) |
| 5 | `ZCC_OP_GEMM_F32` | $C_{m,n} = \sum_{k=0}^{K-1} A_{m,k} B_{k,n}$ (`A=[M,K]`, `B=[K,N]`, `C=[M,N]`) |
| 6 | `ZCC_OP_RMSNORM_F32` | Row-wise for `r in [0, rows)`: $s_r = \frac{1}{D}\sum_{j=0}^{D-1} X_{r,j}^2$, $g_r = 1 / \sqrt{s_r + \varepsilon}$, $Y_{r,j} = X_{r,j} \cdot g_r \cdot W_j$ (`eps > 0` default `1e-5f`; `eps <= 0` or NaN fails closed) |
| 7 | `ZCC_OP_SOFTMAX_F32` | Row-wise stable softmax: $m_r = \max_j X_{r,j}$, $e_{r,j} = \exp(X_{r,j} - m_r)$, $S_r = \sum_j e_{r,j}$, $Y_{r,j} = e_{r,j} / S_r$ |
| 8 | `ZCC_OP_DEQUANT_Q8_0` | Quantized matrix/vector `[N, K]` or `[K]` `Q8_0` -> `f32` |
| 9 | `ZCC_OP_MATVEC_Q8_0` | $Y_r = \sum_{k=0}^{K-1} \text{dequant}(W)_{r,k} \cdot X_k$ (`W=[N,K] Q8_0`, `X=[K] f32`, `Y=[N] f32`) |
| 10 | `ZCC_OP_DEQUANT_Q4_0` | Quantized `[N, K]` or `[K]` `Q4_0` -> `f32` |
| 11 | `ZCC_OP_MATVEC_Q4_0` | Matrix-vector product (`W=[N,K] Q4_0`, `X=[K] f32`, `Y=[N] f32`) |
| 12 | `ZCC_OP_DEQUANT_Q4_K` | Quantized `[N, K]` or `[K]` `Q4_K` -> `f32` |
| 13 | `ZCC_OP_MATVEC_Q4_K` | Matrix-vector product (`W=[N,K] Q4_K`, `X=[K] f32`, `Y=[N] f32`) |

### 4.2 Rounding, Accumulation & Determinism Policies

- **`ZCC_ACCUM_FP32_FMA` (default for accelerated backends):** Binary32 accumulation with fused multiply-add (`_mm256_fmadd_ps` on AVX2, `fma.rn.f32` on PTX, `fmaf` / IEEE FP32 on Scalar) using IEEE-754 round-to-nearest-even (`FE_TONEAREST`).
- **`ZCC_ACCUM_FP64_REF` (oracle / high-precision mode):** Internal binary64 (`double`) accumulation for `DOT`, `GEMM`, `RMSNORM` sum-of-squares, `SOFTMAX` normalizer, and quantized matvec, rounded to `binary32` at final write. Supported natively by `SCALAR_C99` and requested in differential comparisons to bound FP32 associative cancellation drift.
- **`ZCC_DET_FIXED_REDUCTION` (defined deterministic replay, `T21-G08`):** Fixed thread/block traversal order, fixed launch geometry (`grid/block`), fixed RNG `seed` when stochastic kernels exist, and explicit prohibition of non-deterministic `atomicAdd` floating-point reduction races across runs. Identical `(inputs, options, hardware, kernel)` yields **bitwise-identical output digest** within the same backend on repeat runs.

### 4.3 Edge Case & Non-Finite Semantics

- **Invalid shape / stride / null / OOB / readonly output / negative eps:** Return explicit `ZCC_STATUS_ERR_*` BEFORE launching any compute or touching output buffers.
- **Allocation failure (`OOM`):** Return `ZCC_STATUS_ERR_OOM`; no partial frees leaked and clean teardown.
- **NaN / Inf propagation:**
  - Finite elementwise / GEMM / RMSNorm / Softmax inputs containing IEEE `NaN` or `+/-Inf` MUST NOT trap (`SIGFPE`), MUST propagate NaN cleanly (`SOFTMAX` row containing `+Inf` or `NaN` evaluates `NaN` deterministically per IEEE-754 max/sub `Inf - Inf = NaN` rather than silently clamping to garbage numbers; `RMSNORM` with `Inf` yields `0` or `NaN` per `Inf * rsqrt(Inf)` cleanly without crash).
- **PTX hardware JIT / launch error:** Propagate CUDA driver error code via `ZCC_STATUS_ERR_BACKEND_RUNTIME` and capture error string; never ignore return values.

---

## 5. Numerical Tolerance Policy (`zcc_tolerance_policy_t`)

Oracle: Independent 64-bit `double` (`binary64`) reference vector evaluated in Python/NumPy (`float64` / `decimal`) and `SCALAR_C99` reference (`ZCC_ACCUM_FP64_REF` + `ZCC_ACCUM_FP32_FMA`). Bitwise identity across Scalar vs AVX2 vs PTX is **NOT** assumed; instead per-op bounds apply:

| Operation | Absolute Tolerance (`atol`) | Relative Tolerance (`rtol`) | Max ULP (scalar finite well-conditioned) | Notes |
| --- | --- | --- | --- | --- |
| `VEC_ADD_F32`, `VEC_MUL_F32` | `1e-7` | `1e-6` | `2` | Scalar vs AVX2 vs PTX exact/near-exact on finite ranges |
| `VEC_FMA_F32` | `1e-6` | `1e-6` | `4` | Accounts for single rounding (FMA) vs two roundings if FP32 mul+add |
| `DOT_F32`, `GEMM_F32` | `max(1e-5, K * 1e-6 * norm)` (base `atol=1e-4`) | `1e-4` | N/A (tree/order dependent; ULP not meaningful under cancellation) | Scaled by reduction depth $K$ and condition factor $\|A\|_\infty \|B\|_\infty$; high-cancellation inputs verified separately against FP64 oracle |
| `RMSNORM_F32` | `1e-5` | `1e-4` | N/A | Accounts for PTX `rsqrt.approx.f32` vs IEEE `1.0/sqrtf` (~2^-22 relative error) |
| `SOFTMAX_F32` | `1e-5` | `1e-4` | N/A | Accounts for PTX `ex2.approx.f32` (~2^-22) vs `expf` plus sum probability invariant $|\sum_j y_j - 1| \le 1e-4$ and $0 \le y_j \le 1$ |
| `DEQUANT_Q8_0`, `Q4_0`, `Q4_K` | `1e-6` | `1e-5` | `4` | Exact except `f16` conversion & single-precision multiply/sub |
| `MATVEC_Q8_0`, `Q4_0`, `Q4_K` | `5e-4` | `5e-4` | N/A | Reduction over $K$ dequantized weights |

---

## 6. Backend Capability Negotiation & Hardware Detection

```c
typedef struct zcc_cpu_caps_s {
    uint32_t struct_size;
    uint32_t max_basic_leaf;
    uint32_t has_cpuid;
    uint32_t has_sse2;
    uint32_t has_osxsave;     /* CPUID.01H:ECX[bit 27] */
    uint32_t xcr0_xmm;        /* XGETBV(0) bit 1 */
    uint32_t xcr0_ymm;        /* XGETBV(0) bit 2 */
    uint32_t has_avx;         /* CPUID.01H:ECX[bit 28] AND osxsave AND xcr0_xmm AND xcr0_ymm */
    uint32_t has_f16c;        /* CPUID.01H:ECX[bit 29] AND AVX OS state */
    uint32_t has_fma;         /* CPUID.01H:ECX[bit 12] AND AVX OS state (INDEPENDENT check!) */
    uint32_t has_avx2;        /* CPUID.07H(0):EBX[bit 5] AND has_avx */
    uint32_t has_avx512f;     /* Telemetry only (XCR0[7:5]==7 + EBX[16]) */
    uint64_t xcr0_raw;
} zcc_cpu_caps_t;
```

**Mandatory AVX2 Dispatch Rule:**
$$\text{AVX2\_FMA\_ALLOWED} \iff \text{has\_cpuid} \land \text{has\_osxsave} \land \text{xcr0\_xmm} \land \text{xcr0\_ymm} \land \text{has\_avx} \land \text{has\_avx2} \land \text{has\_fma}$$
- Omitting `osxsave` or `xcr0_ymm` fails **T21-G03** (caught by negative control & mutation `M_CAP_DROP_XCR0`).
- Omitting independent `has_fma` check fails **T21-G03** (since Track 21A AVX2 TU uses `_mm256_fmadd_ps`, missing FMA MUST fall back to `SCALAR_C99` or fail if AVX2_REQUIRED, never execute `vfmadd*`!).
- **PTX Backend (`PTX_SM120`)** checks dynamic loader (`libcuda.so.1` / `nvcuda.dll`), `cuInit(0)`, `cuDeviceGetCount > 0`, `CC >= 8.0` (targeting `sm_120` on Blackwell RTX 5070; fallback/reject clearly if compute cap < target PTX), and verified PTX module load (`cuModuleLoadDataEx`).

---

## 7. Execution Provenance Receipt Schema (`track21a.receipt.v1`)

### 7.1 Semantic Distinctions (Normative Honest Labeling)
1. **Hash Integrity (`SHA-256`):** Detects accidental corruption or naive field tampering when recomputed over the canonical receipt payload and I/O buffers. **Not** forge-resistant against an adversary who can modify both data and `receipt_digest_sha256`.
2. **Symmetric Attestation (`HMAC-SHA256`, optional):** When a verifier secret key is supplied (`ZCC_ATTEST_HMAC_KEY`), binds an execution receipt tag under `attestation_kind = "hmac_sha256"`; otherwise explicitly `"unkeyed_sha256_integrity"`.
3. **Deterministic Replay (`T21-G08`):** Re-executing the identical binary, backend, options, seed, and input digests reproduces bitwise-identical output SHA-256 on the same hardware/backend. Distinct from cross-backend numeric equivalence.
4. **Cryptographic Computation Proof (ZK-STARK):** Outside Track 21A runtime scope (`src/zk/zk_stark_emit.c` emits Goldilocks STARK calldata for compiler IR only; Track 21A sets `zk_proof_present = false`). Receipts MUST NEVER claim ZK proof when only SHA-256 integrity was computed.

### 7.2 Required Receipt Fields
- `schema_version`: `"track21a.receipt.v1"`
- `contract_version`: `"1.0.0"` (`0x00010000`)
- `compiler_id` & `compiler_sha256`
- `executable_sha256`
- `ptx_module_sha256` (if `PTX_SM120`, else `"none"`)
- `backend_requested`, `backend_executed`, `selection_policy`, `fallback_occurred`, `selection_reason`
- `hardware_caps`: `{cpuid_avx2, osxsave, xcr0_ymm, fma, f16c, xcr0_hex, gpu_name, gpu_cc}`
- `op_id`, `op_name`, `dtype`, `accum_policy`, `determinism_mode`, `seed`
- `shape_summary` (`M, N, K` / input shapes/strides/alignment)
- `inputs_descriptor_sha256`, `input_buffers_sha256`, `output_descriptor_sha256`, `output_buffer_sha256`
- `status_code`, `execution_status` (`"OK"` | `"ERR_..."`), `error_message`
- `verification_status` (populated by independent verifier ONLY — runtime writes `"UNVERIFIED_RUNTIME_ONLY"` per **ZCC-EF-0** so runtime NEVER self-certifies PASS!)
- `integrity_kind` (`"unkeyed_sha256_integrity"` | `"hmac_sha256_attested"`), `zk_proof_present` (`false`)
- `receipt_canonical_sha256`: FIPS 180-4 SHA-256 over the canonical serialized receipt body preceding `receipt_canonical_sha256`.

---

## 8. Release Gates (`T21-G03 .. T21-G10`, Deferring `G01..G02`)

| Gate | Proposition & Blocking Requirement |
| --- | --- |
| `T21-G01` | SPIR-V emission — **EXPLICITLY DEFERRED to Track 21B** |
| `T21-G02` | Vulkan compute execution — **EXPLICITLY DEFERRED to Track 21B** |
| `T21-G03` | `CPUID + OSXSAVE + XCR0[2:1] + FMA` independent probing + zero AVX opcodes in portable objects (`objdump` check) + simulated pre-AVX2/no-OSXSAVE/no-FMA fallback without `SIGILL` |
| `T21-G04` | Scalar C99 reference execution compiles cleanly with both GCC (`-mno-avx -mno-avx2 -mno-fma`) and `./zcc`, matching independent FP64 oracle |
| `T21-G05` | Differential numerical equivalence (`SCALAR_C99` vs `AVX2_FMA` vs `PTX_SM120` vs FP64 oracle) across normal, boundary, nonaligned, extreme magnitude, invalid shape, NaN/Inf, OOM |
| `T21-G06` | Memory safety: bounds checks, readonly output guard, aliasing guard, integer overflow guard, OOM cleanup, GCC `-fsanitize=address,undefined` zero errors |
| `T21-G07` | Silent Failure Observatory: real injected mutations across all 8 fault classes (`>= 1` mutant per class) executed and killed with honest unknown-signature preservation |
| `T21-G08` | Defined deterministic replay (`SCALAR_C99`, `AVX2_FMA`, `PTX_SM120` repeated runs -> bitwise identical `output_buffer_sha256`) |
| `T21-G09` | Receipt tamper detection: independent verifier rejects modified output hash, input hash, backend ID, status code, canonical hash, schema downgrade, and runtime self-certified `"PASS"` |
| `T21-G10` | Independent fail-closed release verifier (`--self-test` proves verifier fails when evidence is missing, stale, malformed, contradictory, or unverified; enforces existing ZCC `selfhost` & `verify-differential` non-regression) |
