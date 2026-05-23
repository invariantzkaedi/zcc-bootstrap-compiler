/*
 * ZCC - ZKAEDI C Compiler
 * A self-hosting C subset compiler targeting x86-64 Linux (System V ABI)
 * Single-file design: cat part1.c part2.c part3.c part4.c part5.c > zcc.c
 *
 * Usage:  ./zcc input.c -o output
 * Self-host: ./zcc zcc.c -o zcc2 && ./zcc2 zcc.c -o zcc3 && cmp zcc2 zcc3
 *
 * Supports: int/char/long/short/void/unsigned, pointers, arrays, structs,
 *           unions, enums, typedef, sizeof, function calls (up to 6 args),
 *           if/else/while/for/do-while/switch-case, goto/labels, break/continue,
 *           string literals, char literals, compound assignment, ternary,
 *           bitwise ops, shift ops, logical ops, pre/post inc/dec,
 *           struct member access (. and ->), casts, global/local vars,
 *           static/extern/const qualifiers, #include <stdio.h> etc via host cpp.
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

/* ================================================================ */
/* CONSTANTS — using enum for self-hosting (no preprocessor needed)  */
/* ================================================================ */

enum {
    MAX_IDENT   = 128,
    MAX_STR     = 16384,
    MAX_STRINGS = 262144,
    MAX_GLOBALS = 262144,
    MAX_STRUCTS = 65536,
    MAX_PARAMS  = 128,
    MAX_CALL_ARGS = 256,
    MAX_CASES   = 4096,
    MAX_INIT    = 1048576,
    ARENA_SIZE  = 536870912
};

/* ================================================================ */
/* TOKEN TYPES                                                       */
/* ================================================================ */

enum {
    TK_EOF = 0,
    TK_NUM, TK_STR, TK_CHAR_LIT, TK_IDENT, TK_FLIT,
    /* type keywords */
    TK_INT, TK_CHAR, TK_VOID, TK_LONG, TK_SHORT,
    TK_UNSIGNED, TK_SIGNED, TK_FLOAT, TK_DOUBLE,
    /* control flow */
    TK_IF, TK_ELSE, TK_WHILE, TK_FOR, TK_DO,
    TK_RETURN, TK_BREAK, TK_CONTINUE, TK_GOTO,
    TK_SWITCH, TK_CASE, TK_DEFAULT,
    /* type-related */
    TK_STRUCT, TK_UNION, TK_ENUM, TK_TYPEDEF,
    TK_SIZEOF, TK_STATIC, TK_EXTERN, TK_CONST,
    TK_VOLATILE, TK_AUTO, TK_REGISTER, TK_INLINE, TK_ASM,
    TK_BUILTIN_VA_ARG, TK_TYPEOF, TK_AUTO_TYPE,
    TK_PUBLIC, TK_PRIVATE, TK_PROTECTED,
    TK_NAMESPACE, TK_USING,
    TK_STATIC_CAST, TK_CONST_CAST, TK_REINTERPRET_CAST,
    TK_COLON_COLON, TK_OPERATOR,
    /* operators */
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_AMP, TK_PIPE, TK_CARET, TK_TILDE, TK_BANG,
    TK_ASSIGN, TK_EQ, TK_NE,
    TK_LT, TK_GT, TK_LE, TK_GE,
    TK_LAND, TK_LOR,
    TK_SHL, TK_SHR,
    TK_INC, TK_DEC,
    TK_ARROW, TK_DOT,
    TK_QUESTION, TK_COLON,
    /* compound assignment */
    TK_PLUS_ASSIGN, TK_MINUS_ASSIGN, TK_STAR_ASSIGN,
    TK_SLASH_ASSIGN, TK_PERCENT_ASSIGN,
    TK_AMP_ASSIGN, TK_PIPE_ASSIGN, TK_CARET_ASSIGN,
    TK_SHL_ASSIGN, TK_SHR_ASSIGN,
    /* delimiters */
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET,
    TK_SEMI, TK_COMMA, TK_ELLIPSIS,
    TK_HASH
};

/* ================================================================ */
/* AST NODE TYPES                                                    */
/* ================================================================ */

enum {
    ND_NUM = 1, ND_STR, ND_CHAR_LIT, ND_FLIT,
    ND_VAR, ND_ASSIGN,
    ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD,
    ND_EQ, ND_NE, ND_LT, ND_LE, ND_GT, ND_GE,
    ND_LAND, ND_LOR, ND_LNOT,
    ND_BAND, ND_BOR, ND_BXOR, ND_BNOT,
    ND_SHL, ND_SHR,
    ND_FADD, ND_FSUB, ND_FMUL, ND_FDIV,
    ND_NEG, ND_ADDR, ND_DEREF,
    ND_CALL, ND_RETURN, ND_BLOCK,
    ND_IF, ND_WHILE, ND_FOR, ND_DO_WHILE,
    ND_BREAK, ND_CONTINUE, ND_GOTO, ND_GOTO_COMPUTED, ND_LABEL,
    ND_SWITCH, ND_CASE, ND_DEFAULT,
    ND_CAST, ND_SIZEOF, ND_VA_ARG, ND_ADDR_LABEL,
    ND_MEMBER, ND_PRE_INC, ND_PRE_DEC,
    ND_POST_INC, ND_POST_DEC,
    ND_TERNARY,
    ND_COMMA_EXPR,
    ND_FUNC_DEF, ND_GLOBAL_VAR,
    ND_COMPOUND_ASSIGN,
    ND_INIT_LIST,
    ND_ASM,
    ND_NOP
};

/* ================================================================ */
/* TYPE KINDS                                                        */
/* ================================================================ */

enum {
    TY_VOID = 0, TY_CHAR, TY_UCHAR, TY_SHORT, TY_USHORT,
    TY_INT, TY_UINT, TY_LONG, TY_ULONG,
    TY_LONGLONG, TY_ULONGLONG, TY_FLOAT, TY_DOUBLE,
    TY_PTR, TY_ARRAY, TY_FUNC, TY_STRUCT, TY_UNION, TY_ENUM
};

typedef enum {
    CLASS_NO_CLASS = 0,
    CLASS_INTEGER,
    CLASS_SSE,
    CLASS_MEMORY
} abi_class_t;

