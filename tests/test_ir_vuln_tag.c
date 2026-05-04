/*
 * tests/test_ir_vuln_tag.c — Unit tests for the IR vulnerability tag schema
 *
 * Tests:
 *   T01: IR_VULN_NONE is zero (safe default)
 *   T02: ir_vuln_tag_set — single tag on a node
 *   T03: ir_vuln_tag_has — single tag present
 *   T04: ir_vuln_tag_has — tag absent
 *   T05: ir_vuln_tag_set — multiple tags on one node (OR-ed bitmask)
 *   T06: ir_vuln_tag_has — check multi-tag combination present
 *   T07: ir_vuln_tag_has — partial combination check
 *   T08: ir_vuln_tag_to_str — each single-bit tag maps to stable string
 *   T09: ir_vuln_tag_to_str — IR_VULN_NONE returns "IR_VULN_NONE"
 *   T10: ir_vuln_tag_to_str — combined bits returns "IR_VULN_MULTI"
 *   T11: ir_vuln_tag_to_str — unrecognized single bit returns "IR_VULN_UNKNOWN"
 *   T12: ir_vuln_tag_from_str — parse each stable tag name
 *   T13: ir_vuln_tag_from_str — NULL input → IR_VULN_UNKNOWN (no crash)
 *   T14: ir_vuln_tag_from_str — empty string → IR_VULN_UNKNOWN
 *   T15: ir_vuln_tag_from_str — garbage string → IR_VULN_UNKNOWN
 *   T16: ir_vuln_tag_unknown_safe — known bits → identity
 *   T17: ir_vuln_tag_unknown_safe — unknown bits → IR_VULN_UNKNOWN
 *   T18: ir_vuln_map_from_evm_tag — all legacy EVM tags mapped correctly
 *   T19: ir_vuln_map_from_evm_tag — unknown EVM tag → IR_VULN_UNKNOWN
 *   T20: ir_vuln_tags_to_json — zero-tag node → "[]"
 *   T21: ir_vuln_tags_to_json — single tag → JSON array with one name
 *   T22: ir_vuln_tags_to_json — multiple tags → JSON array with all names
 *   T23: ir_pass_vuln_scan — module with no tagged nodes returns 0
 *   T24: ir_pass_vuln_scan — module with tagged nodes returns correct count
 *   T25: EVM lifter DELEGATECALL sets IR_VULN_DELEGATE_CALL + IR_VULN_PRIV_BOUNDARY
 *   T26: EVM lifter CALL sets IR_VULN_UNTRUSTED_CALL in vuln_tags
 *   T27: EVM lifter SSTORE sets IR_VULN_STATE_WRITE in vuln_tags
 *   T28: EVM lifter STATICCALL sets IR_VULN_STATIC_CALL in vuln_tags
 *   T29: ir_vuln_tag_set is idempotent (double-set same tags)
 *   T30: ir_node_alloc() zeroes vuln_tags (safe default)
 *
 * Build and run:
 *   gcc -O0 -std=c99 -Wall -Wextra -I. \
 *       -o /tmp/test_ir_vuln_tag \
 *       tests/test_ir_vuln_tag.c ir_vuln_tag.c evm_lifter.c ir.c -lm
 *   /tmp/test_ir_vuln_tag
 *
 * Coverage note: these tests cover all API paths in ir_vuln_tag.c.
 * Full 95%+ production coverage requires a production harness — see issue.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir_vuln_tag.h"
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

/* ── T01: IR_VULN_NONE is zero ────────────────────────────────────── */
static void test_none_is_zero(void)
{
    TEST("T01: IR_VULN_NONE == 0");
    CHECK(IR_VULN_NONE == 0, "IR_VULN_NONE is 0");
    CHECK((int)IR_VULN_UNTRUSTED_CALL != 0, "IR_VULN_UNTRUSTED_CALL != 0");
}

