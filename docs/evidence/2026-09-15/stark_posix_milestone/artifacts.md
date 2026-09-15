# Artifacts: Verifiable C STARK Proof Engine & POSIX Systems Bridge

## Header Interfaces:
- `zcc_sys_includes/unistd.h`: Core POSIX types, process IDs, file descriptors, fork, pipe, execve, _exit.
- `zcc_sys_includes/fcntl.h`: File creation and access modes.
- `zcc_sys_includes/sys/stat.h`: File permissions and type test macros.
- `zcc_sys_includes/sys/types.h`: Scalar types (mode_t, dev_t, ino_t, time_t).
- `zcc_sys_includes/sys/mman.h`: Virtual memory allocation (mmap, munmap, mprotect).
- `zcc_sys_includes/sys/wait.h`: Process waiting and exit macros (wait, waitpid, WIFEXITED, WEXITSTATUS).

## Cryptographic Engines:
- `tools/zk_compiler_stark_attester.py`: BabyBear field arithmetic (p = 2^31 - 2^27 + 1), execution trace evaluation, binary SHA-256 Merkle tree, and independent verifier.

## Test Suites & Harnesses:
- `tests/test_zk_compiler_stark.py`: 5/5 automated unit tests covering field properties, tree authenticity, and negative controls.
- `tests/test_posix_bridge_native.c`: File I/O and process PID validation test.
- `tests/test_posix_advanced_native.c`: mmap/munmap, pipe IPC, and fork/waitpid multiprocessing test.
- `tools/run_posix_gauntlet.py`: Automated dual-suite POSIX test harness under ZCC.