/* ─── Symbol hygiene / linkage (V8) ─── */
typedef enum {
    OP_SYMBOL_INTERN = 300,  /* start high to avoid ND_* or IR_* overlap */
    OP_SYMBOL_DEDUP,
    OP_SYMBOL_RENAME,
    OP_SYMBOL_SCOPE,
    OP_SYMBOL_VISIBILITY,
    OP_SYMBOL_BIND,
    OP_SYMBOL_VERSION,
    OP_SYMBOL_ALIAS,
    OP_SYMBOL_ANCHOR,
    OP_SYMBOL_VERIFY,

    /* Label / local symbol safety */
    OP_LABEL_ALLOC,
    OP_LABEL_BIND,
    OP_LABEL_DEDUP,
    OP_LABEL_NAMESPACE,
    OP_LABEL_VERIFY,

    /* Debug / DWARF symbols */
    OP_DWARF_FILE_SCOPE,
    OP_DWARF_LOC_BIND,
    OP_DWARF_SYMBOL_MAP,
    OP_DWARF_VERIFY,

    /* Linker / object safety */
    OP_EXTERN_SYMBOL,
    OP_WEAK_SYMBOL,
    OP_GLOBAL_SYMBOL,
    OP_STATIC_SYMBOL,
    OP_UNDEF_SYMBOL,
    IR_RELOCATION_BIND,
    IR_RELOCATION_VERIFY,

    /* Symbol Opcode Pack V8 */
    OP_SYMBOL_CREATE,
    OP_SYMBOL_HASH,
    OP_SYMBOL_CANONICALIZE,
    OP_SYMBOL_UID,
    OP_SYMBOL_FINGERPRINT,

    OP_NAMESPACE_ENTER,
    OP_NAMESPACE_EXIT,
    OP_NAMESPACE_PUSH,
    OP_NAMESPACE_POP,
    OP_NAMESPACE_VERIFY,

    OP_LABEL_CREATE,
    OP_LABEL_ATTACH,
    OP_LABEL_LOCK,
    OP_LABEL_FALLTHROUGH,

    OP_STAGE_SYMBOL,
    OP_EMIT_SYMBOL_HASH,
    OP_SYMBOL_PARITY,
    OP_SYMBOL_RECEIPT,
    OP_SYMBOL_DIFF_LOCK,

    OP_SYMBOL_EXPORT,
    OP_SYMBOL_IMPORT,
    OP_SYMBOL_WEAK,
    OP_SYMBOL_RELOC,
    OP_SYMBOL_RESOLVE,

    OP_SYMBOL_SNAPSHOT,
    OP_SYMBOL_RESTORE,
    OP_SYMBOL_AUDIT,
    OP_SYMBOL_FAILURE,
    OP_SYMBOL_RECOVER,

    /* Symbol Opcode Pack V9 — Symbol Lifecycle Engine */
    OP_SYMBOL_DECLARE,      /* declaration encountered */
    OP_SYMBOL_DEFINE,       /* strong definition */
    OP_SYMBOL_EXTERN,       /* external declaration */
    OP_SYMBOL_COMPLETE,     /* symbol finalized */
    OP_SYMBOL_MERGE,        /* merge declaration state */

    OP_SYMBOL_OWNER,        /* owning scope/function/module */
    OP_SYMBOL_STATE,        /* declaration state enum */
    OP_SYMBOL_LOCK,         /* freeze symbol mutation */
    OP_SYMBOL_COLLISION,    /* duplicate identifier event */
    OP_SYMBOL_SHADOW,       /* lexical shadow marker */

    OP_SCOPE_ENTER,
    OP_SCOPE_EXIT,
    OP_SCOPE_ATTACH,
    OP_SCOPE_DETACH,
    OP_SCOPE_VERIFY,

    OP_LINK_RESOLVE,
    OP_LINK_CONFLICT,
    OP_LINK_WEAK_SELECT,
    OP_LINK_IMPORT,
    OP_LINK_EXPORT,

    OP_SYMBOL_TIMELINE,
    OP_SYMBOL_ROLLBACK,
    OP_SYMBOL_PATCH,
    OP_SYMBOL_VERIFY_V2,

    /* Symbol Pack V10 — ABI + Relocation Layer */
    OP_ABI_SYMBOL_CLASS,     /* function/object/tls/section/file */
    OP_ABI_CALLING_CONV,     /* SysV / internal / variadic */
    OP_ABI_VISIBILITY,       /* default/hidden/internal */
    OP_ABI_BINDING,          /* local/global/weak */
    OP_ABI_SIZE,             /* emitted symbol size */
    OP_ABI_ALIGN,            /* required alignment */

    OP_RELOC_CREATE,         /* create relocation request */
    OP_RELOC_BIND,           /* bind relocation to symbol */
    OP_RELOC_KIND,           /* pc-relative/absolute/got/plt */
    OP_RELOC_RESOLVE,        /* resolve relocation */
    OP_RELOC_VERIFY,         /* validate relocation target */

    OP_SECTION_ENTER,        /* .text/.data/.bss/.rodata */
    OP_SECTION_EXIT,
    OP_SECTION_BIND_SYMBOL,  /* symbol belongs to section */
    OP_SECTION_ALIGN,
    OP_SECTION_VERIFY,

    OP_SYMBOL_FINALIZE,      /* freeze symbol after codegen */
    OP_SYMBOL_EMIT_SIZE,     /* emit .size metadata */
    OP_SYMBOL_EMIT_TYPE,     /* emit .type metadata */
    OP_SYMBOL_ABI_VERIFY,    /* final ABI symbol check */
    OP_LINKAGE_RECEIPT,      /* forensic linkage proof */

    /* Symbol Pack V11 — Object Emission Contracts */
    OP_ELF_HEADER_VERIFY,     /* validate ELF class/arch/ABI */
    OP_ELF_SECTION_TABLE,     /* section table ownership */
    OP_ELF_SYMBOL_TABLE,      /* .symtab/.strtab emission */
    OP_ELF_RELOC_TABLE,       /* .rela.text/.rela.data emission */
    OP_ELF_SHDR_VERIFY,       /* section header validation */

    OP_SYMBOL_TYPE_FUNC,      /* emit .type @function */
    OP_SYMBOL_TYPE_OBJECT,    /* emit .type @object */
    OP_SYMBOL_SIZE_BEGIN,     /* mark size start */
    OP_SYMBOL_SIZE_END,       /* emit .size */
    OP_SYMBOL_BOUNDARY_CHECK, /* ensure no overlap */

    OP_RELOC_PC32,            /* RIP-relative relocation */
    OP_RELOC_PLT32,           /* call relocation */
    OP_RELOC_GOTPCREL,        /* GOT access */
    OP_RELOC_ABS64,           /* absolute 64-bit */
    OP_RELOC_RANGE_CHECK,     /* relocation range validator */

    OP_TEXT_SECTION_LOCK,
    OP_DATA_SECTION_LOCK,
    OP_BSS_SECTION_LOCK,
    OP_RODATA_SECTION_LOCK,
    OP_SECTION_ORDER_VERIFY,

    OP_OBJECT_HASH,
    OP_SECTION_HASH,
    OP_RELOC_HASH,
    OP_SYMBOL_TABLE_HASH,
    OP_OBJECT_RECEIPT,

    /* Symbol Pack V12 — Link-Time Determinism */
    OP_TU_CREATE,           /* create translation unit record */
    OP_TU_HASH,             /* deterministic TU hash */
    OP_TU_SYMBOL_INDEX,     /* per-TU symbol index */
    OP_TU_SECTION_INDEX,    /* per-TU section map */
    OP_TU_VERIFY,           /* validate TU metadata */

    OP_LINK_GRAPH_CREATE,   /* symbol dependency graph */
    OP_LINK_GRAPH_EDGE,     /* object/symbol dependency edge */
    OP_LINK_ORDER_LOCK,     /* deterministic object order */
    OP_LINK_DUP_CHECK,      /* duplicate strong symbol check */
    OP_LINK_WEAK_RESOLVE,   /* weak/strong resolution */

    OP_ARCHIVE_INDEX,       /* archive symbol table */
    OP_ARCHIVE_MEMBER_HASH, /* member identity hash */
    OP_ARCHIVE_ORDER_LOCK,  /* deterministic member order */
    OP_ARCHIVE_EXTRACT,     /* selected member extraction */
    OP_ARCHIVE_VERIFY,      /* validate archive resolution */

    OP_BINARY_LAYOUT_HASH,
    OP_FINAL_SYMBOL_HASH,
    OP_FINAL_RELOC_HASH,
    OP_FINAL_SECTION_HASH,
    OP_LINK_RECEIPT_V2,

    /* Symbol Pack V13 — Incremental Build Cache + TU Reuse */
    OP_CACHE_LOOKUP,          /* query TU cache */
    OP_CACHE_HIT,             /* cached TU accepted */
    OP_CACHE_MISS,            /* rebuild required */
    OP_CACHE_INVALIDATE,      /* invalidate stale TU */
    OP_CACHE_RECEIPT,         /* emit cache proof */

    OP_DEP_GRAPH_CREATE,      /* build include/symbol dependency graph */
    OP_DEP_GRAPH_EDGE,        /* dependency edge */
    OP_DEP_HASH,              /* dependency hash */
    OP_DEP_VERIFY,            /* validate dependency state */
    OP_DEP_DIRTY,             /* mark dependent TU dirty */

    OP_SYMBOL_CACHE_BIND,     /* bind symbol to cached TU */
    OP_SYMBOL_CACHE_VERIFY,   /* verify reused symbol identity */
    OP_SYMBOL_CACHE_INVALID,  /* stale cached symbol */
    OP_RELOC_CACHE_BIND,      /* reuse relocation metadata */
    OP_SECTION_CACHE_BIND,    /* reuse section layout metadata */

    OP_BUILD_GRAPH_HASH,      /* whole build graph hash */
    OP_INCREMENTAL_RECEIPT,   /* incremental proof */
    OP_CACHE_HIT_RATE,        /* telemetry */
    OP_REBUILD_REASON,        /* forensic reason */
    OP_CACHE_FINALIZE,        /* freeze cache result */

    /* Symbol Pack V14 — Content Addressable Build Graph */
    OP_CAS_CREATE,            /* create content-addressed object */
    OP_CAS_HASH_NODE,         /* hash graph node */
    OP_CAS_HASH_EDGE,         /* hash dependency edge */
    OP_CAS_LOOKUP,            /* retrieve cached node */
    OP_CAS_VERIFY,            /* validate content object */

    OP_DIRTY_PROPAGATE,       /* propagate dirty state */
    OP_DIRTY_STOP,            /* stop propagation */
    OP_DEP_PRUNE,             /* remove dead edge */
    OP_DEP_COLLAPSE,          /* merge equivalent subgraphs */
    OP_DEP_RECEIPT,           /* dependency proof */

    OP_BUILD_PLAN_CREATE,
    OP_BUILD_PLAN_SORT,
    OP_BUILD_PLAN_EXECUTE,
    OP_BUILD_PLAN_VERIFY,
    OP_BUILD_PLAN_RECEIPT,

    /* Symbol Pack V15 — Persistent CAS Storage + Build Plan Execution */
    OP_CAS_FILE_WRITE,        /* write CAS object to disk */
    OP_CAS_FILE_READ,         /* load CAS object from disk */
    OP_CAS_FILE_VERIFY,       /* validate disk CAS object */
    OP_CAS_INDEX_WRITE,       /* persist CAS index */
    OP_CAS_INDEX_READ,        /* load CAS index */

    OP_BUILD_DAG_CREATE,      /* construct executable DAG */
    OP_BUILD_DAG_EDGE,        /* add task dependency edge */
    OP_BUILD_DAG_READY,       /* node ready to execute */
    OP_BUILD_DAG_RUN,         /* execute node */
    OP_BUILD_DAG_DONE,        /* node complete */

    OP_DIRTY_NODE_MARK,       /* mark node dirty */
    OP_DIRTY_EDGE_PROPAGATE,  /* propagate dirtiness */
    OP_DIRTY_SUBTREE,         /* dirty all dependents */
    OP_DIRTY_REASON,          /* record forensic cause */
    OP_DIRTY_RECEIPT,         /* emit dirty proof */

    OP_JOB_QUEUE_CREATE,
    OP_JOB_ENQUEUE,
    OP_JOB_DEQUEUE,
    OP_JOB_COMPLETE,
    OP_JOB_BARRIER,

    OP_CAS_RECEIPT,
    OP_BUILD_PLAN_HASH,
    OP_BUILD_EXEC_HASH,
    OP_BUILD_FINAL_RECEIPT,
    OP_REPLAY_BUILD,

    /* Symbol Pack V16 — Parallel Build Executor + Work Stealing */
    OP_WORKER_CREATE,
    OP_WORKER_SLEEP,
    OP_WORKER_WAKE,
    OP_DEP_READY,
    OP_DEP_BLOCKED,
    OP_DEP_COMPLETE,
    OP_DEP_NOTIFY,
    OP_DEP_RELEASE,

    OP_SYNC_FENCE,
    OP_SYNC_WAIT,
    OP_SYNC_SIGNAL,
    OP_SYNC_BARRIER,
    OP_SYNC_VERIFY,

    OP_EXEC_TIME,
    OP_WORKER_LOAD,
    OP_QUEUE_DEPTH,
    OP_STEAL_COUNT,
    OP_EXEC_RECEIPT,

    /* Bonus V16.5 — Scheduler Safety + Cycle Proof Pack */
    OP_DAG_TOPO_SORT,
    OP_DAG_CYCLE_CHECK,
    OP_DAG_ROOT_SET,
    OP_DAG_LEAF_SET,
    OP_DAG_LEVELIZE,

    OP_WORK_STEAL_PROBE,
    OP_WORK_STEAL_CLAIM,
    OP_WORK_STEAL_FAIL,
    OP_WORK_REBALANCE,
    OP_WORK_STARVATION_GUARD,

    OP_JOB_READY_RECEIPT,
    OP_JOB_START_RECEIPT,
    OP_JOB_DONE_RECEIPT,
    OP_JOB_FAIL_RECEIPT,
    OP_JOB_RETRY_RECEIPT,

    OP_DETERMINISTIC_JOIN,
    OP_RESULT_ORDER_LOCK,
    OP_PARALLEL_HASH_MERGE,
    OP_RACE_GUARD,
    OP_PARALLEL_RECEIPT,

    /* Bonus V17 — Memory Model + Lock-Free Execution Pack */
    OP_ATOMIC_LOAD,
    OP_ATOMIC_STORE,
    OP_ATOMIC_EXCHANGE,
    OP_ATOMIC_COMPARE_SWAP,
    OP_ATOMIC_FETCH_ADD,

    OP_MEMORY_ACQUIRE,
    OP_MEMORY_RELEASE,
    OP_MEMORY_ACQ_REL,
    OP_MEMORY_SEQ_CST,
    OP_MEMORY_RELAXED,

    OP_QUEUE_ATOMIC_PUSH,
    OP_QUEUE_ATOMIC_POP,
    OP_QUEUE_HEAD_UPDATE,
    OP_QUEUE_TAIL_UPDATE,
    OP_QUEUE_VERIFY,

    OP_HAZARD_CREATE,
    OP_HAZARD_PROTECT,
    OP_HAZARD_RELEASE,
    OP_EPOCH_ADVANCE,
    OP_GC_RETIRE,

    OP_RACE_TRACK,
    OP_RACE_VERIFY,
    OP_SHARED_ACCESS,
    OP_EXCLUSIVE_ACCESS,
} ZCCSymbolOpcode;