/* ── T02-T07: ir_vuln_tag_set / ir_vuln_tag_has ──────────────────── */
static void test_set_has(void)
{
    ir_node_t *n = ir_node_alloc();

    TEST("T02: ir_vuln_tag_set single tag");
    ir_vuln_tag_set(n, IR_VULN_UNTRUSTED_CALL);
    CHECK(n->vuln_tags == (unsigned int)IR_VULN_UNTRUSTED_CALL,
          "vuln_tags == IR_VULN_UNTRUSTED_CALL after set");

    TEST("T03: ir_vuln_tag_has single tag present");
    CHECK(ir_vuln_tag_has(n, IR_VULN_UNTRUSTED_CALL),
          "has IR_VULN_UNTRUSTED_CALL");

    TEST("T04: ir_vuln_tag_has absent tag");
    CHECK(!ir_vuln_tag_has(n, IR_VULN_DELEGATE_CALL),
          "does not have IR_VULN_DELEGATE_CALL");

    TEST("T05: ir_vuln_tag_set multiple tags");
    ir_vuln_tag_set(n, (ir_vuln_tag_t)(IR_VULN_DELEGATE_CALL | IR_VULN_PRIV_BOUNDARY));
    CHECK(ir_vuln_tag_has(n, IR_VULN_DELEGATE_CALL),
          "has IR_VULN_DELEGATE_CALL after second set");
    CHECK(ir_vuln_tag_has(n, IR_VULN_PRIV_BOUNDARY),
          "has IR_VULN_PRIV_BOUNDARY after second set");
    CHECK(ir_vuln_tag_has(n, IR_VULN_UNTRUSTED_CALL),
          "still has IR_VULN_UNTRUSTED_CALL after second set");

    TEST("T06: ir_vuln_tag_has combination all present");
    CHECK(ir_vuln_tag_has(n,
          (ir_vuln_tag_t)(IR_VULN_UNTRUSTED_CALL |
                          IR_VULN_DELEGATE_CALL   |
                          IR_VULN_PRIV_BOUNDARY)),
          "has all three tags as combination");

    TEST("T07: ir_vuln_tag_has partial combination");
    CHECK(ir_vuln_tag_has(n,
          (ir_vuln_tag_t)(IR_VULN_UNTRUSTED_CALL | IR_VULN_DELEGATE_CALL)),
          "has partial combination of two tags");
    CHECK(!ir_vuln_tag_has(n, IR_VULN_STATE_WRITE),
          "does not have IR_VULN_STATE_WRITE");

    free(n);
}

/* ── T08-T11: ir_vuln_tag_to_str ─────────────────────────────────── */
static void test_to_str(void)
{
    TEST("T08: ir_vuln_tag_to_str each single-bit tag");
    CHECK(strcmp(ir_vuln_tag_to_str(IR_VULN_UNTRUSTED_CALL),
                 "IR_VULN_UNTRUSTED_CALL") == 0,
          "UNTRUSTED_CALL string");
    CHECK(strcmp(ir_vuln_tag_to_str(IR_VULN_DELEGATE_CALL),
                 "IR_VULN_DELEGATE_CALL") == 0,
          "DELEGATE_CALL string");
    CHECK(strcmp(ir_vuln_tag_to_str(IR_VULN_STATIC_CALL),
                 "IR_VULN_STATIC_CALL") == 0,
          "STATIC_CALL string");
    CHECK(strcmp(ir_vuln_tag_to_str(IR_VULN_STATE_WRITE),
                 "IR_VULN_STATE_WRITE") == 0,
          "STATE_WRITE string");
    CHECK(strcmp(ir_vuln_tag_to_str(IR_VULN_PRIV_BOUNDARY),
                 "IR_VULN_PRIV_BOUNDARY") == 0,
          "PRIV_BOUNDARY string");
    CHECK(strcmp(ir_vuln_tag_to_str(IR_VULN_UNKNOWN),
                 "IR_VULN_UNKNOWN") == 0,
          "UNKNOWN string");
    CHECK(strcmp(ir_vuln_tag_to_str(IR_VULN_SELFDESTRUCT),
                 "IR_VULN_SELFDESTRUCT") == 0,
          "SELFDESTRUCT string");
    CHECK(strcmp(ir_vuln_tag_to_str(IR_VULN_CONTRACT_CREATE),
                 "IR_VULN_CONTRACT_CREATE") == 0,
          "CONTRACT_CREATE string");
    CHECK(strcmp(ir_vuln_tag_to_str(IR_VULN_EXEC_BARRIER),
                 "IR_VULN_EXEC_BARRIER") == 0,
          "EXEC_BARRIER string");

    TEST("T09: ir_vuln_tag_to_str IR_VULN_NONE");
    CHECK(strcmp(ir_vuln_tag_to_str(IR_VULN_NONE), "IR_VULN_NONE") == 0,
          "NONE string");

    TEST("T10: ir_vuln_tag_to_str multi-bit returns IR_VULN_MULTI");
    {
        ir_vuln_tag_t combo = (ir_vuln_tag_t)(IR_VULN_UNTRUSTED_CALL |
                                              IR_VULN_DELEGATE_CALL);
        CHECK(strcmp(ir_vuln_tag_to_str(combo), "IR_VULN_MULTI") == 0,
              "combined bits → IR_VULN_MULTI");
    }

    TEST("T11: ir_vuln_tag_to_str unrecognized single bit");
    {
        /* Use bit 29 which is reserved and not in IR_VULN_ALL_KNOWN */
        ir_vuln_tag_t unk = (ir_vuln_tag_t)(1 << 29);
        CHECK(strcmp(ir_vuln_tag_to_str(unk), "IR_VULN_UNKNOWN") == 0,
              "unrecognized single bit → IR_VULN_UNKNOWN");
    }
}

