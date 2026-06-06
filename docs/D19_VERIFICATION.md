# ZXR Attestation Loop Verification

This document freezes the verification runs and validation proofs for the ZCC sovereign build attestation loop.

---

## 1. Valid Attestation Check (PASS)

Compiling the kernel objects, generating the JSON ledger (`record.zxr`), and verifying:

```bash
make auditor verifier
./tools/zcc_topology_auditor kernel/*.o --json > record.zxr
./tools/zcc_zxr_verify record.zxr kernel/*.o
```

Output:
```text
Source Hash:      PASS
Object Hash:      PASS
Topology Hash:    PASS
Merkle Root:      PASS
Build ID:         PASS
Schema:           PASS

Attestation:
VALID
```

---

## 2. Corrupted Attestation Check (FAIL)

Modifying the `topology_root` signature in `record.zxr` using the corruption test script:

```bash
python3 scratch/corrupt_zxr.py corrupt
./tools/zcc_zxr_verify record.zxr kernel/*.o
```

Output:
```text
Source Hash:      PASS
Object Hash:      PASS
Topology Hash:    PASS
Merkle Root:      FAIL
Build ID:         PASS
Schema:           PASS

Attestation:
INVALID
```

The verifier halts parsing and exits with status code `1`, indicating attestation failure.
