# Evidence Artifacts: Sovereign Full-Spectrum Hardening, Enchantment & Verification
Date: 2026-09-13
Topic: sovereign_enchantment
Protocol: ZCC Supercharged v2 & Glazed Brick Standard Level 5

## Key Intermediate & Generated Artifacts

| Component | Path | SHA-256 / Checksum | Status |
|-----------|------|--------------------|--------|
| Verifier Contract | `d:/zkaedi-prime-omega-release/contracts/ZkaediLeviathanPoUWVerifier.sol` | Updated with `attestMerkleRoot` & `verifyCompilerProof` | VERIFIED |
| Verifier Binary | `d:/zkaedi-prime-omega-release/contracts/build/ZkaediLeviathanPoUWVerifier.bin` | 14,002 bytes | COMPILED (`solc v0.8.26 --via-ir --optimize`) |
| Verifier ABI | `d:/zkaedi-prime-omega-release/contracts/build/ZkaediLeviathanPoUWVerifier.abi` | 8,022 bytes | VALIDATED |
| Deployment Bytecode | `d:/zkaedi-prime-omega-release/contracts/deploy_bytecode.json` | 14,068 hex chars (includes enclave constructor argument) | REGENERATED |
| Web Portal | `d:/zkaedi-prime-omega-release/contracts/deploy_to_sepolia.html` | Rule EF-7 (All 5 Gates Passed, 0 errors) | ENCHANTED |
| STARK Calldata | `d:/zkaedi-prime-omega-release/contracts/live_compiler_stark_proof.json` | Merkle Root: `0x85ad3ffe71217e056286a14abd1d2e9949bf1298f9e2176afcb6023a9e0bd1dd` | VERIFIED |
| Master Manifest | `d:/zkaedi-prime-omega-release/data/curated/curated_manifest.json` | `77dd9771caabbc611c21dc0f4b2b4ad320d852879fd11732dc10fdc0914257fd` (86.7 MB, 182,246 files) | AUDITED |
| Manifest Seal | `d:/zkaedi-prime-omega-release/data/curated/CURATED_MANIFEST_SEAL.json` | Partition Merkle Root: `0xdb6ea96eef118d3f99d7ba1133984b1989eca88c14a01776005d8308c548809c` | CRYPTOGRAPHICALLY SEALED |
| Backup DPO Audit | `c:/Users/zkaed/Downloads/dpo_pairs.jsonl.bak` | 108 lines, `150b13239960a77294408c244c9e38dad2b678269a9efd2d1e911a62b407353c` | AUDITED |
| TinyCC Expr Test | `tests/test_tcc_expr.c` | Deterministic sum=81598 | PASS (ZCC + GCC identical) |
| TinyCC Tok Test | `tests/test_tcc_tok.c` | Deterministic total=290 | PASS (ZCC + GCC identical) |