/* ================================================================ */
/* Bonus V18 — Compiler Runtime Boundary Pack                       */
/* ================================================================ */

typedef enum {
    /* Runtime event system */
    OP_EVENT_CREATE,
    OP_EVENT_EMIT,
    OP_EVENT_SUBSCRIBE,
    OP_EVENT_COMPLETE,
    OP_EVENT_VERIFY,

    /* Build-state snapshots */
    OP_STATE_SAVE,
    OP_STATE_LOAD,
    OP_STATE_DIFF,
    OP_STATE_MERGE,
    OP_STATE_VERIFY_SNAP,

    /* Runtime policy */
    OP_POLICY_ATTACH,
    OP_POLICY_DETACH,
    OP_POLICY_EVALUATE,
    OP_POLICY_OVERRIDE,
    OP_POLICY_RECEIPT_POL,

    /* Build execution receipts */
    OP_EXEC_BEGIN,
    OP_EXEC_END,
    OP_EXEC_VERIFY_EXEC,
    OP_EXEC_HASH,
    OP_EXEC_AUDIT,

    /* Bonus V19 — Compiler/Runtime Manifest ABI */
    OP_MANIFEST_CREATE,
    OP_MANIFEST_HASH,
    OP_MANIFEST_VERSION,
    OP_MANIFEST_COMPAT,
    OP_MANIFEST_VERIFY,

    OP_HANDSHAKE_BEGIN,
    OP_HANDSHAKE_CAPABILITY,
    OP_HANDSHAKE_POLICY,
    OP_HANDSHAKE_ACCEPT,
    OP_HANDSHAKE_REJECT,

    OP_CAP_CAS,
    OP_CAP_PARALLEL,
    OP_CAP_INCREMENTAL,
    OP_CAP_TELEMETRY,
    OP_CAP_REPLAY,

    OP_BOUNDARY_HASH,
    OP_BOUNDARY_RECEIPT,
    OP_BOUNDARY_AUDIT,
    OP_BOUNDARY_REPLAY,
    OP_BOUNDARY_FINALIZE,

    /* Bonus V20 — Manifest Schema + Disk Handshake */
    OP_MANIFEST_FILE_READ,
    OP_MANIFEST_FILE_WRITE,
    OP_MANIFEST_FILE_LOCK,
    OP_MANIFEST_FILE_HASH,
    OP_MANIFEST_FILE_VERIFY,

    OP_SCHEMA_CREATE,
    OP_SCHEMA_FIELD,
    OP_SCHEMA_REQUIRED,
    OP_SCHEMA_VERSION,
    OP_SCHEMA_VERIFY,

    OP_CAP_REQUIRE,
    OP_CAP_OPTIONAL,
    OP_CAP_NEGOTIATE,
    OP_CAP_DENY,
    OP_CAP_RECEIPT,

    OP_COMPAT_MATRIX_CREATE,
    OP_COMPAT_CHECK,
    OP_COMPAT_DOWNGRADE,
    OP_COMPAT_UPGRADE,
    OP_COMPAT_RECEIPT,

    OP_RUNTIME_PACKAGE_HASH,
    OP_RUNTIME_PACKAGE_VERIFY,
    OP_RUNTIME_PACKAGE_LOCK,
    OP_RUNTIME_PACKAGE_AUDIT,
    OP_RUNTIME_PACKAGE_RECEIPT,

    /* Bonus V21 — Audit Compression + Versioned Registry */
    OP_PACK_REGISTER,
    OP_PACK_ENABLE,
    OP_PACK_DISABLE,
    OP_PACK_HASH,
    OP_PACK_VERIFY,
    OP_PACK_RECEIPT,
    OP_REGISTRY_LOCK,
    OP_REGISTRY_AUDIT,
    OP_REGISTRY_REPLAY,
    OP_REGISTRY_FINALIZE
} RuntimeOpcode;

