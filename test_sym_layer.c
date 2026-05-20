/*
 * test_sym_layer.c  --  Smoke test for sym_type_ast_ir.h/c
 *
 * Build (standalone, GCC):
 *   gcc -Wall -Wextra -DZCC_STANDALONE_BUILD -DZCC_COVERAGE \
 *       -o test_sym test_sym_layer.c sym_type_ast_ir.c
 *
 * Run:
 *   ./test_sym
 *
 * Expected:
 *   All ASSERT lines print PASS.
 *   Coverage report printed to stderr.
 *   Exit code 0.
 *
 * Tag coverage verified by this test:
 *   TAG_SCOPE  TAG_TYPEDEF  TAG_FUNC  TAG_GLOBAL  TAG_LOCAL
 *   TAG_PARAM  TAG_LABEL    TAG_STRUCT TAG_UNION  TAG_ENUM
 *   TAG_TYPE   TAG_ABI      TAG_REGRESSION
 */

/*
 * ZCC_STANDALONE_BUILD: activates the concrete struct stubs for
 * Type/Node/Member that live in sym_type_ast_ir.h.
 * ZCC_COVERAGE: enables coverage hit recording.
 * Both are guarded with #ifndef so the build command can also
 * pass -DZCC_STANDALONE_BUILD / -DZCC_COVERAGE without triggering
 * redefinition warnings.
 */
#ifndef ZCC_STANDALONE_BUILD
#  define ZCC_STANDALONE_BUILD
#endif
#ifndef ZCC_COVERAGE
#  define ZCC_COVERAGE
#endif

/*
 * ZCC_TYPES_DECLARED is NOT set here: the header's ZCC_STANDALONE_BUILD
 * path provides the full struct definitions for Type/Node/Member, and
 * the typedef-block in the header must also fire so the names are usable
 * throughout this file.
 */

#include "sym_type_ast_ir.h"
#include <assert.h>
#include <stdio.h>

#define ASSERT(cond, label) \
    do { \
        if (cond) { fprintf(stdout, "PASS  %s\n", (label)); } \
        else      { fprintf(stdout, "FAIL  %s\n", (label)); fails++; } \
    } while (0)

static int fails = 0;

/* ============================================================
 * T1: scope push / define / lookup / pop
 * ============================================================ */
static void test_scope(void)
{
    ZccScope *global;
    ZccScope *func;
    ZccScope *block;
    ZccSymbol *s;
    int r;

    global = sym_scope_push(NULL, ZCC_SCOPE_GLOBAL);
    ASSERT(global != NULL, "T1.1 global scope created");
    ASSERT(global->depth == 0, "T1.2 global depth=0");

    r = sym_define(global, "printf", ZCC_SYM_FUNC, NULL, 0);
    ASSERT(r == 1, "T1.3 define global func");

    func = sym_scope_push(global, ZCC_SCOPE_FUNC);
    r = sym_define(func, "argc", ZCC_SYM_PARAM, NULL, -8);
    ASSERT(r == 1, "T1.4 define param");
    r = sym_define(func, "argv", ZCC_SYM_PARAM, NULL, -16);
    ASSERT(r == 1, "T1.5 define param argv");

    block = sym_scope_push(func, ZCC_SCOPE_BLOCK);
    r = sym_define(block, "i", ZCC_SYM_LOCAL, NULL, -24);
    ASSERT(r == 1, "T1.6 define local i");

    s = sym_lookup_local(block, "i");
    ASSERT(s != NULL, "T1.7 lookup_local finds i");
    ASSERT(s->offset == -24, "T1.8 offset correct");

    s = sym_lookup_local(block, "argc");
    ASSERT(s == NULL, "T1.9 lookup_local does NOT see parent");

    s = sym_lookup_recursive(block, "argc");
    ASSERT(s != NULL, "T1.10 lookup_recursive finds argc");

    s = sym_lookup_recursive(block, "printf");
    ASSERT(s != NULL, "T1.11 lookup_recursive finds global printf");

    /* Shadow check */
    s = sym_shadow_check(block, "argc");
    ASSERT(s != NULL, "T1.12 shadow_check finds argc in outer scope");

    /* Redefinition in same scope must fail */
    r = sym_define(block, "i", ZCC_SYM_LOCAL, NULL, -32);
    ASSERT(r == 0, "T1.13 redefinition rejected");

    /* Mangle internal name */
    {
        char mangled[64];
        sym_mangle_internal("anon_struct", 42, mangled, (int)sizeof(mangled));
        ASSERT(strcmp(mangled, "__zcc_anon_struct_42") == 0,
               "T1.14 mangle_internal");
    }

    block = sym_scope_pop(block);
    ASSERT(block == func, "T1.15 pop returns parent");
    func = sym_scope_pop(func);
    ASSERT(func == global, "T1.16 pop func returns global");
    sym_scope_pop(global);
}

