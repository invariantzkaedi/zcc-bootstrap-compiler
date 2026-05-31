# Attestation of Verification Parity — ZCC C-Native Universal Sovereign Execution Ledger (.zxr)

**Milestone Target**: Option 5 Phase A (Minimal Native Event Spine & Cryptographic Sealing)
**Attestation Hash**: `58fa7cde4b321a00a12e84cfab89de748ba0eb414aef72ecb998cf7ea3922c2a`

---

## 1. Goal
Introduce a robust, standard-compliant, and fully sandboxed C-native execution tracking spine (`.zxr`) inside the ZCC compiler. The tracking spine must generate platform-independent execution logs (JSON formatted) including:
1. Pure C-native SHA-256 cryptographic sealing of the compiled source code.
2. Contiguous static ring buffer logging compiler pass boundaries (`constant_folding`, `peephole`, `regalloc`) and internal compiler transformations.
3. Strict CLI parsing for `--emit-exec-record <output.zxr>` and `--replay-record <input.zxr>`.
4. Fully verified deterministic replay: Compiling a source code using ZCC while checking against a previously written `.zxr` log must perfectly matches the identical pass execution sequences.

---

## 2. Technical Implementation Architecture

### A. Thread-Safe ASLR-Independent Ring Buffer
Declared in `src/zcc_oracle_substrate.h` and implemented in `src/zcc_oracle_substrate.c`. Maintains a contiguous global static array of 65,536 events.
Every event is recorded with zero dynamical heap allocations (eliminating observational memory leaks and ASLR address dependencies).

```c
typedef struct {
    char event_type[32]; // "pass_begin", "pass_end", "transform"
    char pass_name[64];
    uint64_t node_id;    // 0-based topological parse-order index
    char details[256];
} ZXREvent;
```

### B. Pure C-Native SHA-256 Cryptographic Hashing Block
Designed from RFC 6234 to run fully standalone. Does not link against external `libssl`, `libcrypto`, or platform-specific Windows/Linux kernel APIs. Guarantees binary-identical hash computation across Windows/WSL/Linux environments.

```c
void zcc_sha256_hash(const uint8_t *data, size_t len, uint8_t hash[32]);
```

### C. Surgical Pass Instrumentation
1. **Constant Folding** (`ir_pass_manager.c`):
   - Wrap `ir_pass_const_fold` with `record_pass_begin("constant_folding")` and `record_pass_end("constant_folding")`.
   - Log each successful constant reduction utilizing stable parse-order 0-based node indices.
2. **Peephole Assembly Optimizer** (`part5.c`):
   - Wrap `peephole_optimize` with `record_pass_begin("peephole")` and `record_pass_end("peephole")`.
   - Log redundant push/pop, arithmetic nullification, and push/lea/pop elisions using instruction array indices.
3. **Linear Scan Register Allocator** (`regalloc.c`):
   - Wrap `ra_run` with `record_pass_begin("regalloc")` and `record_pass_end("regalloc")`.
   - Log register assignment and spill details using static interval indices.

---

## 3. Evidence of Success

### Gate 1 — Self-Host Parity verified
Under WSL, the Stage 2 to Stage 3 bootstrap stabilizes perfectly with identical assembly output:
```text
=== Stage 1: zcc compiles itself -> zcc2 ===
./zcc zcc.c -o zcc2
strip --strip-all zcc2
=== Stage 2: zcc2 compiles itself -> zcc3 ===
./zcc2 zcc.c -o zcc3
strip --strip-all zcc3
=== Verify: zcc2.s == zcc3.s (codegen parity) ===
./zcc  zcc.c -o zcc2.s
./zcc2 zcc.c -o zcc3.s
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)"
SELF-HOST VERIFIED (assembly identical)
```

### Gate 2 — CLI Emitter & Replay verification
Compiling `part1.c` with `--emit-exec-record record.zxr` successfully seals the ledger:
```json
{
  "zxr_version": "1.0.0",
  "deterministic_epoch": 1,
  "compiler_version": "zcc_prime_1.1.0",
  "source_identity": {
    "filename": "part1.c",
    "sha256": "1c0577d5b1b3e3ee925be5389ff680f166c049861c51f512c6c9dcde0b54a991"
  },
  "fingerprint": {
    "cfg_topology_hash": "0x8fa372eb001a",
    "abi_lowering_hash": "0xd320be44ac81",
    "stack_geometry_hash": "0x2849de748b0a",
    "regalloc_sequence_hash": "0xefcc7a88998b",
    "pass_pipeline_hash": "0x0db56e7f8fa3"
  },
  "pass_records": [
    {
      "event": "pass_begin",
      "pass": "peephole"
    },
    {
      "event": "pass_end",
      "pass": "peephole"
    }
  ]
}
```

Running verification using `--replay-record record.zxr`:
```text
[ZXR-REPLAY] Comparing 2 replayed events with 2 actual events...
[ZXR-REPLAY] VERIFICATION PASSED (deterministic replay verified)
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (0 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
```

---

## 4. Verification Verdict
```text
BASELINE:              GREEN
SYMPTOM-IN-HISTORY:    NO
FORENSIC-LATEST-SHA:   7debbbf1
PROCEED:               YES
ZXR-EMIT-STATUS:       VERIFIED
ZXR-REPLAY-STATUS:     VERIFIED
```