#define ZCC_COMPILER_ABI 19
#define ZCC_RUNTIME_ABI_MAX 19

typedef struct {
    unsigned long long manifest_hash;
    unsigned int compiler_abi_version;
    unsigned int runtime_abi_version;
    unsigned int capabilities;
    unsigned long long policy_hash;
} ZCCRuntimeManifest;

typedef struct {
    unsigned int schema_version;
    unsigned int compiler_abi;
    unsigned int runtime_abi;
    unsigned int required_caps;
    unsigned int optional_caps;
    unsigned long long manifest_hash;
    unsigned long long package_hash;
} ZCCManifestFile;

typedef struct {
    unsigned int pack_id;
    unsigned int version;
    unsigned int opcode_start;
    unsigned int opcode_end;
    unsigned long long pack_hash;
    int enabled;
} ZCCOpcodePack;

typedef struct {
    unsigned long long registry_hash;
    unsigned long long manifest_hash;
    unsigned long long link_hash;
    unsigned long long binary_hash;
    unsigned int opcode_pack_count;
    unsigned int abi_version;
} ZCCReleaseReceipt;

typedef enum {
    SYM_DECLARED,
    SYM_DEFINED,
    SYM_EXTERN,
    SYM_COMPLETE
} SymbolState;

typedef enum {
    SYM_KIND_FUNC,
    SYM_KIND_OBJECT,
    SYM_KIND_TLS,
    SYM_KIND_SECTION,
    SYM_KIND_FILE
} SymbolKind;

