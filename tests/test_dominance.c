/*
 * test_dominance.c — Unit test for ir_dominance + dominator-aware GVN
 * ====================================================================
 * Tests:
 *   T1: Basic block construction from linear IR
 *   T2: Dominator tree calculation (diamond CFG)
 *   T3: Cross-block GVN: expression in entry block folded in dominated branch
 *   T4: GVN isolation: expressions in sibling branches don't leak
 *
 * IMPORTANT: T3 and T4 use IR_LOAD operands (not IR_CONST) to prevent
 * constant folding from eliminating the ADD nodes before GVN runs.
 *
 * Compiled with GCC, not ZCC.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "evm_lifter.h"
#include "ir_pass_manager.h"
#include "ir_dominance.h"

#define TEST(name) fprintf(stderr, "  [TEST] %s\n", name)
#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "    \xe2\x9d\x8c FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        failures++; \
    } else { \
        fprintf(stderr, "    \xe2\x9c\x93 PASS: %s\n", msg); \
    } \
} while(0)

static int failures = 0;

/*
 * T1: Build a simple 3-block function and verify BB partitioning.
 */
static void test_bb_construction(void) {
    ir_module_t *mod;
    ir_func_t   *fn;
    dom_cfg_t   cfg;
    int rc;

    TEST("T1: Basic Block construction (3 blocks)");

    mod = ir_module_create();
    fn  = ir_func_create(mod, "test_bb", IR_TY_I64, 0);

    /* Block 0: entry */
    ir_emit(fn, IR_CONST, IR_TY_I64, "t0", NULL, NULL, NULL, 5L, 1);
    ir_emit(fn, IR_BR_IF, IR_TY_VOID, NULL, "t0", NULL, ".L1", 0L, 2);
    /* Block 1: fallthrough */
    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_end", 0L, 3);
    ir_emit(fn, IR_CONST, IR_TY_I64, "t1", NULL, NULL, NULL, 10L, 4);
    ir_emit(fn, IR_RET,   IR_TY_I64, NULL, "t1", NULL, NULL, 0L, 5);
    /* Block 2: branch target */
    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L1", 0L, 6);
    ir_emit(fn, IR_CONST, IR_TY_I64, "t2", NULL, NULL, NULL, 20L, 7);
    ir_emit(fn, IR_RET,   IR_TY_I64, NULL, "t2", NULL, NULL, 0L, 8);

    rc = dom_build_cfg(&cfg, fn);

    CHECK(rc == 0, "cfg build returns 0");
    CHECK(cfg.block_count == 3, "3 basic blocks");
    CHECK(cfg.blocks[0].succ_count == 2, "entry has 2 successors (taken + fallthrough)");
    CHECK(strcmp(cfg.blocks[1].label, ".L_end") == 0, "block 1 label is .L_end");
    CHECK(strcmp(cfg.blocks[2].label, ".L1") == 0, "block 2 label is .L1");

    ir_module_free(mod);
}

/*
 * T2: Diamond CFG — dominator tree calculation.
 *
 *       [B0: entry]
 *       /          \
 *    [B1: .L_left]  [B2: .L_right]
 *       \          /
 *       [B3: .L_merge]
 */
static void test_diamond_dominance(void) {
    ir_module_t *mod;
    ir_func_t   *fn;
    dom_cfg_t   cfg;

    TEST("T2: Diamond CFG \xe2\x80\x94 dominator tree");

    mod = ir_module_create();
    fn  = ir_func_create(mod, "test_diamond", IR_TY_I64, 0);

    ir_emit(fn, IR_CONST, IR_TY_I64, "cond", NULL, NULL, NULL, 1L, 1);
    ir_emit(fn, IR_BR_IF, IR_TY_VOID, NULL, "cond", NULL, ".L_right", 0L, 2);

    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_left", 0L, 3);
    ir_emit(fn, IR_CONST, IR_TY_I64, "t_left", NULL, NULL, NULL, 42L, 4);
    ir_emit(fn, IR_BR,    IR_TY_VOID, NULL, NULL, NULL, ".L_merge", 0L, 5);

    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_right", 0L, 6);
    ir_emit(fn, IR_CONST, IR_TY_I64, "t_right", NULL, NULL, NULL, 99L, 7);
    ir_emit(fn, IR_BR,    IR_TY_VOID, NULL, NULL, NULL, ".L_merge", 0L, 8);

    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_merge", 0L, 9);
    ir_emit(fn, IR_RET,   IR_TY_I64, NULL, "cond", NULL, NULL, 0L, 10);

    dom_build_cfg(&cfg, fn);
    dom_compute_idom(&cfg);
    dom_build_tree(&cfg);

    CHECK(cfg.block_count == 4, "4 basic blocks");
    CHECK(cfg.blocks[0].idom == -1, "entry idom = -1");
    CHECK(cfg.blocks[1].idom == 0, "left idom = entry");
    CHECK(cfg.blocks[2].idom == 0, "right idom = entry");
    CHECK(cfg.blocks[3].idom == 0, "merge idom = entry");
    CHECK(cfg.blocks[0].child_count == 3, "entry dominates 3 children");

    ir_module_free(mod);
}

