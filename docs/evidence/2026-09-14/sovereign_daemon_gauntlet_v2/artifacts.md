# Key Artifacts & Checksums: Sovereign Daemon & Gauntlet V2
Date: 2026-09-14

## Source & Tooling Checksums
| File Path | MD5 Hash | Purpose |
| :--- | :--- | :--- |
| `zkaedi_prime/daemon.py` | `7b9a0328826f0db3389d5eea0b78315e` | Persistent VRAM server with UDS & TCP IPC |
| `zkaedi_prime/cli.py` | `d1ab8df72f96a945b0a9eb0fc4cb4130` | CLI frontend with auto-detecting IPC client & ZCC bridge |
| `zkaedi_prime/code_grammar_engine.py` | `073e6a9f9ff0a3ac79729dcb958d10d7` | C99 pushdown automaton, FIM clamp, struct rule & BPE space fix |
| `tools/prime/run_sovereign_gauntlet_v2.py` | `254656c43de52fcf4a544ec9e69e22b5` | 5-workload stress and cryptographic test harness |
| `tests/test_zkaedi_prime_daemon.py` | `fa2efce76e46a5e34cb2a900f3b03df9` | Daemon lifecycle unit test suite |
| `reports/SOVEREIGN_GAUNTLET_V2_REPORT.json` | `2ed2a709a12ca06b4bd2d32a2040a7e6` | Machine-readable audit report for 5/5 gauntlet pass |

## Self-Host Assembly Checksums
| Stage | MD5 Hash | Status |
| :--- | :--- | :--- |
| `zcc2.s` | `7f57aec6245d941b66b55dd7340634b3` | Bit-identical |
| `zcc3.s` | `7f57aec6245d941b66b55dd7340634b3` | Bit-identical |