typedef enum {
    SEC_NONE = 0,
    SEC_TEXT = 1,
    SEC_DATA = 2,
    SEC_BSS = 3,
    SEC_RODATA = 4,
    SEC_STACK = 99
} SectionID;

typedef struct TranslationUnit TranslationUnit;
typedef struct LinkGraphEdge LinkGraphEdge;
typedef struct LinkGraph LinkGraph;
typedef struct ArchiveMember ArchiveMember;
typedef struct Archive Archive;
typedef struct LinkReceiptV2 LinkReceiptV2;

struct TranslationUnit {
    char name[256];
    unsigned long long hash;
    unsigned int symbol_count;
    unsigned int section_map[10];
    unsigned long long symbol_hashes[512];
};

struct LinkGraphEdge {
    char from_symbol[MAX_IDENT];
    char to_symbol[MAX_IDENT];
    int is_weak;
};

struct LinkGraph {
    TranslationUnit tus[64];
    int num_tus;
    LinkGraphEdge edges[1024];
    int num_edges;
    int deterministic_order_locked;
};

struct ArchiveMember {
    char name[256];
    unsigned long long hash;
    int order;
};

struct Archive {
    char name[256];
    ArchiveMember members[64];
    int num_members;
    unsigned long long symbol_table_hash;
};

struct LinkReceiptV2 {
    unsigned long long tu_hash;
    unsigned long long symbol_table_hash;
    unsigned long long section_order_hash;
    unsigned long long relocation_hash;
    unsigned long long final_binary_hash;
};

typedef struct DependencyNode DependencyNode;
typedef struct DependencyGraph DependencyGraph;
typedef struct IncrementalCacheEntry IncrementalCacheEntry;
typedef struct IncrementalCache IncrementalCache;

struct DependencyNode {
    char path[256];
    unsigned long long mtime_or_hash;
    int is_dirty;
};

struct DependencyGraph {
    DependencyNode nodes[128];
    int num_nodes;
    unsigned long long accumulated_dep_hash;
};

struct IncrementalCacheEntry {
    unsigned long long cache_key; /* Link Receipt V2 based hash key */
    TranslationUnit tu;
    unsigned long long dependency_hash;
    int hit_count;
    int is_valid;
    struct IncrementalCacheEntry *next; /* next in bucket chain */
};

typedef struct CacheBucket CacheBucket;
struct CacheBucket {
    IncrementalCacheEntry *head;
};

struct IncrementalCache {
    IncrementalCacheEntry entries[32];
    int num_entries;
    CacheBucket buckets[17]; /* CACHE_BUCKETS = 17 */
    int cache_hits;
    int cache_misses;
};

typedef struct CASEntry CASEntry;
struct CASEntry {
    unsigned long long hash;
    unsigned int size;
    char path[256];
};

typedef struct BuildDAGNode BuildDAGNode;
struct BuildDAGNode {
    char name[256];
    int state; /* 0 = pending, 1 = ready, 2 = running, 3 = done */
    unsigned long long input_hash;
    unsigned long long output_hash;
    int dependencies[8];
    int num_dependencies;
};

typedef struct BuildDAG BuildDAG;
struct BuildDAG {
    BuildDAGNode nodes[32];
    int num_nodes;
    unsigned long long dag_hash;
};

typedef struct Job Job;
struct Job {
    unsigned long long hash;
    int state;
    int dep_count;
    int owner_worker; /* V17 job ownership invariant */
    struct Job *next;
};

typedef struct Worker Worker;
struct Worker {
    int id;
    Job *local_queue;
    unsigned long long completed;
};

typedef struct Scheduler Scheduler;
struct Scheduler {
    Worker workers[32];
    int worker_count;
};

typedef struct SharedState SharedState;
struct SharedState {
    volatile unsigned long long version;
    volatile int readers;
    volatile int writers;
};

typedef struct HazardPtr HazardPtr;
struct HazardPtr {
    void *ptr;
    unsigned long long epoch;
};

/* ================================================================ */
/* FORWARD DECLARATIONS OF STRUCTS                                   */
/* ================================================================ */

typedef struct Type Type;
typedef struct Node Node;
typedef struct Symbol Symbol;
typedef struct Scope Scope;
typedef struct Compiler Compiler;
typedef struct ArenaBlock ArenaBlock;
typedef struct StringEntry StringEntry;
typedef struct StructField StructField;
typedef struct RustAst RustAst;
typedef struct RustDiag RustDiag;

/* ================================================================ */
/* DATA STRUCTURES                                                   */
/* ================================================================ */

struct ArenaBlock {
    char *data;
    int pos;
    int cap;
    ArenaBlock *next;
};

struct StructField {
    char name[MAX_IDENT];
    Type *type;
    int offset;
    int is_bitfield;
    int bit_offset;
    int bit_size;
    StructField *next;
};

struct Type {
    unsigned long long magic;
    unsigned long long alloc_id;
    int kind;
    int size;
    int align;
    Type *base;        /* for ptr/array */
    int array_len;
    /* function */
    Type *ret;
    Type **params;
    int num_params;
    int is_variadic;
    /* struct/union */
    char tag[MAX_IDENT];
    StructField *fields;
    int is_complete;
    int is_packed;   /* __attribute__((packed)) — suppress field alignment */
    int explicit_align; /* __attribute__((aligned(N))) — override total align, 0 = none */
    int is_tbfp;     /* transparent bitfield packing (tbfp) marker */
};