/*
 * T3: Cross-block GVN with non-foldable operands.
 *
 * Uses IR_LOAD to produce runtime values that const_fold cannot reduce.
 * The ADD(slot0, slot1) in the entry block should be matched by the
 * identical ADD(slot0, slot1) in the dominated branch.
 *
 *   B0 (entry):
 *     slot0 = LOAD addr0       ← runtime value (not const-foldable)
 *     slot1 = LOAD addr1       ← runtime value
 *     sum   = ADD slot0, slot1 ← first computation
 *     BR_IF cond → .L_branch
 *
 *   B1 (.L_fall) — fallthrough:
 *     RET sum
 *
 *   B2 (.L_branch) — dominated by entry:
 *     sum2  = ADD slot0, slot1 ← SAME: should fold to COPY sum
 *     RET sum2
 */
static void test_crossblock_gvn(void) {
    ir_module_t *mod;
    ir_func_t   *fn;
    ir_node_t   *add2_node;

    TEST("T3: Cross-block GVN \xe2\x80\x94 entry expression folded in dominated branch");

    mod = ir_module_create();
    fn  = ir_func_create(mod, "test_xblock", IR_TY_I64, 0);

    /* B0: entry — use LOADs so const_fold can't reduce the ADDs */
    ir_emit(fn, IR_CONST, IR_TY_I64, "addr0", NULL, NULL, NULL, 0L, 1);
    ir_emit(fn, IR_CONST, IR_TY_I64, "addr1", NULL, NULL, NULL, 8L, 2);
    ir_emit(fn, IR_LOAD,  IR_TY_I64, "slot0", "addr0", NULL, NULL, 0L, 3);
    ir_emit(fn, IR_LOAD,  IR_TY_I64, "slot1", "addr1", NULL, NULL, 0L, 4);
    ir_emit(fn, IR_ADD,   IR_TY_I64, "sum",   "slot0", "slot1", NULL, 0L, 5);
    ir_emit(fn, IR_CONST, IR_TY_I64, "cond", NULL, NULL, NULL, 1L, 6);
    ir_emit(fn, IR_BR_IF, IR_TY_VOID, NULL, "cond", NULL, ".L_branch", 0L, 7);

    /* B1: fallthrough */
    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_fall", 0L, 8);
    ir_emit(fn, IR_RET,   IR_TY_I64, NULL, "sum", NULL, NULL, 0L, 9);

    /* B2: .L_branch — duplicate ADD (non-foldable) */
    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_branch", 0L, 10);
    add2_node = ir_emit(fn, IR_ADD, IR_TY_I64, "sum2", "slot0", "slot1", NULL, 0L, 11);
    ir_emit(fn, IR_RET,   IR_TY_I64, NULL, "sum2", NULL, NULL, 0L, 12);

    /* Run full pass pipeline via ir_pm_run_default */
    {
        ir_module_t *run_mod = ir_module_create();
        run_mod->funcs[0] = fn;
        run_mod->func_count = 1;
        ir_pm_run_default(run_mod, 0);
        run_mod->funcs[0] = NULL;
        run_mod->func_count = 0;
        ir_module_free(run_mod);
    }

    CHECK(add2_node->op == IR_COPY,
          "redundant ADD in dominated block rewritten to COPY");
    if (add2_node->op == IR_COPY) {
        CHECK(strcmp(add2_node->src1, "sum") == 0,
              "COPY source is 'sum' (the canonical ADD)");
    } else {
        fprintf(stderr, "    (ADD op=%d, skipping COPY source check)\n", add2_node->op);
    }

    ir_module_free(mod);
}