/* ── T12-T15: ir_vuln_tag_from_str ───────────────────────────────── */
static void test_from_str(void)
{
    TEST("T12: ir_vuln_tag_from_str parse each stable name");
    CHECK(ir_vuln_tag_from_str("IR_VULN_UNTRUSTED_CALL")  == IR_VULN_UNTRUSTED_CALL,
          "parse UNTRUSTED_CALL");
    CHECK(ir_vuln_tag_from_str("IR_VULN_DELEGATE_CALL")   == IR_VULN_DELEGATE_CALL,
          "parse DELEGATE_CALL");
    CHECK(ir_vuln_tag_from_str("IR_VULN_STATIC_CALL")     == IR_VULN_STATIC_CALL,
          "parse STATIC_CALL");
    CHECK(ir_vuln_tag_from_str("IR_VULN_STATE_WRITE")     == IR_VULN_STATE_WRITE,
          "parse STATE_WRITE");
    CHECK(ir_vuln_tag_from_str("IR_VULN_PRIV_BOUNDARY")   == IR_VULN_PRIV_BOUNDARY,
          "parse PRIV_BOUNDARY");
    CHECK(ir_vuln_tag_from_str("IR_VULN_UNKNOWN")         == IR_VULN_UNKNOWN,
          "parse UNKNOWN");
    CHECK(ir_vuln_tag_from_str("IR_VULN_SELFDESTRUCT")    == IR_VULN_SELFDESTRUCT,
          "parse SELFDESTRUCT");
    CHECK(ir_vuln_tag_from_str("IR_VULN_CONTRACT_CREATE") == IR_VULN_CONTRACT_CREATE,
          "parse CONTRACT_CREATE");
    CHECK(ir_vuln_tag_from_str("IR_VULN_EXEC_BARRIER")    == IR_VULN_EXEC_BARRIER,
          "parse EXEC_BARRIER");
    CHECK(ir_vuln_tag_from_str("IR_VULN_NONE")            == IR_VULN_NONE,
          "parse NONE");

    TEST("T13: ir_vuln_tag_from_str NULL → IR_VULN_UNKNOWN (no crash)");
    CHECK(ir_vuln_tag_from_str(NULL) == IR_VULN_UNKNOWN,
          "NULL → IR_VULN_UNKNOWN");

    TEST("T14: ir_vuln_tag_from_str empty string → IR_VULN_UNKNOWN");
    CHECK(ir_vuln_tag_from_str("") == IR_VULN_UNKNOWN,
          "empty string → IR_VULN_UNKNOWN");

    TEST("T15: ir_vuln_tag_from_str garbage → IR_VULN_UNKNOWN");
    CHECK(ir_vuln_tag_from_str("TOTAL_GARBAGE_99999") == IR_VULN_UNKNOWN,
          "garbage → IR_VULN_UNKNOWN");
    CHECK(ir_vuln_tag_from_str("ir_vuln_untrusted_call") == IR_VULN_UNKNOWN,
          "lowercase name → IR_VULN_UNKNOWN (case-sensitive)");
}

