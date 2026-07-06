# RFC: ZCC IR Optimization Roadmap (InstCombine → SCCP → MVP Unroll/Inline)

- **Status**: Draft
- **Author**: Compiler Team
- **Target**: `zcc_ir_opt_passes.h` + optimizer pipeline integration
- **Last Updated**: July 6, 2026

---

## 1) Summary

This RFC defines a staged rollout for new ZCC IR optimization passes with low-risk sequencing and measurable outcomes:

1. **InstCombine (Phase 1)**
2. **Sparse Conditional Constant Propagation (SCCP) (Phase 2)**
3. **IR utility infrastructure for cloning/SSA repair (Phase 3)**
4. **MVP Loop Unrolling + MVP Function Inlining (Phase 4)**

The proposal includes:
- exact data structures,
- pass interfaces,
- pseudocode,
- and acceptance tests.

---

## 2) Goals and Non-Goals

### Goals
- Improve runtime performance with bounded compile-time overhead.
- Increase optimization composability with existing GVN, LICM, scalar promotion, and escape analysis.
- Establish shared utilities needed for higher-complexity transforms.

### Non-Goals
- Full-featured aggressive inliner in first iteration.
- Profile-guided or ML-guided heuristics.
- Complex loop transforms (peeling, unswitching, vectorization) in this RFC.

---

## 3) Current Baseline (Audit)

Already implemented:
- Global Value Numbering (GVN)
- LICM (with preheaders)
- Scalar Promotion (Mem2Reg-style)
- Escape Analysis

Open opportunities:
- SCCP
- Comprehensive InstCombine
- Function Inlining
- Loop Unrolling

---

## 4) Rollout Plan

## Phase 1 — InstCombine (Low risk, immediate wins)

### Scope (MVP)
- Algebraic identities:
  - `x + 0 -> x`, `x - 0 -> x`, `x * 1 -> x`, `x * 0 -> 0`, `x / 1 -> x`
- Bitwise/logical identities:
  - `x & 0 -> 0`, `x & -1 -> x`, `x | 0 -> x`, `x ^ 0 -> x`
- Reassociation of constants:
  - `(x + c1) + c2 -> x + (c1 + c2)`
  - `(x * c1) * c2 -> x * (c1 * c2)` (with overflow policy)
- Canonical compare simplifications:
  - `x == x -> true` (for non-NaN-safe integer domain)

### Out of Scope
- Floating-point unsafe folds unless fast-math mode exists.
- Cross-block speculative rewrites.

---

## Phase 2 — SCCP (Medium risk, high payoff)

### Scope (MVP)
- 3-state lattice per SSA value:
  - `UNDEF`
  - `CONST(value)`
  - `OVERDEFINED`
- Executable-edge tracking in CFG.
- Constant folding of terminators to mark untaken edges unreachable.
- Dead block elimination cleanup after SCCP fixpoint.

### Expected Synergy
SCCP should amplify GVN/LICM by simplifying control flow and exposing constants globally.

---

## Phase 3 — Shared IR Utilities (Prereq for Phase 4)

### Deliverables
- Instruction cloning helper.
- Basic block cloning helper.
- Virtual register remapper.
- PHI incoming-edge/value repair helper.
- SSA renaming/update utilities.
- CFG edge redirect helpers.

---

## Phase 4 — Controlled MVP transforms

### 4A) Loop Unrolling (MVP)
- Only loops with:
  - constant trip count <= threshold (e.g., 8),
  - single latch/backedge,
  - reducible natural loop,
  - small body size cap.
- No nested-loop unroll in MVP.

### 4B) Function Inlining (MVP)
- Only direct-call candidates:
  - leaf/static functions,
  - single return block (or normalized first),
  - no varargs, no recursion, no indirect call targets.
- Strict code growth budget.

---

## 5) Metrics & Success Criteria (Per Pass)

For each pass execution, record:

1. **IR instruction count delta**
   - `delta_instr = instr_after - instr_before`
2. **CFG block count delta**
   - `delta_blocks = blocks_after - blocks_before`
