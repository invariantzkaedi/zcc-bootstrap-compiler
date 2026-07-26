# 🎙️ NOTEBOOKLM PODCAST PART 2: FORENSIC BUG HUNTING & ICP PRECISION FIX

> **Note for NotebookLM Audio Overview:**  
> Part 2 of 4 detailing the forensic investigation, empirical root cause analysis, surgical C patch, and verification trace of the Floating-Point Interprocedural Constant Propagation (ICP) precision truncation defect.

---

## 1. Case Study: The Floating-Point Division Anomaly

During differential execution testing between host `gcc` and `zcc` using `tests/test_cast_prec.c`, a critical precision truncation bug was discovered in ZCC's constant folding engine.

### The Trigger Code (`tests/test_cast_prec.c`)
```c
#include <stdio.h>

int main(void) {
    int w = 1920;
    int h = 1080;
    double aspect_ratio = (double)w / h;
    printf("Aspect ratio: %f\n", aspect_ratio);
    return 0;
}
```

### Empirical Trace Comparison
- **Reference Host GCC Output**: `Aspect ratio: 1.777778`
- **Defective ZCC Bug Output**: `Aspect ratio: 1.000000`

---

## 2. Forensic Investigation & Root Cause Analysis

We performed a step-by-step forensic trace of ZCC's optimization passes:

1. **Parser & AST Generation**: The expression `(double)w / h` was correctly parsed into an `AST_BINARY_EXPR` node with operator `/`, left child `AST_CAST (double)` wrapping variable `w`, and right child variable `h`.
2. **ICP Optimization Pass (`part3.c`)**: Inside `icp_eval_expr` and `propagate_local_assignments`, ZCC's Interprocedural Constant Propagation lattice evaluates local assignment expressions to replace variables with constant literals at compile time.
3. **The Discovered Bug**: The ICP lattice structure was implemented assuming integer-only types (`long long`). When evaluating `icp_eval_expr(node->child)` for `AST_CAST`:

```c
// Vulnerable Code in part3.c (BEFORE FIX):
static long long icp_eval_expr(ASTNode *node) {
    if (!node) return 0;
    if (node->op == AST_CAST) {
        long long val = icp_eval_expr(node->child);
        return val; // Forced truncation of double precision to integer!
    }
    if (node->op == OP_DIV) {
        long long left = icp_eval_expr(node->left);
        long long right = icp_eval_expr(node->right);
        if (right != 0) return left / right; // Evaluates 1920 / 1080 -> 1 !
    }
    ...
}
```

Because `1920 / 1080` integer division equaled `1`, the ICP pass folded variable `aspect_ratio` to constant double `1.000000` in the AST, completely erasing the fractional precision `.777778`!

---

## 3. The Surgical Patch in `part3.c`

To fix this defect without breaking integer constant propagation, we added strict type guards for `TY_FLOAT` and `TY_DOUBLE` types. Whenever the ICP engine encounters floating-point operations, it returns `LATTICE_BOT`, forcing ZCC to emit dynamic FPU/SSE assembly instructions (`divsd`, `cvtsi2sd`) at runtime instead of incorrect integer constant folding.

```c
// Surgical Fix Applied in part3.c (AFTER FIX):
static long long icp_eval_expr(ASTNode *node) {
    if (!node) return LATTICE_BOT;

    // Safety Guard: Floating-point precision cannot be evaluated by integer ICP lattice
    if (node->type && (node->type->kind == TY_FLOAT || node->type->kind == TY_DOUBLE)) {
        return LATTICE_BOT;
    }

    if (node->op == AST_CAST) {
        if (node->type && (node->type->kind == TY_FLOAT || node->type->kind == TY_DOUBLE)) {
            return LATTICE_BOT;
        }
        return icp_eval_expr(node->child);
    }
    ...
}
```

---

## 4. Verification Evidence & Ledger Attestation

Following the patch application, we executed the full validation suite:

### Step 1: Direct Binary Execution Verification
```bash
$ ./zcc tests/test_cast_prec.c -o /tmp/test_cast_prec
$ /tmp/test_cast_prec
Aspect ratio: 1.777778
```
*Output matches host GCC 100% perfectly.*

### Step 2: Self-Host Fixed Point Verification
```bash
$ set -o pipefail; make selfhost
=== STAGE 1: cc0 -> zcc1 ===
=== STAGE 2: zcc1 -> zcc2 ===
=== STAGE 3: zcc2 -> zcc3 ===
★ ZKAEDI PRIME FIXED POINT REPO - H0 CONVERGED - exit 0 (⟐ BYTE-IDENTICAL SEAL ⟐) ★
```
*`cmp zcc2.s zcc3.s` exit 0 confirmed byte-identical assembly output.*

### Step 3: Bootstrap Baseline Hash Locking
Because `part3.c` codegen changed, the Stage 2 baseline hash updated to `130ad64ec7cacd4bc226e12017f8c4a6`. We updated `BOOTSTRAP_BASELINES.tsv` and committed as [`ed2457c2`](file:///H:/__DOWNLOADS/zcc_github_upload/BOOTSTRAP_BASELINES.tsv):

```bash
$ bash scripts/log_bootstrap_hash.sh
[BOOTSTRAP HASH] Hash matches locked baseline 130ad64ec7cacd4bc226e12017f8c4a6. BOOTSTRAP_HASH_EXIT=0
```

---

## 5. Key Discussion Points for Audio Hosts

When discussing Part 2 on your podcast, highlight:
- How silent optimization bugs (like folding `1.777778` to `1.000000`) are among the most dangerous compiler defects because they compile cleanly without errors.
- Why returning `LATTICE_BOT` in abstract interpretation lattices safely forces runtime instruction emission.
- How baseline hashing (`BOOTSTRAP_BASELINES.tsv`) acts as an immediate alarm whenever compiler code changes affect generated assembly output.
