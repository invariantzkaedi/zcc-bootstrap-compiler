/*
 * sym_type_ast_ir.h  --  ZCC Unified Recursive Symbol/Type/AST/IR Registry
 *
 * Six function families:
 *   1.  sym_*          Recursive symbol-table layer
 *   2.  tag_*          Tag namespace layer (struct/union/enum)
 *   3.  type_*         Type-recursion layer
 *   4.  ast_*          AST recursive walkers
 *   5.  ir_emit_* / ir_validate_*   IR bridge / completeness
 *   6.  TAG_* / ZCC_TAG()           Coverage/gate tags
 *
 * INSERTION POINT in concat / build order:
 *   ... ir_bridge.h  -->  sym_type_ast_ir.c  -->  part4.c  ...
 *   For GCC link: add sym_type_ast_ir.c to the command line.
 *
 * C89 strict:
 *   - No // comments
 *   - All variable declarations at top of block
 *   - No designated initializers  (.field = val not allowed)
 *   - No compound literals         ((Type){...} not allowed)
 *   - No __attribute__
 *   - LP64: unsigned long = 8 bytes on this platform
 *
 * ADAPT markers indicate seams where ZCC's actual field/constant
 * names must be substituted.  Six seams total; all are clearly marked.
 */

#ifndef ZCC_SYM_TYPE_AST_IR_H
#define ZCC_SYM_TYPE_AST_IR_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 * PART A: Coverage / Gate Tags
 *
 * All 19 tag strings.  ZCC_TAG(tag) fires zcc_coverage_hit()
 * when ZCC_COVERAGE is defined, otherwise compiles to nothing.
 * ============================================================ */

#define TAG_SCOPE      "TAG:SCOPE"
#define TAG_TYPE       "TAG:TYPE"
#define TAG_STRUCT     "TAG:STRUCT"
#define TAG_UNION      "TAG:UNION"
#define TAG_ENUM       "TAG:ENUM"
#define TAG_FUNC       "TAG:FUNC"
#define TAG_GLOBAL     "TAG:GLOBAL"
#define TAG_LOCAL      "TAG:LOCAL"
#define TAG_PARAM      "TAG:PARAM"
#define TAG_MEMBER     "TAG:MEMBER"
#define TAG_TYPEDEF    "TAG:TYPEDEF"
#define TAG_LABEL      "TAG:LABEL"
#define TAG_IR_OP      "TAG:IR_OP"
#define TAG_ABI        "TAG:ABI"
#define TAG_PREPROC    "TAG:PREPROC"
#define TAG_PARSER     "TAG:PARSER"
#define TAG_CODEGEN    "TAG:CODEGEN"
#define TAG_SELFHOST   "TAG:SELFHOST"
#define TAG_REGRESSION "TAG:REGRESSION"

#ifdef ZCC_COVERAGE
extern void zcc_coverage_hit(const char *tag, const char *file, int line);
#define ZCC_TAG(tag)  zcc_coverage_hit((tag), __FILE__, __LINE__)
#else
#define ZCC_TAG(tag)  ((void)0)
#endif

/* ============================================================
 * PART B: Integer constants
 * ============================================================ */

/* Scope kinds */
#define ZCC_SCOPE_GLOBAL    0
#define ZCC_SCOPE_FUNC      1
#define ZCC_SCOPE_BLOCK     2
#define ZCC_SCOPE_PROTO     3   /* function prototype scope */

/* Symbol namespace kinds */
#define ZCC_SYM_ORDINARY    0   /* plain identifier */
#define ZCC_SYM_TYPEDEF     1
#define ZCC_SYM_ENUM_CONST  2
#define ZCC_SYM_LABEL       3
#define ZCC_SYM_FUNC        4
#define ZCC_SYM_GLOBAL      5
#define ZCC_SYM_LOCAL       6
#define ZCC_SYM_PARAM       7
#define ZCC_SYM_MEMBER      8