3. **Compile-time overhead**
   - wall-clock microseconds per pass and percent of total compile time
4. **Benchmark runtime delta**
   - geometric mean speedup across benchmark suite

Recommended output format (per function and aggregate):
- `pass_name, fn_name, instr_before, instr_after, delta_instr, blocks_before, blocks_after, delta_blocks, pass_time_us`

Release gates:
- Phase 1: no correctness regressions, compile-time +<3%, benchmark geomean >= +1%
- Phase 2: compile-time +<8%, benchmark geomean >= +3%
- Phase 4 MVPs: compile-time +<15%, benchmark geomean >= +5% on hot benchmarks

---

## 6) Data Structures

> Names are illustrative and can be adapted to existing ZCC conventions.

```c
typedef struct {
    const char *pass_name;
    const char *fn_name;
    int instr_before;
    int instr_after;
    int blocks_before;
    int blocks_after;
    long pass_time_us;
} OptPassMetricRow;

typedef struct {
    OptPassMetricRow *rows;
    int n_rows;
    int cap_rows;
} OptMetricsSink;
```

```c
typedef enum {
    SCCP_UNDEF = 0,
    SCCP_CONST = 1,
    SCCP_OVERDEFINED = 2
} SccpLatticeTag;

typedef struct {
    SccpLatticeTag tag;
    long imm;  // extend as needed for type width/union
} SccpValue;

typedef struct {
    // indexed by vreg id
    SccpValue *value_of;
    unsigned char *in_ssa_wl;     // bitset/byte flags
    unsigned char *block_exec;    // block executable?
    unsigned char *edge_exec;     // per-edge executable?
    int *ssa_worklist;
    int ssa_wl_n, ssa_wl_cap;
    int *cfg_worklist;            // edge IDs or (pred,succ) packed
    int cfg_wl_n, cfg_wl_cap;
} SccpState;
```

```c
typedef struct {
    int *reg_map_old2new;      // size old_n_regs
    int old_n_regs;
    int new_n_regs_base;
} RegRemap;

typedef struct {
    int *bb_map_old2new;       // old bb id -> cloned bb id
    int n_bb;
} BbRemap;
```

---

## 7) Pass Interfaces

```c
// Return true if IR changed
bool opt_instcombine_pass(Function *fn, OptMetricsSink *metrics);
bool opt_sccp_pass(Function *fn, OptMetricsSink *metrics);

// Cleanup utility pass often run after SCCP
bool opt_cfg_simplify_pass(Function *fn, OptMetricsSink *metrics);

// Phase 4
bool opt_loop_unroll_mvp_pass(Function *fn, OptMetricsSink *metrics);
bool opt_inline_mvp_pass(Module *m, OptMetricsSink *metrics);
```

Pipeline suggestion:
1. peephole (existing)
2. instcombine
3. sccp
4. cfg_simplify / DCE
5. gvn (existing)
6. licm (existing)
7. (optional iterative loop with budget)

---

## 8) Pseudocode

### 8.1 InstCombine

```text
changed = false
for bb in function.blocks:
  for inst in bb.instructions:
    match inst.op:
      case ADD:
        if is_const(inst.b, 0): replace_all_uses(inst.dst, inst.a); delete inst; changed=true
        else if def(inst.a) is ADD(x, c1) and is_const(inst.b, c2):
          newc = c1 + c2
          rewrite inst as ADD(x, newc)
          changed=true
      case MUL:
        if is_const(inst.b, 1): replace_all_uses(dst, a); delete inst; changed=true
        if is_const(inst.b, 0): rewrite_to_const(dst, 0); changed=true
      ...
return changed
```

### 8.2 SCCP

