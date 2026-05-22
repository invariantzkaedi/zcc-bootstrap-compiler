# pointer_deref — Pre-existing FAIL

**Status:** Open / Pre-existing  
**First observed:** E4 escape analysis gate (commit: pending)  
**Test:** `./tests/zcc_test_suite.sh --quick` → `[FAIL] pointer_deref (rc=139)`  
**Exit code:** 139 (SIGSEGV)

## Symptom
ZCC IR backend SIGSEGVs when compiling pointer dereference patterns.
Test compiles with AST backend cleanly; IR path crashes.

## Suspected root cause
Pointer SSA / dereference modeling incomplete in IR lowering path.
OP_LOAD of pointer-typed operand may produce an uninitialized or
null IR node that is later dereferenced during codegen.

## Gate history
- E4 escape analysis fix (this commit): pre-existing, not introduced.
  FAIL baseline confirmed via `git stash && ./tests/zcc_test_suite.sh --quick`.

## Deferred until
Pointer SSA modeling pass is implemented in the IR backend.