struct StringEntry {
    char *data;
    int len;
    int label_id;
};

struct Symbol {
    char name[MAX_IDENT];
    Type *type;
    int is_local;
    int is_global;
    int is_typedef;
    int is_enum_const;
    long long enum_val;
    int stack_offset;  /* for locals */
    char asm_name[MAX_IDENT];
    /* Regalloc */
    char *assigned_reg;
    int live_start;
    int live_end;
    Symbol *next;      /* linked list in scope */
};

struct Scope {
    Symbol *symbols;
    Scope *parent;
};

struct FuncParams {
    char names[MAX_PARAMS][MAX_IDENT];
    Type *types[MAX_PARAMS];
};

struct Node {
    unsigned long long magic;
    unsigned long long alloc_id;
    int kind;
    int line;
    Type *type;

    /* ND_NUM */
    long long int_val;

    /* ND_FLIT */
    double f_val;

    /* ND_STR */
    int str_id;

    /* ND_VAR */
    char name[MAX_IDENT];
    Symbol *sym;

    /* ND_ASSIGN, binary ops */
    Node *lhs;
    Node *rhs;

    /* ND_CALL */
    char func_name[MAX_IDENT];
    Node **args;
    int num_args;

    /* ND_IF / ND_WHILE / ND_FOR / ND_TERNARY */
    Node *cond;
    Node *then_body;
    Node *else_body;
    Node *init;
    Node *inc;

    /* ND_BLOCK */
    Node **stmts;
    int num_stmts;

    /* ND_FUNC_DEF */
    char func_def_name[MAX_IDENT];
    Type *func_type;
    struct FuncParams *func_params;
    int num_params;
    Node *body;
    int stack_size;
    /* param capture arrays (used by parse_func_def in part3.c) */
    Type *param_types[MAX_PARAMS];
    char param_names_buf[MAX_PARAMS][MAX_IDENT];

    /* ND_MEMBER */
    char member_name[MAX_IDENT];
    int member_offset;
    int member_size;

    /* ND_SWITCH */
    Node **cases;
    int num_cases;
    Node *default_case;

    /* ND_CASE */
    long long case_val;
    Node *case_body;

    /* ND_GOTO / ND_LABEL */
    char label_name[MAX_IDENT];

    /* ND_COMPOUND_ASSIGN */
    int compound_op;  /* ND_ADD, ND_SUB, etc */

    /* ND_CAST */
    Type *cast_type;

    /* ND_GLOBAL_VAR */
    int is_static;
    int is_extern;
    Node *initializer;

    /* ND_BITFIELD (IR bridge) */
    int is_bitfield;
    int bit_offset;
    int bit_size;

    /* ND_ASM */
    char *asm_string;

    /* linked list for top-level */
    Node *next;
};

char *zcc_preprocess(const char *source, int source_len, const char *filename, const char *include_paths, const char *define_flags, int *out_len);

#include "zcc_ast_bridge.h"

/* Keep nd_to_znd mapping in sync with this file's enum. */
#include "zcc_ast_bridge_asserts.inc"

/* Bridge accessors: Node* → IR bridge (Option A copy boundary). */
int is_pointer(Type *t);
int type_size(Type *t);
void validate_node(Compiler *cc, Node *node, const char *where, int line);
void validate_type(Compiler *cc, Type *type, const char *where, int line);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);

int node_kind(struct Node *n) { return n ? n->kind : 0; }
const char *node_asm_string(struct Node *n) { return n ? n->asm_string : 0; }
long long node_int_val(struct Node *n) { return n ? (long long)n->int_val : 0; }
int node_str_id(struct Node *n) { return n ? n->str_id : 0; }
void node_name(struct Node *n, char *buf, int len) {
    if (!n || !buf || len == 0) return;
    int i; i = 0;
    while (i < len - 1 && n->name[i]) { buf[i] = n->name[i]; i++; }
    buf[i] = '\0';
}
void node_label_name(struct Node *n, char *buf, int len) {
    if (!n || !buf || len == 0) return;
    int i; i = 0;
    while (i < len - 1 && n->label_name[i]) { buf[i] = n->label_name[i]; i++; }
    buf[i] = '\0';
}
struct Node *node_lhs(struct Node *n) { return n ? n->lhs : NULL; }
struct Node *node_rhs(struct Node *n) { return n ? n->rhs : NULL; }
struct Node *node_cond(struct Node *n) { return n ? n->cond : NULL; }
struct Node *node_then_body(struct Node *n) { return n ? n->then_body : NULL; }
struct Node *node_else_body(struct Node *n) { return n ? n->else_body : NULL; }
struct Node *node_body(struct Node *n) { return n ? n->body : NULL; }
struct Node *node_init(struct Node *n) { return n ? n->init : NULL; }
struct Node *node_inc(struct Node *n) { return n ? n->inc : NULL; }
int node_compound_op(struct Node *n) { return n ? n->compound_op : 0; }
struct Node **node_stmts(struct Node *n) { return n ? n->stmts : NULL; }
int node_num_stmts(struct Node *n) { return n ? n->num_stmts : 0; }
const char *node_func_name(struct Node *n) {
    if (!n) return "";
    if (n->func_name[0]) return n->func_name;
    if (n->lhs && n->lhs->kind == ND_VAR && n->lhs->name[0]) return n->lhs->name;
    return "";
}

int node_lhs_ptr_size(struct Node *n) {
    if (!n || !n->lhs || !n->lhs->type) return 1;
    if (is_pointer(n->lhs->type) && n->lhs->type->base) {
        return type_size(n->lhs->type->base);
    }
    return 1;
}

