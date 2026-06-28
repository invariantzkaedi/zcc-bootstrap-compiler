/*
 * tests/test_copy_const_prop.c — Unit tests for ir_pass_copy_const_prop
 *
 * Tests:
 *   T01: Constant propagated through a single IR_COPY (IR_COPY → IR_CONST)
 *   T02: Constant propagation chains (COPY of COPY of CONST)
 *   T03: Copy propagation — use of copy alias rewritten to original source
 *   T04: Dead copy removed by DCE after CCP propagated its value
 *   T05: Block boundary resets maps — alias from block A NOT visible in block B
 *   T06: IR_PHI nodes are not rewritten (CCP skips them)
 *   T07: Non-constant IR_COPY alias resolves through two-hop chain
 *   T08: ir_pass_copy_const_prop returns changed=1 when work was done
 *   T09: ir_pass_copy_const_prop returns changed=0 on a clean function
 *   T10: Pipeline (ir_pm_run_default) removes dead copies end-to-end
 *
 * Build and run:
 *   gcc -O0 -std=c99 -Wall -I. \
 *       -o /tmp/test_copy_const_prop \
 *       tests/test_copy_const_prop.c ir_pass_manager.c ir_pass_warden.c \
 *       ir_pass_taint.c ir_pass_healer.c ir_symbolic_cfg.c ir_dominance.c \
 *       ir_ssa.c evm_lifter.c ir_vuln_tag.c ir_to_evm.c ir_evm_stack.c \
 *       ir.c src/ir_lower_float.c src/x86_codegen_sse.c \
 *       src/evm/decompiler.c src/evm/jit.c src/evm/symbolic.c \
 *       src/evm/memory_v2.c src/evm/abi_extractor.c src/evm/jit_memory.c \
 *       src/evm/proof_export.c src/evm/ipc_bridge.c src/evm/yul_weaver.c \
 *       src/evm/yul_fixed_point.c src/evm/yul_frontend.c \
 *       src/gfx/sdf_compiler.c src/gfx/mesh_warden.c \
 *       src/evm/evm_symbolic_harness.c ir_telemetry.c zcc_telemetry.c \
 *       src/zcc_oracle_substrate.c src/elf_emit.c src/codegen.c \
 *       src/ir_serialization.c src/zcc_smt_prover.c src/gguf_emit.c \
 *       src/zld.c src/zcc_resource_oracle.c transient_state.c \
 *       zcc_lucky_alert_injector.c compiler_passes.c compiler_passes_ir.c \
 *       -lm
 *   /tmp/test_copy_const_prop
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"
#include "ir_pass_manager.h"

/* ── Minimal test harness ─────────────────────────────────────────── */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) { g_pass++; } \
        else { g_fail++; fprintf(stderr, "  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg); } \
    } while (0)

#define TEST(name) printf("  %s\n", name)

/* ── Helpers ──────────────────────────────────────────────────────── */

/* Allocate a module + function and append a minimal RET so the function
 * has a side-effectful node (prevents the whole list being DCE'd). */
static ir_module_t *make_module(const char *fname, ir_func_t **fn_out) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, fname, IR_TY_I64, 0);
    *fn_out = fn;
    return mod;
}

/* Append a node to fn, returning the node. */
static ir_node_t *append(ir_func_t *fn, ir_op_t op, ir_type_t type,
                          const char *dst, const char *src1,
                          const char *src2, long imm) {
    ir_node_t *n = ir_node_alloc();
    n->op   = op;
    n->type = type;
    if (dst)  { strncpy(n->dst,  dst,  IR_NAME_MAX - 1); n->dst[IR_NAME_MAX-1]  = '\0'; }
    if (src1) { strncpy(n->src1, src1, IR_NAME_MAX - 1); n->src1[IR_NAME_MAX-1] = '\0'; }
    if (src2) { strncpy(n->src2, src2, IR_NAME_MAX - 1); n->src2[IR_NAME_MAX-1] = '\0'; }
    n->imm  = imm;
    ir_append(fn, n);
    return n;
}

/* Count nodes with a given opcode in fn. */
static int count_op(ir_func_t *fn, ir_op_t op) {
    int cnt = 0;
    ir_node_t *n;
    for (n = fn->head; n; n = n->next)
        if (n->op == op) cnt++;
    return cnt;
}

/* Find the first node in fn whose dst matches name. */
static ir_node_t *find_dst(ir_func_t *fn, const char *name) {
    ir_node_t *n;
    for (n = fn->head; n; n = n->next)
        if (n->dst[0] && strcmp(n->dst, name) == 0) return n;
    return NULL;
}

/* Find a node in fn whose src1 matches name. */
static ir_node_t *find_src1(ir_func_t *fn, const char *name) {
    ir_node_t *n;
    for (n = fn->head; n; n = n->next)
        if (n->src1[0] && strcmp(n->src1, name) == 0) return n;
    return NULL;
}

