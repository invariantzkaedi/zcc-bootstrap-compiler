# ZXR Attestation Milestone Gates

This document freezes the compile-time gates for the Phase D-19 milestone release.

---

## 1. Self-Host Parity Gate (`make selfhost`)

Exact output showing Stage 2 compiling Stage 3 to byte-identical codegen parity:

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

---

## 2. Makefile Integration Gate (`make verify-attestation`)

Exact output showing the unified verification gate run:

```text
=== Running ZXR Attestation Pipeline Gate ===
./tools/zcc_topology_auditor kernel/*.o --json > record_test.zxr
./tools/zcc_zxr_verify record_test.zxr kernel/*.o
Source Hash:      PASS
Object Hash:      PASS
Topology Hash:    PASS
Merkle Root:      PASS
Build ID:         PASS
Schema:           PASS

Attestation:
VALID
=== ZXR Attestation Pipeline Gate: VERIFIED ===
```
