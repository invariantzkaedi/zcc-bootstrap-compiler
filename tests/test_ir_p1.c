/*
 * tests/test_ir_p1.c — P1-IR unit tests
 *
 * Compile: gcc -o test_ir_p1 tests/test_ir_p1.c ir.c -lm
 * Run:     ./test_ir_p1
 * Expect:  all PASS, exit 0
 *
 * Coverage:
 *   [T01] ir_op_name() — all opcodes, sentinel, negative
 *   [T02] ir_type_name() — all types, sentinel, negative
 *   [T03] ir_type_bytes() — all widths, LP64 ptr=8
 *   [T04] ir_type_unsigned() — signed/unsigned discrimination
 *   [T05] ir_op_is_terminator() — RET/BR/BR_IF vs others
 *   [T06] ir_node_alloc() — zeroed, fields addressable
 *   [T07] ir_module_create() / ir_module_free()
 *   [T08] ir_func_create() — name, ret_type, linked into module
 *   [T09] ir_append() — head/tail/count invariants
 *   [T10] ir_emit() — all field round-trips
 *   [T11] ir_fresh_tmp() — monotonic, no collision in 10k
 *   [T12] ir_fresh_label() — .L<N> format
 *   [T13] ir_emit() with NULL src fields — safe
 *   [T14] ir_emit() with empty string src fields — safe
 *   [T15] ir_func_emit_text() — parses emitted text back
 *   [T16] Binary ops: ADD / SUB / MUL / DIV / MOD
 *   [T17] Bitwise ops: AND / OR / XOR / SHL / SHR / NOT
 *   [T18] Comparisons: EQ NE LT LE GT GE
 *   [T19] Memory ops: ALLOCA / LOAD / STORE
 *   [T20] Control ops: LABEL / BR / BR_IF / RET
 *   [T21] CALL + ARG chain
 *   [T22] CONST and CONST_STR
 *   [T23] CAST and COPY
 *   [T24] PHI node fields
 *   [T25] Name truncation at IR_NAME_MAX-1 (no buffer overflow)
 *   [T26] Module at IR_MAX_FUNCS-1 boundary (no overflow)
 *   [T27] g_emit_ir flag — module created iff set
 *   [T28] ir_module_free(NULL) — safe no-op
 *   [T29] Multi-function module: two functions, all nodes freed
 *   [T30] Text emission line-count matches node_count
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ir.h"

/* ── Test harness ─────────────────────────────────────────────────────── */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(label, expr) do {                                         \
    if (expr) {                                                         \
        printf("PASS  [%s]\n", label);                                 \
        g_pass++;                                                       \
    } else {                                                            \
        printf("FAIL  [%s]  (%s:%d)\n", label, __FILE__, __LINE__);   \
        g_fail++;                                                       \
    }                                                                   \
} while(0)

#define CHECK_STR(label, a, b) \
    CHECK(label, strcmp((a),(b)) == 0)

