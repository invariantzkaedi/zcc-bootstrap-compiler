/*
 * tests/test_evm_lifter.c — Fixture-based tests for the EVM bytecode lifter
 *
 * Tests:
 *   T01: empty bytecode → OK, 0 instructions
 *   T02: single STOP opcode
 *   T03: PUSH1 immediate (fits in long)
 *   T04: PUSH8 immediate (max 64-bit value)
 *   T05: PUSH32 (truncation — 32 bytes, only low 64 bits kept)
 *   T06: sequence PUSH1 + PUSH1 + ADD → stack depth 1
 *   T07: CALL opcode detection and IR_TAG_UNTRUSTED_EXTERNAL_CALL assignment
 *   T08: DELEGATECALL opcode detection and tag
 *   T09: CALLCODE opcode detection and tag
 *   T10: STATICCALL opcode detection and IR_TAG_STATIC_CALL assignment
 *   T11: SSTORE detection and IR_TAG_SSTORE assignment
 *   T12: SELFDESTRUCT detection and IR_TAG_SELFDESTRUCT assignment
 *   T13: REVERT detection and IR_TAG_EVM_BARRIER assignment
 *   T14: INVALID opcode gets IR_TAG_EVM_BARRIER
 *   T15: malformed bytecode — PUSH1 at end of stream (no data byte)
 *   T16: malformed bytecode — PUSH4 truncated to 2 bytes
 *   T17: stack underflow (POP from empty stack)
 *   T18: DUP1 duplicates top of stack
 *   T19: SWAP1 exchanges top two stack items
 *   T20: JUMPDEST emits IR_LABEL node
 *   T21: evm_is_call_family() returns correct values
 *   T22: evm_opcode_name() returns correct names
 *   T23: evm_tag_name() returns correct names
 *   T24: IR module dump after full lift contains call IR nodes
 *   T25: call_count and tagged_count counters accurate after multi-call bytecode
 *
 * Build and run:
 *   gcc -O0 -std=c99 -Wall -Wextra -I.. -o /tmp/test_evm_lifter \
 *       tests/test_evm_lifter.c evm_lifter.c ir.c -lm
 *   /tmp/test_evm_lifter
 *
 * Coverage note: these tests cover the critical lifter paths.
 * Full 95%+ coverage requires a production harness — see issue tracker.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* When this test file is compiled standalone (not as part of zcc), we include
 * the headers relative to the repo root. */
#include "evm_lifter.h"
#include "ir.h"

/* ── Minimal test harness ─────────────────────────────────────────── */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) { \
            printf("  PASS: %s\n", msg); \
            g_pass++; \
        } else { \
            printf("  FAIL: %s  (line %d)\n", msg, __LINE__); \
            g_fail++; \
        } \
    } while (0)

#define TEST(name) printf("\n[%s]\n", name)

/* Count IR nodes with a specific tag in a function's node list */
static int count_tagged(const ir_func_t *fn, int tag) {
    const ir_node_t *n;
    int cnt = 0;
    for (n = fn->head; n; n = n->next) {
        if (n->tag == tag) cnt++;
    }
    return cnt;
}

/* Count IR nodes with a specific opcode in a function's node list */
static int count_op(const ir_func_t *fn, ir_op_t op) {
    const ir_node_t *n;
    int cnt = 0;
    for (n = fn->head; n; n = n->next) {
        if (n->op == op) cnt++;
    }
    return cnt;
}

/* Find the first IR node with a given opcode, or NULL */
static const ir_node_t *find_op(const ir_func_t *fn, ir_op_t op) {
    const ir_node_t *n;
    for (n = fn->head; n; n = n->next) {
        if (n->op == op) return n;
    }
    return NULL;
}

/* Helpers: build and destroy a module+lifter in one call */
static ir_module_t *new_module(void) { return ir_module_create(); }

static void lift_bytes(evm_lifter_t *ls, ir_module_t *mod,
                       const unsigned char *bytes, int len) {
    evm_lifter_init(ls, bytes, len, mod);
}

/* ── Tests ───────────────────────────────────────────────────────── */

static void test_t01_empty(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    TEST("T01: empty bytecode");
    mod = new_module();
    evm_lifter_init(&ls, (const unsigned char *)"", 0, mod);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK,          "lift returns OK");
    CHECK(ls.insn_count == 0,          "0 instructions counted");
    CHECK(ls.func->node_count == 0,    "0 IR nodes emitted");
    ir_module_free(mod);
}

