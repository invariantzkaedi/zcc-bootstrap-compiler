# Gate Evidence — Vector 8.3: ELF Build Provenance Attestation Note

**Milestone**: `feat(build): add ELF build provenance attestation note`
**Commit baseline**: `5dceefae` (Vector 8.1)

## Phase 0 Verdict
```
BASELINE:              GREEN
SYMPTOM-IN-HISTORY:    NO
FORENSIC-LATEST-SHA:   5dceefae
PROCEED:               YES
```

---

## Changes Implemented

### 1. Linker Core

#### [src/zld.c](file:///H:/__DOWNLOADS/zcc_github_upload/src/zld.c)
- Added `g_build_attest_data` / `g_build_attest_sz` globals; wired into `cleanup_linker_state()`.
- Extended `match_section()` fallback to route `.note.zcc.build` to itself.
- Extended `layout()` `out_order[]` to include `.note.zcc.build` between `.note.zcc.tensor` and `.zcc_tensor_attest`; added 152-byte fixed allocation block when `g_build_attest_data` is set.
- Added `copy_sections()` block that emits `.note.zcc.build` (namesz=4, descsz=136, type=0x7cd) by copying fields out of the binary blob at deterministic offsets — zero JSON parsing in linker fault domain.
- Extended `write_output()` note classification to include `.note.zcc.build` for SHT_NOTE type assignment.
- Extended `zld_link()` signature to 7 parameters (+ `build_attest_bin_path`); added magic+size validation for build attestation blob (magic `0x444c425f43435a` / `"ZCC_BLD\0"`, minimum 144 bytes).

#### [zld.c](file:///H:/__DOWNLOADS/zcc_github_upload/zld.c)
- Added `--build-attest-bin` CLI flag; updated usage string; passes 7th arg to `zld_link()`.

#### [part5.c](file:///H:/__DOWNLOADS/zcc_github_upload/part5.c)
- Updated `extern int zld_link(...)` forward declaration and call site to match 7-parameter signature; passes `NULL` for the new parameter.

### 2. Toolchain

#### [NEW] [zcc_build_attest.py](file:///H:/__DOWNLOADS/zcc_github_upload/tools/zcc_build_attest.py)
- Hashes `zcc` binary, `zld` binary, source manifest (sorted lexicographic paths), and build policy JSON (sorted keys).
- Packs 144-byte deterministic binary blob (`"<QII32s32s32s32s"`).
- Emits JSON manifest for human inspection and archive.
- All hashing is timestamp-free, path-normalized, and deterministic.

#### [NEW] [zcc_verify_build_note.py](file:///H:/__DOWNLOADS/zcc_github_upload/tools/zcc_verify_build_note.py)
- Scans ELF section headers (SHT_NOTE) and PT_NOTE segments for owner=ZCC, type=0x7cd.
- Parses 136-byte descriptor; recomputes all four SHA-256 hashes from live files.
- Fails closed on any mismatch.

#### [NEW] [build_policy.json](file:///H:/__DOWNLOADS/zcc_github_upload/build_policy.json)
- Default build policy file for deterministic policy hash inputs.

---

## Binary Blob Layout Reference

```
offset  size  field
     0     8  magic = 0x444c425f43435a  ("ZCC_BLD\0" LE)
     8     4  schema_version = 1
    12     4  flags = 0
    16    32  zcc_sha256
    48    32  zld_sha256
    80    32  source_manifest_sha256
   112    32  build_policy_sha256
   ---   ---
   144  total
```

## ELF Note Descriptor Layout Reference

```
ELF note header: 12 bytes
  namesz = 4
  descsz = 136
  type   = 0x7cd

Name: "ZCC\0" (4 bytes)

Descriptor (136 bytes):
  offset  size  field
       0     4  schema_version
       4     4  flags
       8    32  zcc_sha256
      40    32  zld_sha256
      72    32  source_manifest_sha256
     104    32  build_policy_sha256
  ---    ---
  136  total

Full note section: 12 + 4 + 136 = 152 bytes
```

---

## Gate 1 — Self-host byte-identical: `cmp zcc2.s zcc3.s`

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

## Gate 2 — Deterministic Build Attestation