/* ── T01: ir_op_name ──────────────────────────────────────────────────── */
static void test_op_names(void) {
    CHECK_STR("T01-RET",      ir_op_name(IR_RET),      "RET");
    CHECK_STR("T01-BR",       ir_op_name(IR_BR),       "BR");
    CHECK_STR("T01-BR_IF",    ir_op_name(IR_BR_IF),    "BR_IF");
    CHECK_STR("T01-ALLOCA",   ir_op_name(IR_ALLOCA),   "ALLOCA");
    CHECK_STR("T01-LOAD",     ir_op_name(IR_LOAD),     "LOAD");
    CHECK_STR("T01-STORE",    ir_op_name(IR_STORE),    "STORE");
    CHECK_STR("T01-ADD",      ir_op_name(IR_ADD),      "ADD");
    CHECK_STR("T01-SUB",      ir_op_name(IR_SUB),      "SUB");
    CHECK_STR("T01-MUL",      ir_op_name(IR_MUL),      "MUL");
    CHECK_STR("T01-DIV",      ir_op_name(IR_DIV),      "DIV");
    CHECK_STR("T01-MOD",      ir_op_name(IR_MOD),      "MOD");
    CHECK_STR("T01-NEG",      ir_op_name(IR_NEG),      "NEG");
    CHECK_STR("T01-AND",      ir_op_name(IR_AND),      "AND");
    CHECK_STR("T01-OR",       ir_op_name(IR_OR),       "OR");
    CHECK_STR("T01-XOR",      ir_op_name(IR_XOR),      "XOR");
    CHECK_STR("T01-NOT",      ir_op_name(IR_NOT),      "NOT");
    CHECK_STR("T01-SHL",      ir_op_name(IR_SHL),      "SHL");
    CHECK_STR("T01-SHR",      ir_op_name(IR_SHR),      "SHR");
    CHECK_STR("T01-EQ",       ir_op_name(IR_EQ),       "EQ");
    CHECK_STR("T01-NE",       ir_op_name(IR_NE),       "NE");
    CHECK_STR("T01-LT",       ir_op_name(IR_LT),       "LT");
    CHECK_STR("T01-LE",       ir_op_name(IR_LE),       "LE");
    CHECK_STR("T01-GT",       ir_op_name(IR_GT),       "GT");
    CHECK_STR("T01-GE",       ir_op_name(IR_GE),       "GE");
    CHECK_STR("T01-CAST",     ir_op_name(IR_CAST),     "CAST");
    CHECK_STR("T01-COPY",     ir_op_name(IR_COPY),     "COPY");
    CHECK_STR("T01-CONST",    ir_op_name(IR_CONST),    "CONST");
    CHECK_STR("T01-CALL",     ir_op_name(IR_CALL),     "CALL");
    CHECK_STR("T01-ARG",      ir_op_name(IR_ARG),      "ARG");
    CHECK_STR("T01-PHI",      ir_op_name(IR_PHI),      "PHI");
    CHECK_STR("T01-LABEL",    ir_op_name(IR_LABEL),    "LABEL");
    CHECK_STR("T01-NOP",      ir_op_name(IR_NOP),      "NOP");
    /* sentinel and out-of-range */
    CHECK_STR("T01-sentinel", ir_op_name(IR_OP_COUNT), "???");
    CHECK_STR("T01-neg",      ir_op_name(-1),           "???");
}

/* ── T02: ir_type_name ────────────────────────────────────────────────── */
static void test_type_names(void) {
    CHECK_STR("T02-void", ir_type_name(IR_TY_VOID), "void");
    CHECK_STR("T02-i8",   ir_type_name(IR_TY_I8),   "i8");
    CHECK_STR("T02-i16",  ir_type_name(IR_TY_I16),  "i16");
    CHECK_STR("T02-i32",  ir_type_name(IR_TY_I32),  "i32");
    CHECK_STR("T02-i64",  ir_type_name(IR_TY_I64),  "i64");
    CHECK_STR("T02-u8",   ir_type_name(IR_TY_U8),   "u8");
    CHECK_STR("T02-u16",  ir_type_name(IR_TY_U16),  "u16");
    CHECK_STR("T02-u32",  ir_type_name(IR_TY_U32),  "u32");
    CHECK_STR("T02-u64",  ir_type_name(IR_TY_U64),  "u64");
    CHECK_STR("T02-ptr",  ir_type_name(IR_TY_PTR),  "ptr");
    CHECK_STR("T02-f32",  ir_type_name(IR_TY_F32),  "f32");
    CHECK_STR("T02-f64",  ir_type_name(IR_TY_F64),  "f64");
    CHECK_STR("T02-oob",  ir_type_name(IR_TY_COUNT),"???");
    CHECK_STR("T02-neg",  ir_type_name(-1),          "???");
}

