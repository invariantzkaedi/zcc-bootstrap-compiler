# ZXR Attestation Loop Architecture (D-19 Milestone)

This document describes the architectural layout of the Verification & Intelligence Layer (Phases D-11 to D-18) and the independent Verification loop (Phase D-19) in the ZCC compiler toolchain.

---

## 1. Flow Pipeline

```text
Source Code
   │
   ▼
  ZCC
   │
   ▼
ELF Objects
   │
   ▼
Verification Layer
   ├── ABI validation
   ├── relocation intelligence
   ├── symbol provenance
   ├── topology IR
   ├── security surface scan
   ├── Merkle roots
   └── attestation metadata
   │
   ▼
record.zxr (Topology IR JSON)
   │
   ▼
zcc_zxr_verify
   │
   ▼
VALID / INVALID
```

---

## 2. Component Layout

### A. Shared ELF Substrate (`zcc_elf_parser.h`)
Eliminates duplicate ELF parsing by providing a standardized, zero-allocation parser layout `Elf64_Obj` loaded into memory. Used identically by both `zcc_topology_auditor` and `zcc_zxr_verify`.

### B. Verification & Intelligence Layer
* **Symbol Provenance (D-13)**: Maps functions to files, section names, and computed cryptographically hashed function bytes.
* **Relocation Analysis (D-12)**: Aggregates GOT/PLT/PC32 counters and evaluates compile-time relocation risk.
* **Security Surface Scanner (D-16)**: Profiles writable globals, indirect calls, and interrupt descriptor entry functions.
* **Merkle Topology Builder (D-17)**: Categorizes code modules into logical domains (`Boot`, `Memory`, `Interrupts`, `IO_Serial`, `Console`, `General`) and computes independent Merkle trees. Domain roots are hashed to generate a single deterministic `topology_root` signature.

### C. Sovereign Attestation Ledger (`record.zxr`)
Serializes metadata, telemetry, and structured nodes/edges/signatures into a standardized JSON representation.

### D. Verification Engine (`zcc_zxr_verify`)
A zero-allocation validator reading `record.zxr` and checking compiled binary state on disk against expected attestation metrics.
