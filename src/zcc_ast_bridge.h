/**
 * zcc_ast_bridge.h — Option A copy boundary: ZCC Node* → ZCCNode* → IR.
 *
 * compiler_passes.c implements zcc_node_from(Node*) using the accessors below.
 * zcc.c implements the accessors and calls zcc_node_from / zcc_ast_to_ir when
 * linking the two. Standalone passes build uses weak stub accessors.
 */
#ifndef ZCC_AST_BRIDGE_H
#define ZCC_AST_BRIDGE_H

#include <stdint.h>

#define ZCC_BRIDGE_NAME_LEN  64
#define ZCC_AST_MAX_STMTS    4096
#define ZCC_CALL_NAME_LEN    128
#define ZCC_MAX_CALL_ARGS    16

/* Sentinel ND_* values used by nd_to_znd(). Must match zcc.c (part1.c) enum exactly.
 * ND_FADD=27..ND_FDIV=30 exist in part1.c but are handled by AST FP codegen only.
 * Everything from ND_NEG onward must account for those 4 FP slots. */
#define ZCC_ND_NUM     1
#define ZCC_ND_STR     2
#define ZCC_ND_VAR     5
#define ZCC_ND_ASSIGN  6
#define ZCC_ND_ADD     7
#define ZCC_ND_SUB     8
#define ZCC_ND_MUL     9
#define ZCC_ND_DIV     10
#define ZCC_ND_MOD     11
#define ZCC_ND_EQ      12
#define ZCC_ND_NE      13
#define ZCC_ND_LT      14
#define ZCC_ND_LE      15
#define ZCC_ND_GT      16
#define ZCC_ND_GE      17
#define ZCC_ND_LAND    18
#define ZCC_ND_LOR     19
#define ZCC_ND_LNOT    20
#define ZCC_ND_BAND    21
#define ZCC_ND_BOR     22
#define ZCC_ND_BXOR    23
#define ZCC_ND_BNOT    24
#define ZCC_ND_SHL     25
#define ZCC_ND_SHR     26
/* ND_FADD=27 .. ND_FDIV=30: FP ops, AST path only, not in bridge */
#define ZCC_ND_NEG     31
#define ZCC_ND_ADDR    32
#define ZCC_ND_DEREF   33
#define ZCC_ND_CALL    34
#define ZCC_ND_RETURN  35
/* ZCC_ND_BLOCK intentionally set to 0 (no-match) to prevent runaway ZND_BLOCK
 * recursion in zcc_node_from_stmt. With cc_alloc infinite-loop bug still present,
 * routing nested ND_BLOCK nodes through the bridge causes OOM in Stage 2/3.
 * The top-level function body block is still handled by zcc_run_passes_emit_body_pgo
 * directly; nested blocks fall through to AST path via nd_to_znd returning -1.  */
#define ZCC_ND_BLOCK   36
#define ZCC_ND_IF      37
#define ZCC_ND_WHILE   38
#define ZCC_ND_FOR     39
/* ND_DO_WHILE=40 not in bridge */
#define ZCC_ND_BREAK   41
#define ZCC_ND_CONTINUE 42
/* ND_GOTO=43, ND_LABEL=44 not in bridge */
#define ZCC_ND_SWITCH  45
/* ND_CASE=46, ND_DEFAULT=47 inside SWITCH lowering */
#define ZCC_ND_CAST    48
#define ZCC_ND_SIZEOF  49
/* ND_VA_ARG=50 not in bridge */
#define ZCC_ND_MEMBER  51
#define ZCC_ND_PRE_INC 52
#define ZCC_ND_PRE_DEC 53
#define ZCC_ND_POST_INC 54
#define ZCC_ND_POST_DEC 55
#define ZCC_ND_TERNARY 56
/* ND_COMMA_EXPR=57 not in bridge */
#define ZCC_ND_COMPOUND_ASSIGN 60
#define ZCC_ND_NOP     62

/* Opaque ZCC AST node; layout defined in zcc.c */
struct Node;