```text
init all values = UNDEF
mark entry block executable
push entry outgoing edges to CFG worklist

while cfg_wl not empty or ssa_wl not empty:
  while cfg_wl not empty:
    e = pop cfg_wl
    if edge already executable: continue
    mark edge executable
    b = succ(e)
    if block first time executable:
      visit all PHIs in b (maybe update values; push users)
      visit all non-PHI in b in order:
        newv = eval_lattice(inst)
        if meet(value[dst], newv) changed:
          value[dst] = meet(...)
          push SSA users of dst
        if inst is branch and condition becomes CONST:
          push only taken edge
        else if unknown:
          push all successor edges

  while ssa_wl not empty:
    v = pop ssa_wl
    for user in users(v):
      if user.block executable:
        newv = eval_lattice(user)
        if dst value changed:
          update + push users(dst)
        if user is terminator:
          update cfg_wl based on condition lattice

rewrite constants into IR
remove unreachable blocks (separate cleanup pass)
return changed
```

### 8.3 Loop Unroll MVP

```text
for each natural loop L:
  if !is_mvp_eligible(L): continue
  tc = constant_trip_count(L)
  uf = min(tc, UNROLL_CAP)
  clone loop body uf-1 times using clone helpers
  remap regs per clone
  stitch control flow linearly across clones
  repair PHIs at exits and header as needed
  update analyses
```

### 8.4 Inline MVP

```text
for each callsite C in caller:
  callee = resolve_direct_target(C)
  if !eligible(callee, caller): continue
  if growth_budget_exceeded: continue

  split caller block at C into (pre, post)
  clone callee CFG into caller with bb/reg remap
  map callee params to call args (COPY or direct map)
  replace returns with jumps to post; merge returned values via phi/copy
  remove call inst
  update CFG + SSA
```

---

## 9) Acceptance Tests

## 9.1 Correctness Suite

- Golden IR tests per transform:
  - InstCombine identity/reassociation cases.
  - SCCP branch pruning and constant propagation.
  - Unroll/inline structural rewrites on eligible patterns.
- End-to-end semantic tests:
  - compile + run outputs match baseline for existing test corpus.

## 9.2 Regression Guards

- Verify no malformed SSA:
  - each vreg has single def in SSA regions,
  - PHI incoming count matches predecessor count.
- CFG integrity:
  - no dangling edges,
  - all reachable blocks have valid terminators.

## 9.3 Performance/Compile-Time Gates

- Bench harness runs A/B:
  - baseline vs pass-enabled.
- Record:
  - runtime geomean delta,
  - total compile-time delta,
  - per-pass metric rows.

---

## 10) Heuristics (Initial Defaults)

- `INSTCOMBINE_MAX_REWRITE_PER_FN = 10_000`
- `SCCP_MAX_ITERS = 1_000_000` (safety only)
- `UNROLL_TRIPCOUNT_CAP = 8`
- `UNROLL_BODY_INST_CAP = 40`
- `INLINE_CALLEE_INST_CAP = 60`
- `INLINE_CALLER_GROWTH_PCT_CAP = 10%`
- `INLINE_MODULE_GROWTH_PCT_CAP = 5%`

All tunables should be CLI flags for quick experimentation.

---

## 11) Risks and Mitigations

- **Risk**: SCCP compile-time blowup on large CFGs  
  **Mitigation**: executable-edge filtering + bounded worklists + metric logging.

- **Risk**: SSA breakage in cloning-based passes  
  **Mitigation**: land Phase 3 utilities first + mandatory verifier after each transform.

- **Risk**: Code size explosion (unroll/inline)  
  **Mitigation**: strict size/growth budgets and early bailouts.

---

## 12) Implementation Checklist

- [ ] Add `OptMetricsSink` and pass timing hooks.
- [ ] Implement InstCombine MVP + tests.
- [ ] Implement SCCP lattice + executable-edge engine + tests.
- [ ] Add CFG simplify/unreachable cleanup.
- [ ] Add SSA/clone/remap utility layer.
- [ ] Implement Loop Unroll MVP behind flag.
- [ ] Implement Inline MVP behind flag.
- [ ] Enable staged rollout in pipeline with benchmark gates.

---

## 13) Decision

Proceed with phased implementation.  
Do **not** start unroll/inline before SCCP + utility layer + verifier are stable.