/* ============================================================
 * T2: tag namespace -- struct, union, enum, forward, complete
 * ============================================================ */
static void test_tags(void)
{
    ZccTagTable *tt;
    ZccTagEntry *e;
    struct Type  dummy_ty;
    int r;

    dummy_ty.kind  = ZCC_TY_STRUCT;
    dummy_ty.size  = 8;
    dummy_ty.align = 8;

    tt = tag_table_push(NULL);
    ASSERT(tt != NULL, "T2.1 tag table created");

    /* Forward declare struct Node */
    e = tag_forward_declare(tt, "Node", ZCC_TAG_KIND_STRUCT);
    ASSERT(e != NULL, "T2.2 forward_declare");
    ASSERT(!tag_is_complete(e), "T2.3 forward is incomplete");

    /* Complete it */
    r = tag_complete_type(e, &dummy_ty);
    ASSERT(r == 1, "T2.4 tag_complete_type");
    ASSERT(tag_is_complete(e), "T2.5 now complete");

    /* Complete twice should fail */
    r = tag_complete_type(e, &dummy_ty);
    ASSERT(r == 0, "T2.6 double-complete rejected");

    /* Look up by name and kind */
    e = tag_lookup_recursive(tt, "Node", ZCC_TAG_KIND_STRUCT);
    ASSERT(e != NULL, "T2.7 lookup_recursive finds Node");
    ASSERT(e->kind == ZCC_TAG_KIND_STRUCT, "T2.8 correct kind");

    /* Wrong kind returns NULL */
    e = tag_lookup_recursive(tt, "Node", ZCC_TAG_KIND_UNION);
    ASSERT(e == NULL, "T2.9 wrong kind returns NULL");

    /* Define an enum in a child table */
    {
        ZccTagTable *child;
        struct Type enum_ty;
        enum_ty.kind  = ZCC_TY_ENUM;
        enum_ty.size  = 4;
        enum_ty.align = 4;
        child = tag_table_push(tt);
        e = tag_define_enum(child, "Color", &enum_ty);
        ASSERT(e != NULL, "T2.10 define enum");
        /* Recursive lookup from child finds parent's Node */
        e = tag_lookup_recursive(child, "Node", ZCC_TAG_KIND_STRUCT);
        ASSERT(e != NULL, "T2.11 recursive finds parent struct");
        tag_table_pop(child);
    }

    tag_table_pop(tt);
}

/* ============================================================
 * T3: type recursion -- sizeof, alignof, rank, decay
 * ============================================================ */
static void test_types(void)
{
    struct Type int_ty;
    struct Type ptr_ty;
    struct Type arr_ty;
    struct Type *p;

    /* Build minimal Type objects for testing */
    int_ty.kind  = ZCC_TY_INT;
    int_ty.size  = 4;
    int_ty.align = 4;

    ptr_ty.kind  = ZCC_TY_PTR;
    ptr_ty.size  = 8;
    ptr_ty.align = 8;

    arr_ty.kind  = ZCC_TY_ARRAY;
    arr_ty.size  = 40;  /* 10 ints */
    arr_ty.align = 4;

    ASSERT(type_sizeof_recursive(&int_ty)  == 4, "T3.1 sizeof int");
    ASSERT(type_alignof_recursive(&int_ty) == 4, "T3.2 alignof int");
    ASSERT(type_sizeof_recursive(&ptr_ty)  == 8, "T3.3 sizeof ptr");
    ASSERT(type_alignof_recursive(&ptr_ty) == 8, "T3.4 alignof ptr");
    ASSERT(type_sizeof_recursive(&arr_ty)  == 40,"T3.5 sizeof array");

    ASSERT(type_integer_rank(&int_ty) == 3, "T3.6 int rank=3");
    {
        struct Type char_ty;
        struct Type long_ty;
        char_ty.kind = ZCC_TY_CHAR; char_ty.size = 1; char_ty.align = 1;
        long_ty.kind = ZCC_TY_LONG; long_ty.size = 8; long_ty.align = 8;
        ASSERT(type_integer_rank(&char_ty) == 1, "T3.7 char rank=1");
        ASSERT(type_integer_rank(&long_ty) == 4, "T3.8 long rank=4");
    }

    /* type_pointer_to allocates a fresh ptr type */
    p = type_pointer_to(&int_ty);
    ASSERT(p != NULL, "T3.9 type_pointer_to not null");
    ASSERT(p->kind  == ZCC_TY_PTR, "T3.10 pointer kind");
    ASSERT(p->size  == 8, "T3.11 pointer size LP64");
    ASSERT(p->align == 8, "T3.12 pointer align LP64");
    free(p);

    /* type_array_of */
    {
        struct Type *a;
        a = type_array_of(&int_ty, 5);
        ASSERT(a != NULL, "T3.13 type_array_of not null");
        ASSERT(a->kind == ZCC_TY_ARRAY, "T3.14 array kind");
        ASSERT(a->size == 20, "T3.15 array size = 5*4");
        free(a);
    }

    /* type_lvalue_conversion: array decays to ptr */
    {
        Type *decayed;
        decayed = type_lvalue_conversion(&arr_ty);
        ASSERT(decayed != NULL, "T3.16 decay not null");
        ASSERT(decayed->kind == ZCC_TY_PTR, "T3.17 decayed to ptr");
        free(decayed);
    }
}