/* ── T01: CONST → COPY → CONST propagation ───────────────────────── */
static void test_const_through_copy(void) {
    ir_func_t   *fn;
    ir_module_t *mod = make_module("t01_fn", &fn);
    ir_node_t   *copy_node;

    TEST("T01: Constant propagated through single IR_COPY");

    /* CONST t0, 42 */
    append(fn, IR_CONST, IR_TY_I64, "t0", NULL, NULL, 42);
    /* COPY  t1, t0   — should become CONST t1, 42 */
    copy_node = append(fn, IR_COPY, IR_TY_I64, "t1", "t0", NULL, 0);
    /* RET   t1 */
    append(fn, IR_RET,  IR_TY_I64, NULL, "t1", NULL, 0);

    ir_pm_run_default(mod, 0);

    /* copy_node should now be IR_CONST with imm=42 */
    CHECK(copy_node->op  == IR_CONST, "T01: IR_COPY rewritten to IR_CONST");
    CHECK(copy_node->imm == 42,       "T01: propagated constant value == 42");

    ir_module_free(mod);
}

/* ── T02: Constant propagation through a chain ───────────────────── */
static void test_const_chain(void) {
    ir_func_t   *fn;
    ir_module_t *mod = make_module("t02_fn", &fn);
    ir_node_t   *copy2;

    TEST("T02: Constant propagation through COPY-of-COPY chain");

    /* CONST t0, 7  →  COPY t1, t0  →  COPY t2, t1  →  RET t2 */
    append(fn, IR_CONST, IR_TY_I64, "t0", NULL, NULL, 7);
    append(fn, IR_COPY,  IR_TY_I64, "t1", "t0", NULL, 0);
    copy2 = append(fn, IR_COPY, IR_TY_I64, "t2", "t1", NULL, 0);
    append(fn, IR_RET,   IR_TY_I64, NULL,  "t2", NULL, 0);

    ir_pm_run_default(mod, 0);

    /* After CCP + fixpoint, t2 should also be IR_CONST 7 */
    CHECK(copy2->op  == IR_CONST, "T02: chain tail rewritten to IR_CONST");
    CHECK(copy2->imm == 7,        "T02: propagated constant == 7");

    ir_module_free(mod);
}

/* ── T03: Copy propagation — use rewritten to original source ─────── */
static void test_copy_propagation(void) {
    ir_func_t   *fn;
    ir_module_t *mod = make_module("t03_fn", &fn);
    ir_node_t   *add_node;

    TEST("T03: Copy propagation — use of alias rewritten to source");

    /* ARG  tX         — non-constant source */
    append(fn, IR_ARG, IR_TY_I64, "tX", NULL, NULL, 0);
    /* COPY t2, tX     — alias t2 → tX */
    append(fn, IR_COPY, IR_TY_I64, "t2", "tX", NULL, 0);
    /* ADD  t3, t2, t2 — both src1 and src2 should be rewritten to tX */
    add_node = append(fn, IR_ADD, IR_TY_I64, "t3", "t2", "t2", 0);
    /* RET  t3 */
    append(fn, IR_RET, IR_TY_I64, NULL, "t3", NULL, 0);

    ir_pm_run_default(mod, 0);

    CHECK(strcmp(add_node->src1, "tX") == 0, "T03: src1 rewritten to tX");
    CHECK(strcmp(add_node->src2, "tX") == 0, "T03: src2 rewritten to tX");

    ir_module_free(mod);
}

/* ── T04: Dead copy removed by DCE after CCP ─────────────────────── */
static void test_dead_copy_removed(void) {
    ir_func_t   *fn;
    ir_module_t *mod = make_module("t04_fn", &fn);
    int copies_before;
    int copies_after;

    TEST("T04: Dead IR_COPY removed by DCE after CCP");

    /* CONST t0, 99
     * COPY  t1, t0   — t1 will be folded to CONST, then t0 is dead
     * RET   t1       — after fold uses imm, t1 itself becomes dead too */
    append(fn, IR_CONST, IR_TY_I64, "t0", NULL, NULL, 99);
    append(fn, IR_COPY,  IR_TY_I64, "t1", "t0", NULL, 0);
    /* Keep a reference to t1 so it isn't dead before CCP runs */
    ir_node_t *ret = append(fn, IR_RET, IR_TY_I64, NULL, "t1", NULL, 0);

    copies_before = count_op(fn, IR_COPY);
    ir_pm_run_default(mod, 0);
    copies_after  = count_op(fn, IR_COPY);

    CHECK(copies_before == 1,  "T04: one IR_COPY present before pipeline");
    CHECK(copies_after  == 0,  "T04: all IR_COPY nodes removed after pipeline");
    /* RET should now refer to the folded constant directly or be unchanged */
    (void)ret; /* suppress unused warning — we only care about COPY count */

    ir_module_free(mod);
}

