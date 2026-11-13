# Evidence Artifacts — Quad-ISA Differential Gate & Live Bridge Wiring (`2026-11-12`)

| Artifact Path | Size (Bytes) | SHA-256 Checksum | Role |
| :--- | ---: | :--- | :--- |
| `zcc2.s` / `zcc3.s` | 10,955,179 | `e32ad1c2ae636c1cc0cc54285186346ade67e96affe01ee67d5f045e9d289e53` | Stage 2 == Stage 3 converged bootstrap assembly |
| `zcc2` / `zcc3` | — | `de143d8157bfd676ea6b34af33fd7871658564f52513f72c99084bbd0fac0f8f` | Stage 2 == Stage 3 converged compiler binaries |
| `src/evm/yul_weaver.c` | 11,180 | `0ee8b8f565207ff5cedebfb2ff1e34f0ce0068ed8af6a4a42029471140a916a2` | Cleaned incomplete diff-splice restoring `make selfhost` |
| `tests/zcc_quad_isa_torture.c` | 13,240 | `bb56e978212ce3ca664628a300a9775e21055b4fcfa17f7910524e299aceb140` | Stratified 3-Tier (10-kernel) C99 torture TU |
| `tools/zcc_hyperdim_ui_parts.py` | 156,368 | `cbd4f635708b3af9b53d7ec312a09ac6abd31e7b3732fed38dea591f7984f130` | Zero-dependency HTML/CSS/JS UI & Independent Browser Oracle |
| `tools/zcc_quad_isa_gate.py` | 64,888 | `05075b9fd60e8b9dc82fd242c92c1b767b4d70b615c8a7c5c79d5c8e7b54826e` | 7-Lane × 3-Tier QEMU/GCC/Clang/WASM32 gate + `:8799` bridge |
| `zcc_hyperdim_test_harness.html` | 307,814 | `aa1ae1944ccf92d9a808051f8e5499fdb78cf908cd84edfbccd6d35726320266` | Generated standalone + live-bridge Quad-ISA harness |
| `artifacts/ZCC_QUAD_ISA_GATE_RECEIPT.json` | 9,318 | `59d1017a5b6f751554df8288249d2fad34924667b3d2622954b1990b24f8a1e0` | Merkle-stamped (`seq=622`) 3-Tier × 7-Lane gate receipt |