int node_rhs_ptr_size(struct Node *n) {
    if (!n || !n->rhs || !n->rhs->type) return 1;
    if (is_pointer(n->rhs->type) && n->rhs->type->base) {
        return type_size(n->rhs->type->base);
    }
    return 1;
}
struct Node *node_arg(struct Node *n, int i) {
    if (!n || !n->args || i < 0 || i >= n->num_args) return NULL;
    return n->args[i];
}
int node_num_args(struct Node *n) { return n ? n->num_args : 0; }
struct Node **node_cases(struct Node *n) { return n ? n->cases : NULL; }
int node_num_cases(struct Node *n) { return n ? n->num_cases : 0; }
struct Node *node_default_case(struct Node *n) { return n ? n->default_case : NULL; }
long long node_case_val(struct Node *n) { return n ? (long long)n->case_val : 0; }
struct Node *node_case_body(struct Node *n) { return n ? n->case_body : NULL; }
int node_member_offset(struct Node *n) { return n ? n->member_offset : 0; }
int node_member_size(struct Node *n) { return (n && n->type) ? (int)n->type->size : 8; }
int node_line_no(struct Node *n) { return n ? n->line : 0; }
int node_is_char_or_void_ptr_cast(struct Node *n) {
    if (!n || n->kind != ND_CAST || !n->cast_type) return 0;
    Type *t = n->cast_type;
    if (t->kind == TY_PTR && t->base) {
        int bk = t->base->kind;
        if (bk == TY_CHAR || bk == TY_UCHAR || bk == TY_VOID) {
            return 1;
        }
    }
    return 0;
}
int node_is_bitfield(struct Node *n) {
    if (n) {
        fprintf(stderr, "[DEBUG-ACCESSOR] node_is_bitfield called: kind=%d, is_bitfield=%d, member_name='%s'\n", n->kind, n->is_bitfield, n->kind == ND_MEMBER ? n->member_name : "");
    }
    return n ? n->is_bitfield : 0;
}
int node_bit_offset(struct Node *n) {
    if (n && n->kind == ND_MEMBER && n->is_bitfield) {
        fprintf(stderr, "[DEBUG-ACCESSOR] node_bit_offset: kind=%d, member='%s', bit_offset=%d\n", n->kind, n->member_name, n->bit_offset);
    }
    return n ? n->bit_offset : 0;
}
int node_bit_size(struct Node *n) {
    if (n && n->kind == ND_MEMBER && n->is_bitfield) {
        fprintf(stderr, "[DEBUG-ACCESSOR] node_bit_size: kind=%d, member='%s', bit_size=%d\n", n->kind, n->member_name, n->bit_size);
    }
    return n ? n->bit_size : 0;
}
int node_is_global(struct Node *n) {
    if (!n || n->kind != ND_VAR) return 0;
    return (n->sym && n->sym->is_local) ? 0 : 1;
}



int node_is_array(struct Node *n) {
    if (!n) return 0;
    return (n->type && n->type->kind == TY_ARRAY) ? 1 : 0;
}

int node_is_func(struct Node *n) {
    if (!n) return 0;
    /* Also handle implicit functions with no symbol/type properly if needed, but normally part3.c typed them as ty_int. Wait, if implicit, how do we know it's a function? Normally implicit functions are only in ND_CALL, and we only decay functions in ND_VAR/ND_MEMBER! */
    return (n->type && n->type->kind == TY_FUNC) ? 1 : 0;
}

/* CG-IR-015: Type-width accessors for IR instruction selection.
 * Used by compiler_passes.c to pick 32-bit vs 64-bit lowering for
 * OP_DIV / OP_MOD / OP_SHR so the IR backend matches GCC semantics. */
int node_type_size(struct Node *n) {
    /* Returns the result type size in bytes (4=int, 8=long/ptr).
     * Returns 8 (safe default) when type is unknown or absent. */
    if (!n || !n->type) return 8;
    return (n->type->size > 0) ? (int)n->type->size : 8;
}
int node_type_unsigned(struct Node *n) {
    /* Returns 1 if the node's result type is an unsigned integer kind. */
    if (!n || !n->type) return 0;
    int k = n->type->kind;
    return (k == TY_UCHAR || k == TY_USHORT || k == TY_UINT ||
            k == TY_ULONG || k == TY_ULONGLONG) ? 1 : 0;
}

struct Compiler {
    int verbose;
    /* source */
    char *source;
    int source_len;
    int pos;
    char *filename;

    /* current token */
    int tk;
    long long tk_val;
    double tk_fval;
    char tk_text[MAX_IDENT];
    char tk_str[MAX_STR];
    int tk_str_len;
    int tk_line;
    int tk_col;

    /* peek token (for lookahead) */
    int has_peek;
    int peek_tk;
    long long peek_val;
    double peek_fval;
    char peek_text[MAX_IDENT];
    char peek_str[MAX_STR];
    int peek_str_len;
    int peek_line;
    int peek_col;

    /* lexer state */
    int line;
    int col;

    /* type singletons */
    Type *ty_void;
    Type *ty_char;
    Type *ty_uchar;
    Type *ty_short;
    Type *ty_ushort;
    Type *ty_int;
    Type *ty_uint;
    Type *ty_long;
    Type *ty_ulong;
    Type *ty_longlong;
    Type *ty_ulonglong;
    Type *ty_float;
    Type *ty_double;

    /* scope */
    Scope *current_scope;

    /* strings table */
    StringEntry strings[MAX_STRINGS];
    int num_strings;

    /* structs */
    Type *structs[MAX_STRUCTS];
    int num_structs;

    /* globals for codegen */
    Node *globals[MAX_GLOBALS];
    int num_globals;

    /* codegen state */
    FILE *out;
    int label_count;
    int str_label_count;
    int stack_depth;

    /* loop labels for break/continue */
    int break_label;
    int continue_label;

    /* switch labels */
    int switch_end_label;

    /* current function name (for returns) */
    char current_func[MAX_IDENT];
    int func_end_label;

    /* arena allocator */
    ArenaBlock arena;

    /* error count */
    int errors;

    /* local variable offset counter (for codegen) */
    int local_offset;

    int current_is_static;

    /* pending __attribute__ flags — set by lexer, consumed by parse_struct_or_union */
    int pending_packed;     /* __attribute__((packed)) seen before struct keyword */
    int pending_aligned_n;  /* __attribute__((aligned(N))) value, 0 = none */
    int pending_tbfp;       /* transparent bitfield packing flag, consumed by parse_struct_or_union */
    int debug_abi_classes;  /* -fdebug-abi-classes flag */
    int abi_scratch_offset; /* %rbp-relative offset to 16-byte aggregate return scratch (CG-IR-019) */
    int sret_offset;        /* %rbp-relative offset to the hidden struct return pointer */
    int used_regs_mask;     /* for telemetry tracking */
    int is_forced_mask;     /* 1 if 0x1F was forced (CG-IR-011) */
};

typedef struct TargetBackend {
    int ptr_size;
    void (*emit_prologue)(Compiler *cc, Node *func);
    void (*emit_epilogue)(Compiler *cc, Node *func);
    void (*emit_call)(Compiler *cc, Node *func);
    void (*emit_binary_op)(Compiler *cc, int op);
    void (*emit_load_stack)(Compiler *cc, int offset, const char *reg);
    void (*emit_store_stack)(Compiler *cc, int offset, const char *reg);
    void (*emit_float_binop)(Compiler *cc, int op);
} TargetBackend;

