# Rust Frontend Foundation

This document records the current Rust v1 frontend/backend subset wired into `zcc`.

## Current Pipeline

- C path remains unchanged: read source -> preprocess -> C lexer/parser -> codegen.
- Rust path is selected for `.rs` input: read source -> Rust lexer/parser -> diagnostics/AST dump -> boundary hooks.

## Language Dispatch Contract

- `FrontendLang` selects frontend by file extension.
- C files continue through existing `Compiler` + C AST pipeline.
- Rust files use `rust_frontend_compile_file(...)` for parse/resolve/typecheck/lower paths and `rust_backend_bridge_compile_file(...)` for `--rust-backend-v1`.

## Rust Subset (Current)

- Tokenizer supports: identifiers, integers, keywords (`fn`, `let`, `return`, `if`, `else`, `while`, `true`, `false`), and punctuation/operators used by the v1 grammar.
- Parser supports:
  - top-level functions with typed params and return (`i32` / `bool`)
  - statements: `let` (including `let mut`), assignment (`name = expr;`), `return`, `if/else`, `while`, expression statements
  - expressions: literals/identifiers, calls, unary `!`, arithmetic, comparisons, `&&`, `||`, parentheses
- Name resolution/typecheck:
  - function + block-local scopes, shadowing, forward function references
  - bool/i32 expression typing across calls, params, and returns
  - mutable assignment checks (`let mut` required for assignment targets)
- Backend v1 (`--rust-backend-v1`) supports runtime codegen for the subset above, including:
  - recursive and mutually recursive calls
  - SysV call ABI with stack args for `>6` parameters
- Diagnostics include stable code + span + hint output.
- `--dump-rust-ast` emits deterministic text AST for golden tests.
- `--dump-rust-ast` remains the compatibility-stable dump (no symbol annotations).
- `--dump-rust-ast-with-symbols` is gated debug output with deterministic symbol IDs.
- `--dump-rust-symbol-table` emits a flat resolver symbol table in source order:
  - `symbol <id> <kind> <name> <file>:<line>:<col> scope=<depth>`

## Strictness Flags

- `--rust-strict-let-annotations`: require explicit type annotation on all `let` bindings.
- `--rust-strict-function-signatures`: require explicit function return type (`-> i32` / `-> bool`).
- `--rust-strict`: convenience flag enabling both strict modes.

## Notes

- C frontend behavior is unchanged.
- `tests/rust/test_rust_frontend.py` + `make rust-front-smoke` is the source-of-truth compatibility gate for this subset.

## Examples

- Strict mode parse/typecheck only:
  - `./zcc tests/rust/strict-let-missing-annotation.rs --rust-strict`
- Backend v1 compile + run:
  - `./zcc tests/rust/run-runtime-call-seven-params.rs --rust-backend-v1 -o /tmp/r7.bin && /tmp/r7.bin; echo $?`

---

## P0.5 — Unsupported features and safety rails (enforced)

**Invariant:** valid-but-unsupported Rust, and parse/type errors, must **fail with a structured diagnostic** before assembly or linking. The backend bridge does not open codegen output until parse, resolve, and typecheck have succeeded with **zero** diagnostics.

### Feature pipeline (every future Rust slice)

```text
idea → tiny vertical slice → rust-front-smoke
  → parse → resolve → typecheck → (lower) → codegen
  → positive tests → negative/golden stderr → ABI/runtime checks → docs
```

### Unsupported-feature codes (`RUST-UNS-E04xx`)

| Code | Meaning |
|------|---------|
| `RUST-UNS-E0401` | `struct` definitions |
| `RUST-UNS-E0402` | `enum` definitions |
| `RUST-UNS-E0403` | `match` expressions (statement position) |
| `RUST-UNS-E0404` | Generic parameters (`fn name<...>(...)`) |
| `RUST-UNS-E0405` | `async fn` or `async` blocks |
| `RUST-UNS-E0406` | `impl` blocks |
| `RUST-UNS-E0407` | `trait` definitions |
| `RUST-UNS-E0408` | Reference types (`&T` / `&mut T`) in type positions |
| `RUST-UNS-E0409` | `mod` |
| `RUST-UNS-E0410` | `use` imports |
| `RUST-UNS-E0411` | `unsafe fn` |
| `RUST-UNS-E0413` | Other unsupported items (`const`, `static`, `type`, `union`, `extern`, …) |

Parse errors (malformed syntax) continue to use `RUSTPARSE*` / `RUSTLEX*` codes.

### Phase breadcrumbs

- `--rust-dump-phase` prints one line per major milestone to **stderr**: `rust-parse`, `rust-resolve`, `rust-typecheck`, `rust-lower` (frontend path), and `rust-codegen` (backend bridge after typecheck).

### Roadmap (next)

| Phase | Focus |
|-------|--------|
| **P1.1** | `u32` end-to-end (parse, types, codegen, tests) |
| **P1.2** | `i64` / `usize` |
| **P1.3** | Unit type `()` |
| **P2+** | Product types, richer control flow, then optional IR unification with the C backend |

### Feature status (snapshot)

| Area | Status |
|------|--------|
| `fn`, `i32`, `bool`, `let`/`let mut`, assignment | supported |
| `struct` / `enum` / `trait` / `impl` / `mod` / `use` | diagnosed (`RUST-UNS-*`) |
| `match`, `async`, generics, references, `unsafe fn` | diagnosed (`RUST-UNS-*`) |
| `u32`, `i64`, `usize`, `()` | not started (P1) |