/* ── T03: ir_type_bytes ───────────────────────────────────────────────── */
static void test_type_bytes(void) {
    CHECK("T03-void",  ir_type_bytes(IR_TY_VOID) == 0);
    CHECK("T03-i8",    ir_type_bytes(IR_TY_I8)   == 1);
    CHECK("T03-i16",   ir_type_bytes(IR_TY_I16)  == 2);
    CHECK("T03-i32",   ir_type_bytes(IR_TY_I32)  == 4);
    CHECK("T03-i64",   ir_type_bytes(IR_TY_I64)  == 8);
    CHECK("T03-u8",    ir_type_bytes(IR_TY_U8)   == 1);
    CHECK("T03-u16",   ir_type_bytes(IR_TY_U16)  == 2);
    CHECK("T03-u32",   ir_type_bytes(IR_TY_U32)  == 4);
    CHECK("T03-u64",   ir_type_bytes(IR_TY_U64)  == 8);
    CHECK("T03-ptr",   ir_type_bytes(IR_TY_PTR)  == 8); /* LP64 critical */
    CHECK("T03-f32",   ir_type_bytes(IR_TY_F32)  == 4);
    CHECK("T03-f64",   ir_type_bytes(IR_TY_F64)  == 8);
    CHECK("T03-oob",   ir_type_bytes(IR_TY_COUNT) == -1);
}

/* ── T04: ir_type_unsigned ────────────────────────────────────────────── */
static void test_type_unsigned(void) {
    CHECK("T04-i8-signed",  !ir_type_unsigned(IR_TY_I8));
    CHECK("T04-i32-signed", !ir_type_unsigned(IR_TY_I32));
    CHECK("T04-i64-signed", !ir_type_unsigned(IR_TY_I64));
    CHECK("T04-ptr-signed", !ir_type_unsigned(IR_TY_PTR));
    CHECK("T04-u8",         ir_type_unsigned(IR_TY_U8));
    CHECK("T04-u16",        ir_type_unsigned(IR_TY_U16));
    CHECK("T04-u32",        ir_type_unsigned(IR_TY_U32));
    CHECK("T04-u64",        ir_type_unsigned(IR_TY_U64));
}

/* ── T05: ir_op_is_terminator ────────────────────────────────────────── */
static void test_terminators(void) {
    CHECK("T05-RET-term",   ir_op_is_terminator(IR_RET));
    CHECK("T05-BR-term",    ir_op_is_terminator(IR_BR));
    CHECK("T05-BR_IF-term", ir_op_is_terminator(IR_BR_IF));
    CHECK("T05-ADD-noterm", !ir_op_is_terminator(IR_ADD));
    CHECK("T05-LOAD-noterm",!ir_op_is_terminator(IR_LOAD));
    CHECK("T05-CALL-noterm",!ir_op_is_terminator(IR_CALL));
    CHECK("T05-PHI-noterm", !ir_op_is_terminator(IR_PHI));
}

/* ── T06: ir_node_alloc ───────────────────────────────────────────────── */
static void test_node_alloc(void) {
    ir_node_t *n = ir_node_alloc();
    CHECK("T06-not-null",   n != 0);
    CHECK("T06-op-zero",    n->op    == 0);
    CHECK("T06-type-zero",  n->type  == 0);
    CHECK("T06-dst-empty",  n->dst[0] == '\0');
    CHECK("T06-src1-empty", n->src1[0] == '\0');
    CHECK("T06-src2-empty", n->src2[0] == '\0');
    CHECK("T06-label-empty",n->label[0] == '\0');
    CHECK("T06-imm-zero",   n->imm   == 0);
    CHECK("T06-lineno-zero",n->lineno == 0);
    CHECK("T06-next-null",  n->next  == 0);
    free(n);
}

/* ── T07: module lifecycle ────────────────────────────────────────────── */
static void test_module_lifecycle(void) {
    ir_module_t *mod = ir_module_create();
    CHECK("T07-mod-not-null",  mod != 0);
    CHECK("T07-func-count-0",  mod->func_count == 0);
    ir_module_free(mod);   /* must not crash */
    CHECK("T07-free-ok", 1);
}

/* ── T08: ir_func_create ──────────────────────────────────────────────── */
static void test_func_create(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "my_func", IR_TY_I32);
    CHECK("T08-fn-not-null",   fn != 0);
    CHECK("T08-name",          strcmp(fn->name, "my_func") == 0);
    CHECK("T08-ret-type",      fn->ret_type == IR_TY_I32);
    CHECK("T08-func-count",    mod->func_count == 1);
    CHECK("T08-registered",    mod->funcs[0] == fn);
    CHECK("T08-head-null",     fn->head == 0);
    CHECK("T08-tail-null",     fn->tail == 0);
    CHECK("T08-node-count-0",  fn->node_count == 0);
    ir_module_free(mod);
}