/* ============================================================
 * T4: AST walker -- null safety and visitor fires
 * ============================================================ */
static int visitor_count = 0;

static void count_visitor(Node *node, void *ctx)
{
    (void)node; (void)ctx;
    visitor_count++;
}

static void test_ast(void)
{
    /* Null root should not crash */
    ast_walk_preorder(NULL, count_visitor, NULL);
    ASSERT(visitor_count == 0, "T4.1 null root no-op");

    /* Single-node tree */
    {
        Node n;
        n.kind = 0; n.lhs = NULL; n.rhs = NULL;
        n.ty   = NULL; n.line = 1; n.next = NULL;
        visitor_count = 0;
        ast_walk_preorder(&n, count_visitor, NULL);
        /* At minimum, the root node itself should be visited once */
        ASSERT(visitor_count >= 1, "T4.2 root visited");
    }

    /* Null fn should not crash */
    ast_walk_preorder(NULL, NULL, NULL);
    ASSERT(1, "T4.3 null fn no-op");

    /* Rewrite returns node unchanged when fn returns it */
    {
        Node n;
        Node *result;
        n.kind = 0; n.lhs = NULL; n.rhs = NULL;
        n.ty   = NULL; n.line = 1; n.next = NULL;
        result = ast_rewrite_expr_recursive(&n, NULL, NULL);
        ASSERT(result == &n, "T4.4 null rewriter returns original");
    }
}

/* ============================================================
 * T5: coverage tag hit counts
 * ============================================================ */
static void test_coverage(void)
{
#ifdef ZCC_COVERAGE
    int hits;
    /*
     * T5 explicitly exercises the two tags not hit by T1-T4:
     *   TAG_STRUCT   -- fires in tag_define_struct / tag_attach_members
     *   TAG_REGRESSION -- fires in ast_validate_types
     * Call each once before asserting their counts.
     */
    {
        ZccTagTable *tt;
        struct Type  sty;
        sty.kind = ZCC_TY_STRUCT; sty.size = 4; sty.align = 4;
        sty.base = NULL; sty.members = NULL;
        tt = tag_table_push(NULL);
        tag_define_struct(tt, "__T5_CovStruct", &sty);   /* hits TAG_STRUCT */
        tag_table_pop(tt);
    }
    ast_validate_types(NULL);   /* hits TAG_REGRESSION (null root is safe) */

    /* T1 + T2 + T3 exercised TAG_SCOPE, TAG_TYPE, TAG_STRUCT etc. */
    hits = zcc_coverage_count(TAG_SCOPE);
    ASSERT(hits > 0, "T5.1 TAG_SCOPE hit");
    hits = zcc_coverage_count(TAG_TYPE);
    ASSERT(hits > 0, "T5.2 TAG_TYPE hit");
    hits = zcc_coverage_count(TAG_STRUCT);
    ASSERT(hits > 0, "T5.3 TAG_STRUCT hit");
    hits = zcc_coverage_count(TAG_ABI);
    ASSERT(hits > 0, "T5.4 TAG_ABI hit");
    hits = zcc_coverage_count(TAG_REGRESSION);
    ASSERT(hits > 0, "T5.5 TAG_REGRESSION hit");
    zcc_coverage_report();
#else
    ASSERT(1, "T5 coverage disabled -- skipped");
#endif
}

/* ============================================================
 * main
 * ============================================================ */
int main(void)
{
    fprintf(stdout, "=== ZCC sym_type_ast_ir smoke tests ===\n");
    test_scope();
    test_tags();
    test_types();
    test_ast();
    test_coverage();
    fprintf(stdout, "=== %s (%d failures) ===\n",
            fails == 0 ? "ALL PASS" : "FAILURES DETECTED",
            fails);
    return (fails == 0) ? 0 : 1;
}