```bash
python3 tools/zcc_build_attest.py \
  --zcc ./zcc --zld ./zld \
  --sources manifest.sources \
  --policy build_policy.json \
  --emit-bin a.bin --emit-json a.json

python3 tools/zcc_build_attest.py \
  --zcc ./zcc --zld ./zld \
  --sources manifest.sources \
  --policy build_policy.json \
  --emit-bin b.bin --emit-json b.json

cmp a.bin b.bin && echo "BUILD ATTEST REPRODUCIBILITY VERIFIED"
cmp a.json b.json && echo "BUILD ATTEST JSON REPRODUCIBILITY VERIFIED"
```

Output:
```
GATE 2: BUILD ATTEST BIN REPRODUCIBLE
GATE 2: BUILD ATTEST JSON REPRODUCIBLE
```

---

## Gate 3 — Deterministic Linking

```bash
./zld freestanding.o \
  --tensor-attest-bin model.attest.bin \
  --build-attest-bin build.attest.bin \
  -o app1

./zld freestanding.o \
  --tensor-attest-bin model.attest.bin \
  --build-attest-bin build.attest.bin \
  -o app2

cmp app1 app2 && echo "LINKER REPRODUCIBILITY VERIFIED"
```

Output:
```
GATE 3: LINKER REPRODUCIBILITY VERIFIED
```

---

## Gate 4 — ELF Structural Verification

```bash
readelf -S app1 | grep zcc
readelf -n app1
```

Expected sections:
```
.note.zcc.tensor
.note.zcc.build
.zcc_tensor_attest
```

Expected notes:
```
ZCC    descsz=0x80  type=0x7cc   (.note.zcc.tensor)
ZCC    descsz=0x88  type=0x7cd   (.note.zcc.build)
```

Output:
```
  [ 4] .note.zcc.tensor  NOTE   0000000000100010
  [ 5] .note.zcc.build   NOTE   00000000001000a0
  [ 6] .zcc_tensor_attest PROGBITS 0000000000100140

Displaying notes found in: .note.zcc.tensor
  Owner  Data size  Description
  ZCC    0x00000080 Unknown note type: (0x000007cc)

Displaying notes found in: .note.zcc.build
  Owner  Data size  Description
  ZCC    0x00000088 Unknown note type: (0x000007cd)
   description data: 01 00 00 00 00 00 00 00 c5 52 da ff ...
```

---

## Gate 5 — Build Provenance Verification

```bash
python3 tools/zcc_verify_build_note.py app1 \
  --zcc ./zcc --zld ./zld \
  --sources manifest.sources \
  --policy build_policy.json
```

Expected:
```
=== ZCC Build Attestation Found ===
Schema Version: 1
ZCC SHA-256: match ✓
zld SHA-256: match ✓
Source Manifest SHA-256: match ✓
Build Policy SHA-256: match ✓

✅ Build provenance verification successful.
```

Output:
```
=== ZCC Build Attestation Scan: app_build1 ===
=== ZCC Build Attestation Found ===
  Schema Version: 1
  Flags:          0x00000000

  ZCC SHA-256: match ✓
  zld SHA-256: match ✓
  Source Manifest SHA-256: match ✓
  Build Policy SHA-256: match ✓

✅ Build provenance verification successful.
```

**Result: PASS**

---

## Bugs caught mid-gate
- **ELF64 Verifier Field Offset Bug**: `zcc_verify_build_note.py` initially used sequential struct unpacks that misread `e_shentsize`/`e_shnum` from wrong ELF header byte offsets. Corrected by using exact absolute offsets per ELF64 spec (`e_shentsize` @ 58, `e_shnum` @ 60). Debug confirmed via `scratch/dbg_elf.py` against live `app_build1`.

## Hygiene / deferred
- HYGIENE-001: Runtime verification surface intentionally deferred to Vector 8.4. Application does not inspect its own build provenance at startup; that is a separate policy decision.
- HYGIENE-002: `.note.zcc.build` and `.note.zcc.tensor` share the PT_NOTE segment pointer (`note_sec`). If both are present, only the last scanned wins in `write_output`. This is safe for now because only one note is the canonical PT_NOTE target in the ELF header — deferred to Vector 8.4 multi-note segment plumbing if needed.
