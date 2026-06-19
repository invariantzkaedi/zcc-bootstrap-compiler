# Gate Evidence — Vector 8: Reproducible GGUF Tensor Pipeline with ELF Attestations

**Milestone**: `feat(gguf): GGUF tensor validation pipeline with two-layer ELF attestations`
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

### 1. Linker Core

#### [zld.c](file:///H:/__DOWNLOADS/zcc_github_upload/zld.c)
- Parsed options `--tensor-attest-bin` and `--tensor-note-json` in the CLI parser.
- Forwarded both options to the static linking entry point `zld_link()`.

#### [src/zld.c](file:///H:/__DOWNLOADS/zcc_github_upload/src/zld.c)
- Modified `zld_link()` signature to take 6 parameters.
- Implemented undefined section symbols pre-collection (`__start_` and `__stop_`) inside `collect_symbols()` to support standard ELF section boundary tracking in runtime binaries.
- Configured section layout, alignment, and sizing for `.zcc_tensor_attest` (32-byte alignment) and `.note.zcc.tensor` (4-byte alignment, 88-byte header size) inside `layout()`.
- Serialized the cryptographic attestation records and ELF note structure (Owner: `"ZCC"`, Type: `0x7cc`, Descriptor containing `manifest_sha256`, `gguf_sha256`, `record_count`, and `policy_version`) in `copy_sections()`.
- Mapped `.note.zcc.tensor` section type as `SHT_NOTE` in `write_output()`.

#### [part5.c](file:///H:/__DOWNLOADS/zcc_github_upload/part5.c)
- Updated the forward declaration and call site of `zld_link` to prevent calling signature mismatches.

#### [src/codegen.c](file:///H:/__DOWNLOADS/zcc_github_upload/src/codegen.c)
- Added checking for `--emit-gguf` inside the main codegen wrapper function to intercept compiler output execution, return `zcc_main` immediately, and bypass downstream linking blocks.

### 2. Verification Runtime

#### [src/zcc_tensor_attest.h](file:///H:/__DOWNLOADS/zcc_github_upload/src/zcc_tensor_attest.h)
- Defined packed structure headers (`ZccTensorAttestHeader` and `ZccTensorAttestRecord`) representing GGUF model attestations.
- Declared the verifier contract API `zcc_verify_tensor_manifest`.

#### [src/zcc_tensor_attest.c](file:///H:/__DOWNLOADS/zcc_github_upload/src/zcc_tensor_attest.c)
- Implemented a dependency-free GGUF header parser and a custom SHA-256 hash calculator.
- Validates the GGUF signature, metadata records, individual tensor shapes/dtypes/offsets/payloads, and global manifest hashes, failing closed on any layout or checksum drift.

---

## Gate 1 — Self-host byte-identical: `cmp zcc2.s zcc3.s`

Verified by executing:
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

## Gate 2 — Linker Reproduction and Stability Check

Linked freestanding application twice with the same attestation binary:
```bash
./zld freestanding.o --tensor-attest-bin model.attest.bin --tensor-note-json model.attest.json -o freestanding_app1
./zld freestanding.o --tensor-attest-bin model.attest.bin --tensor-note-json model.attest.json -o freestanding_app2
cmp freestanding_app1 freestanding_app2 && echo "LINKER REPRODUCIBILITY VERIFIED"
```
Output:
```
LINKER REPRODUCIBILITY VERIFIED
```

**Result: PASS**

---

## Gate 3 — ELF Structural Verification (`readelf`)

Inspected output executable headers and ELF notes:
```bash
readelf -S freestanding_app
readelf -n freestanding_app
```
Output:
```
Section Headers:
  [Nr] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  [ 4] .note.zcc.tensor  NOTE             0000000000100010  00001010
       0000000000000058  0000000000000000   A       0     0     4
  [ 5] .zcc_tensor_attest PROGBITS         0000000000100080  00001080
       0000000000000160  0000000000000000   A       0     0     32

Displaying notes found in: .note.zcc.tensor
  Owner                Data size 	Description
  ZCC                  0x00000048	Unknown note type: (0x000007cc)
   description data: c7 ee f6 0c 60 aa ... 
```

**Result: PASS**

---

## Gate 4 — Target-Specific Provenance Checks

### 1. Verification Success Path
```bash
./infer_runtime_gcc my_model.gguf
```
Output:
```
[infer_runtime] Embedded ELF attestation found at 0x59253fdb04ef (size = 352 bytes)
[infer_runtime] SUCCESS: GGUF weights verification passed!
```

### 2. Verification Fail-Closed (Tampered Payload)
```bash
./infer_runtime_gcc my_model.bad.gguf
```
Output:
```
[infer_runtime] FAILURE: Verification failed (status = 3): GGUF SHA-256 mismatch
```

### 3. Toolchain Pipeline Verifier
```bash
python3 tools/zcc_verify_attest.py freestanding_app my_model.gguf
```
Output:
```
=== ELF Attestation Found: freestanding_app ===
  Record Count: 1
  Manifest SHA-256: c7eef60c60aa8be33c2bbc126668037510b663d0965adc3a8e15559e1b99ee8f
  Expected GGUF SHA-256: 95a09fe23150a9c1f2ab1ecac7779d17f48747151c497a09ae84912398160057
✅ GGUF cryptographic file signature matches ELF attestation.
  - Tensor 'tensor_b' verified: shape=[2] type=F32 payload=OK

✅ Verification SUCCESS! Linked ELF executable matches GGUF tensor pipeline payload.
```

**Result: PASS**

---

## Bugs caught mid-gate
- **Uncollected Section Symbol References**: Linked freestanding programs originally failed with `undefined symbol: __start__zcc_tensor_attest` because undefined section boundary symbols were bypassed during standard symbol collection pass. Resolved by scanning all object files' undefined symbols list and pre-registering symbols starting with `__start_` or `__stop_` into the global linker symbol table.

## Hygiene / deferred
None.