/* Accessors: implemented in zcc.c when linked; weak stubs in compiler_passes.c when standalone */
int           node_kind(struct Node *n);
long long     node_int_val(struct Node *n);
int           node_str_id(struct Node *n);   /* ND_STR: string table index */
void          node_name(struct Node *n, char *buf, int len);
struct Node  *node_lhs(struct Node *n);
struct Node  *node_rhs(struct Node *n);
struct Node  *node_cond(struct Node *n);
struct Node  *node_then_body(struct Node *n);
struct Node  *node_else_body(struct Node *n);
struct Node  *node_body(struct Node *n);
struct Node  *node_init(struct Node *n);   /* ND_FOR init */
struct Node  *node_inc(struct Node *n);     /* ND_FOR increment */
struct Node **node_cases(struct Node *n);   /* ND_SWITCH: array of ND_CASE nodes */
int           node_num_cases(struct Node *n);      /* ND_SWITCH */
struct Node  *node_default_case(struct Node *n);   /* ND_SWITCH: default case node or NULL */
long long     node_case_val(struct Node *n);       /* ND_CASE: case constant */
struct Node  *node_case_body(struct Node *n);      /* ND_CASE: body stmt */
int           node_member_offset(struct Node *n);  /* ND_MEMBER: byte offset of member */
int           node_member_size(struct Node *n);    /* ND_MEMBER: size of member type (for load/store) */
int           node_is_global(struct Node *n);       /* ND_VAR: 1 if global, 0 if local */
int           node_is_array(struct Node *n);
int           node_compound_op(struct Node *n);  /* ND_COMPOUND_ASSIGN: ND_ADD etc. */
struct Node **node_stmts(struct Node *n);
int           node_num_stmts(struct Node *n);
/* For ND_CALL */
const char   *node_func_name(struct Node *n);
struct Node  *node_arg(struct Node *n, int i);
int           node_num_args(struct Node *n);
int           node_line_no(struct Node *n);   /* source line for DWARF .loc */

int           node_lhs_ptr_size(struct Node *n);
int           node_rhs_ptr_size(struct Node *n);
int           node_is_array(struct Node *n);
int           node_is_func(struct Node *n);

/* ZCCNode: IR-bridge AST (scalars + control flow only). Same as compiler_passes.c internal type. */
enum {
    ZND_NUM = 1, ZND_STR, ZND_VAR, ZND_ASSIGN,
    ZND_ADD, ZND_SUB, ZND_MOD, ZND_MUL, ZND_BAND, ZND_SHL, ZND_SHR,
    ZND_LT, ZND_LE, ZND_GT, ZND_GE, ZND_EQ, ZND_NE,
    ZND_IF, ZND_WHILE, ZND_FOR, ZND_BREAK, ZND_CONTINUE, ZND_RETURN, ZND_BLOCK,
    ZND_CAST, ZND_CALL, ZND_NOP,
    ZND_POST_INC,      /* a++ (expr): load, add 1, store, result = old value */
    ZND_COMPOUND_ASSIGN, /* a += b etc. (stmt or expr) */
    ZND_ADDR,    /* &expr — want_address forces child to yield address */
    ZND_DEREF,   /* *expr — want_address: return ptr; else OP_LOAD */
    ZND_MEMBER,  /* base + offset; want_address for store */
    ZND_SWITCH,  /* switch(cond) { case val: body; default: body; } */
    ZND_DIV, ZND_NEG,
    ZND_LAND, ZND_LOR, ZND_LNOT,
    ZND_BOR, ZND_BXOR, ZND_BNOT,
    ZND_TERNARY,  /* cond ? then_expr : else_expr */
    ZND_SIZEOF,   /* sizeof(type) or sizeof expr — resolved size in int_val */
    ZND_POST_DEC, ZND_PRE_INC, ZND_PRE_DEC  /* --x, ++x, x-- */
};

