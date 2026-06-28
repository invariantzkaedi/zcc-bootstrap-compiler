/*
 * tests/test_copy_const_prop_stubs.c
 * ────────────────────────────────────
 * No-op stubs for PARTS-dependent symbols referenced by
 * src/zcc_oracle_substrate.c in run_oracle_abi / run_oracle_layout /
 * run_abi_trace.  Those code paths are NOT exercised by the CCP pass
 * tests; these stubs exist only to satisfy the linker.
 *
 * This file is compiled only as part of the check-copy-const-prop target.
 * Do not include it in the production (selfhost) build.
 */

/* ── Forward declarations matching oracle substrate's extern decls ── */
typedef enum {
    CCP_STUB_CLASS_NO_CLASS = 0,
    CCP_STUB_CLASS_INTEGER,
    CCP_STUB_CLASS_SSE,
    CCP_STUB_CLASS_MEMORY
} ccp_abi_class_t;

/* Opaque struct — only pointer-type args needed for these stubs */
struct Type;

void classify_aggregate(struct Type *type, ccp_abi_class_t classes[2]) {
    (void)type;
    classes[0] = CCP_STUB_CLASS_NO_CLASS;
    classes[1] = CCP_STUB_CLASS_NO_CLASS;
}

int type_size(struct Type *t) {
    (void)t;
    return 0;
}

int is_float_type(struct Type *t) {
    (void)t;
    return 0;
}

int type_align(struct Type *t) {
    (void)t;
    return 1;
}