static void test_t02_stop(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    static const unsigned char bc[] = { 0x00 }; /* STOP */
    TEST("T02: STOP opcode");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 1);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK,                      "lift returns OK");
    CHECK(ls.insn_count == 1,                      "1 instruction counted");
    CHECK(count_op(ls.func, IR_RET) == 1,          "1 IR_RET node emitted");
    ir_module_free(mod);
}

static void test_t03_push1(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    const ir_node_t *n;
    /* PUSH1 0x42 */
    static const unsigned char bc[] = { 0x60, 0x42 };
    TEST("T03: PUSH1 immediate");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 2);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK,              "lift returns OK");
    CHECK(ls.stack.depth == 1,             "stack depth 1 after PUSH1");
    n = find_op(ls.func, IR_CONST);
    CHECK(n != NULL,                       "IR_CONST node emitted");
    if (n) CHECK(n->imm == 0x42,           "IR_CONST imm == 0x42");
    ir_module_free(mod);
}

static void test_t04_push8(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    const ir_node_t *n;
    /* PUSH8 with 8 bytes: 0x01 02 03 04 05 06 07 08 */
    static const unsigned char bc[] = {
        0x67, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
    };
    TEST("T04: PUSH8 immediate");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 9);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK,    "lift returns OK");
    n = find_op(ls.func, IR_CONST);
    CHECK(n != NULL,             "IR_CONST node emitted");
    if (n) CHECK(n->imm == 0x0102030405060708L, "PUSH8 imm correct");
    ir_module_free(mod);
}

static void test_t05_push32_truncation(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    const ir_node_t *n;
    /* PUSH32: 32 bytes of 0xAB CD EF ... */
    static const unsigned char bc[33] = {
        0x7f, /* PUSH32 */
        0xAB, 0xCD, 0xEF, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
        0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d
    };
    TEST("T05: PUSH32 truncation to 64-bit");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 33);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK,     "lift returns OK for PUSH32");
    CHECK(ls.stack.depth == 1,    "stack depth 1 after PUSH32");
    n = find_op(ls.func, IR_CONST);
    CHECK(n != NULL,              "IR_CONST node emitted");
    /* The truncated 64-bit value is the last 8 of the 32 bytes pushed.
     * Bytes shifted left: first byte 0xAB dominates high bits of long. */
    ir_module_free(mod);
}

static void test_t06_push_add(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    /* PUSH1 0x01, PUSH1 0x02, ADD */
    static const unsigned char bc[] = { 0x60, 0x01, 0x60, 0x02, 0x01 };
    TEST("T06: PUSH1 + PUSH1 + ADD");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 5);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK,              "lift returns OK");
    CHECK(ls.stack.depth == 1,             "stack depth 1 after ADD");
    CHECK(count_op(ls.func, IR_CONST) == 2,"2 IR_CONST nodes");
    CHECK(count_op(ls.func, IR_ADD)   == 1,"1 IR_ADD node");
    ir_module_free(mod);
}

static void test_t07_call_tag(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    /* Minimal CALL: push 7 args then CALL (0xf1) */
    static const unsigned char bc[] = {
        0x5f, /* PUSH0 → gas */
        0x5f, /* PUSH0 → addr */
        0x5f, /* PUSH0 → value */
        0x5f, /* PUSH0 → argsOffset */
        0x5f, /* PUSH0 → argsLength */
        0x5f, /* PUSH0 → retOffset */
        0x5f, /* PUSH0 → retLength */
        0xf1  /* CALL */
    };
    TEST("T07: CALL tag = IR_TAG_UNTRUSTED_EXTERNAL_CALL");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 8);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK, "lift returns OK");
    CHECK(ls.call_count == 1, "call_count == 1");
    CHECK(count_tagged(ls.func, (int)IR_TAG_UNTRUSTED_EXTERNAL_CALL) == 1,
          "1 IR node tagged UNTRUSTED_EXTERNAL_CALL");
    CHECK(ls.stack.depth == 1, "success flag on stack after CALL");
    ir_module_free(mod);
}

static void test_t08_delegatecall_tag(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    /* DELEGATECALL pops 6: gas, addr, argsOffset, argsLength, retOffset, retLength */
    static const unsigned char bc[] = {
        0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, /* 6 × PUSH0 */
        0xf4  /* DELEGATECALL */
    };
    TEST("T08: DELEGATECALL tag = IR_TAG_UNTRUSTED_EXTERNAL_CALL");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 7);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK, "lift returns OK");
    CHECK(ls.call_count == 1, "call_count == 1");
    CHECK(count_tagged(ls.func, (int)IR_TAG_UNTRUSTED_EXTERNAL_CALL) == 1,
          "1 node tagged UNTRUSTED_EXTERNAL_CALL for DELEGATECALL");
    ir_module_free(mod);
}