/* ── T16-T17: ir_vuln_tag_unknown_safe ───────────────────────────── */
static void test_unknown_safe(void)
{
    TEST("T16: ir_vuln_tag_unknown_safe known bits → identity");
    CHECK(ir_vuln_tag_unknown_safe((unsigned int)IR_VULN_UNTRUSTED_CALL) ==
          IR_VULN_UNTRUSTED_CALL,
          "known single bit preserved");
    CHECK(ir_vuln_tag_unknown_safe((unsigned int)(IR_VULN_UNTRUSTED_CALL |
                                                  IR_VULN_STATIC_CALL)) ==
          (ir_vuln_tag_t)(IR_VULN_UNTRUSTED_CALL | IR_VULN_STATIC_CALL),
          "known combo preserved");
    CHECK(ir_vuln_tag_unknown_safe(0) == IR_VULN_NONE,
          "zero → IR_VULN_NONE");

    TEST("T17: ir_vuln_tag_unknown_safe unknown bits → IR_VULN_UNKNOWN");
    /* bit 20 is in reserved space, not in IR_VULN_ALL_KNOWN */
    CHECK(ir_vuln_tag_unknown_safe(1u << 20) == IR_VULN_UNKNOWN,
          "reserved bit → IR_VULN_UNKNOWN");
    CHECK(ir_vuln_tag_unknown_safe(0xFFFFFFFFu) == IR_VULN_UNKNOWN,
          "all bits set → IR_VULN_UNKNOWN");
}

/* ── T18-T19: ir_vuln_map_from_evm_tag ───────────────────────────── */
static void test_evm_tag_mapping(void)
{
    TEST("T18: ir_vuln_map_from_evm_tag all legacy EVM tags");
    /* evm_ir_tag_t values: 0=NONE, 1=UNTRUSTED_EXT_CALL, 2=STATIC_CALL,
     * 3=SELFDESTRUCT, 4=SSTORE(state write), 5=CREATE, 6=BARRIER */
    CHECK(ir_vuln_map_from_evm_tag(0) == IR_VULN_NONE,
          "evm_tag=0 → IR_VULN_NONE");
    CHECK(ir_vuln_map_from_evm_tag(1) == IR_VULN_UNTRUSTED_CALL,
          "evm_tag=1 (UNTRUSTED_EXTERNAL_CALL) → IR_VULN_UNTRUSTED_CALL");
    CHECK(ir_vuln_map_from_evm_tag(2) == IR_VULN_STATIC_CALL,
          "evm_tag=2 (STATIC_CALL) → IR_VULN_STATIC_CALL");
    CHECK(ir_vuln_map_from_evm_tag(3) == IR_VULN_SELFDESTRUCT,
          "evm_tag=3 (SELFDESTRUCT) → IR_VULN_SELFDESTRUCT");
    CHECK(ir_vuln_map_from_evm_tag(4) == IR_VULN_STATE_WRITE,
          "evm_tag=4 (SSTORE) → IR_VULN_STATE_WRITE");
    CHECK(ir_vuln_map_from_evm_tag(5) == IR_VULN_CONTRACT_CREATE,
          "evm_tag=5 (CREATE) → IR_VULN_CONTRACT_CREATE");
    CHECK(ir_vuln_map_from_evm_tag(6) == IR_VULN_EXEC_BARRIER,
          "evm_tag=6 (EVM_BARRIER) → IR_VULN_EXEC_BARRIER");

    TEST("T19: ir_vuln_map_from_evm_tag unknown → IR_VULN_UNKNOWN");
    CHECK(ir_vuln_map_from_evm_tag(99)  == IR_VULN_UNKNOWN,
          "evm_tag=99 → IR_VULN_UNKNOWN");
    CHECK(ir_vuln_map_from_evm_tag(-1)  == IR_VULN_UNKNOWN,
          "evm_tag=-1 → IR_VULN_UNKNOWN");
    CHECK(ir_vuln_map_from_evm_tag(255) == IR_VULN_UNKNOWN,
          "evm_tag=255 → IR_VULN_UNKNOWN");
}

