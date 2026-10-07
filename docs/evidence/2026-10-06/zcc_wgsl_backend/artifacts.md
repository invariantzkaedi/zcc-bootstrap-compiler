# Artifact Inventory: ZCC Native C-to-WGSL WebGPU Backend

| Artifact Path | Format | Size (Bytes) | SHA-256 Checksum | Description |
|---|---|---|---|---|
| `include/zcc_wgsl.h` | C Header | 2,752 | `4e26ebf8b44917578ec09dd0e4ad9b3fe11ad552291583d739818817730e7fe7` | Public C API for WGSL lowering & validation |
| `src/backend/ir_to_wgsl.c` | C Source | 19,465 | `6022e2ce23d04e5dcaefab3b8c368d0676735503046bc6594d2ba85387429676` | ZCC SSA IR to WGSL emitter with bounds guards |
| `tests/test_zcc_wgsl_hardening.c` | C Source | 6,767 | `e4f40fbbd0979603e839e559381e46c77a94d039f727339d1b0928e18db7dfc7` | 5-stage pre/post hardening test suite |
| `tools/zcc_wgsl_cli.c` | C Source | 6,108 | `786ca1c42db1415177431e7845f69c5750278788e0e674a2cb5dd5c48b29f7cf` | CLI compiler (`bin/zcc_wgsl`) |
| `artifacts/wgsl/vec_add.wgsl` | WGSL Shader | 1,598 | `05cfee63d8cd0ca43f1a95bd4fca8515da64971161f4ae5d7afbaf4d72871c18` | Emitted vector add compute shader |
| `artifacts/wgsl/vec_add.wgsl.js` | JS Bridge | 1,282 | `574b99902f2db66ba7d0c442d5d233c3c3ba37e035d1e39a024a44d49bc7f32f` | Emitted WebGPU JavaScript launch bridge |
| `artifacts/wgsl/vec_fma.wgsl` | WGSL Shader | 1,753 | `07bf90ee59c91f7b4487a1741f5b6c4f2c58389eb4035ebf4f999c7e8227146f` | Emitted vector FMA compute shader |
| `artifacts/wgsl/vec_fma.wgsl.js` | JS Bridge | 1,282 | `6db3c0290b210d2e40a8ec821ebe8b0fcbef3a54eb458af1761c3d0a73e4f746` | Emitted WebGPU JavaScript launch bridge |
| `artifacts/wgsl/quantum_step.wgsl` | WGSL Shader | 1,819 | `ba6cb31b804f46d7459c996ba37080bdc3f9a1236f50308cf86a8b2b8e5daf87` | Emitted 3D CTQW quantum lattice compute shader |
| `artifacts/wgsl/quantum_step.wgsl.js` | JS Bridge | 1,308 | `044c9f52649a1d84b54d7c2b7d7e4dd8ed9b994301f07b9abf36a771c577417e` | Emitted WebGPU JavaScript launch bridge |
| `H:/artifacts/ZCC_WGSL_BACKEND_RECEIPT.json` | JSON Receipt | 2,330 | `ca9ec4ec7a73f8a0ffbb9c4a45a646c243eb03a5ad779d77f884fbfcfebc94a5` | Cryptographically sealed backend audit receipt |
| `H:/agents/wgsl_test_runner.html` | HTML / WebGPU | 20,412 | `3d649ab749c9527ec56113b2909fdfcf6694605e5264b3ef133b3a6e3ce45d8b` | Standalone physical silicon WebGPU gauntlet runner |
| `H:/agents/enhanced-ui.html` | HTML / WebGPU | 55,230 | `9a75908b98b030b777a83d31fe10bb73cb90e442ee5fc6522ae182183b9cfb09` | Sovereign Enhanced Observatory with Tab 5 acceleration |