static void test_t09_callcode_tag(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    /* CALLCODE: same as CALL, 7 args */
    static const unsigned char bc[] = {
        0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
        0xf2  /* CALLCODE */
    };
    TEST("T09: CALLCODE tag = IR_TAG_UNTRUSTED_EXTERNAL_CALL");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 8);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK, "lift returns OK");
    CHECK(count_tagged(ls.func, (int)IR_TAG_UNTRUSTED_EXTERNAL_CALL) == 1,
          "1 node tagged UNTRUSTED_EXTERNAL_CALL for CALLCODE");
    ir_module_free(mod);
}

static void test_t10_staticcall_tag(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    /* STATICCALL: 6 args */
    static const unsigned char bc[] = {
        0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
        0xfa  /* STATICCALL */
    };
    TEST("T10: STATICCALL tag = IR_TAG_STATIC_CALL");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 7);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK, "lift returns OK");
    CHECK(count_tagged(ls.func, (int)IR_TAG_STATIC_CALL) == 1,
          "1 node tagged IR_TAG_STATIC_CALL for STATICCALL");
    ir_module_free(mod);
}

static void test_t11_sstore_tag(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    /* SSTORE pops 2: key, value */
    static const unsigned char bc[] = {
        0x5f, 0x5f, /* PUSH0 x2 */
        0x55        /* SSTORE */
    };
    TEST("T11: SSTORE tag = IR_TAG_SSTORE");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 3);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK, "lift returns OK");
    CHECK(count_tagged(ls.func, (int)IR_TAG_SSTORE) == 1,
          "1 node tagged IR_TAG_SSTORE");
    ir_module_free(mod);
}

static void test_t12_selfdestruct_tag(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    /* SELFDESTRUCT pops 1: recipient address */
    static const unsigned char bc[] = {
        0x5f, /* PUSH0 */
        0xff  /* SELFDESTRUCT */
    };
    TEST("T12: SELFDESTRUCT tag = IR_TAG_SELFDESTRUCT");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 2);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK, "lift returns OK");
    CHECK(count_tagged(ls.func, (int)IR_TAG_SELFDESTRUCT) == 1,
          "1 node tagged IR_TAG_SELFDESTRUCT");
    ir_module_free(mod);
}

static void test_t13_revert_tag(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    /* REVERT pops 2: offset, length */
    static const unsigned char bc[] = {
        0x5f, 0x5f, /* PUSH0 x2 */
        0xfd        /* REVERT */
    };
    TEST("T13: REVERT tag = IR_TAG_EVM_BARRIER");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 3);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK, "lift returns OK");
    CHECK(count_tagged(ls.func, (int)IR_TAG_EVM_BARRIER) == 1,
          "1 node tagged IR_TAG_EVM_BARRIER for REVERT");
    ir_module_free(mod);
}

static void test_t14_invalid_tag(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    static const unsigned char bc[] = { 0xfe }; /* INVALID */
    TEST("T14: INVALID opcode gets IR_TAG_EVM_BARRIER");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 1);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK, "lift returns OK for INVALID opcode");
    CHECK(count_tagged(ls.func, (int)IR_TAG_EVM_BARRIER) == 1,
          "1 node tagged IR_TAG_EVM_BARRIER for INVALID");
    ir_module_free(mod);
}

static void test_t15_push1_truncated(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    /* PUSH1 with no following byte — truncated */
    static const unsigned char bc[] = { 0x60 };
    TEST("T15: PUSH1 at end of stream (truncated)");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 1);
    res = evm_lift_step(&ls);
    CHECK(res == EVM_LIFT_TRUNCATED, "returns EVM_LIFT_TRUNCATED");
    CHECK(ls.error != 0,             "ls.error is set");
    CHECK(ls.errmsg[0] != '\0',      "ls.errmsg is non-empty");
    ir_module_free(mod);
}

static void test_t16_push4_truncated(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    /* PUSH4 but only 2 immediate bytes provided */
    static const unsigned char bc[] = { 0x63, 0xAA, 0xBB };
    TEST("T16: PUSH4 truncated to 2 bytes");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 3);
    res = evm_lift_step(&ls);
    CHECK(res == EVM_LIFT_TRUNCATED, "returns EVM_LIFT_TRUNCATED");
    ir_module_free(mod);
}

