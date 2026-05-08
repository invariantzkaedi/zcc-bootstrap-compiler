/*
 * test_symbolic_cfg.c — Unit test for ir_pass_symbolic_cfg
 * =========================================================
 * Tests backward symbolic resolution of ghost jumps in the IR.
 *
 * Test 1: IR_CONST(8) + IR_CONST(2) → IR_ADD(t2=t0+t1) → IR_NOP(src1=t2)
 *         + IR_LABEL(.L_evm_10) → should resolve NOP to IR_BR .L_evm_10
 *
 * Test 2: IR_NOP with src1 from IR_LOAD (unresolvable) → should remain
 *         NOP with IR_TAG_EVM_BARRIER
 *
 * Test 3: Chained arithmetic: CONST(100) → SUB(CONST(5)) → t2=95
 *         IR_NOP(src1=t2) + IR_LABEL(.L_evm_95) → should resolve
 *
 * Compiled with GCC, not ZCC.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "evm_lifter.h"
#include "ir_pass_manager.h"
#include "ir_symbolic_cfg.h"

#define TEST(name) fprintf(stderr, "  [TEST] %s\n", name)
#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "    ❌ FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        failures++; \
    } else { \
        fprintf(stderr, "    ✓ PASS: %s\n", msg); \
    } \
} while(0)

static int failures = 0;

/*
 * Manually build IR for: CONST 8 → CONST 2 → ADD → NOP(ghost jump)
 * with a LABEL .L_evm_10 present as a valid target.
 */
static void test_resolved_add_jump(void) {
    ir_module_t *mod;
    ir_func_t   *fn;
    ir_node_t   *nop_node;
    ir_pass_result_t r;

    TEST("T1: CONST(8) + CONST(2) → ADD → ghost JUMP → resolve to .L_evm_10");

    mod = ir_module_create();
    fn  = ir_func_create(mod, "test_add_jump", IR_TY_I64, 0);

    /* const t0 = 8 */
    ir_emit(fn, IR_CONST, IR_TY_I64, "t0", NULL, NULL, NULL, 8L, 1);
    /* const t1 = 2 */
    ir_emit(fn, IR_CONST, IR_TY_I64, "t1", NULL, NULL, NULL, 2L, 2);
    /* t2 = t0 + t1 → should resolve to 10 */
    ir_emit(fn, IR_ADD,   IR_TY_I64, "t2", "t0", "t1", NULL, 0L, 3);
    /* ghost jump: NOP with src1 = t2 (unresolved target VReg) */
    nop_node = ir_emit(fn, IR_NOP, IR_TY_VOID, NULL, "t2", NULL, NULL, 0L, 4);
    /* Valid JUMPDEST label at offset 10 */
    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_evm_10", 0L, 10);
    /* something after the label to make the graph non-trivial */
    ir_emit(fn, IR_RET, IR_TY_I64, NULL, "t0", NULL, NULL, 0L, 11);

    /* Run the symbolic CFG pass */
    r = ir_pass_symbolic_cfg(fn);

    CHECK(nop_node->op == IR_BR,
          "NOP rewritten to IR_BR");
    CHECK(strcmp(nop_node->label, ".L_evm_10") == 0,
          "label set to .L_evm_10");
    CHECK(r.nodes_modified == 1,
          "1 jump resolved");
    CHECK(r.changed != 0,
          "pass reports changed=true");

    ir_module_free(mod);
}

/*
 * Unresolvable ghost: NOP with src1 = t0, but t0 is from an IR_LOAD
 * (cannot be statically resolved).
 */
static void test_unresolvable_jump(void) {
    ir_module_t *mod;
    ir_func_t   *fn;
    ir_node_t   *nop_node;
    ir_pass_result_t r;

    TEST("T2: IR_LOAD → ghost JUMP → unresolvable (runtime-dependent)");

    mod = ir_module_create();
    fn  = ir_func_create(mod, "test_unresolvable", IR_TY_I64, 0);

    /* const t0 = 0 (slot address) */
    ir_emit(fn, IR_CONST, IR_TY_I64, "t0", NULL, NULL, NULL, 0L, 1);
    /* t1 = LOAD from slot t0 — unknown value */
    ir_emit(fn, IR_LOAD, IR_TY_I64, "t1", "t0", NULL, NULL, 0L, 2);
    /* ghost jump: NOP with src1 = t1 (unresolvable) */
    nop_node = ir_emit(fn, IR_NOP, IR_TY_VOID, NULL, "t1", NULL, NULL, 0L, 3);
    /* some label to make the pass interesting */
    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_evm_42", 0L, 42);
    ir_emit(fn, IR_RET, IR_TY_I64, NULL, "t0", NULL, NULL, 0L, 43);

    r = ir_pass_symbolic_cfg(fn);

    CHECK(nop_node->op == IR_NOP,
          "NOP remains NOP (not rewritten)");
    CHECK(nop_node->tag == (int)IR_TAG_EVM_BARRIER,
          "tagged as EVM_BARRIER");
    CHECK(r.nodes_modified == 0,
          "0 jumps resolved");
    CHECK(r.changed == 0,
          "pass reports changed=false");

    ir_module_free(mod);
}

