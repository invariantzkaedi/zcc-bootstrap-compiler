# ZCC: High-Integrity Multi-Target C Compiler, EVM Engine & Sovereign Systems Suite

[![ZCC Self-Host Verification](https://github.com/invariantzkaedi/zcc-bootstrap-compiler/actions/workflows/selfhost.yml/badge.svg)](https://github.com/invariantzkaedi/zcc-bootstrap-compiler/actions/workflows/selfhost.yml)
[![Quantum Verification Assurance](https://github.com/invariantzkaedi/zcc-bootstrap-compiler/actions/workflows/quantum-ci.yml/badge.svg)](https://github.com/invariantzkaedi/zcc-bootstrap-compiler/actions/workflows/quantum-ci.yml)
[![ZCC Boundary Contract Gates](https://github.com/invariantzkaedi/zcc-bootstrap-compiler/actions/workflows/gate-ir1.yml/badge.svg)](https://github.com/invariantzkaedi/zcc-bootstrap-compiler/actions/workflows/gate-ir1.yml)
[![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Stage 2-3 Byte Identical](https://img.shields.io/badge/Bootstrap-Byte--Identical%20Verified-brightgreen.svg)](https://github.com/invariantzkaedi/zcc-bootstrap-compiler)

**ZCC** is a high-assurance, multi-target, self-hosting C compiler, bare-metal operating system kernel toolchain, and EVM (Ethereum Virtual Machine) translation engine. Engineered for cryptographic determinism, mathematical verifiability, and multi-stage self-hosting validation, ZCC bridges native systems-level compilation across modern architectures with formal execution tracing and symbolic verification.

---

## 🔱 Key Capabilities & Core Subsystems

* **Triple-Stage Self-Hosting Verification**: Compiles itself across 3 distinct generations (`zcc` $\rightarrow$ `zcc2` $\rightarrow$ `zcc3`), proving byte-for-byte assembly and binary identity (`cmp zcc2.s zcc3.s`).
* **Multi-Target Direct Backend Suite**:
  * **x86-64 Linux (System V AMD64 ABI)**: Native AST-direct and 3-address SSA IR backends.
  * **Win64 Direct PE32+ Emitter**: Generates standalone Windows executables (`.exe`) with DOS/COFF headers and Section Table relocations without external linkers.
  * **RISC-V 64-bit (RV64GC)**: Direct psABI register mapping and 16-byte stack frame alignment.
  * **ARM64 (AArch64 AAPCS64)**: Native register-based parameter lowering and instruction selection.
  * **WebAssembly (WASM32-WASI)**: Direct `.wasm` bytecode emission with LEB128 encoding and structured stack framing.
* **Bare-Metal Kernel & Native Linker (`zld`)**:
  * Bundles `zkernel`, a freestanding x86-64 Multiboot2 OS kernel featuring PMM, identity/higher-half paging, IDT exception handling, and COM1 serial telemetry.
  * Native self-hosting ELF linker (`zld`) capable of linking standalone executables and OS kernels with QEMU bare-metal verification.
* **256-bit EVM Lifter & Decompilation Engine**:
  * Translates EVM bytecode into SSA IR and native x86-64 machine code.
  * **SwarmDecompile**: Fuzzed across 5,000+ real-world Ethereum smart contracts.
  * Formal symbolic verification (`--prove no-revert`) to mathematically assert contract safety and absence of unhandled reverts.
* **Quantum & Post-Quantum Cryptographic Assurance**:
  * State-vector quantum simulator verifying unitary gate conservation ($U^\dagger U = I$) modeling 3-stage bootstrap convergence.
  * Kyber-1024 Number Theoretic Transform (NTT) polynomial arithmetic and bare-metal speculation barriers (SLS, retpolines).
* **High-Assurance Systems Verification**: Compiles complex industry workloads including **SQLite 3.53.1**, **Lua 5.4.6**, and **DOOM 1.10**.

---

## 🔱 Quick Start

```bash
# Clone the repository
git clone https://github.com/invariantzkaedi/zcc-bootstrap-compiler.git
cd zcc-bootstrap-compiler

# Build ZCC compiler
make zcc

# Run triple-stage compiler bootstrap verification (Stage 2 <-> Stage 3 identity)
make selfhost

# Compile freestanding bare-metal kernel and verify boot in QEMU
make -C kernel verify

# Execute multi-target test gauntlets (Win64 PE, RV64GC, ARM64, Quantum, SQLite)
make m10-verify
make quantum-test
```

---

## 🔱 Verification & Milestone Status

| Metric / Target | Status | Verification Evidence |
| :--- | :--- | :--- |
| **Self-Hosting (Gate 1)** | **PASS** | `zcc2.s` and `zcc3.s` byte-identical (`cmp` exit code 0) |
| **ZXR Attestation Loop** | **PASS** | Merkle root & topology audit verified (`Attestation: VALID`) |
| **Freestanding Kernel (`zkernel`)** | **PASS** | Stage 2 & 3 ELF binaries boot in QEMU with COM1 handshake (`ZKAEDI_V2_BOOT_SUCCESS`) |
| **Self-Hosted Linker (`zld`)** | **PASS** | `zld-zcc` links bootable OS kernels (`=== ZLD SELF-HOST VERIFIED ===`) |
| **SQLite 3.53.1 Amalgamation** | **PASS** | Resolved `SQL-CRASH-38060` via native 424-byte `Parse` layout & float limits constant folding |
| **Lua 5.4.6 VM** | **PASS** | 100% compliance on core `testes/all.lua` VM test suite |
| **DOOM 1.10** | **PASS** | `linuxdoom-1.10` compiles, links, parses WADs, and renders framebuffer frames cleanly |
| **Direct Win64 PE32+ Emitter** | **PASS** | Emits valid `.exe` binaries with DOS `MZ` + `PE00` headers and 4K section alignment |
| **RISC-V (RV64GC)** | **PASS** | Verified register assignment, floating-point load/store, and psABI compliance |
| **Quantum State-Vector Gate** | **PASS** | Unitary conservation verified within $10^{-6}$ tolerance |
| **Post-Quantum Kyber Lattice** | **PASS** | Polynomial NTT multiplication, ring reduction modulo $q=3329$, and KEM verified |

---

## 🔱 Workload Integration Details

### 1. SQLite 3.53.1 (85,000+ Lines)
ZCC compiles the monolithic SQLite amalgamation out of the box:
* **Container ABI Parity**: Uses native System V layout offsets (`sizeof(Parse) = 424`, `offsetof(Parse, sLastToken) = 288`) to prevent memory truncation.
* **Float Limits Initializer Parity (`CG-GINIT-FLOAT-002`)**: Evaluates static constant expressions including `INFINITY`, `DBL_MAX`, `1.0f/2.0f`, `NAN`, and subnormals.
* **Transactional Parity**: Executes complete SQL workflows (B-Tree allocations, page cache management, dynamic schema updates).

### 2. Lua 5.4.6 (30,000+ Lines)
Passes Lua's complete test harness (`testes/all.lua`). Confirms stability of garbage collection (`gc.lua`), debug reflection (`db.lua`), lexical closures (`closure.lua`), numeric ranges (`math.lua`), and coroutine yields.

### 3. id Software DOOM 1.10 (45,000+ Lines)
Compiles and links the classic `linuxdoom-1.10` engine (`scripts/build_doom.sh`), parsing game WADs and executing the rendering loop without pointer corruption or alignment faults.

### 4. Metacompiler Chain
ZCC compiles third-party C compilers (such as TinyCC), which recursively compile verified executables:
```text
ZCC ──> Compiles tinycc.c ──> Generates tcc binary
tcc ──> Compiles "int main() { return 42; }" ──> Generates target binary
target binary ──> Returns exit code 42
```

---

## 🔱 Architectural Layout

The ZCC compiler core is modularly organized into distinct parts concatenated into a unified `zcc.c` translation unit:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            zcc.c (Concatenated Core)                         │
│                                                                             │
│  part1.c ───────── Type system, symbols, scopes, memory allocators          │
│  part0_pp.c ────── C macro preprocessor, macro expansion, header resolver   │
│  part2.c ───────── Lexical scanner, token mapping, literal decoding         │
│  part3.c ───────── Recursive descent parser for statements & expressions    │
│  ir.h ──────────── SSA Intermediate Representation definitions              │
│  ir_emit_dispatch.h  Dispatch tables for AST-to-IR translation              │
│  part4.c ───────── x86-64 System V ABI code generator & static evaluator     │
│  zcc_ast_serializer.c  Topological AST serialization                        │
│  part5.c ───────── Compiler driver, CLI parser, peephole optimizer          │
│  part7_rust.c ──── Rust frontend binding & tokenization smoke               │
│  part6_arm.c ───── ARM64 code generation & register allocation              │
│  ir.c ──────────── IR module construction & optimization                    │
│  ir_to_x86.c ───── 3-Address IR lowering to x86-64 machine instructions     │
│  regalloc.c ────── Linear scan & graph coloring register allocator          │
├─────────────────────────────────────────────────────────────────────────────┤
│                    Multi-Target & Systems Components:                       │
│                                                                             │
│  src/win64_pe_emit.c ── Direct Windows PE32+ executable generator           │
│  src/riscv_codegen.c ── RISC-V 64-bit (RV64GC) code generator               │
│  src/arm64_codegen.c ── ARM64 (AArch64 AAPCS64) code generator              │
│  src/wasm_emit.c ────── WebAssembly (WASM32-WASI) emitter                   │
│  src/zld.c ──────────── Custom standalone ELF linker                        │
│  kernel/ ────────────── Freestanding x86-64 Multiboot2 OS kernel            │
│  src/evm/ ───────────── 256-bit EVM lifter, JIT compiler, and symbolic VM  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 🔱 Multi-Target Compilation Guide

### 1. Compile Standard C Program
```bash
./zcc hello.c -o hello.s
gcc -o hello hello.s
./hello
```

### 2. Emit Direct Windows PE Executable (`.exe`)
```bash
./zcc -target win64-pe hello.c -o hello.exe
```

### 3. Emit Freestanding Object Files
```bash
./zcc -emit-obj -c kernel_module.c -o kernel_module.o
```

### 4. Compile and Run with EVM JIT / Symbolic Prover
```bash
# Compile and run EVM bytecode via native x86-64 JIT
./zcc --jit contract.bin -o contract.exe

# Prove that a contract never encounters an unhandled revert
./zcc --prove contract.bin "no-revert"
```

---

## 🔱 Interactive Visual Observatories

ZCC includes web-based visual cockpits for live compiler telemetry, AST inspection, Hamiltonian optimization, and systems physics:

* **God's Eye Observatory** (`GODS_EYE_OBSERVATORY.html`): Real-time compiler telemetry, register allocation graph coloring visualizer, and quantum state monitor.
* **Hamiltonian Phase-Space Cockpit** (`dashboard_hamiltonian_visualizer.html`): Visual solver for recursive Hamiltonian energy optimization and parameter trajectories.
* **Procedural World & Animation Engines** (`procedural_world_gen.html`, `zkaedi_prime_animation_engine.html`, `universal_app_creator_prime.html`).
* **Technical Architecture Podcast Spec** ([`PODCAST_NOTEBOOKLM_ZCC_SPEC.md`](file:///H:/__DOWNLOADS/zcc_github_upload/PODCAST_NOTEBOOKLM_ZCC_SPEC.md)): Comprehensive deep-dive specification for audio synthesis and technical briefings.

To launch the local observatory:
```bash
python3 -m http.server 8080
# Open http://localhost:8080/GODS_EYE_OBSERVATORY.html in any browser
```

---

## 🔱 Supported C Language Specifications

* **Primitive Types**: `char`, `short`, `int`, `long`, `long long` (signed/unsigned), `float`, `double`, `_Bool`, `void`.
* **Derived Types**: Multi-dimensional arrays, multi-level pointers, structures (with alignment & packing), unions, function pointers, and `typedef` chains.
* **Control Flow**: `if`/`else`, `switch`/`case`/`default`, `while`, `do`/`while`, `for`, `goto`, `break`, `continue`, `return`.
* **Expressions**: Complete C operator precedence tree, compound assignments, pre/post inc/dec, ternary (`?:`), comma operator, `sizeof`, explicit casting, member dereferencing (`.`, `->`), variable argument macros (`va_list`, `va_start`, `va_arg`).
* **Integrated Preprocessor**: Macro substitution, object-like and function-like macros, stringification (`#`), token pasting (`##`), `#include`, conditional compilation (`#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`, `#pragma once`).

---

## 🔱 License & Governance

ZCC is open-source software licensed under the **[Apache License 2.0](LICENSE)**.

Developed and maintained by **ZKAEDI** ([zkaedi.ai](https://zkaedi.ai) | [GitHub: invariantzkaedi](https://github.com/invariantzkaedi)).