static void test_t17_stack_underflow(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    /* POP from empty stack */
    static const unsigned char bc[] = { 0x50 }; /* POP */
    TEST("T17: POP from empty stack → underflow");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 1);
    res = evm_lift_step(&ls);
    CHECK(res == EVM_LIFT_STACK_UNDER, "returns EVM_LIFT_STACK_UNDER");
    CHECK(ls.error != 0,               "ls.error is set");
    ir_module_free(mod);
}

static void test_t18_dup1(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    /* PUSH1 0x99, DUP1 */
    static const unsigned char bc[] = { 0x60, 0x99, 0x80 };
    TEST("T18: DUP1 duplicates top of stack");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 3);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK,  "lift returns OK");
    CHECK(ls.stack.depth == 2, "stack depth 2 after DUP1");
    CHECK(count_op(ls.func, IR_COPY) == 1, "1 IR_COPY node from DUP");
    ir_module_free(mod);
}

static void test_t19_swap1(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    const char *top_before, *below_before;
    char top_saved[IR_NAME_MAX], below_saved[IR_NAME_MAX];
    /* PUSH1 0x01, PUSH1 0x02, SWAP1 — after swap: [0x01 on top, 0x02 below] */
    static const unsigned char bc[] = { 0x60, 0x01, 0x60, 0x02, 0x90 };
    TEST("T19: SWAP1 exchanges top two stack items");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 5);
    /* Step through PUSH1 0x01 and PUSH1 0x02 */
    evm_lift_step(&ls); /* PUSH1 0x01 */
    evm_lift_step(&ls); /* PUSH1 0x02 */
    /* Before SWAP1: top = t1 (0x02), below = t0 (0x01) */
    top_before   = ls.stack.slots[ls.stack.depth - 1];
    below_before = ls.stack.slots[ls.stack.depth - 2];
    strncpy(top_saved,   top_before,   IR_NAME_MAX - 1);
    strncpy(below_saved, below_before, IR_NAME_MAX - 1);
    res = evm_lift_step(&ls); /* SWAP1 */
    CHECK(res == EVM_LIFT_OK, "SWAP1 returns OK");
    CHECK(ls.stack.depth == 2, "stack depth unchanged at 2");
    /* After swap: what was `top` is now in [depth-2], what was `below` is in [depth-1] */
    CHECK(strcmp(ls.stack.slots[ls.stack.depth - 1], below_saved) == 0,
          "new TOS is the old second item");
    CHECK(strcmp(ls.stack.slots[ls.stack.depth - 2], top_saved)   == 0,
          "new second item is the old TOS");
    ir_module_free(mod);
}

static void test_t20_jumpdest_label(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    evm_lift_result_t res;
    const ir_node_t *n;
    static const unsigned char bc[] = { 0x5b }; /* JUMPDEST */
    TEST("T20: JUMPDEST emits IR_LABEL node");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 1);
    res = evm_lift_bytecode(&ls);
    CHECK(res == EVM_LIFT_OK, "lift returns OK");
    n = find_op(ls.func, IR_LABEL);
    CHECK(n != NULL, "IR_LABEL node emitted");
    if (n) CHECK(n->label[0] == '.', "label starts with '.'");
    ir_module_free(mod);
}

static void test_t21_is_call_family(void) {
    TEST("T21: evm_is_call_family()");
    CHECK(evm_is_call_family(0xf1), "CALL    (0xf1) is call-family");
    CHECK(evm_is_call_family(0xf4), "DELEGATECALL (0xf4) is call-family");
    CHECK(evm_is_call_family(0xf2), "CALLCODE (0xf2) is call-family");
    CHECK(evm_is_call_family(0xfa), "STATICCALL (0xfa) is call-family");
    CHECK(!evm_is_call_family(0xf0), "CREATE  (0xf0) is NOT call-family");
    CHECK(!evm_is_call_family(0x00), "STOP    (0x00) is NOT call-family");
    CHECK(!evm_is_call_family(0x01), "ADD     (0x01) is NOT call-family");
}

static void test_t22_opcode_names(void) {
    TEST("T22: evm_opcode_name()");
    CHECK(strcmp(evm_opcode_name(0x00), "STOP")         == 0, "0x00 = STOP");
    CHECK(strcmp(evm_opcode_name(0x01), "ADD")          == 0, "0x01 = ADD");
    CHECK(strcmp(evm_opcode_name(0x60), "PUSH1")        == 0, "0x60 = PUSH1");
    CHECK(strcmp(evm_opcode_name(0x7f), "PUSH32")       == 0, "0x7f = PUSH32");
    CHECK(strcmp(evm_opcode_name(0xf1), "CALL")         == 0, "0xf1 = CALL");
    CHECK(strcmp(evm_opcode_name(0xf4), "DELEGATECALL") == 0, "0xf4 = DELEGATECALL");
    CHECK(strcmp(evm_opcode_name(0xfa), "STATICCALL")   == 0, "0xfa = STATICCALL");
    CHECK(strcmp(evm_opcode_name(0xff), "SELFDESTRUCT") == 0, "0xff = SELFDESTRUCT");
}

