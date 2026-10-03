# Artifacts Inventory

| File | SHA-256 | Purpose |
|------|---------|---------|
| `src/codegen.c` | fd951d4babb496b20a170cb4dfdd3fb5356f28866bffc36759f6d2efefe881c0 | Hardware profiler analyzer & CLI flags (`--profile`, `--profile-json`, `--reproducible`, `--guard-cf`) |
| `src/win64_pe_emit.c` | c7990619783a12be67eadebacc0b3c5111794ee9239c8c87ffd494ce724dd118 | Deterministic TimeDateStamp zeroing & Guard CF DllCharacteristics bit flag |
| `src/win64_pe_emit.h` | 028d4c0eba48c912b38a8bdf9a055abe577e8a6a94771153f01f061081bd7de9 | Header declarations for `g_win64_reproducible` & `g_win64_guard_cf` |
| `tests/test_win64_pe_emitter.py` | 272ff92ebe19663378de08380f2b78a9e768958982f617d0deebb95b5474d38c | Test 33 unit test checking deterministic headers and CFG security |