/*
 * Chained arithmetic: CONST(100) → CONST(5) → SUB → NOP(ghost)
 * Should resolve to 95.
 */
static void test_resolved_sub_jump(void) {
    ir_module_t *mod;
    ir_func_t   *fn;
    ir_node_t   *nop_node;
    ir_pass_result_t r;

    TEST("T3: CONST(100) - CONST(5) → ghost JUMP → resolve to .L_evm_95");

    mod = ir_module_create();
    fn  = ir_func_create(mod, "test_sub_jump", IR_TY_I64, 0);

    ir_emit(fn, IR_CONST, IR_TY_I64, "t0", NULL, NULL, NULL, 100L, 1);
    ir_emit(fn, IR_CONST, IR_TY_I64, "t1", NULL, NULL, NULL, 5L, 2);
    ir_emit(fn, IR_SUB,   IR_TY_I64, "t2", "t0", "t1", NULL, 0L, 3);
    nop_node = ir_emit(fn, IR_NOP, IR_TY_VOID, NULL, "t2", NULL, NULL, 0L, 4);
    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_evm_95", 0L, 95);
    ir_emit(fn, IR_RET,   IR_TY_I64, NULL, "t0", NULL, NULL, 0L, 96);

    r = ir_pass_symbolic_cfg(fn);

    CHECK(nop_node->op == IR_BR,
          "NOP rewritten to IR_BR");
    CHECK(strcmp(nop_node->label, ".L_evm_95") == 0,
          "label set to .L_evm_95");
    CHECK(r.nodes_modified == 1,
          "1 jump resolved");

    ir_module_free(mod);
}

/*
 * Invalid target: arithmetic resolves to 42, but no .L_evm_42 label exists.
 */
static void test_invalid_target(void) {
    ir_module_t *mod;
    ir_func_t   *fn;
    ir_node_t   *nop_node;
    ir_pass_result_t r;

    TEST("T4: resolved to offset 42 but no JUMPDEST → barrier");

    mod = ir_module_create();
    fn  = ir_func_create(mod, "test_invalid_target", IR_TY_I64, 0);

    ir_emit(fn, IR_CONST, IR_TY_I64, "t0", NULL, NULL, NULL, 42L, 1);
    nop_node = ir_emit(fn, IR_NOP, IR_TY_VOID, NULL, "t0", NULL, NULL, 0L, 2);
    /* NO label .L_evm_42 — only a different one */
    ir_emit(fn, IR_LABEL, IR_TY_VOID, NULL, NULL, NULL, ".L_evm_99", 0L, 99);
    ir_emit(fn, IR_RET,   IR_TY_I64, NULL, "t0", NULL, NULL, 0L, 100);

    r = ir_pass_symbolic_cfg(fn);

    CHECK(nop_node->op == IR_NOP,
          "NOP remains NOP (invalid target)");
    CHECK(nop_node->tag == (int)IR_TAG_EVM_BARRIER,
          "tagged as EVM_BARRIER");
    CHECK(r.nodes_modified == 0,
          "0 jumps resolved");

    ir_module_free(mod);
}

int main(void) {
    fprintf(stderr, "\n╔══════════════════════════════════════════════════════════════╗\n");
    fprintf(stderr, "║  ▸ ZKAEDI SYMBOLIC CFG TEST SUITE                            ║\n");
    fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n\n");

    test_resolved_add_jump();
    test_unresolvable_jump();
    test_resolved_sub_jump();
    test_invalid_target();

    fprintf(stderr, "\n  Results: %d FAILED\n", failures);
    if (failures == 0) {
        fprintf(stderr, "  ✓ ALL TESTS GREEN\n\n");
    }

    return failures > 0 ? 1 : 0;
}