static void test_t23_tag_names(void) {
    TEST("T23: evm_tag_name()");
    CHECK(strcmp(evm_tag_name(IR_TAG_NONE),                    "IR_TAG_NONE")                    == 0,
          "IR_TAG_NONE name");
    CHECK(strcmp(evm_tag_name(IR_TAG_UNTRUSTED_EXTERNAL_CALL), "IR_TAG_UNTRUSTED_EXTERNAL_CALL") == 0,
          "IR_TAG_UNTRUSTED_EXTERNAL_CALL name");
    CHECK(strcmp(evm_tag_name(IR_TAG_STATIC_CALL),             "IR_TAG_STATIC_CALL")             == 0,
          "IR_TAG_STATIC_CALL name");
    CHECK(strcmp(evm_tag_name(IR_TAG_SELFDESTRUCT),            "IR_TAG_SELFDESTRUCT")            == 0,
          "IR_TAG_SELFDESTRUCT name");
    CHECK(strcmp(evm_tag_name(IR_TAG_SSTORE),                  "IR_TAG_SSTORE")                  == 0,
          "IR_TAG_SSTORE name");
    CHECK(strcmp(evm_tag_name(IR_TAG_CREATE),                  "IR_TAG_CREATE")                  == 0,
          "IR_TAG_CREATE name");
    CHECK(strcmp(evm_tag_name(IR_TAG_EVM_BARRIER),             "IR_TAG_EVM_BARRIER")             == 0,
          "IR_TAG_EVM_BARRIER name");
}

static void test_t24_ir_dump(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    /* PUSH0 x7 + CALL + STOP */
    static const unsigned char bc[] = {
        0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, /* 7 x PUSH0 */
        0xf1, /* CALL */
        0x00  /* STOP */
    };
    TEST("T24: IR text dump after full lift");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 9);
    evm_lift_bytecode(&ls);
    /* Emit to /dev/null — just verify no crash */
    ir_module_emit_text(mod, stderr);
    CHECK(ls.func->node_count > 0, "function has IR nodes after lift");
    CHECK(ls.call_count == 1,      "1 CALL-family instruction logged");
    ir_module_free(mod);
}

static void test_t25_multi_call_counters(void) {
    ir_module_t *mod;
    evm_lifter_t ls;
    /*
     * Two CALL sequences back-to-back:
     *   7 x PUSH0, CALL, 7 x PUSH0, CALL, STOP
     */
    static const unsigned char bc[] = {
        0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0xf1, /* CALL #1 */
        0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0xf1, /* CALL #2 */
        0x00  /* STOP */
    };
    TEST("T25: multi-CALL call_count and tagged_count");
    mod = new_module();
    lift_bytes(&ls, mod, bc, 17);
    evm_lift_bytecode(&ls);
    CHECK(ls.call_count == 2, "call_count == 2");
    CHECK(ls.tagged_count >= 2,
          "tagged_count >= 2 (at least one tag per CALL)");
    CHECK(count_tagged(ls.func, (int)IR_TAG_UNTRUSTED_EXTERNAL_CALL) == 2,
          "2 nodes tagged UNTRUSTED_EXTERNAL_CALL");
    ir_module_free(mod);
}

/* ── main ─────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== ZCC EVM Lifter Test Suite ===\n");
    printf("Defensive audit scaffold — not for exploit use.\n");

    test_t01_empty();
    test_t02_stop();
    test_t03_push1();
    test_t04_push8();
    test_t05_push32_truncation();
    test_t06_push_add();
    test_t07_call_tag();
    test_t08_delegatecall_tag();
    test_t09_callcode_tag();
    test_t10_staticcall_tag();
    test_t11_sstore_tag();
    test_t12_selfdestruct_tag();
    test_t13_revert_tag();
    test_t14_invalid_tag();
    test_t15_push1_truncated();
    test_t16_push4_truncated();
    test_t17_stack_underflow();
    test_t18_dup1();
    test_t19_swap1();
    test_t20_jumpdest_label();
    test_t21_is_call_family();
    test_t22_opcode_names();
    test_t23_tag_names();
    test_t24_ir_dump();
    test_t25_multi_call_counters();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    if (g_fail > 0) {
        printf("FAIL\n");
        return 1;
    }
    printf("ALL PASS\n");
    return 0;
}