/* Tag namespace kinds */
#define ZCC_TAG_KIND_STRUCT  0
#define ZCC_TAG_KIND_UNION   1
#define ZCC_TAG_KIND_ENUM    2

/*
 * ADAPT-1: Type kind constants.
 * These mirror ZCC's TY_* enum from part1.c/part2.c.
 * If ZCC already defines TY_INT etc., replace these #defines with
 * a mapping table:  #define ZCC_TY_INT  TY_INT
 */
#define ZCC_TY_VOID     0
#define ZCC_TY_CHAR     1
#define ZCC_TY_SHORT    2
#define ZCC_TY_INT      3
#define ZCC_TY_LONG     4
#define ZCC_TY_LLONG    5
#define ZCC_TY_UCHAR    6
#define ZCC_TY_USHORT   7
#define ZCC_TY_UINT     8
#define ZCC_TY_ULONG    9
#define ZCC_TY_ULLONG   10
#define ZCC_TY_FLOAT    11
#define ZCC_TY_DOUBLE   12
#define ZCC_TY_PTR      13
#define ZCC_TY_ARRAY    14
#define ZCC_TY_FUNC     15
#define ZCC_TY_STRUCT   16
#define ZCC_TY_UNION    17
#define ZCC_TY_ENUM     18

/* Table sizes -- must be powers of two */
#define ZCC_SCOPE_HASH_SIZE  256
#define ZCC_TAG_HASH_SIZE    128
#define ZCC_SYM_NAME_MAX     256

/* Coverage table capacity */
#define ZCC_COVERAGE_MAX_HITS  4096

/* ============================================================
 * PART C: Forward declarations (self-referential structs)
 * ============================================================ */

typedef struct ZccSymbol    ZccSymbol;
typedef struct ZccScope     ZccScope;
typedef struct ZccTagEntry  ZccTagEntry;
typedef struct ZccTagTable  ZccTagTable;

/*
 * ADAPT-2: ZCC internal types.
 * Type, Node, Member are defined in part1.c/part2.c.
 * In the concat build they are already in scope -- guard the
 * re-typedef with ZCC_TYPES_DECLARED.
 *
 * ZCC_STANDALONE_BUILD: concrete stub definitions are provided here
 * so the smoke-test binary compiles without ZCC internals.  The fields
 * cover every ADAPT-3/4/5 seam used in this file.  When integrating into
 * the real ZCC concat, set ZCC_TYPES_DECLARED so these are suppressed.
 */
#ifndef ZCC_TYPES_DECLARED
#  ifdef ZCC_STANDALONE_BUILD
/*
 * Standalone stub structs.  Order matters: Member first (Type uses it),
 * Type second (Node uses it), Node last.
 */
struct Member {
    char          name[256];   /* ADAPT-5: Member->name    */
    int           offset;      /* ADAPT-5: Member->offset  */
    struct Type  *ty;          /* ADAPT-5: Member->ty      */
    struct Member *next;       /* ADAPT-5: Member->next    */
};

struct Type {
    int            kind;       /* ADAPT-3: ZCC_TY_* constant */
    int            size;       /* ADAPT-3: sizeof in bytes   */
    int            align;      /* ADAPT-3: alignment         */
    struct Type   *base;       /* ADAPT-3: pointee / element / return type */
    struct Member *members;    /* ADAPT-3: struct/union field list */
};

struct Node {
    int           kind;        /* ADAPT-4: ND_* constant */
    struct Node  *lhs;         /* ADAPT-4: left child    */
    struct Node  *rhs;         /* ADAPT-4: right child   */
    struct Type  *ty;          /* ADAPT-4: type annotation */
    int           line;        /* ADAPT-4: source line   */
    struct Node  *next;        /* ADAPT-4: sibling / list link */
    int           val;         /* literal integer value  */
};
#  endif /* ZCC_STANDALONE_BUILD */

typedef struct Type    Type;
typedef struct Node    Node;
typedef struct Member  Member;
#endif /* !ZCC_TYPES_DECLARED */

/* ============================================================
 * PART D: Struct definitions
 * ============================================================ */