/* ── T20-T22: ir_vuln_tags_to_json ───────────────────────────────── */
static void test_json(void)
{
    char buf[512];
    ir_node_t *n = ir_node_alloc();

    TEST("T20: ir_vuln_tags_to_json zero-tag node → []");
    ir_vuln_tags_to_json(n, buf, (int)sizeof(buf));
    CHECK(strcmp(buf, "[]") == 0, "zero tags → \"[]\"");

    TEST("T21: ir_vuln_tags_to_json single tag");
    ir_vuln_tag_set(n, IR_VULN_UNTRUSTED_CALL);
    ir_vuln_tags_to_json(n, buf, (int)sizeof(buf));
    CHECK(strstr(buf, "\"IR_VULN_UNTRUSTED_CALL\"") != NULL,
          "single tag in JSON");
    CHECK(buf[0] == '[', "starts with [");
    CHECK(buf[strlen(buf)-1] == ']', "ends with ]");

    TEST("T22: ir_vuln_tags_to_json multiple tags");
    ir_vuln_tag_set(n, (ir_vuln_tag_t)(IR_VULN_DELEGATE_CALL | IR_VULN_PRIV_BOUNDARY));
    ir_vuln_tags_to_json(n, buf, (int)sizeof(buf));
    CHECK(strstr(buf, "\"IR_VULN_UNTRUSTED_CALL\"")  != NULL,
          "multiple tags: UNTRUSTED_CALL present");
    CHECK(strstr(buf, "\"IR_VULN_DELEGATE_CALL\"")   != NULL,
          "multiple tags: DELEGATE_CALL present");
    CHECK(strstr(buf, "\"IR_VULN_PRIV_BOUNDARY\"")   != NULL,
          "multiple tags: PRIV_BOUNDARY present");

    free(n);
}

/* ── T23-T24: ir_pass_vuln_scan ──────────────────────────────────── */
static void test_pass_scan(void)
{
    ir_module_t *mod;
    ir_func_t   *fn;
    ir_node_t   *n;
    int cnt;

    TEST("T23: ir_pass_vuln_scan module with no tagged nodes → 0");
    mod = ir_module_create();
    fn  = ir_func_create(mod, "clean_func", IR_TY_VOID, 0);
    ir_emit(fn, IR_NOP, IR_TY_VOID, NULL, NULL, NULL, NULL, 0L, 0);
    cnt = ir_pass_vuln_scan(mod, NULL);  /* NULL fp → stdout */
    CHECK(cnt == 0, "no tagged nodes → scan returns 0");
    ir_module_free(mod);

    TEST("T24: ir_pass_vuln_scan module with tagged nodes → correct count");
    mod = ir_module_create();
    fn  = ir_func_create(mod, "vuln_func", IR_TY_I64, 0);

    /* node 1: CALL tagged untrusted */
    n = ir_emit(fn, IR_CALL, IR_TY_I64, "t0", NULL, NULL, "target", 0L, 1);
    ir_vuln_tag_set(n, IR_VULN_UNTRUSTED_CALL);

    /* node 2: STORE tagged state write + privilege boundary */
    n = ir_emit(fn, IR_STORE, IR_TY_I64, "slot", "val", NULL, NULL, 0L, 2);
    ir_vuln_tag_set(n, (ir_vuln_tag_t)(IR_VULN_STATE_WRITE | IR_VULN_PRIV_BOUNDARY));

    /* node 3: untagged */
    ir_emit(fn, IR_NOP, IR_TY_VOID, NULL, NULL, NULL, NULL, 0L, 3);

    cnt = ir_pass_vuln_scan(mod, NULL);
    CHECK(cnt == 2, "2 tagged nodes → scan returns 2");
    ir_module_free(mod);
}

