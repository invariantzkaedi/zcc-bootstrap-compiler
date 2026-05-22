# switch_stmt — Pre-existing FAIL

**Status:** Open / Pre-existing  
**First observed:** E4 escape analysis gate (commit: pending)  
**Test:** `./tests/zcc_test_suite.sh --quick` → `[FAIL] switch_stmt (rc=1)`  
**Exit code:** 1

## Symptom
Switch statement compiled under IR backend exits rc=1 instead of
expected return value. AST backend handles switch correctly.

## Suspected root cause
Branch mapping / jump table translation not implemented in IR lowering.
Switch dispatch likely falls through to default or emits wrong jump target.

## Gate history
- E4 escape analysis fix (this commit): pre-existing, not introduced.
  FAIL baseline confirmed via `git stash && ./tests/zcc_test_suite.sh --quick`.

## Deferred until
IR switch lowering pass (jump table or if-chain translation) is implemented.