/*
 * T4: GVN isolation — sibling branches must not leak values.
 *
 * Uses IR_LOAD so const_fold doesn't interfere. Both branches compute
 * ADD(slot0, slot1) but neither dominates the other.
 *
 *   B0: entry → BR_IF → .L_right
 *   B1: .L_left  → ADD(slot0,slot1) → BR .L_end
 *   B2: .L_right → ADD(slot0,slot1) → BR .L_end
 *   B3: .L_end   → RET
 */
static void test_sibling_isolation(void) {
    ir_module_t *mod;
    ir_func_t   *fn;
    ir_node_t   *add_left, *add_right;

    TEST("T4: GVN isolation \xe2\x80\x94 sibling branches don't leak");

    mod = ir_module_create();
    fn  = ir_func_create(mod, "test_isolation", IR_TY_I64, 0);

    /* B0: entry */
    ir_emit(fn, IR_CONST, IR_TY_I64, "addr0", NULL, NULL, NULL, 0L, 1);
    ir_emit(fn, IR_CONST, IR_TY_I64, "addr1", NULL, NULL, NULL, 8L, 2);
    ir_emit(fn, IR_LOAD,  IR_TY_I64, "slot0", "addr0", NULL, NULL, 0L, 3);
    ir_emit(fn, IR_LOAD,  IR_TY_I64, "slot1", "addr1", NULL, NULL, 0L, 4);
    ir_emit(fn, IR_CONST, IR_TY_I64, "cond", NULL, NULL, NULL, 1L, 5);
    ir_emit(fn, IR_BR_IF, IR_TY_VOID, NULL, "cond", NULL, ".L_right", 0L, 6);

    /* B1: .L_left — fallthrough */
    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_left", 0L, 7);
    add_left = ir_emit(fn, IR_ADD, IR_TY_I64, "sum_l", "slot0", "slot1", NULL, 0L, 8);
    ir_emit(fn, IR_BR,    IR_TY_VOID, NULL, NULL, NULL, ".L_end", 0L, 9);

    /* B2: .L_right */
    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_right", 0L, 10);
    add_right = ir_emit(fn, IR_ADD, IR_TY_I64, "sum_r", "slot0", "slot1", NULL, 0L, 11);
    ir_emit(fn, IR_BR,    IR_TY_VOID, NULL, NULL, NULL, ".L_end", 0L, 12);

    /* B3: .L_end */
    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_end", 0L, 13);
    /* Use both sums to prevent DCE from killing them */
    ir_emit(fn, IR_ADD,   IR_TY_I64, "total", "sum_l", "sum_r", NULL, 0L, 14);
    ir_emit(fn, IR_RET,   IR_TY_I64, NULL, "total", NULL, NULL, 0L, 15);

    /* Run full pipeline */
    {
        ir_module_t *run_mod = ir_module_create();
        run_mod->funcs[0] = fn;
        run_mod->func_count = 1;
        ir_pm_run_default(run_mod, 0);
        run_mod->funcs[0] = NULL;
        run_mod->func_count = 0;
        ir_module_free(run_mod);
    }

    /* Both ADDs should remain as ADD — siblings don't dominate each other */
    CHECK(add_left->op == IR_ADD,
          "left ADD remains ADD (not folded)");
    CHECK(add_right->op == IR_ADD,
          "right ADD remains ADD (sibling isolation preserved)");

    ir_module_free(mod);
}

int main(void) {
    fprintf(stderr, "\n\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x97\n");
    fprintf(stderr, "\xe2\x95\x91  \xe2\x96\xb8 ZKAEDI DOMINANCE + GVN TEST SUITE                          \xe2\x95\x91\n");
    fprintf(stderr, "\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\n\n");

    test_bb_construction();
    test_diamond_dominance();
    test_crossblock_gvn();
    test_sibling_isolation();

    fprintf(stderr, "\n  Results: %d FAILED\n", failures);
    if (failures == 0) {
        fprintf(stderr, "  \xe2\x9c\x93 ALL TESTS GREEN\n\n");
    }

    return failures > 0 ? 1 : 0;
}