struct ZccSymbol {
    char        name[ZCC_SYM_NAME_MAX];  /* null-terminated symbol name */
    int         kind;                     /* ZCC_SYM_* */
    int         scope_depth;              /* 0=global, n=block nesting depth */
    Type       *ty;                       /* ZCC type pointer */
    int         is_typedef;               /* 1 if typedef name */
    int         is_param;                 /* 1 if function parameter */
    int         offset;                   /* stack byte offset (locals/params) */
    int         enum_value;               /* value for ZCC_SYM_ENUM_CONST */
    ZccSymbol  *next;                     /* next in hash bucket chain */
    ZccSymbol  *shadow;                   /* outer-scope symbol this shadows */
};

struct ZccScope {
    ZccSymbol  *buckets[ZCC_SCOPE_HASH_SIZE];
    ZccScope   *parent;
    int         depth;
    int         n_symbols;
    int         kind;    /* ZCC_SCOPE_* */
};

struct ZccTagEntry {
    char          name[ZCC_SYM_NAME_MAX];
    int           kind;          /* ZCC_TAG_KIND_* */
    int           is_complete;   /* 1=fully defined, 0=forward declared */
    Type         *ty;            /* NULL until tag_complete_type() is called */
    ZccTagEntry  *next;          /* hash bucket chain */
};

struct ZccTagTable {
    ZccTagEntry *buckets[ZCC_TAG_HASH_SIZE];
    ZccTagTable *parent;   /* enclosing tag scope */
    int          n_entries;
};

/* Visitor / rewriter callbacks for AST walkers */
typedef void  (*ZccASTVisitor) (Node *node, void *ctx);
typedef Node *(*ZccASTRewriter)(Node *node, void *ctx);

/* ============================================================
 * PART E: Coverage hit entry (only needed when ZCC_COVERAGE=1)
 * ============================================================ */

#ifdef ZCC_COVERAGE
typedef struct {
    const char *tag;
    const char *file;
    int         line;
    int         count;
} ZccCoverageHit;

extern ZccCoverageHit g_zcc_coverage_hits[ZCC_COVERAGE_MAX_HITS];
extern int            g_zcc_coverage_n;

void zcc_coverage_report(void);
int  zcc_coverage_count(const char *tag);
#endif

/* ============================================================
 * PART F: Layer 1 -- Recursive symbol-table layer
 * ============================================================ */

ZccScope  *sym_scope_push      (ZccScope *parent, int kind);
ZccScope  *sym_scope_pop       (ZccScope *scope);
int        sym_define          (ZccScope *sc, const char *name,
                                 int kind, Type *ty, int offset);
ZccSymbol *sym_lookup_local    (ZccScope *sc, const char *name);
ZccSymbol *sym_lookup_recursive(ZccScope *sc, const char *name);

ZccTagEntry *sym_lookup_tag_recursive    (ZccTagTable *tt,
                                           const char *name, int kind);
ZccSymbol   *sym_lookup_member_recursive (Type *struct_or_union_ty,
                                           const char *name);
ZccSymbol   *sym_shadow_check            (ZccScope *sc, const char *name);

void  sym_mangle_internal(const char *base, int uid,
                           char *out, int out_len);
void  sym_dump_scope_tree(ZccScope *sc, int indent);

/* ============================================================
 * PART G: Layer 2 -- Tag namespace layer
 * ============================================================ */

ZccTagTable *tag_table_push      (ZccTagTable *parent);
ZccTagTable *tag_table_pop       (ZccTagTable *tt);
ZccTagEntry *tag_define_struct   (ZccTagTable *tt,
                                   const char *name, Type *ty);
ZccTagEntry *tag_define_union    (ZccTagTable *tt,
                                   const char *name, Type *ty);
ZccTagEntry *tag_define_enum     (ZccTagTable *tt,
                                   const char *name, Type *ty);
ZccTagEntry *tag_lookup_recursive(ZccTagTable *tt,
                                   const char *name, int kind);