typedef struct ZCCNode ZCCNode;
struct ZCCNode {
    int       kind;
    int64_t   int_val;
    char      name[ZCC_BRIDGE_NAME_LEN];
    ZCCNode  *lhs;
    ZCCNode  *rhs;
    ZCCNode  *cond;
    ZCCNode  *then_body;
    ZCCNode  *else_body;
    ZCCNode  *body;
    ZCCNode  *init;   /* ZND_FOR: init stmt/expr */
    ZCCNode  *inc;    /* ZND_FOR: increment expr */
    int       member_offset;  /* ZND_MEMBER: byte offset */
    int       member_size;    /* ZND_MEMBER: size for OP_LOAD/OP_STORE (0 = default 8) */
    /* ZND_SWITCH */
    int       num_cases;
    int64_t  *case_vals;      /* length num_cases */
    ZCCNode **case_bodies;    /* length num_cases */
    ZCCNode  *default_body;   /* NULL if no default */
    ZCCNode **stmts;
    unsigned int  num_stmts;
    /* ZND_CALL */
    char      func_name[ZCC_CALL_NAME_LEN];
    ZCCNode **args;
    int       num_args;
    /* ZND_IF: unique id for block names (if.then.%d etc.); set in zcc_node_from_stmt */
    int       if_id;
    /* ZND_COMPOUND_ASSIGN: ND_ADD, ND_SUB, etc. */
    int       compound_op;
    int       is_global;    /* ZND_VAR: 1 if global symbol, 0 if local */
    int       is_array;     /* 1 if variable has TY_ARRAY type */
    int       is_func;      /* 1 if variable has TY_FUNC type */
    int       line_no;     /* source line for DWARF (from node_line_no) */
};

/* Copy: deep-copy Node* tree into ZCCNode* (only supported kinds). Implemented in compiler_passes.c. */
ZCCNode *zcc_node_from(struct Node *n);

/* Free tree produced by zcc_node_from (recursive; frees stmts arrays). */
void zcc_node_free(ZCCNode *z);

/* IR build and pipeline. Implemented in compiler_passes.c. */
struct Function;
#define ZCC_PGO_MAX_BLOCKS 2048   /* must be >= MAX_BLOCKS in compiler_passes.c */
#define ZCC_BLOCK_NAME_LEN 64
struct PassResult {
    unsigned int order[ZCC_PGO_MAX_BLOCKS];  /* block IDs in emission order */
    char     block_names[ZCC_PGO_MAX_BLOCKS][ZCC_BLOCK_NAME_LEN];
    unsigned int n_blocks;
    unsigned int licm_hoisted;
    unsigned int dce_instrs_removed;
};
typedef struct PassResult PassResult;
struct Function *zcc_ast_to_ir(ZCCNode *body_ast, const char *func_name);
void run_all_passes(struct Function *fn, struct PassResult *result, const char *profile_path, int num_params);
void zcc_set_use_profile(const char *path);  /* set from main when -use-profile=<path> */
void zcc_ir_free(struct Function *fn);

/* Opaque ABI: ZCC passes ir_tree + profile_path + func_name; we allocate PassResult and log. */
void zcc_run_passes_log(ZCCNode *body_ast, const char *profile_path, const char *func_name);

/* Serialize function IR to JSON */
void ir_dump_json(void *f_ptr, struct Function *fn, const char *func_name, int is_first);
void zcc_ir_bridge_dump_and_free(ZCCNode *z_node, const char *func_name, void *dump_f, int is_first);

/* PGO instrumented: emit function body from IR; return n_blocks if emitted, 0 otherwise. */
int zcc_run_passes_emit_body(ZCCNode *body_ast, const char *profile_path, const char *func_name,
    void *out_file, int stack_size, int num_params, int func_end_label,
    int global_block_offset, int func_label_id, char *out_names, int *out_edges, int max_blocks_capacity);

/* PGO-optimized (no instrument): emit body from IR using profile for block order. */
int zcc_run_passes_emit_body_pgo(ZCCNode *body_ast, const char *profile_path, const char *func_name,
    void *out_file, int stack_size, int num_params, int func_end_label, int func_label_id);

#endif /* ZCC_AST_BRIDGE_H */