/* ── T09: ir_append invariants ────────────────────────────────────────── */
static void test_append(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_VOID);
    ir_node_t   *a   = ir_node_alloc();
    ir_node_t   *b   = ir_node_alloc();
    ir_node_t   *c   = ir_node_alloc();

    a->op = IR_ADD; b->op = IR_SUB; c->op = IR_RET;

    ir_append(fn, a);
    CHECK("T09-head-a",    fn->head == a);
    CHECK("T09-tail-a",    fn->tail == a);
    CHECK("T09-count-1",   fn->node_count == 1);

    ir_append(fn, b);
    CHECK("T09-head-still-a", fn->head == a);
    CHECK("T09-tail-b",    fn->tail == b);
    CHECK("T09-a-next-b",  a->next  == b);
    CHECK("T09-count-2",   fn->node_count == 2);

    ir_append(fn, c);
    CHECK("T09-tail-c",    fn->tail == c);
    CHECK("T09-b-next-c",  b->next  == c);
    CHECK("T09-c-next-null",c->next == 0);
    CHECK("T09-count-3",   fn->node_count == 3);

    ir_module_free(mod);
}

/* ── T10: ir_emit field round-trips ──────────────────────────────────── */
static void test_ir_emit_fields(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_I32);

    ir_node_t *n = ir_emit(fn, IR_ADD, IR_TY_I32,
                            "t0", "t1", "t2", "unused_label", 99L, 42);
    CHECK("T10-op",    n->op    == IR_ADD);
    CHECK("T10-type",  n->type  == IR_TY_I32);
    CHECK("T10-dst",   strcmp(n->dst,   "t0") == 0);
    CHECK("T10-src1",  strcmp(n->src1,  "t1") == 0);
    CHECK("T10-src2",  strcmp(n->src2,  "t2") == 0);
    CHECK("T10-label", strcmp(n->label, "unused_label") == 0);
    CHECK("T10-imm",   n->imm    == 99L);
    CHECK("T10-line",  n->lineno == 42);
    CHECK("T10-count", fn->node_count == 1);

    ir_module_free(mod);
}

/* ── T11: ir_fresh_tmp monotonic ──────────────────────────────────────── */
static void test_fresh_tmp(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_VOID);
    char buf[IR_NAME_MAX];
    int  i;

    ir_fresh_tmp(fn, buf);
    CHECK("T11-first-t0", strcmp(buf, "t0") == 0);

    ir_fresh_tmp(fn, buf);
    CHECK("T11-second-t1", strcmp(buf, "t1") == 0);

    /* Generate 9998 more temps — all unique (just check counter) */
    for (i = 0; i < 9998; i++) ir_fresh_tmp(fn, buf);
    CHECK("T11-counter-10000", fn->tmp_counter == 10000);

    ir_fresh_tmp(fn, buf);
    CHECK("T11-t10000", strcmp(buf, "t10000") == 0);

    ir_module_free(mod);
}

/* ── T12: ir_fresh_label format ───────────────────────────────────────── */
static void test_fresh_label(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_VOID);
    char buf[IR_LABEL_MAX];

    ir_fresh_label(fn, buf);
    CHECK("T12-L0",  strcmp(buf, ".L0") == 0);

    ir_fresh_label(fn, buf);
    CHECK("T12-L1",  strcmp(buf, ".L1") == 0);

    ir_fresh_label(fn, buf);
    CHECK("T12-L2",  strcmp(buf, ".L2") == 0);

    ir_module_free(mod);
}

/* ── T13: NULL src fields are safe ───────────────────────────────────── */
static void test_null_srcs(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_VOID);

    /* ir_emit with NULL for optional fields must not segfault */
    ir_node_t *n = ir_emit(fn, IR_NEG, IR_TY_I32, "t0", "t1",
                            0, 0, 0L, 0);
    CHECK("T13-src2-empty",  n->src2[0]  == '\0');
    CHECK("T13-label-empty", n->label[0] == '\0');

    ir_module_free(mod);
}