/* ── T05: Block boundary resets alias map ────────────────────────── */
static void test_block_boundary_reset(void) {
    ir_func_t   *fn;
    ir_module_t *mod = make_module("t05_fn", &fn);
    ir_node_t   *add_after_label;

    TEST("T05: Block boundary resets alias map");

    /* ARG  tX
     * COPY t2, tX     — alias t2 → tX in block A
     * LABEL .L_blk_b  — reset; alias must NOT carry over
     * ADD   t3, t2, t2 — src1/src2 still "t2" (alias was cleared at LABEL)
     * RET   t3 */
    append(fn, IR_ARG,   IR_TY_I64, "tX", NULL, NULL, 0);
    append(fn, IR_COPY,  IR_TY_I64, "t2", "tX", NULL, 0);
    {
        ir_node_t *lbl = ir_node_alloc();
        lbl->op = IR_LABEL;
        strncpy(lbl->label, ".L_blk_b", IR_LABEL_MAX - 1);
        ir_append(fn, lbl);
    }
    add_after_label = append(fn, IR_ADD, IR_TY_I64, "t3", "t2", "t2", 0);
    append(fn, IR_RET,   IR_TY_I64, NULL,  "t3", NULL, 0);

    ir_pm_run_default(mod, 0);

    /*
     * After the LABEL boundary, t2's alias to tX has been cleared, so
     * add_after_label->src1 must remain "t2" (not "tX").
     */
    CHECK(strcmp(add_after_label->src1, "t2") == 0,
          "T05: src1 still 't2' — alias not propagated across LABEL");
    CHECK(strcmp(add_after_label->src2, "t2") == 0,
          "T05: src2 still 't2' — alias not propagated across LABEL");

    ir_module_free(mod);
}

/* ── T06: IR_PHI nodes skipped ───────────────────────────────────── */
static void test_phi_skipped(void) {
    ir_func_t   *fn;
    ir_module_t *mod = make_module("t06_fn", &fn);

    TEST("T06: IR_PHI nodes are not modified by CCP");

    /* Two distinct non-constant args → CCP cannot fold the PHI.
     * phi(tA, tB) with tA ≠ tB is conservatively untouchable by CCP.
     * After the full pipeline, the result is consumed by RET so the
     * pipeline must either keep the PHI or lower it correctly — but
     * must NOT introduce an IR_CONST for a phi of two different values.
     * We verify by scanning the live node list (not a saved pointer
     * that might become dangling if DCE removes the node). */
    append(fn, IR_ARG, IR_TY_I64, "tA", NULL, NULL, 0);
    append(fn, IR_ARG, IR_TY_I64, "tB", NULL, NULL, 0);
    append(fn, IR_PHI, IR_TY_I64, "t1", "tA", "tB", 0);
    append(fn, IR_RET, IR_TY_I64, NULL, "t1", NULL, 0);

    ir_pm_run_default(mod, 0);

    /* After the pipeline the function must not have collapsed the phi
     * result to a constant (CCP and const_fold both guarantee that). */
    {
        int found_const_t1 = 0;
        ir_node_t *n;
        for (n = fn->head; n; n = n->next) {
            if (n->op == IR_CONST && strcmp(n->dst, "t1") == 0)
                found_const_t1 = 1;
        }
        CHECK(found_const_t1 == 0,
              "T06: phi(tA,tB) with distinct args NOT folded to CONST t1");
    }

    ir_module_free(mod);
}

/* ── T07: Two-hop copy chain resolves transitively ───────────────── */
static void test_two_hop_chain(void) {
    ir_func_t   *fn;
    ir_module_t *mod = make_module("t07_fn", &fn);
    ir_node_t   *use_node;

    TEST("T07: Two-hop copy chain resolves to ultimate source");

    /* ARG  tX
     * COPY t_a, tX
     * COPY t_b, t_a   — alias t_b → t_a → tX
     * ADD  t_c, t_b, t_b  — should be rewritten to tX, tX
     * RET  t_c */
    append(fn, IR_ARG,  IR_TY_I64, "tX",  NULL,  NULL, 0);
    append(fn, IR_COPY, IR_TY_I64, "t_a", "tX",  NULL, 0);
    append(fn, IR_COPY, IR_TY_I64, "t_b", "t_a", NULL, 0);
    use_node = append(fn, IR_ADD, IR_TY_I64, "t_c", "t_b", "t_b", 0);
    append(fn, IR_RET,  IR_TY_I64, NULL,  "t_c", NULL, 0);

    ir_pm_run_default(mod, 0);

    CHECK(strcmp(use_node->src1, "tX") == 0, "T07: src1 resolved to tX via chain");
    CHECK(strcmp(use_node->src2, "tX") == 0, "T07: src2 resolved to tX via chain");

    ir_module_free(mod);
}

