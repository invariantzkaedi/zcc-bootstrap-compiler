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
/* Removed stdatomic.h, using volatile on x86_64 instead */

/* Forward declarations */
#define ZCC_BRIDGE_NAME_LEN  64
#define ZCC_AST_MAX_STMTS    4096
#define ZCC_CALL_NAME_LEN    128
#define ZCC_MAX_CALL_ARGS    16

/* Auto-generated values used by nd_to_znd(). Must match zcc.c enum. */
#include "zcc_ast_bridge_constants.h"

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
int           node_is_bitfield(struct Node *n);
int           node_bit_offset(struct Node *n);
int           node_bit_size(struct Node *n);
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
    ZND_POST_DEC, ZND_PRE_INC, ZND_PRE_DEC,  /* --x, ++x, x-- */
    ZND_ASM = 58,
    ZND_VLA_ALLOC = 59
};

typedef struct ZCCNode ZCCNode;
struct ZCCNode {
    int       kind;
    long long int_val;
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
    int       is_bitfield;
    int       bit_offset;
    int       bit_size;
    /* ZND_SWITCH */
    int       num_cases;
    long long *case_vals;      /* length num_cases */
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
    char     *asm_string;
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

/* ================================================================ */
/* Rust Frontend FFI: Recursive Initializer Bridge                  */
/* ================================================================ */

typedef struct ZccInitNode ZccInitNode;

typedef enum {
    ZCC_INIT_VALUE,      /* leaf scalar or string */
    ZCC_INIT_LIST,       /* { ... } node */
    ZCC_INIT_DESIGNATED  /* .field = or [idx] = (future) */
} ZccInitKind;

struct ZccInitNode {
    ZccInitKind kind;
    struct Type* ty;     /* owning type for this subtree */
    union {
        struct Node* value_node;  /* for leaf */
        struct {
            ZccInitNode** children;
            int           count;
            int           capacity;
        } list;
    };
};

/* Public builder API for Rust → ZCC */
ZccInitNode* zcc_init_list_begin(struct Type* ty);
void         zcc_init_list_append(ZccInitNode* list, ZccInitNode* child);
void         zcc_init_list_end(ZccInitNode* list);

ZccInitNode* zcc_init_value(struct Node* expr_node);

/* Convert Rust-built tree into full ZCC initializer (hooks directly into emit path) */
struct Node* zcc_build_initializer(struct Compiler* cc, ZccInitNode* root, struct Type* target_type);

/* Rust-side free callback (weakly implemented in C for standalone builds) */
void zcc_rust_free_init_tree(ZccInitNode* tree);

/* ================================================================ */
/* Flat Wire Protocol for Shared Memory Ring Buffer                 */
/* ================================================================ */

#define WIRE_INIT_MAGIC 0x5A434349  /* "ZCCI" */
#define SHM_RING_SIZE   (16 * 1024 * 1024)
#define MAX_SLOTS       256
#define WIRE_SLOT_SIZE  (SHM_RING_SIZE / MAX_SLOTS)

typedef enum {
    WIRE_LIST             = 1,
    WIRE_VALUE            = 2,
    WIRE_DESIGNATED_FIELD = 3,
    WIRE_DESIGNATED_INDEX = 4,
} WireKind;

typedef struct WireInitNode WireInitNode;

struct WireInitNode {
    uint32_t magic;
    uint8_t  kind;
    uint32_t payload_size;     /* total bytes of this node + all descendants */

    union {
        struct { uint32_t child_count; } list;           /* children follow immediately */

        struct {                                         /* WIRE_VALUE */
            uint8_t  vkind;      /* 0=i64, 1=f64, 2=str, 3=ident */
            uint32_t len;
            /* followed by raw bytes */
        } value;

        struct {                                         /* DESIGNATED */
            uint8_t  dkind;      /* 0=field, 1=index */
            uint32_t name_len;   /* 0 for index */
            long long index;
            /* name bytes (if field) + child node follow */
        } designated;
    } u;
};

typedef struct {
    volatile uint64_t   head;          // writer (Rust)
    volatile uint64_t   tail;          // reader (ZCC)
    volatile uint32_t   drop_count;    // backpressure signal
    uint8_t            data[SHM_RING_SIZE];
} ShmRingBuffer;

/* Public API */
struct Node* zcc_build_from_wire(struct Compiler *cc, const uint8_t *data, size_t len);
ShmRingBuffer* zcc_shm_ring_open(void);
struct Node* zcc_shm_ring_read(struct Compiler *cc, ShmRingBuffer* ring);

/* Cross-platform SHM entry point — called from part3 parse_program intercept.
 * Returns NULL on open failure, otherwise spins and returns parsed tree. */
struct Node* zcc_rust_shm_entry(struct Compiler *cc);

#endif /* ZCC_AST_BRIDGE_H */