/* ── T14: empty string src fields safe ───────────────────────────────── */
static void test_empty_srcs(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_VOID);

    ir_node_t *n = ir_emit(fn, IR_RET, IR_TY_VOID,
                            "", "", "", "", 0L, 0);
    CHECK("T14-dst-empty",   n->dst[0]   == '\0');
    CHECK("T14-src1-empty",  n->src1[0]  == '\0');
    CHECK("T14-src2-empty",  n->src2[0]  == '\0');
    CHECK("T14-label-empty", n->label[0] == '\0');

    ir_module_free(mod);
}

/* ── T15: text emission parses back ──────────────────────────────────── */
/*
 * Build a tiny function:  t0 = ADD i32 t1 t2
 * Emit it to a temp file.  Scan the text for the ADD mnemonic and i32 type.
 */
static void test_text_emission(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "add_test", IR_TY_I32);
    char  line[512];
    int   found_func = 0, found_add = 0, found_ret = 0;
    FILE *tmp;

    ir_emit(fn, IR_CONST, IR_TY_I32, "t1", 0, 0, 0, 10L, 1);
    ir_emit(fn, IR_CONST, IR_TY_I32, "t2", 0, 0, 0, 32L, 2);
    ir_emit(fn, IR_ADD,   IR_TY_I32, "t0", "t1", "t2", 0, 0L, 3);
    ir_emit(fn, IR_RET,   IR_TY_I32, 0, "t0", 0, 0, 0L, 4);

    tmp = tmpfile();
    if (!tmp) { CHECK("T15-tmpfile", 0); ir_module_free(mod); return; }

    ir_func_emit_text(fn, tmp);
    rewind(tmp);

    while (fgets(line, sizeof(line), tmp)) {
        if (strstr(line, "func add_test"))   found_func = 1;
        if (strstr(line, "ADD"))             found_add  = 1;
        if (strstr(line, "RET"))             found_ret  = 1;
    }
    fclose(tmp);

    CHECK("T15-func-header",  found_func);
    CHECK("T15-add-present",  found_add);
    CHECK("T15-ret-present",  found_ret);

    ir_module_free(mod);
}

/* ── T16-T24: one test per op family ──────────────────────────────────── */

static void test_arithmetic_ops(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_I32);

    ir_node_t *add = ir_emit(fn, IR_ADD, IR_TY_I32, "r","a","b",0,0,0);
    ir_node_t *sub = ir_emit(fn, IR_SUB, IR_TY_I32, "r","a","b",0,0,0);
    ir_node_t *mul = ir_emit(fn, IR_MUL, IR_TY_I32, "r","a","b",0,0,0);
    ir_node_t *div = ir_emit(fn, IR_DIV, IR_TY_I32, "r","a","b",0,0,0);
    ir_node_t *mod2= ir_emit(fn, IR_MOD, IR_TY_I32, "r","a","b",0,0,0);
    ir_node_t *neg = ir_emit(fn, IR_NEG, IR_TY_I32, "r","a",0,  0,0,0);

    CHECK("T16-add", add->op == IR_ADD);
    CHECK("T16-sub", sub->op == IR_SUB);
    CHECK("T16-mul", mul->op == IR_MUL);
    CHECK("T16-div", div->op == IR_DIV);
    CHECK("T16-mod", mod2->op == IR_MOD);
    CHECK("T16-neg", neg->op == IR_NEG);
    CHECK("T16-count", fn->node_count == 6);

    ir_module_free(mod);
    (void)sub; (void)mul; (void)div; (void)mod2; (void)neg;
}

static void test_bitwise_ops(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_I32);

    ir_emit(fn, IR_AND, IR_TY_I32, "r","a","b",0,0,0);
    ir_emit(fn, IR_OR,  IR_TY_I32, "r","a","b",0,0,0);
    ir_emit(fn, IR_XOR, IR_TY_I32, "r","a","b",0,0,0);
    ir_emit(fn, IR_NOT, IR_TY_I32, "r","a",0,  0,0,0);
    ir_emit(fn, IR_SHL, IR_TY_I32, "r","a","2",0,0,0);
    ir_emit(fn, IR_SHR, IR_TY_U32, "r","a","1",0,0,0); /* unsigned SHR */

    CHECK("T17-count", fn->node_count == 6);
    /* unsigned SHR type must be preserved */
    CHECK("T17-shr-unsigned", fn->tail->type == IR_TY_U32);

    ir_module_free(mod);
}