/* ── T25-T28: EVM lifter → vuln_tags integration ─────────────────── */
static void test_evm_lifter_vuln_tags(void)
{
    ir_module_t  *mod;
    evm_lifter_t  ls;
    const ir_node_t *n;
    int found_delegate    = 0;
    int found_priv        = 0;
    int found_untrusted   = 0;
    int found_state_write = 0;
    int found_static_call = 0;

    /* Build bytecode:
     *   6x PUSH0 (stack padding so pops succeed)
     *   PUSH1 0x00  PUSH1 0x00  (extra stack for 7-arg DELEGATECALL)
     *   DELEGATECALL  (0xf4)
     *   PUSH1 0x00 * 7 + CALL (0xf1)
     *   PUSH1 0x00 * 2 + SSTORE (0x55)
     *   PUSH1 0x00 * 6 + STATICCALL (0xfa)
     *   STOP
     */
    static const unsigned char bc[] = {
        /* pre-push 8 zeros for DELEGATECALL (6 args) */
        0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,   /* PUSH0 x6 */
        0xf4,                                   /* DELEGATECALL */
        0x50,                                   /* POP (pop result) */
        /* pre-push 7 zeros for CALL (7 args) */
        0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
        0xf1,                                   /* CALL */
        0x50,                                   /* POP */
        /* pre-push 2 zeros for SSTORE (2 args) */
        0x5f, 0x5f,
        0x55,                                   /* SSTORE */
        /* pre-push 6 zeros for STATICCALL (6 args) */
        0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
        0xfa,                                   /* STATICCALL */
        0x50,                                   /* POP */
        0x00                                    /* STOP */
    };

    mod = ir_module_create();
    evm_lifter_init(&ls, bc, (int)sizeof(bc), mod);
    evm_lift_bytecode(&ls);

    /* Scan all nodes for expected vuln_tags */
    for (n = ls.func->head; n; n = n->next) {
        if (ir_vuln_tag_has(n, IR_VULN_DELEGATE_CALL))  found_delegate    = 1;
        if (ir_vuln_tag_has(n, IR_VULN_PRIV_BOUNDARY))  found_priv        = 1;
        if (ir_vuln_tag_has(n, IR_VULN_UNTRUSTED_CALL)) found_untrusted   = 1;
        if (ir_vuln_tag_has(n, IR_VULN_STATE_WRITE))    found_state_write = 1;
        if (ir_vuln_tag_has(n, IR_VULN_STATIC_CALL))    found_static_call = 1;
    }

    TEST("T25: EVM DELEGATECALL → IR_VULN_DELEGATE_CALL + IR_VULN_PRIV_BOUNDARY");
    CHECK(found_delegate, "DELEGATECALL sets IR_VULN_DELEGATE_CALL in vuln_tags");
    CHECK(found_priv,     "DELEGATECALL sets IR_VULN_PRIV_BOUNDARY in vuln_tags");

    TEST("T26: EVM CALL → IR_VULN_UNTRUSTED_CALL in vuln_tags");
    CHECK(found_untrusted, "CALL sets IR_VULN_UNTRUSTED_CALL in vuln_tags");

    TEST("T27: EVM SSTORE → IR_VULN_STATE_WRITE in vuln_tags");
    CHECK(found_state_write, "SSTORE sets IR_VULN_STATE_WRITE in vuln_tags");

    TEST("T28: EVM STATICCALL → IR_VULN_STATIC_CALL in vuln_tags");
    CHECK(found_static_call, "STATICCALL sets IR_VULN_STATIC_CALL in vuln_tags");

    ir_module_free(mod);
}

/* ── T29-T30: idempotency and zero default ───────────────────────── */
static void test_idempotent_and_default(void)
{
    ir_node_t *n = ir_node_alloc();

    TEST("T29: ir_vuln_tag_set is idempotent");
    ir_vuln_tag_set(n, IR_VULN_STATE_WRITE);
    ir_vuln_tag_set(n, IR_VULN_STATE_WRITE);
    CHECK(n->vuln_tags == (unsigned int)IR_VULN_STATE_WRITE,
          "double-set same tag: vuln_tags unchanged");

    free(n);

    TEST("T30: ir_node_alloc() zeroes vuln_tags");
    {
        ir_node_t *fresh = ir_node_alloc();
        CHECK(fresh->vuln_tags == 0, "fresh node: vuln_tags == 0");
        CHECK(fresh->tag == 0,       "fresh node: legacy tag == 0");
        free(fresh);
    }
}

/* ── main ─────────────────────────────────────────────────────────── */
int main(void)
{
    printf("=== IR Vulnerability Tag Schema Tests ===\n");

    test_none_is_zero();
    test_set_has();
    test_to_str();
    test_from_str();
    test_unknown_safe();
    test_evm_tag_mapping();
    test_json();
    test_pass_scan();
    test_evm_lifter_vuln_tags();
    test_idempotent_and_default();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    if (g_fail == 0)
        printf("ALL PASS\n");
    else
        printf("FAILURES PRESENT\n");

    return g_fail ? 1 : 0;
}