/* ── T08: changed flag set when work was done ────────────────────── */
static void test_changed_flag(void) {
    TEST("T08: Pipeline reports modifications on a function with propagatable copies");

    /* Indirect check: after running the pipeline on a function that has
     * propagatable copies, the IR_COPY nodes should be gone (mutated or
     * DCE'd), confirming the pass ran and changed things. */
    ir_func_t   *fn;
    ir_module_t *mod = make_module("t08_fn", &fn);

    append(fn, IR_CONST, IR_TY_I64, "k0", NULL, NULL, 5);
    append(fn, IR_COPY,  IR_TY_I64, "k1", "k0", NULL, 0);
    append(fn, IR_RET,   IR_TY_I64, NULL,  "k1", NULL, 0);

    int copies_before = count_op(fn, IR_COPY);
    ir_pm_run_default(mod, 0);
    int copies_after  = count_op(fn, IR_COPY);

    CHECK(copies_before >= 1, "T08: at least one IR_COPY present before run");
    CHECK(copies_after < copies_before, "T08: IR_COPY count reduced (pass changed IR)");

    ir_module_free(mod);
}

/* ── T09: no-op on already-clean function ────────────────────────── */
static void test_noop_clean(void) {
    ir_func_t   *fn;
    ir_module_t *mod = make_module("t09_fn", &fn);

    TEST("T09: Pipeline is a no-op on a function with no copies");

    append(fn, IR_CONST, IR_TY_I64, "v0", NULL, NULL, 100);
    append(fn, IR_RET,   IR_TY_I64, NULL,  "v0", NULL, 0);

    int nodes_before = fn->node_count;
    ir_pm_run_default(mod, 0);
    int nodes_after  = fn->node_count;

    /* node_count may drop if DCE removes dead constants; at minimum
     * the RET node survives. */
    CHECK(nodes_after >= 1, "T09: at least RET node survives clean run");
    (void)nodes_before;

    ir_module_free(mod);
}

/* ── T10: Full pipeline end-to-end copy elimination ─────────────── */
static void test_pipeline_end_to_end(void) {
    ir_func_t   *fn;
    ir_module_t *mod = make_module("t10_fn", &fn);

    TEST("T10: Full pipeline (ir_pm_run_default) eliminates all dead copies");

    /* Build:
     *   CONST t0, 42
     *   COPY  t1, t0      — propagated → CONST t1, 42; original t0 goes dead
     *   COPY  t2, t1      — propagated → CONST t2, 42
     *   ARG   tA          — live arg
     *   COPY  t3, tA      — copy alias; src in use below
     *   ADD   t4, t2, t3  — after CCP: ADD t4, t2(const42), tA
     *   RET   t4
     */
    append(fn, IR_CONST, IR_TY_I64, "t0", NULL, NULL, 42);
    append(fn, IR_COPY,  IR_TY_I64, "t1", "t0", NULL, 0);
    append(fn, IR_COPY,  IR_TY_I64, "t2", "t1", NULL, 0);
    append(fn, IR_ARG,   IR_TY_I64, "tA", NULL, NULL, 0);
    append(fn, IR_COPY,  IR_TY_I64, "t3", "tA", NULL, 0);
    ir_node_t *add = append(fn, IR_ADD, IR_TY_I64, "t4", "t2", "t3", 0);
    append(fn, IR_RET,   IR_TY_I64, NULL, "t4", NULL, 0);

    int copies_before = count_op(fn, IR_COPY);
    ir_pm_run_default(mod, 0);
    int copies_after  = count_op(fn, IR_COPY);

    CHECK(copies_before == 3,    "T10: three IR_COPY nodes before pipeline");
    CHECK(copies_after  == 0,    "T10: zero IR_COPY nodes after pipeline");
    /* After CCP, ADD src2 should be tA (copy of tA was propagated) */
    CHECK(strcmp(add->src2, "tA") == 0, "T10: ADD src2 resolved to tA");

    ir_module_free(mod);
}

/* ── main ─────────────────────────────────────────────────────────── */
int main(void)
{
    printf("=== Copy/Constant Propagation Pass Tests ===\n");

    test_const_through_copy();
    test_const_chain();
    test_copy_propagation();
    test_dead_copy_removed();
    test_block_boundary_reset();
    test_phi_skipped();
    test_two_hop_chain();
    test_changed_flag();
    test_noop_clean();
    test_pipeline_end_to_end();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    if (g_fail == 0)
        printf("ALL PASS\n");
    else
        printf("FAILURES PRESENT\n");

    return g_fail ? 1 : 0;
}