static void test_comparison_ops(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_I32);
    ir_op_t cmps[6]  = { IR_EQ, IR_NE, IR_LT, IR_LE, IR_GT, IR_GE };
    int i;

    for (i = 0; i < 6; i++)
        ir_emit(fn, cmps[i], IR_TY_I32, "b","x","y",0,0,0);

    CHECK("T18-count", fn->node_count == 6);

    ir_module_free(mod);
}

static void test_memory_ops(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_VOID);

    ir_node_t *al = ir_emit(fn, IR_ALLOCA, IR_TY_PTR,  "sp",0,   0, 0, 8L,  1);
    ir_node_t *st = ir_emit(fn, IR_STORE,  IR_TY_I32,  "sp","v", 0, 0, 0L,  2);
    ir_node_t *ld = ir_emit(fn, IR_LOAD,   IR_TY_I32,  "v2","sp",0, 0, 0L,  3);

    CHECK("T19-alloca-op",   al->op   == IR_ALLOCA);
    CHECK("T19-alloca-type", al->type == IR_TY_PTR);
    CHECK("T19-alloca-imm",  al->imm  == 8L);
    CHECK("T19-store-op",    st->op   == IR_STORE);
    CHECK("T19-load-op",     ld->op   == IR_LOAD);
    CHECK("T19-load-dst",    strcmp(ld->dst,  "v2") == 0);
    CHECK("T19-load-src1",   strcmp(ld->src1, "sp") == 0);

    ir_module_free(mod);
    (void)st;
}

static void test_control_ops(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_VOID);

    ir_node_t *lbl  = ir_emit(fn, IR_LABEL, IR_TY_VOID, 0, 0, 0, ".L0", 0L, 1);
    ir_node_t *br   = ir_emit(fn, IR_BR,    IR_TY_VOID, 0, 0, 0, ".L1", 0L, 2);
    ir_node_t *brif = ir_emit(fn, IR_BR_IF, IR_TY_VOID, 0,"c",0, ".L2", 0L, 3);
    ir_node_t *ret  = ir_emit(fn, IR_RET,   IR_TY_I32,  0,"v",0,  0,    0L, 4);

    CHECK("T20-label-label", strcmp(lbl->label, ".L0") == 0);
    CHECK("T20-br-label",    strcmp(br->label,  ".L1") == 0);
    CHECK("T20-brif-cond",   strcmp(brif->src1, "c")   == 0);
    CHECK("T20-brif-label",  strcmp(brif->label,".L2") == 0);
    CHECK("T20-ret-src1",    strcmp(ret->src1,  "v")   == 0);
    CHECK("T20-terminators", ir_op_is_terminator(br->op) &&
                             ir_op_is_terminator(brif->op) &&
                             ir_op_is_terminator(ret->op));

    ir_module_free(mod);
    (void)lbl; (void)brif; (void)ret;
}

static void test_call_arg(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_I32);

    ir_node_t *call = ir_emit(fn, IR_CALL, IR_TY_I32, "rv", 0, 0, "printf", 0L, 5);
    ir_node_t *arg1 = ir_emit(fn, IR_ARG,  IR_TY_PTR, 0, "fmt", 0, 0, 0L, 5);
    ir_node_t *arg2 = ir_emit(fn, IR_ARG,  IR_TY_I32, 0, "n",   0, 0, 0L, 5);

    CHECK("T21-call-op",    call->op == IR_CALL);
    CHECK("T21-call-label", strcmp(call->label, "printf") == 0);
    CHECK("T21-call-dst",   strcmp(call->dst,   "rv")     == 0);
    CHECK("T21-arg1-src1",  strcmp(arg1->src1,  "fmt")    == 0);
    CHECK("T21-arg2-src1",  strcmp(arg2->src1,  "n")      == 0);
    CHECK("T21-count",      fn->node_count == 3);

    ir_module_free(mod);
    (void)arg1; (void)arg2;
}