int          tag_complete_type   (ZccTagEntry *e, Type *ty);
int          tag_is_complete     (ZccTagEntry *e);
ZccTagEntry *tag_forward_declare (ZccTagTable *tt,
                                   const char *name, int kind);
int          tag_attach_members  (ZccTagEntry *e,
                                   Member **members, int n);

/* ============================================================
 * PART H: Layer 3 -- Type-recursion layer
 * ============================================================ */

Type *type_canonical          (Type *ty);
int   type_equal_recursive    (Type *a, Type *b);
Type *type_decay_array        (Type *ty);
Type *type_decay_func         (Type *ty);
Type *type_pointer_to         (Type *base);
Type *type_array_of           (Type *base, int n);
Type *type_function_of        (Type *ret, Type **params,
                                int n_params, int is_vararg);
int   type_struct_member_offset (Type *ty, const char *member_name);
int   type_sizeof_recursive   (Type *ty);
int   type_alignof_recursive  (Type *ty);
int   type_integer_rank       (Type *ty);
Type *type_usual_arith_conv   (Type *a, Type *b);
Type *type_lvalue_conversion  (Type *ty);

/* ============================================================
 * PART I: Layer 4 -- AST recursive walkers
 * ============================================================ */

void  ast_walk_preorder         (Node *root,
                                  ZccASTVisitor fn, void *ctx);
void  ast_walk_postorder        (Node *root,
                                  ZccASTVisitor fn, void *ctx);
Node *ast_rewrite_expr_recursive(Node *expr,
                                  ZccASTRewriter fn, void *ctx);
Node *ast_rewrite_stmt_recursive(Node *stmt,
                                  ZccASTRewriter fn, void *ctx);
void  ast_collect_symbols       (Node *root, ZccScope *out_scope);
void  ast_collect_tags          (Node *root, ZccTagTable *out_tt);
int   ast_validate_types        (Node *root);
int   ast_resolve_identifiers   (Node *root,
                                  ZccScope *sc, ZccTagTable *tt);
void  ast_dump_tree             (Node *root, int indent);

/* ============================================================
 * PART J: Layer 5 -- IR bridge / completeness
 *
 * These extend ir_bridge.h's emit patterns for full recursive
 * emission.  They depend on ir_bridge_fresh_tmp(), ir_save_result(),
 * ir_map_type(), and IRAsmCtx -- all from ir_bridge.h.
 * Must appear AFTER ir_bridge.h in the concat order.
 * ============================================================ */

/*
 * Forward declarations for IR types defined in compiler_passes.c.
 * In the concat build these are already visible; the struct tags
 * prevent a duplicate-typedef error.
 */
struct Function;
struct IRAsmCtx;

void ir_emit_expr_recursive (struct IRAsmCtx *ctx, Node *expr);
void ir_emit_stmt_recursive (struct IRAsmCtx *ctx, Node *stmt);
void ir_emit_lvalue_addr    (struct IRAsmCtx *ctx, Node *expr);
void ir_emit_load_if_lvalue (struct IRAsmCtx *ctx, Node *expr);
void ir_emit_store          (struct IRAsmCtx *ctx,
                              const char *addr_tmp,
                              const char *val_tmp, Type *ty);
void ir_emit_cast           (struct IRAsmCtx *ctx,
                              const char *src_tmp,
                              Type *from_ty, Type *to_ty,
                              char *dst_out, int dst_len);
void ir_emit_short_circuit  (struct IRAsmCtx *ctx, Node *expr,
                              int is_and,
                              char *result_out, int out_len);
void ir_emit_call_args      (struct IRAsmCtx *ctx, Node *arg_list,
                              int *n_args_out);
void ir_emit_control_flow   (struct IRAsmCtx *ctx, Node *stmt);
int  ir_validate_module     (struct Function **funcs, int n_funcs);
int  ir_validate_func       (struct Function *fn);
int  ir_validate_operand_types(struct Function *fn);

#endif /* ZCC_SYM_TYPE_AST_IR_H */
