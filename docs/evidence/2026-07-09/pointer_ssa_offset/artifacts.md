# Artifacts for pointer_ssa offset-aware points-to pass

## Changed Files & Checksums
- `src/opt/pointer_ssa.c`: `b2373e55b263e05d69e939cc33ff2419c72a02ee951961ed7e5c701209554cfe`
- `compiler_passes.c`: `0b03c9b8434bcc365deafebb14d4853c085079a0c1567fb45c91b573b342f39c`
- `zcc_test_suite.sh`: `ff6f6dda4bcafa801deee9b44938f676d230970bc2341bef0e036e4b3c341230`

## Temporary Artifacts Checked
- `/tmp/test_ptr_rewrite.s`: Generated assembly for `test_ptr_rewrite.c` showing correct offset tracking and escape gating.
- `/tmp/test_harness`: GCC binary successfully calling `test_rewrite()` and executing correctly with return value 42.