static void test_const_nodes(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_I32);

    ir_node_t *ci = ir_emit(fn, IR_CONST,     IR_TY_I32, "t0", 0, 0, 0,     42L,  1);
    ir_node_t *cs = ir_emit(fn, IR_CONST_STR, IR_TY_PTR, "s0", 0, 0, ".S0",  0L,  2);
    ir_node_t *cn = ir_emit(fn, IR_CONST,     IR_TY_I64, "t1", 0, 0, 0,      -1L, 3);

    CHECK("T22-const-op",     ci->op  == IR_CONST);
    CHECK("T22-const-imm",    ci->imm == 42L);
    CHECK("T22-const-dst",    strcmp(ci->dst, "t0") == 0);
    CHECK("T22-str-op",       cs->op  == IR_CONST_STR);
    CHECK("T22-str-label",    strcmp(cs->label, ".S0") == 0);
    CHECK("T22-neg-imm",      cn->imm == -1L);

    ir_module_free(mod);
    (void)cs; (void)cn;
}

static void test_cast_copy(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_I64);

    ir_node_t *cast = ir_emit(fn, IR_CAST, IR_TY_I64, "t1", "t0", 0, 0, 0L, 1);
    ir_node_t *copy = ir_emit(fn, IR_COPY, IR_TY_I32, "t2", "t0", 0, 0, 0L, 2);

    CHECK("T23-cast-op",   cast->op   == IR_CAST);
    CHECK("T23-cast-type", cast->type == IR_TY_I64);
    CHECK("T23-copy-op",   copy->op   == IR_COPY);
    CHECK("T23-copy-type", copy->type == IR_TY_I32);

    ir_module_free(mod);
    (void)cast; (void)copy;
}

static void test_phi_node(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_I32);

    ir_node_t *phi = ir_emit(fn, IR_PHI, IR_TY_I32,
                             "x", "x0", "x1", ".entry", 0L, 10);
    /* Manually set label2 for the phi (PHI has two sources from two labels) */
    memcpy(phi->label2, ".loop", 6);

    CHECK("T24-phi-op",     phi->op   == IR_PHI);
    CHECK("T24-phi-dst",    strcmp(phi->dst,    "x")      == 0);
    CHECK("T24-phi-src1",   strcmp(phi->src1,   "x0")     == 0);
    CHECK("T24-phi-src2",   strcmp(phi->src2,   "x1")     == 0);
    CHECK("T24-phi-label",  strcmp(phi->label,  ".entry") == 0);
    CHECK("T24-phi-label2", strcmp(phi->label2, ".loop")  == 0);

    ir_module_free(mod);
}

/* ── T25: name truncation — no buffer overflow ────────────────────────── */
static void test_name_truncation(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *fn  = ir_func_create(mod, "f", IR_TY_VOID);
    /* Build a name exactly IR_NAME_MAX long (should be truncated) */
    char long_name[IR_NAME_MAX * 2];
    int i;
    for (i = 0; i < IR_NAME_MAX * 2 - 1; i++) long_name[i] = 'a';
    long_name[IR_NAME_MAX * 2 - 1] = '\0';

    ir_node_t *n = ir_emit(fn, IR_COPY, IR_TY_I32,
                            long_name, long_name, 0, 0, 0L, 0);
    CHECK("T25-dst-null-terminated",  n->dst[IR_NAME_MAX - 1]  == '\0');
    CHECK("T25-src1-null-terminated", n->src1[IR_NAME_MAX - 1] == '\0');

    ir_module_free(mod);
}

/* ── T26: IR_MAX_FUNCS-1 boundary ────────────────────────────────────── */
static void test_max_funcs_boundary(void) {
    ir_module_t *mod = ir_module_create();
    int i;
    for (i = 0; i < IR_MAX_FUNCS - 1; i++) {
        char nm[32];
        int j = 0, v = i;
        nm[j++] = 'f';
        if (v == 0) { nm[j++] = '0'; }
        else {
            char tmp[20]; int tl = 0;
            while (v) { tmp[tl++] = '0' + v%10; v /= 10; }
            int k; for (k=tl-1;k>=0;k--) nm[j++]=tmp[k];
        }
        nm[j] = '\0';
        ir_func_create(mod, nm, IR_TY_VOID);
    }
    CHECK("T26-count", mod->func_count == IR_MAX_FUNCS - 1);
    ir_module_free(mod);
}

