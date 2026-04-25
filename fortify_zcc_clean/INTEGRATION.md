# Compiler-side Integration Notes

The files in `compiler/` are well-formed C and the **only** non-test C files in
this clean pack — the original pack shipped seven additional C files that were
fragments, pseudocode, or `_Static_assert` macro-erasure shims. Those are gone.

What's here, and what each one assumes about your ZCC tree:

## `zcc_diagnostics.h`

A diagnostic-code enum that matches the names required by `AGENTS.md`. Drop
this in your tree as the canonical list.

The original pack also shipped `diagnostic_codes.h` (different prefixes,
`E_STATIC_ASSERT_FAIL` vs `_FAILED`, etc.) and `layout.h` (a 257-byte stub).
Both are discarded; this header is the single source of truth.

## `zcc_layout.h`

Public API of the unified layout engine. Declares:

```c
TypeLayout zcc_get_layout(Type *type, LayoutPhase phase);
size_t zcc_sizeof(Type *type);
size_t zcc_alignof(Type *type);
```

Includes `zcc_types.h` and `zcc_source.h` from your existing tree.

## `zcc_layout_dump.{h,c}`

Self-contained record-layout dumper that emits the format consumed by
`tools/normalize_clang_layout.py` and `tools/compare_layout_outputs.py`. Uses:

- `Type` with fields `kind`, `name`, `struct_.num_members`, `struct_.members`,
  `union_.num_members`, `union_.members`
- `Member` with fields `name`, `offset`
- `TYPE_STRUCT`, `TYPE_UNION` enum values

If your `Type` representation differs, this needs an adapter pass before it
will compile against your tree. It is otherwise complete and not a stub.

## `zcc_static_assert.c`

Real `_Static_assert` handler. Calls `zcc_const_eval` from `zcc_consteval.h`
and `zcc_diag` from `zcc_diagnostics.h`. Uses the diagnostic codes
`E_STATIC_ASSERT_NOT_CONSTANT` and `E_STATIC_ASSERT_FAILED` from
`zcc_diagnostics.h`.

If your existing parser already calls a `_Static_assert` handler,
that handler's body should look like this. If you don't have one,
this is a drop-in starting point; the only integration is wiring the
parser to call `zcc_handle_static_assert(condition, message, loc)`
when it sees the keyword in declaration position.

## What's deliberately NOT in this pack

The original `zcc_layout.c` was four lines — fragments of a function body
that referenced undefined helpers (`apply_member_alignas`, `invalid_layout`)
and did not define any of the functions that `zcc_layout.h` declares. It
cannot compile and cannot be salvaged into a working layout engine without
rewriting it from your existing layout code.

Writing the real `zcc_layout.c` is **the integration task** for your tree.
The `AGENT_TASK.md` "Implementation goals → Compiler semantics" section
describes the invariants it must hold. The tests under `tests/layout/`
are the regression seeds it must pass.

## Suggested integration order

1. Add `zcc_diagnostics.h` codes to your existing diagnostic enum (or
   replace it).
2. Add `zcc_static_assert.c` and wire the parser entry point.
3. Adapt `zcc_layout_dump.c` to your `Type` representation.
4. Implement the real `zcc_layout.c` against `zcc_layout.h` using your
   existing struct-layout code as the seed.
5. Add the contents of `tests/layout/` and `tests/` to your regression suite.
6. Run `ci/fortify-layout.sh` from your repo root.