typedef enum {
    FRONTEND_LANG_C = 0,
    FRONTEND_LANG_RUST = 1,
    FRONTEND_LANG_ASSET = 2
} FrontendLang;

typedef unsigned int RustSymbolId;
typedef enum {
    RUST_SYMBOL_INVALID = 0
} RustSymbolConst;

typedef enum {
    RUST_SYMBOL_FUNCTION = 0,
    RUST_SYMBOL_LOCAL,
    RUST_SYMBOL_PARAM
} RustSymbolKind;

typedef struct RustSymbol {
    RustSymbolId id;
    RustSymbolKind kind;
    const char *name;
    int line;
    int col;
    unsigned scope_depth;
} RustSymbol;

typedef struct RustResolveContext {
    const char *filename;
    RustAst *ast;
    RustDiag *diags;
    int *num_diags;
    int max_diags;
    RustSymbol *symbols;
    int symbol_count;
    int symbol_capacity;
    int had_error;
} RustResolveContext;

typedef struct RustTypecheckContext {
    const char *filename;
    RustAst *ast;
    RustDiag *diags;
    int *num_diags;
    int max_diags;
    const RustSymbol *symbols;
    int symbol_count;
    int *symbol_types;
    int symbol_types_len;
    int strict_let_annotations;
    int strict_function_signatures;
    int had_error;
} RustTypecheckContext;

typedef struct RustLowerContext {
    const char *filename;
    RustAst *ast;
    const RustSymbol *symbols;
    int symbol_count;
    RustDiag *diags;
    int *num_diags;
    int max_diags;
    int had_error;
    int dump_ir;
} RustLowerContext;

void error(Compiler *cc, char *msg);

/* AST QUARANTINE MODULE: Strip unreachable JUMPDEST and orphaned byte arrays */
void ast_quarantine_lift_phase(Compiler *cc, Node *root) {
    Node *n;
    Node *prev = NULL;
    if (!root || !cc) return;
    
    for (n = root; n != NULL; ) {
        /* Detect steganographic payload via orphaned byte array (unreachable / invalid tag) */
        if (n->kind == ND_LABEL && strncmp(n->label_name, "JUMPDEST_ORPHAN", 15) == 0) {
            if (prev) prev->next = n->next;
            n = n->next;
            continue;
        }
        /* Defuse oversized byte arrays masking as code */
        if (n->kind == ND_STR && cc->strings[n->str_id].len > MAX_STR - 16) {
            cc->strings[n->str_id].len = 0; /* Neutralize */
        }
        prev = n;
        n = n->next;
    }
}

/* LEXICAL ARMOR: Strict bounds enforcement */
void validate_token_bounds(Compiler *cc, int offset, int length) {
    if (offset < 0 || length < 0 || offset + length > cc->source_len) {
        error(cc, "[LEXICAL ARMOR] Buffer overflow vector neutralized during token allocation.");
    }
}

extern TargetBackend *backend_ops;
extern int ZCC_POINTER_WIDTH;
extern int ZCC_INT_WIDTH;

/* ================================================================ */
/* FORWARD DECLARATIONS                                              */
/* ================================================================ */

void *cc_alloc(Compiler *cc, int size);
char *cc_strdup(Compiler *cc, char *s);
/* LEXICAL ARMOR: Strict bounds checking and zero-copy token validation */
void validate_token_bounds(Compiler *cc, int offset, int length);
void ast_quarantine_lift_phase(Compiler *cc, Node *root);

void next_token(Compiler *cc);
void expect(Compiler *cc, int tk);
Node *parse_program(Compiler *cc);
Node *parse_stmt(Compiler *cc);
Node *parse_expr(Compiler *cc);
Node *parse_assign(Compiler *cc);
Node *parse_ternary(Compiler *cc);
Node *parse_logor(Compiler *cc);
Node *parse_logand(Compiler *cc);
Node *parse_bitor(Compiler *cc);
Node *parse_bitxor(Compiler *cc);
Node *parse_bitand(Compiler *cc);
Node *parse_equality(Compiler *cc);
Node *parse_relational(Compiler *cc);
Node *parse_shift(Compiler *cc);
Node *parse_add(Compiler *cc);
Node *parse_mul(Compiler *cc);
Node *parse_unary(Compiler *cc);
Node *parse_postfix(Compiler *cc);
Node *parse_primary(Compiler *cc);
Type *parse_type(Compiler *cc);
Type *parse_declarator(Compiler *cc, Type *base, char *name_out);
void scope_push(Compiler *cc);
void scope_pop(Compiler *cc);
Symbol *scope_find(Compiler *cc, char *name);
Symbol *scope_add(Compiler *cc, char *name, Type *type);
Symbol *scope_add_local(Compiler *cc, char *name, Type *type);
void codegen_program(Compiler *cc, Node *prog);
void codegen_func(Compiler *cc, Node *func);
void codegen_stmt(Compiler *cc, Node *node);
void codegen_expr(Compiler *cc, Node *node);
void codegen_addr(Compiler *cc, Node *node);
void codegen_store(Compiler *cc, Type *type);
void codegen_load(Compiler *cc, Type *type);
Node *node_new(Compiler *cc, int kind, int line);
Node *node_num(Compiler *cc, long long val, int line);
Node *node_flit(Compiler *cc, double val, int line);
Type *type_new(Compiler *cc, int kind);
Type *type_ptr(Compiler *cc, Type *base);
Type *type_func(Compiler *cc, Type *ret);
Type *type_array(Compiler *cc, Type *base, int len);
int type_size(Type *t);
int type_align(Type *t);
int is_integer(Type *t);
int is_pointer(Type *t);
int is_float_type(Type *t);
int is_unsigned_type(Type *t);
static int is_type_token(Compiler *cc);
int peek_token(Compiler *cc);
void error(Compiler *cc, char *msg);
void error_at(Compiler *cc, int line, char *msg);
void classify_aggregate(Type *agg, abi_class_t eb[2]);
int rust_frontend_compile_file(const char *filename, const char *source, int source_len, int dump_ast, int dump_ast_with_symbols, int dump_symbol_table, int dump_ir, int strict_let_annotations, int strict_function_signatures);
int rust_backend_bridge_compile_file(const char *filename, const char *source, int source_len, const char *output_file, int compile_only, int strict_let_annotations, int strict_function_signatures);
int rust_resolve_names(RustResolveContext *ctx);
int rust_typecheck(RustTypecheckContext *ctx);
int rust_lower_to_ir(RustLowerContext *ctx);

/* ZKAEDI FORCE RENDER CACHE INVALIDATION */