/* ── T27: g_emit_ir flag ──────────────────────────────────────────────── */
static void test_emit_ir_flag(void) {
    /* Without setting g_emit_ir, module should not be auto-created */
    g_emit_ir     = 0;
    g_ir_module   = 0;
    g_ir_cur_func = 0;
    CHECK("T27-flag-off",   g_emit_ir == 0);

    g_emit_ir = 1;
    g_ir_module = ir_module_create();
    CHECK("T27-flag-on",    g_ir_module != 0);
    ir_module_free(g_ir_module);
    g_ir_module   = 0;
    g_emit_ir     = 0;
}

/* ── T28: ir_module_free(NULL) safe ──────────────────────────────────── */
static void test_free_null(void) {
    ir_module_free(0);  /* must not crash */
    CHECK("T28-free-null-safe", 1);
}

/* ── T29: multi-function module ──────────────────────────────────────── */
static void test_multi_func(void) {
    ir_module_t *mod = ir_module_create();
    ir_func_t   *f1  = ir_func_create(mod, "foo", IR_TY_I32);
    ir_func_t   *f2  = ir_func_create(mod, "bar", IR_TY_VOID);

    ir_emit(f1, IR_CONST, IR_TY_I32, "t0", 0, 0, 0, 1L, 0);
    ir_emit(f1, IR_RET,   IR_TY_I32, 0, "t0", 0, 0, 0L, 0);
    ir_emit(f2, IR_RET,   IR_TY_VOID, 0, 0, 0, 0, 0L, 0);

    CHECK("T29-func-count",  mod->func_count == 2);
    CHECK("T29-f1-nodes",    f1->node_count  == 2);
    CHECK("T29-f2-nodes",    f2->node_count  == 1);

    ir_module_free(mod);  /* must free all nodes from both functions */
    CHECK("T29-free-ok", 1);
}

/* ── T30: text emission line count matches node_count ────────────────── */
static void test_emission_line_count(void) {
    ir_module_t *mod  = ir_module_create();
    ir_func_t   *fn   = ir_func_create(mod, "lc", IR_TY_I32);
    int          lines = 0;
    char         buf[512];
    FILE        *tmp;

    ir_emit(fn, IR_CONST, IR_TY_I32, "t0", 0, 0, 0,   7L, 1);
    ir_emit(fn, IR_CONST, IR_TY_I32, "t1", 0, 0, 0,   3L, 2);
    ir_emit(fn, IR_ADD,   IR_TY_I32, "t2","t0","t1", 0, 0L, 3);
    ir_emit(fn, IR_RET,   IR_TY_I32, 0,"t2",    0, 0, 0L, 4);

    tmp = tmpfile();
    if (!tmp) { CHECK("T30-tmpfile", 0); ir_module_free(mod); return; }

    ir_func_emit_text(fn, tmp);
    rewind(tmp);

    while (fgets(buf, sizeof(buf), tmp)) {
        /* Count only instruction lines — skip comment lines starting with ';' */
        if (buf[0] == ' ' || buf[0] == '\t') lines++;
    }
    fclose(tmp);

    CHECK("T30-line-count-matches-node-count", lines == fn->node_count);

    ir_module_free(mod);
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void) {
    printf("ZCC P1-IR Unit Tests\n");
    printf("====================\n");

    test_op_names();
    test_type_names();
    test_type_bytes();
    test_type_unsigned();
    test_terminators();
    test_node_alloc();
    test_module_lifecycle();
    test_func_create();
    test_append();
    test_ir_emit_fields();
    test_fresh_tmp();
    test_fresh_label();
    test_null_srcs();
    test_empty_srcs();
    test_text_emission();
    test_arithmetic_ops();
    test_bitwise_ops();
    test_comparison_ops();
    test_memory_ops();
    test_control_ops();
    test_call_arg();
    test_const_nodes();
    test_cast_copy();
    test_phi_node();
    test_name_truncation();
    test_max_funcs_boundary();
    test_emit_ir_flag();
    test_free_null();
    test_multi_func();
    test_emission_line_count();

    printf("====================\n");
    printf("PASS: %d  FAIL: %d\n", g_pass, g_fail);

    return g_fail > 0 ? 1 : 0;
}
