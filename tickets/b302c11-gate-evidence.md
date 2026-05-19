fix(codegen): stabilize linear scan for inverted intervals

Goal
Resolve assembly drift (string corruption `\0CC\0D\0\0\0G`) and `cmp zcc2.s zcc3.s` parity failures caused by register clobbering on PGO-hoisted loop variables.

Outcome
The linear scan register allocator now expands the start sequence of inverted intervals (`def_seq > last_use`) backward to safely cover the `use` point, preventing the same physical register from being assigned to overlapping variables during loop back-edges, achieving binary parity.

Gates

Gate 1: Self-host byte-identical — VERIFIED via `make selfhost` and `cmp`
```
$ make selfhost
...
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)" || (echo "SELF-HOST FAILED (assembly diverged)"; diff zcc2.s zcc3.s | head -20; exit 1)
SELF-HOST VERIFIED (assembly identical)

$ cmp zcc2.s zcc3.s
(no output, identical)
```

Gate 2: Inter-op (if codegen-touching) — N/A (Internal IR register lifetime state only; ABI unchanged).

Bugs caught mid-gate

BUG-3 FIX (Initial Implementation): Extending ONLY `last_use` forward to `seq - 1` without rewinding `def_seq` backward to `last_use` caused the linear scanner to process the interval too late. This created a lethal conflict at the `use` point, corrupting the `cc_strdup` loop state and emitting `\0CC\0D\0\0\0G`. The fix now properly expands `def_seq[r] = last_use[r]`.

Hygiene / deferred
None.

Forensic notes
The `BUG-3 FIX` initial implementation triggered an infinite loop during `make selfhost` Phase 2 because `pp_expand_ident` loop counters were clobbered due to the un-expanded interval start point sharing a physical register at the `use` site. Rewinding `start` to `last_use` correctly reserves the physical register across the entire emission window of the inverted dependency.
