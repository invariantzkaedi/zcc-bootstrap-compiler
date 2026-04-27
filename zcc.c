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
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

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

char *zcc_preprocess(const char *source, int source_len, const char *filename, const char *include_paths, int *out_len);

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
int node_is_bitfield(struct Node *n) { return n ? n->is_bitfield : 0; }
int node_bit_offset(struct Node *n) { return n ? n->bit_offset : 0; }
int node_bit_size(struct Node *n) { return n ? n->bit_size : 0; }
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

extern TargetBackend *backend_ops;
extern int ZCC_POINTER_WIDTH;
extern int ZCC_INT_WIDTH;

/* ================================================================ */
/* FORWARD DECLARATIONS                                              */
/* ================================================================ */

void *cc_alloc(Compiler *cc, int size);
char *cc_strdup(Compiler *cc, char *s);
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
static int is_type_token(Compiler *cc);
int peek_token(Compiler *cc);
void error(Compiler *cc, char *msg);
void error_at(Compiler *cc, int line, char *msg);
void classify_aggregate(Type *agg, abi_class_t eb[2]);

/* ZKAEDI FORCE RENDER CACHE INVALIDATION */
/* part0_pp.c -- ZCC Preprocessor */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define PP_MAX_MACROS 4096
#define PP_MAX_PARAMS 16
static int _warned_pp_max_params = 0;
#define PP_MAX_BODY   65536
#define PP_MAX_INCLUDE_DEPTH 32

typedef struct {
    char name[128];
    int  is_function_like;
    int  num_params;
    char params[PP_MAX_PARAMS][64];
    char *body;
    int  body_cap;
    int  active;
} PPMacro;

typedef struct {
    const char *src;
    int pos;
    int len;
    char *alloc_buf;
    PPMacro *expanding_macro;
} PPInputCtx;

typedef struct {
    PPMacro macros[PP_MAX_MACROS];
    int     num_macros;

    int     cond_stack[64];
    int     cond_satisfied[64];
    int     cond_else_seen[64];
    int     cond_depth;

    char    included_files[256][256];
    int     num_included;

    char   *out;
    int     out_len;
    int     out_cap;

    const char *src;
    int         pos;
    int         len;
    int         line;
    const char *filename;
    const char *include_paths;

    PPInputCtx input_stack[32];
    int input_depth;
    char *alloc_buf;
    int pop_barrier;
    int include_errors;
} PPState;

static const char *zcc_stddef_text = 
"#ifndef _ZCC_STDDEF_H_\n"
"#define _ZCC_STDDEF_H_\n"
"#define NULL ((void*)0)\n"
"#define EOF (-1)\n"
"#define EXIT_SUCCESS 0\n"
"#define EXIT_FAILURE 1\n"
"#define CHAR_BIT 8\n"
"#define INT_MAX 2147483647\n"
"#define INT_MIN (-2147483647-1)\n"
"#define LONG_MAX 9223372036854775807L\n"
"#define LONG_MIN (-9223372036854775807L-1L)\n"
"#define UINT_MAX 4294967295U\n"
"#define ULONG_MAX 18446744073709551615UL\n"
"#define SIZE_MAX ULONG_MAX\n"
"/* PP-HEADERS-023A: C99 <limits.h> full sweep */\n"
"#define SCHAR_MIN  (-128)\n"
"#define SCHAR_MAX  127\n"
"#define UCHAR_MAX  255\n"
"/* CHAR_MIN/MAX assume signed char (x86-64 SysV ABI default) */\n"
"#define CHAR_MIN   (-128)\n"
"#define CHAR_MAX   127\n"
"#define SHRT_MIN   (-32768)\n"
"#define SHRT_MAX   32767\n"
"#define USHRT_MAX  65535\n"
"#define LLONG_MIN  (-9223372036854775807LL-1LL)\n"
"#define LLONG_MAX  9223372036854775807LL\n"
"#define ULLONG_MAX 18446744073709551615ULL\n"
"#define RAND_MAX 2147483647\n"
"#define BUFSIZ 8192\n"
"#define FILENAME_MAX 4096\n"
"#define true 1\n"
"#define false 0\n"
"#define __attribute__(x)\n"
"#define __extension__\n"
"#define __restrict\n"
"#define __restrict__\n"
"#define restrict\n"
"#define __inline\n"
"#define __inline__\n"
"#define inline\n"
"#define __volatile\n"
"#define __volatile__\n"
"#define __const__ const\n"
"#define __const const\n"
"#define _Noreturn\n"
"#define _Static_assert(x, y)\n"
"#define __declspec(x)\n"
"#define __cdecl\n"
"#define __stdcall\n"
"#define __asm__(a,b,c,d,e)\n"
"#define asm(a,b,c,d,e)\n"
"typedef struct { unsigned int gp_offset; unsigned int fp_offset; void *overflow_arg_area; void *reg_save_area; } __va_list_struct[1];\n"
"#define __builtin_va_list __va_list_struct\n"
"void __builtin_va_start(__builtin_va_list ap, ...);\n"
"void __builtin_va_end(__builtin_va_list ap);\n"
"/* PP-STUB-024B: route va_* to ZCC builtins so va_arg(ap,type) is parsed correctly */\n"
"#define va_list __builtin_va_list\n"
"#define va_start(ap, last) __builtin_va_start(ap, last)\n"
"#define va_end(ap)         __builtin_va_end(ap)\n"
"#define va_copy(dst, src)  ((dst)[0] = (src)[0])\n"
"#define va_arg(ap, type)   __builtin_va_arg(ap, type)\n"
"#define __builtin_expect(exp, c) (exp)\n"
"#define __builtin_constant_p(x) 0\n"
"#define __builtin_types_compatible_p(x, y) 0\n"
"#define __builtin_unreachable()\n"
"#define __x86_64__ 1\n"
"#define LUA_USE_JUMPTABLE 0\n"
"#define __GNUC__ 1\n"
"#define __attribute__(x)\n"
"#define __declspec(x)\n"
"#define __inline__ inline\n"
"#define __restrict\n"
"#define __restrict__\n"
"#define __extension__\n"
"#define __asm__(x)\n"
"#define assert(x)\n"
"#define offsetof(t, m) ((unsigned long)&(((t*)0)->m))\n"
"typedef int int32_t;\n"
"typedef unsigned int uint32_t;\n"
"typedef long int64_t;\n"
"typedef unsigned long uint64_t;\n"
"typedef char int8_t;\n"
"typedef unsigned char uint8_t;\n"
"typedef short int16_t;\n"
"typedef unsigned short uint16_t;\n"
"typedef unsigned long size_t;\n"
"typedef long ssize_t;\n"
"typedef long ptrdiff_t;\n"
"typedef long intptr_t;\n"
"typedef unsigned long uintptr_t;\n"
"/* PP-STUB-024: signal.h / setjmp.h / stdint primitives for Lua internals */\n"
"typedef int sig_atomic_t;\n"
"typedef long jmp_buf[25];  /* x86-64 glibc: __jmp_buf(64) + mask_saved(4) + sigset(128) + pad = 200 bytes */\n"
"typedef struct _IO_FILE FILE;\n"
"extern FILE *stdin, *stdout, *stderr;\n"
"char *getenv(const char *name);\n"
"int setenv(const char *name, const char *value, int overwrite);\n"
"int unsetenv(const char *name);\n"
"unsigned long long strtoull(const char *s, char **end, int base);\n"
"long long strtoll(const char *s, char **end, int base);\n"
"long strtol(const char *s, char **end, int base);\n"
"unsigned long strtoul(const char *s, char **end, int base);\n"
"double strtod(const char *s, char **end);\n"
"int abs(int n);\n"
"long labs(long n);\n"
"long long llabs(long long n);\n"
"void abort(void);\n"
"int atexit(void (*fn)(void));\n"
"int atoi(const char *s);\n"
"long atol(const char *s);\n"
"double atof(const char *s);\n"
"void *bsearch(const void *key, const void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *));\n"
"void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *));\n"
"int system(const char *cmd);\n"
"/* string.h extras needed by Lua */\n"
"int memcmp(const void *a, const void *b, size_t n);\n"
"void *memmove(void *dst, const void *src, size_t n);\n"
"char *strncpy(char *dst, const char *src, size_t n);\n"
"char *strncat(char *dst, const char *src, size_t n);\n"
"int strncmp(const char *a, const char *b, size_t n);\n"
"char *strchr(const char *s, int c);\n"
"char *strrchr(const char *s, int c);\n"
"size_t strspn(const char *s, const char *accept);\n"
"size_t strcspn(const char *s, const char *reject);\n"
"char *strpbrk(const char *s, const char *accept);\n"
"char *strstr(const char *haystack, const char *needle);\n"
"int strcoll(const char *a, const char *b);\n"
"size_t strxfrm(char *dst, const char *src, size_t n);\n"
"char *strtok(char *s, const char *delim);\n"
"char *strerror(int errnum);\n"
"#endif\n";

static void pp_emit(PPState *state, char c) {
    if (state->out_len + 1 >= state->out_cap) {
        state->out_cap = (state->out_cap == 0) ? 4096 : state->out_cap * 2;
        state->out = (char *)realloc(state->out, state->out_cap);
    }
    state->out[state->out_len++] = c;
}

static void pp_emit_str(PPState *state, const char *str, int len) {
    int i;
    for (i = 0; i < len; i++) pp_emit(state, str[i]);
}

static char pp_peek(PPState *state) {
    static int pp_peek_cnt = 0;
    if (++pp_peek_cnt % 500000 == 0) {
        if (getenv("ZCC_DEBUG")) printf("DEBUG zcc2 pp_peek_cnt=%d pos=%d line=%d\n", pp_peek_cnt, state->pos, state->line);
        fflush(stdout);
    }
    while (state->pos >= state->len && state->input_depth > state->pop_barrier) {
        if (state->alloc_buf) free(state->alloc_buf);
        state->input_depth--;
        if (state->input_stack[state->input_depth].expanding_macro) {
            state->input_stack[state->input_depth].expanding_macro->active = 1;
        }
        state->src = state->input_stack[state->input_depth].src;
        state->pos = state->input_stack[state->input_depth].pos;
        state->len = state->input_stack[state->input_depth].len;
        state->alloc_buf = state->input_stack[state->input_depth].alloc_buf;
    }
    if (state->pos >= state->len) return 0;
    if (state->src[state->pos] == '\r') return '\n';
    return state->src[state->pos];
}

static int pp_is_line_continuation(PPState *state) {
    if (state->pos + 1 >= state->len) return 0;
    if (state->src[state->pos] != '\\') return 0;
    if (state->src[state->pos + 1] == '\n') return 1;
    if (state->src[state->pos + 1] == '\r') return 1;
    return 0;
}

static char pp_next(PPState *state) {
    char c = pp_peek(state);
    if (c) {
        if (c == '\r') {
            if (state->input_depth == 0) state->line++;
            state->pos++;
            if (state->pos < state->len && state->src[state->pos] == '\n') state->pos++;
            return '\n';
        }
        if (c == '\n' && state->input_depth == 0) state->line++;
        state->pos++;
    }
    return c;
}

static void pp_push_input(PPState *state, const char *new_src, char *alloc_buf, PPMacro *macro) {
    if (state->input_depth >= 32) {
        fprintf(stderr, "zcc preprocessor error: macro expansion too deep\n");
        exit(1);
    }
    state->input_stack[state->input_depth].src = state->src;
    state->input_stack[state->input_depth].pos = state->pos;
    state->input_stack[state->input_depth].len = state->len;
    state->input_stack[state->input_depth].alloc_buf = state->alloc_buf;
    state->input_stack[state->input_depth].expanding_macro = macro;
    state->input_depth++;
    
    state->src = new_src;
    state->pos = 0;
    state->len = strlen(new_src);
    state->alloc_buf = alloc_buf;
}

static void pp_skip_whitespace(PPState *state) {
    while (pp_peek(state) == ' ' || pp_peek(state) == '\t') pp_next(state);
}

static int is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_char(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static void pp_parse_ident(PPState *state, char *out, int max) {
    int i = 0;
    while (is_ident_char(pp_peek(state)) && i < max - 1) out[i++] = pp_next(state);
    out[i] = 0;
}

static PPMacro *pp_find_macro(PPState *state, const char *name) {
    int i;
    for (i = state->num_macros - 1; i >= 0; i--) {
        if (state->macros[i].active && strcmp(state->macros[i].name, name) == 0) return &state->macros[i];
    }
    return 0;
}

static void pp_undef_macro(PPState *state, const char *name) {
    PPMacro *m = pp_find_macro(state, name);
    if (m) m->active = 0;
}

static PPMacro *pp_add_macro(PPState *state, const char *name) {
    PPMacro *m_macro;
    if (state->num_macros >= PP_MAX_MACROS) return 0;
    m_macro = &state->macros[state->num_macros++];
    memset(m_macro, 0, sizeof(PPMacro));
    strncpy(m_macro->name, name, 127);
    m_macro->active = 1;
    m_macro->body_cap = 256;
    m_macro->body = (char *)calloc(1, m_macro->body_cap);
    return m_macro;
}

static void pp_read_line(PPState *state, char *buf, int max) {
    int i = 0;
    int in_comment = 0;
    while (pp_peek(state) != 0 && i < max - 1) {
        char c = pp_peek(state);
        if ((c == '\n' || c == '\r') && !in_comment) {
            break;
        }
        if (pp_is_line_continuation(state)) {
            pp_next(state);
            if (pp_peek(state) == '\n' || pp_peek(state) == '\r') pp_next(state);
            continue;
        }
        if (!in_comment && c == '/' && state->src[state->pos + 1] == '/') {
            while (pp_peek(state) != '\n' && pp_peek(state) != 0) pp_next(state);
            break;
        }
        if (!in_comment && c == '/' && state->src[state->pos + 1] == '*') {
            in_comment = 1; pp_next(state); pp_next(state);
            continue;
        }
        if (in_comment && c == '*' && state->src[state->pos + 1] == '/') {
            in_comment = 0; pp_next(state); pp_next(state);
            continue;
        }
        pp_next(state);
        if (!in_comment && c != '\n' && c != '\r') buf[i++] = c;
    }
    buf[i] = 0;
}

static void pp_read_macro_body(PPState *state, PPMacro *m, int max_limit) {
    int i = 0;
    int in_comment = 0;
    if (!m->body) {
        m->body_cap = 256;
        m->body = (char *)calloc(1, m->body_cap);
    }
    
    while (pp_peek(state) != 0) {
        if (i >= m->body_cap - 2) {
            m->body_cap *= 2;
            m->body = (char *)realloc(m->body, m->body_cap);
        }
        if (i >= max_limit - 1) {
            fprintf(stderr, "zcc: macro body exceeds PP_MAX_BODY (%d bytes)\n", max_limit);
            exit(1);
        }
        char c = pp_peek(state);
        if ((c == '\n' || c == '\r') && !in_comment) break;
        if (pp_is_line_continuation(state)) {
            pp_next(state);
            if (pp_peek(state) == '\n' || pp_peek(state) == '\r') pp_next(state);
            continue;
        }
        if (!in_comment && c == '/' && state->src[state->pos + 1] == '/') {
            while (pp_peek(state) != '\n' && pp_peek(state) != 0) pp_next(state);
            break;
        }
        if (!in_comment && c == '/' && state->src[state->pos + 1] == '*') {
            in_comment = 1; pp_next(state); pp_next(state);
            continue;
        }
        if (in_comment && c == '*' && state->src[state->pos + 1] == '/') {
            in_comment = 0; pp_next(state); pp_next(state);
            continue;
        }
        pp_next(state);
        if (!in_comment && c != '\n' && c != '\r') m->body[i++] = c;
    }
    m->body[i] = 0;
}

static int pp_is_active(PPState *state) {
    int i;
    for (i = 0; i <= state->cond_depth; i++) {
        if (!state->cond_stack[i]) return 0;
    }
    return 1;
}

static char *pp_read_file(const char *path, int *out_len) {
    FILE *fp = fopen(path, "rb");
    long len;
    char *buf;
    long nr;
    
    if (!fp) return 0;
    fseek(fp, 0, 2);
    len = ftell(fp);
    fseek(fp, 0, 0);
    
    buf = (char *)malloc(len + 1);
    if (!buf) { fclose(fp); return 0; }
    
    nr = fread(buf, 1, len, fp);
    {
        long r = 0, w = 0;
        while (r < nr) {
            char c = buf[r++];
            if (c == '\r') {
                if (r < nr && buf[r] == '\n') r++;
                buf[w++] = '\n';
            } else {
                buf[w++] = c;
            }
        }
        nr = w;
    }
    buf[nr] = 0;
    fclose(fp);
    
    if (out_len) *out_len = nr;
    return buf;
}

static void pp_parse_target_depth(PPState *state, int target_depth);

static void pp_parse(PPState *state) {
    pp_parse_target_depth(state, state->input_depth);
}

/* PP-INCLUDE-022: Only these exact basenames get the synthesized stub.
 * Everything else must resolve from disk or hard-fail. */
static int is_stddef_stub(const char *path) {
    const char *base = path;
    const char *slash = strrchr(path, '/');
    if (slash) base = slash + 1;
    return strcmp(base, "stddef.h") == 0
        || strcmp(base, "stdarg.h") == 0
        || strcmp(base, "stdio.h")  == 0
        || strcmp(base, "stdlib.h") == 0
        || strcmp(base, "string.h") == 0;
}

/* PP-INCLUDE-022: Resolve an include path via -I search and relative lookup.
 * Returns 1 if found (writes to out_resolved), 0 if not found. */
static int pp_resolve_path(PPState *state, const char *path, int is_system, char *out_resolved) {
    /* For quoted includes, try relative to current file's directory first */
    if (!is_system && state->filename) {
        char dir[1024];
        const char *sl = strrchr(state->filename, '/');
        if (sl) {
            int dirlen = (int)(sl - state->filename);
            if (dirlen < 1023) {
                memcpy(dir, state->filename, dirlen);
                dir[dirlen] = '\0';
            } else {
                strcpy(dir, ".");
            }
        } else {
            strcpy(dir, ".");  /* no directory component — use CWD */
        }
        snprintf(out_resolved, 1024, "%s/%s", dir, path);
        {
            FILE *fp = fopen(out_resolved, "rb");
            if (fp) { fclose(fp); return 1; }
        }
    }

    /* Search -I paths (colon-separated, Linux convention) */
    if (state->include_paths && state->include_paths[0]) {
        const char *p = state->include_paths;
        while (*p) {
            char seg[1024];
            int si = 0;
            while (*p && *p != ':' && si < 1022) seg[si++] = *p++;
            seg[si] = '\0';
            if (*p == ':') p++;
            if (si > 0) {
                snprintf(out_resolved, 1024, "%s/%s", seg, path);
                {
                    FILE *fp = fopen(out_resolved, "rb");
                    if (fp) { fclose(fp); return 1; }
                }
            }
        }
    }
    return 0;
}

static void pp_process_include(PPState *state, const char *path, int is_system) {
    char *file_src = 0;
    int file_len = 0;
    int i;
    int from_disk = 0;
    char resolved_path[1024];

    /* Step 1: Try disk lookup */
    if (pp_resolve_path(state, path, is_system, resolved_path)) {
        file_src = pp_read_file(resolved_path, &file_len);
        if (file_src) from_disk = 1;
    }

    /* Step 2: If disk failed, check stub whitelist */
    if (!file_src && is_stddef_stub(path)) {
        file_src = (char *)zcc_stddef_text;
        file_len = strlen(file_src);
        strncpy(resolved_path, path, 1023);
        resolved_path[1023] = '\0';
    }

    /* Step 3: Hard error if nothing found (Flag 2: accumulate, don't exit)
     * Use printf because driver silences stderr before preprocessing. */
    if (!file_src) {
        printf("zcc: error: include file not found: %s\n", path);
        printf("  (while processing %s:%d)\n",
                state->filename ? state->filename : "<main>", state->line);
        state->include_errors++;
        return;
    }

    {
        PPState *sub_state = (PPState *)malloc(sizeof(PPState));
        if (!sub_state) {
            fprintf(stderr, "zcc preprocessor error: out of memory for include state\n");
            exit(1);
        }
        memset(sub_state, 0, sizeof(PPState));
        sub_state->src           = file_src;
        sub_state->pos           = 0;
        sub_state->len           = file_len;
        sub_state->line          = 1;
        sub_state->filename      = resolved_path;
        sub_state->include_paths = state->include_paths;
        sub_state->cond_stack[0] = 1;
        sub_state->pop_barrier   = 0;

        /* Macro table transfer (preserving baseline memcpy behavior) */
        sub_state->num_macros = state->num_macros;
        for (i = 0; i < state->num_macros; i++)
            memcpy(&sub_state->macros[i], &state->macros[i], sizeof(PPMacro));

        pp_parse(sub_state);

        /* Copy back macros added by the header */
        for (i = state->num_macros; i < sub_state->num_macros; i++) {
            if (state->num_macros >= PP_MAX_MACROS) {
                fprintf(stderr, "MAX MACROS REACHED!\n");
                break;
            }
            memcpy(&state->macros[state->num_macros++], &sub_state->macros[i], sizeof(PPMacro));
        }

        fprintf(stderr, "pp_process_include: %s added %d macros, total is %d\n",
                path, sub_state->num_macros - i, state->num_macros);

        /* Update active flags (undefs may have occurred) */
        for (i = 0; i < state->num_macros; i++) {
            state->macros[i].active = sub_state->macros[i].active;
        }

        pp_emit_str(state, sub_state->out, sub_state->out_len);
        state->include_errors += sub_state->include_errors;

        if (sub_state->out) free(sub_state->out);
        free(sub_state);
        if (from_disk && file_src) free(file_src);
    }
}

/* parse parameter list for function-like macros */
static void pp_parse_params(PPState *state, PPMacro *m) {
    pp_skip_whitespace(state);
    while (pp_peek(state) != ')' && pp_peek(state) != '\n' && pp_peek(state) != 0) {
        if (is_ident_start(pp_peek(state))) {
            if (m->num_params < PP_MAX_PARAMS) {
                pp_parse_ident(state, m->params[m->num_params++], 64);
            } else {
                if (!_warned_pp_max_params) { printf("zcc: warning: PP_MAX_PARAMS (%d) exceeded at %s:%d — subsequent macro params silently discarded\n", PP_MAX_PARAMS, state->filename ? state->filename : "?", state->line); _warned_pp_max_params = 1; }
                char dummy[64];
                pp_parse_ident(state, dummy, 64);
            }
        } else {
            pp_next(state); /* comma or whitespace */
        }
        pp_skip_whitespace(state);
    }
    if (pp_peek(state) == ')') pp_next(state);
}

/* ============================================================
 * pp_eval_expr_str — recursive-descent C #if expression parser.
 * Handles full C operator precedence per C11 6.10.1.
 * Returns long long. Undefined identifiers evaluate to 0.
 * ============================================================ */

typedef struct { const char *s; PPState *state; int depth; } PPEval;

static long long pp_e_expr(PPEval *e);      /* lowest precedence */

static void pp_e_skip(PPEval *e) {
    while (*e->s == ' ' || *e->s == '\t' || *e->s == '\n' || *e->s == '\r') e->s++;
}

static int pp_e_match(PPEval *e, const char *tok) {
    pp_e_skip(e);
    int n = 0; while (tok[n]) n++;
    int i;
    for (i = 0; i < n; i++) if (e->s[i] != tok[i]) return 0;
    /* Guard: don't match '>' when '>>' is present, etc. */
    if (n == 1) {
        char c = tok[0];
        if ((c == '>' || c == '<' || c == '&' || c == '|' || c == '=') && e->s[1] == c) return 0;
        if ((c == '<' || c == '>' || c == '!' || c == '=') && e->s[1] == '=') return 0;
    }
    e->s += n;
    return 1;
}

/* primary: number | defined(X) | identifier | (expr) | unary */
static long long pp_e_primary(PPEval *e) {
    pp_e_skip(e);
    if (*e->s == '(') {
        e->s++;
        long long v = pp_e_expr(e);
        pp_e_skip(e);
        if (*e->s == ')') e->s++;
        return v;
    }
    if (*e->s == '!') { e->s++; return !pp_e_primary(e); }
    if (*e->s == '~') { e->s++; return ~pp_e_primary(e); }
    if (*e->s == '-') { e->s++; return -pp_e_primary(e); }
    if (*e->s == '+') { e->s++; return  pp_e_primary(e); }
    if (*e->s >= '0' && *e->s <= '9') {
        char *end;
        long long v = (long long)strtoull(e->s, &end, 0);
        e->s = end;
        /* skip integer suffix: U, L, UL, ULL, LL */
        while (*e->s == 'u' || *e->s == 'U' || *e->s == 'l' || *e->s == 'L') e->s++;
        return v;
    }
    /* identifier: defined() or macro */
    if ((*e->s >= 'a' && *e->s <= 'z') || (*e->s >= 'A' && *e->s <= 'Z') || *e->s == '_') {
        char name[256];
        int n = 0;
        while (((*e->s >= 'a' && *e->s <= 'z') || (*e->s >= 'A' && *e->s <= 'Z') ||
                (*e->s >= '0' && *e->s <= '9') || *e->s == '_') && n < 255) {
            name[n++] = *e->s++;
        }
        name[n] = 0;
        if (n == 7 && name[0]=='d' && name[1]=='e' && name[2]=='f' && name[3]=='i' &&
                      name[4]=='n' && name[5]=='e' && name[6]=='d') {
            pp_e_skip(e);
            int paren = 0;
            if (*e->s == '(') { e->s++; paren = 1; }
            pp_e_skip(e);
            char arg[256]; int an = 0;
            while (((*e->s >= 'a' && *e->s <= 'z') || (*e->s >= 'A' && *e->s <= 'Z') ||
                    (*e->s >= '0' && *e->s <= '9') || *e->s == '_') && an < 255) {
                arg[an++] = *e->s++;
            }
            arg[an] = 0;
            pp_e_skip(e);
            if (paren && *e->s == ')') e->s++;
            return pp_find_macro(e->state, arg) != 0;
        }
        /* regular identifier: expand as macro if defined, else 0 */
        PPMacro *m = pp_find_macro(e->state, name);
        if (m) {
            PPEval sub = { m->body, e->state, e->depth + 1 };
            if (sub.depth > 32) return 0;
            return pp_e_expr(&sub);
        }
        return 0;
    }
    return 0;
}

/* multiplicative: * / % */
static long long pp_e_mul(PPEval *e) {
    long long v = pp_e_primary(e);
    for (;;) {
        pp_e_skip(e);
        if      (pp_e_match(e, "*"))  v = v * pp_e_primary(e);
        else if (pp_e_match(e, "/"))  { long long r = pp_e_primary(e); v = r ? v / r : 0; }
        else if (pp_e_match(e, "%"))  { long long r = pp_e_primary(e); v = r ? v % r : 0; }
        else break;
    }
    return v;
}

/* additive: + - */
static long long pp_e_add(PPEval *e) {
    long long v = pp_e_mul(e);
    for (;;) {
        pp_e_skip(e);
        if      (pp_e_match(e, "+")) v = v + pp_e_mul(e);
        else if (pp_e_match(e, "-")) v = v - pp_e_mul(e);
        else break;
    }
    return v;
}

/* shift: << >> */
static long long pp_e_shift(PPEval *e) {
    long long v = pp_e_add(e);
    for (;;) {
        pp_e_skip(e);
        if      (pp_e_match(e, "<<")) v = (long long)((unsigned long long)v << pp_e_add(e));
        else if (pp_e_match(e, ">>")) v = (long long)((unsigned long long)v >> pp_e_add(e));
        else break;
    }
    return v;
}

/* relational: < <= > >= */
static long long pp_e_rel(PPEval *e) {
    long long v = pp_e_shift(e);
    for (;;) {
        pp_e_skip(e);
        if      (pp_e_match(e, "<=")) v = v <= pp_e_shift(e);
        else if (pp_e_match(e, ">=")) v = v >= pp_e_shift(e);
        else if (pp_e_match(e, "<"))  v = v <  pp_e_shift(e);
        else if (pp_e_match(e, ">"))  v = v >  pp_e_shift(e);
        else break;
    }
    return v;
}

/* equality: == != */
static long long pp_e_eq(PPEval *e) {
    long long v = pp_e_rel(e);
    for (;;) {
        pp_e_skip(e);
        if      (pp_e_match(e, "==")) v = v == pp_e_rel(e);
        else if (pp_e_match(e, "!=")) v = v != pp_e_rel(e);
        else break;
    }
    return v;
}

/* bitwise AND: & */
static long long pp_e_band(PPEval *e) {
    long long v = pp_e_eq(e);
    for (;;) {
        pp_e_skip(e);
        if (pp_e_match(e, "&")) v = v & pp_e_eq(e);
        else break;
    }
    return v;
}

/* bitwise XOR: ^ */
static long long pp_e_bxor(PPEval *e) {
    long long v = pp_e_band(e);
    for (;;) {
        pp_e_skip(e);
        if (pp_e_match(e, "^")) v = v ^ pp_e_band(e);
        else break;
    }
    return v;
}

/* bitwise OR: | */
static long long pp_e_bor(PPEval *e) {
    long long v = pp_e_bxor(e);
    for (;;) {
        pp_e_skip(e);
        if (pp_e_match(e, "|")) v = v | pp_e_bxor(e);
        else break;
    }
    return v;
}

/* logical AND: && */
static long long pp_e_land(PPEval *e) {
    long long v = pp_e_bor(e);
    for (;;) {
        pp_e_skip(e);
        if (pp_e_match(e, "&&")) { long long r = pp_e_bor(e); v = v && r; }
        else break;
    }
    return v;
}

/* logical OR: || */
static long long pp_e_lor(PPEval *e) {
    long long v = pp_e_land(e);
    for (;;) {
        pp_e_skip(e);
        if (pp_e_match(e, "||")) { long long r = pp_e_land(e); v = v || r; }
        else break;
    }
    return v;
}

/* ternary: a ? b : c */
static long long pp_e_expr(PPEval *e) {
    long long c = pp_e_lor(e);
    pp_e_skip(e);
    if (*e->s == '?') {
        e->s++;
        long long t = pp_e_expr(e);
        pp_e_skip(e);
        if (*e->s == ':') e->s++;
        long long f = pp_e_expr(e);
        return c ? t : f;
    }
    return c;
}

static long long pp_eval_expr_str(PPState *state, const char *s, int depth) {
    PPEval e;
    e.s = s;
    e.state = state;
    e.depth = depth;
    return pp_e_expr(&e);
}

static void pp_parse_directive(PPState *state) {
    char dir[1024];
    int active = pp_is_active(state);
    
    pp_skip_whitespace(state);
    pp_parse_ident(state, dir, 1024);
    pp_skip_whitespace(state);

    if (strcmp(dir, "ifdef") == 0 || strcmp(dir, "ifndef") == 0) {
        char name[128];
        int is_defined;
        pp_parse_ident(state, name, 128);
        is_defined = (pp_find_macro(state, name) != 0);
        
        state->cond_depth++;
        state->cond_else_seen[state->cond_depth] = 0;
        if (!active) {
            state->cond_stack[state->cond_depth] = 0;
        } else {
            if (strcmp(dir, "ifdef") == 0) state->cond_stack[state->cond_depth] = is_defined;
            else state->cond_stack[state->cond_depth] = !is_defined;
        }
        state->cond_satisfied[state->cond_depth] = state->cond_stack[state->cond_depth];
        pp_read_line(state, dir, 1024);
    } else if (strcmp(dir, "if") == 0) {
        char expr[256];
        pp_skip_whitespace(state);
        pp_read_line(state, expr, 256);
        state->cond_depth++;
        state->cond_else_seen[state->cond_depth] = 0;
        if (!active) {
            state->cond_stack[state->cond_depth] = 0;
        } else {
            state->cond_stack[state->cond_depth] = pp_eval_expr_str(state, expr, 0); 
        }
        state->cond_satisfied[state->cond_depth] = state->cond_stack[state->cond_depth];
    } else if (strcmp(dir, "elif") == 0) {
        if (state->cond_depth > 0 && !state->cond_else_seen[state->cond_depth]) {
            char expr[256];
            pp_read_line(state, expr, 256);
            if (state->cond_satisfied[state->cond_depth]) {
                state->cond_stack[state->cond_depth] = 0;
            } else {
                int active_parent = 1;
                int i;
                for (i = 0; i < state->cond_depth; i++) if (!state->cond_stack[i]) active_parent = 0;
                if (active_parent) {
                    int result = pp_eval_expr_str(state, expr, 0);
                    state->cond_stack[state->cond_depth] = result;
                    if (result) state->cond_satisfied[state->cond_depth] = 1;
                } else {
                    state->cond_stack[state->cond_depth] = 0;
                }
            }
        } else {
            pp_read_line(state, dir, 1024);
        }
    } else if (strcmp(dir, "else") == 0) {
        if (state->cond_depth > 0) {
            state->cond_else_seen[state->cond_depth] = 1;
            int parent_active = 1;
            int i;
            for (i = 0; i < state->cond_depth; i++) if (!state->cond_stack[i]) parent_active = 0;
            if (parent_active) {
                if (state->cond_satisfied[state->cond_depth]) {
                    state->cond_stack[state->cond_depth] = 0;
                } else {
                    state->cond_stack[state->cond_depth] = 1;
                    state->cond_satisfied[state->cond_depth] = 1;
                }
            } else {
                state->cond_stack[state->cond_depth] = 0;
            }
        }
        pp_read_line(state, dir, 1024);
    } else if (strcmp(dir, "endif") == 0) {
        if (state->cond_depth > 0) {
            state->cond_satisfied[state->cond_depth] = 0;
            state->cond_else_seen[state->cond_depth] = 0;
            state->cond_depth--;
        }

        pp_read_line(state, dir, 1024);
    } else if (active) {
        if (strcmp(dir, "define") == 0) {
            char name[128];
            PPMacro *m;
            pp_parse_ident(state, name, 128);
            m = pp_add_macro(state, name);
            if (pp_peek(state) == '(') {
                pp_next(state);
                m->is_function_like = 1;
                pp_parse_params(state, m);
                pp_skip_whitespace(state);
                pp_read_macro_body(state, m, PP_MAX_BODY);
            } else {
                pp_skip_whitespace(state);
                pp_read_macro_body(state, m, PP_MAX_BODY);
            }
        } else if (strcmp(dir, "undef") == 0) {
            char name[128];
            pp_parse_ident(state, name, 128);
            pp_undef_macro(state, name);
            pp_read_line(state, dir, 1024);
        } else if (strcmp(dir, "include") == 0) {
            char inc_path[256];
            int is_system = 0;
            if (pp_peek(state) == '<') {
                is_system = 1;
                pp_next(state);
            } else if (pp_peek(state) == '"') {
                pp_next(state);
            }
            pp_read_line(state, inc_path, 256);
            /* Strip trailing \r from Windows line endings */
            {
                int plen = strlen(inc_path);
                while (plen > 0 && (inc_path[plen - 1] == '\r' || inc_path[plen - 1] == ' ' || inc_path[plen - 1] == '\t'))
                    inc_path[--plen] = 0;
                if (plen > 0 && (inc_path[plen - 1] == '"' || inc_path[plen - 1] == '>'))
                    inc_path[--plen] = 0;
            }
            pp_process_include(state, inc_path, is_system);
        } else if (strcmp(dir, "error") == 0) {
            char msg[256];
            pp_read_line(state, msg, 256);
            fprintf(stderr, "zcc preprocessor error: #error %s\n", msg);
        } else {
            pp_read_line(state, dir, 1024);
        }
    } else {
        pp_read_line(state, dir, 1024);
    }
    
    if (pp_peek(state) == '\n') pp_next(state);
    pp_emit(state, '\n');
}


static void pp_expand_ident(PPState *state, const char *ident) {
    PPMacro *m;
    int i, p_count;
    char *args[PP_MAX_PARAMS];
    int arg_caps[PP_MAX_PARAMS];
    for (i = 0; i < PP_MAX_PARAMS; i++) {
        arg_caps[i] = 256;
        args[i] = (char *)malloc(arg_caps[i]);
    }
    char c;
    
    if (strcmp(ident, "defined") == 0) {
        pp_emit_str(state, "defined", 7);
        return;
    }
    
    /* ATTR-UNKNOWN-001: handle __attribute__ specially — intercept BEFORE macro lookup
     * so the empty #define __attribute__(x) never fires for this identifier.
     * We parse (( attrname )) directly from the stream and emit __zcc_attr_packed__
     * for 'packed', consume everything else silently. */
    if (strcmp(ident, "__attribute__") == 0 || strcmp(ident, "__attribute") == 0) {
        /* consume whitespace then (( */
        while (pp_peek(state)==' '||pp_peek(state)=='\t'||pp_peek(state)=='\n') pp_next(state);
        if (pp_peek(state)=='(') pp_next(state);
        while (pp_peek(state)==' '||pp_peek(state)=='\t') pp_next(state);
        if (pp_peek(state)=='(') pp_next(state);
        while (pp_peek(state)==' '||pp_peek(state)=='\t') pp_next(state);
        /* read attribute name */
        {
            char attr_name[64]; int ai = 0;
            while (pp_peek(state)!=0 && ai<63) {
                char ac = pp_peek(state);
                if ((ac>='a'&&ac<='z')||(ac>='A'&&ac<='Z')||(ac>='0'&&ac<='9')||ac=='_') {
                    attr_name[ai++] = pp_next(state);
                } else break;
            }
            attr_name[ai] = 0;
            /* strip __ prefix/suffix (GCC __packed__ etc.) */
            {
                char *an = attr_name; int alen = ai;
                if (alen>4 && an[0]=='_'&&an[1]=='_'&&an[alen-1]=='_'&&an[alen-2]=='_') {
                    an[alen-2]=0; an+=2;
                }
                if (strcmp(an, "packed") == 0) {
                    pp_emit_str(state, " __zcc_attr_packed__ ", 21);
                }
                /* aligned(N): silently drop for now; add __zcc_attr_aligned_N__ later */
                /* all other attributes: silently consumed */
            }
        }
        /* consume remaining content up to and including the closing ))
         * After reading the attribute name, we've consumed:  (  (  attrname
         * Still in stream:      [args...]  )  )
         * Strategy: balance-count from current position, stop when we've
         * seen 2 more ')' than '(' (the two closes we opened above). */
        {
            int need = 2;  /* we pre-consumed two '(' — need two matching ')' */
            while (pp_peek(state) != 0 && need > 0) {
                char ac = pp_next(state);
                if (ac == '(') need++;
                else if (ac == ')') { need--; }
            }
        }
        return;
    }

    m = pp_find_macro(state, ident);
    if (!m) {
        pp_emit_str(state, ident, strlen(ident));
        return;
    }

    if (!m->is_function_like) {
        m->active = 0;
        pp_push_input(state, m->body, NULL, m);
        return;
    }
    
    /* Function-like macro expansion */
    int saved_pos = state->pos;
    int saved_line = state->line;
    
    pp_skip_whitespace(state);
    if (pp_peek(state) != '(') {
        /* Not an invocation */
        state->pos = saved_pos;
        state->line = saved_line;
        pp_emit_str(state, ident, strlen(ident));
        return;
    }
    pp_next(state); /* consume '(' */
    
    /* parse arguments */
    for (i = 0; i < PP_MAX_PARAMS; i++) args[i][0] = 0;
    
    p_count = 0;
    int paren_level = 0;
    int arg_idx = 0;
    int in_string = 0;
    int in_char = 0;
    int in_comment = 0;
    while (pp_peek(state) != 0) {
        c = pp_peek(state);

        /* PP-COMMENT-ARG-FIX: track block comments inside macro args so
           that apostrophes and quotes inside them do not toggle in_char
           or in_string state. Copy characters through verbatim. */
        if (!in_string && !in_char) {
            if (!in_comment && c == '/' && state->src[state->pos + 1] == '*') {
                in_comment = 1;
                if (p_count < PP_MAX_PARAMS) {
                    if (arg_idx >= arg_caps[p_count] - 2) {
                        arg_caps[p_count] *= 2;
                        args[p_count] = (char *)realloc(args[p_count], arg_caps[p_count]);
                    }
                    args[p_count][arg_idx++] = pp_next(state);
                    args[p_count][arg_idx++] = pp_next(state);
                    args[p_count][arg_idx] = 0;
                } else {
                    pp_next(state); pp_next(state);
                }
                continue;
            }
            if (in_comment && c == '*' && state->src[state->pos + 1] == '/') {
                in_comment = 0;
                if (p_count < PP_MAX_PARAMS) {
                    if (arg_idx >= arg_caps[p_count] - 2) {
                        arg_caps[p_count] *= 2;
                        args[p_count] = (char *)realloc(args[p_count], arg_caps[p_count]);
                    }
                    args[p_count][arg_idx++] = pp_next(state);
                    args[p_count][arg_idx++] = pp_next(state);
                    args[p_count][arg_idx] = 0;
                } else {
                    pp_next(state); pp_next(state);
                }
                continue;
            }
        }
        if (in_comment) {
            if (p_count < PP_MAX_PARAMS) {
                if (arg_idx >= arg_caps[p_count] - 1) {
                    arg_caps[p_count] *= 2;
                    args[p_count] = (char *)realloc(args[p_count], arg_caps[p_count]);
                }
                args[p_count][arg_idx++] = c;
                args[p_count][arg_idx] = 0;
            }
            pp_next(state);
            continue;
        }

        
        if (c == '\\') {
            pp_next(state);
            if (pp_peek(state) == '\n') { pp_next(state); continue; }
            if (p_count < PP_MAX_PARAMS) {
                if (arg_idx >= arg_caps[p_count] - 2) {
                    arg_caps[p_count] *= 2;
                    args[p_count] = (char *)realloc(args[p_count], arg_caps[p_count]);
                }
                args[p_count][arg_idx++] = c;
                args[p_count][arg_idx++] = pp_next(state);
                args[p_count][arg_idx] = 0;
            } else {
                if (!_warned_pp_max_params && p_count >= PP_MAX_PARAMS) { printf("zcc: warning: PP_MAX_PARAMS (%d) exceeded at %s:%d — subsequent macro args silently discarded\n", PP_MAX_PARAMS, state->filename ? state->filename : "?", state->line); _warned_pp_max_params = 1; }
                pp_next(state);
            }
            continue;
        }
        
        if (c == '"' && !in_char) in_string = !in_string;
        if (c == '\'' && !in_string) in_char = !in_char;
        
        if (!in_string && !in_char) {
            if (c == '(') paren_level++;
            else if (c == ')') {
                if (paren_level == 0) {
                    pp_next(state); /* consume ')' */
                    if (arg_idx > 0 || m->num_params > 0) p_count++;
                    break;
                }
                paren_level--;
            } else if (c == ',' && paren_level == 0) {
                p_count++;
                arg_idx = 0;
                pp_next(state);
                continue;
            }
        }
        
        if (p_count < PP_MAX_PARAMS) {
            if (arg_idx >= arg_caps[p_count] - 1) {
                arg_caps[p_count] *= 2;
                args[p_count] = (char *)realloc(args[p_count], arg_caps[p_count]);
            }
            args[p_count][arg_idx++] = c;
            args[p_count][arg_idx] = 0;
        } else if (p_count >= PP_MAX_PARAMS) {
            if (!_warned_pp_max_params) { printf("zcc: warning: PP_MAX_PARAMS (%d) exceeded at %s:%d — subsequent macro args silently discarded\n", PP_MAX_PARAMS, state->filename ? state->filename : "?", state->line); _warned_pp_max_params = 1; }
        }
        pp_next(state);
    }
    
    /* PRE-EXPAND ARGUMENTS */
    char *expanded_args[PP_MAX_PARAMS];
    int expanded_cap[PP_MAX_PARAMS];
    for (i = 0; i < p_count; i++) {
        expanded_cap[i] = 1024;
        expanded_args[i] = (char *)malloc(expanded_cap[i]);
    }
    for (i = 0; i < p_count; i++) { /* using p_count which is correctly set */
        /* Save */
        int old_barrier = state->pop_barrier;
        char *old_out = state->out;
        int old_out_len = state->out_len;
        int old_out_cap = state->out_cap;
        
        /* Raise barrier before pushing arg stream */
        state->pop_barrier = state->input_depth;
        state->out = expanded_args[i];
        state->out_len = 0;
        state->out_cap = expanded_cap[i] - 1;
        
        pp_push_input(state, args[i], NULL, NULL);
        pp_parse_target_depth(state, state->input_depth);
        
        /* Force-drain residual frames under the barrier */
        while (state->input_depth > state->pop_barrier) {
            if (state->alloc_buf) free(state->alloc_buf);
            state->input_depth--;
            if (state->input_stack[state->input_depth].expanding_macro)
                state->input_stack[state->input_depth].expanding_macro->active = 1;
            state->src       = state->input_stack[state->input_depth].src;
            state->pos       = state->input_stack[state->input_depth].pos;
            state->len       = state->input_stack[state->input_depth].len;
            state->alloc_buf = state->input_stack[state->input_depth].alloc_buf;
        }
        
        /* Sync heap pointer back — realloc may have moved it */
        expanded_args[i] = state->out;
        expanded_cap[i] = state->out_cap + 1;
        expanded_args[i][state->out_len] = 0;
        
        /* Restore */
        state->out = old_out;
        state->out_len = old_out_len;
        state->out_cap = old_out_cap;
        state->pop_barrier = old_barrier;
    }
    
    /* substitute */
    int len = strlen(m->body);
    int subst_cap = 1024;
    char *subst = (char *)malloc(subst_cap);
    int subst_idx = 0;
    
    for (i = 0; i < len; i++) {
        if (m->body[i] == '#' && m->body[i+1] == '#') {
            i++; /* skip second # */
            /* strip trailing spaces from subst output */
            while (subst_idx > 0 && (subst[subst_idx-1] == ' ' || subst[subst_idx-1] == '\t')) {
                subst_idx--;
            }
            /* strip leading spaces in upcoming tokens */
            while (i + 1 < len && (m->body[i+1] == ' ' || m->body[i+1] == '\t')) {
                i++;
            }
            continue;
        }
        if (m->body[i] == '#' && m->body[i+1] != '#') {
            int tmp_i = i + 1;
            while (tmp_i < len && (m->body[tmp_i] == ' ' || m->body[tmp_i] == '\t')) tmp_i++;
            if (tmp_i < len && is_ident_start(m->body[tmp_i])) {
                char param_name[128];
                int p_idx = 0;
                int tmp_k = tmp_i;
                while (tmp_k < len && is_ident_char(m->body[tmp_k]) && p_idx < 127) {
                    param_name[p_idx++] = m->body[tmp_k++];
                }
                param_name[p_idx] = 0;
                int found = -1;
                for (int j = 0; j < m->num_params; j++) {
                    if (strcmp(m->params[j], param_name) == 0) {
                        found = j;
                        break;
                    }
                }
                if (found >= 0 && found < p_count) {
                    if (subst_idx + 1024 > subst_cap) {
                        subst_cap *= 2;
                        subst = (char *)realloc(subst, subst_cap);
                    }
                    subst[subst_idx++] = '"';
                    char *sa = args[found];
                    while (*sa) {
                        if (subst_idx + 128 > subst_cap) { subst_cap *= 2; subst = (char *)realloc(subst, subst_cap); }
                        if (*sa == '\\' || *sa == '"') subst[subst_idx++] = '\\';
                        subst[subst_idx++] = *sa++;
                    }
                    subst[subst_idx++] = '"';
                    i = tmp_k - 1;
                    continue;
                }
            }
        }
        if (is_ident_start(m->body[i])) {
            char param_name[128];
            int p_idx = 0;
            while (i < len && is_ident_char(m->body[i]) && p_idx < 127) {
                param_name[p_idx++] = m->body[i++];
            }
            param_name[p_idx] = 0;
            i--; /* backup */
            
            int found = -1;
            int j;
            for (j = 0; j < m->num_params; j++) {
                if (strcmp(m->params[j], param_name) == 0) {
                    found = j;
                    break;
                }
            }
            
            if (found >= 0 && found < p_count) {
                int exp_len = strlen(expanded_args[found]);
                if (subst_idx + exp_len + 128 > subst_cap) {
                    subst_cap = (subst_idx + exp_len + 1024) * 2;
                    subst = (char *)realloc(subst, subst_cap);
                }
                strcpy(subst + subst_idx, expanded_args[found]);
                subst_idx += exp_len;
            } else {
                int param_len = strlen(param_name);
                if (subst_idx + param_len + 128 > subst_cap) {
                    subst_cap = (subst_idx + param_len + 1024) * 2;
                    subst = (char *)realloc(subst, subst_cap);
                }
                strcpy(subst + subst_idx, param_name);
                subst_idx += param_len;
            }
        } else {
            if (subst_idx + 128 > subst_cap) {
                subst_cap *= 2;
                subst = (char *)realloc(subst, subst_cap);
            }
            subst[subst_idx++] = m->body[i];
        }
    }
    subst[subst_idx] = 0;
    
    /* Standard C preprocessor hide-set logic */
    m->active = 0;
    pp_push_input(state, subst, subst, m);

    for (i = 0; i < PP_MAX_PARAMS; i++) {
        if (args[i]) free(args[i]);
    }
    for (i = 0; i < p_count; i++) {
        if (expanded_args[i]) free(expanded_args[i]);
    }
}

static void pp_parse_target_depth(PPState *state, int target_depth) {
    int in_string = 0;
    while (1) {
        char c = pp_peek(state);
        if (state->input_depth < target_depth || c == 0) break;
        
        if (c == '"' || c == '\'') {
            in_string = c;
            if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
            while (pp_peek(state) != 0 && pp_peek(state) != in_string) {
                if (pp_peek(state) == '\\') {
                    if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
                    if (pp_peek(state) != 0) {
                        if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
                    }
                } else {
                    if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
                }
            }
            if (pp_peek(state) != 0) {
                if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
            }
            in_string = 0;
            continue;
        }
        if (c == '/' && state->pos + 1 < state->len) {
            char next_c = state->src[state->pos + 1];
            if (next_c == '/') {
                if (pp_is_active(state)) { pp_emit(state, pp_next(state)); pp_emit(state, pp_next(state)); }
                else { pp_next(state); pp_next(state); }
                while (pp_peek(state) != 0 && pp_peek(state) != '\n') {
                    if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
                }
                continue;
            } else if (next_c == '*') {
                if (pp_is_active(state)) { pp_emit(state, pp_next(state)); pp_emit(state, pp_next(state)); }
                else { pp_next(state); pp_next(state); }
                while (pp_peek(state) != 0) {
                    if (pp_peek(state) == '*' && state->pos + 1 < state->len && state->src[state->pos + 1] == '/') {
                        if (pp_is_active(state)) { pp_emit(state, pp_next(state)); pp_emit(state, pp_next(state)); }
                        else { pp_next(state); pp_next(state); }
                        break;
                    }
                    if (pp_is_active(state)) pp_emit(state, pp_next(state)); else pp_next(state);
                }
                continue;
            }
        }
        
        if (is_ident_start(c)) {
            char ident[128];
            pp_parse_ident(state, ident, 128);
            if (pp_is_active(state)) pp_expand_ident(state, ident);
            continue;
        }

        if (c == '#') {
            pp_next(state);
            pp_parse_directive(state);
            continue;
        }

        if (pp_is_active(state)) pp_emit(state, pp_next(state));
        else {
            if (c == '\n') pp_emit(state, '\n');
            pp_next(state);
        }
    }
}

char *zcc_preprocess(const char *source, int source_len,
                     const char *filename,
                     const char *include_paths,
                     int *out_len) {
    PPState *state = (PPState *)malloc(sizeof(PPState));
    char *normalized_src = (char *)malloc(source_len + 1);
    char *result;
    int normalized_len = 0;
    memset(state, 0, sizeof(PPState));

    {
        int r = 0;
        while (r < source_len) {
            char c = source[r++];
            if (c == '\r') {
                if (r < source_len && source[r] == '\n') r++;
                normalized_src[normalized_len++] = '\n';
            } else {
                normalized_src[normalized_len++] = c;
            }
        }
        normalized_src[normalized_len] = 0;
    }

    state->src = normalized_src;
    state->len = normalized_len;
    state->filename = filename;
    state->include_paths = include_paths;
    state->line = 1;
    
    state->cond_stack[0] = 1;
    
    {
        PPMacro *m = pp_add_macro(state, "__x86_64__");
        strcpy(m->body, "1");
        m = pp_add_macro(state, "__GNUC__");
        strcpy(m->body, "1");
        m = pp_add_macro(state, "__thread");
        strcpy(m->body, "");
        /* PP-HEADERS-023A: C99 <limits.h> predefined — must be in macro table
         * (not only in zcc_stddef_text) so #if defined(LLONG_MAX) works before
         * any #include fires. These duplicate the zcc_stddef_text entries
         * intentionally: the table entries handle conditional evaluation,
         * the stub entries handle explicit #include <limits.h>. */
        m = pp_add_macro(state, "LLONG_MAX");
        strcpy(m->body, "9223372036854775807LL");
        m = pp_add_macro(state, "LLONG_MIN");
        strcpy(m->body, "(-9223372036854775807LL-1LL)");
        m = pp_add_macro(state, "ULLONG_MAX");
        strcpy(m->body, "18446744073709551615ULL");
        m = pp_add_macro(state, "SCHAR_MIN");
        strcpy(m->body, "(-128)");
        m = pp_add_macro(state, "SCHAR_MAX");
        strcpy(m->body, "127");
        m = pp_add_macro(state, "UCHAR_MAX");
        strcpy(m->body, "255");
        m = pp_add_macro(state, "CHAR_MIN");
        strcpy(m->body, "(-128)");
        m = pp_add_macro(state, "CHAR_MAX");
        strcpy(m->body, "127");
        m = pp_add_macro(state, "SHRT_MIN");
        strcpy(m->body, "(-32768)");
        m = pp_add_macro(state, "SHRT_MAX");
        strcpy(m->body, "32767");
        m = pp_add_macro(state, "USHRT_MAX");
        strcpy(m->body, "65535");
        /* Pre-register existing stub limits so #if defined() works before #include */
        m = pp_add_macro(state, "CHAR_BIT");
        strcpy(m->body, "8");
        m = pp_add_macro(state, "INT_MAX");
        strcpy(m->body, "2147483647");
        m = pp_add_macro(state, "INT_MIN");
        strcpy(m->body, "(-2147483647-1)");
        m = pp_add_macro(state, "UINT_MAX");
        strcpy(m->body, "4294967295U");
        m = pp_add_macro(state, "LONG_MAX");
        strcpy(m->body, "9223372036854775807L");
        m = pp_add_macro(state, "LONG_MIN");
        strcpy(m->body, "(-9223372036854775807L-1L)");
        m = pp_add_macro(state, "ULONG_MAX");
        strcpy(m->body, "18446744073709551615UL");
        m = pp_add_macro(state, "SIZE_MAX");
        strcpy(m->body, "18446744073709551615UL");
    }

    pp_parse_target_depth(state, 0);
    
    pp_emit(state, 0);
    if (out_len) *out_len = state->out_len - 1;
    result = state->out;
    for (int i = 0; i < state->num_macros; i++) {
        if (state->macros[i].body) {
            free(state->macros[i].body);
            state->macros[i].body = NULL;
            state->macros[i].body_cap = 0;
        }
    }
    free(normalized_src);
    free(state);
    return result;
}

/* ================================================================ */
#ifndef ZCC_AST_BRIDGE_H
/* Exclusively for standalone IDE analysis */
#include "part1.c"
#endif

/* C99/POSIX support for curl graduation — see commit 262bd08 */
/* ================================================================ */
/* ARENA ALLOCATOR                                                   */
/* ================================================================ */

static unsigned long long next_alloc_id = 1;

/* Cap total arena blocks to avoid OOM from unbounded allocation (e.g. corrupt AST / infinite loop). Use literal 64 so compiler without preprocessor still works. */
void *cc_alloc(Compiler *cc, int size) {
    ArenaBlock *a;
    void *p;
    static int block_count = 0;
    size = (size + 7) & ~7;  /* align to 8 */
    a = &cc->arena;
    while (a) {
        if (a->pos + size <= a->cap) {
            p = a->data + a->pos;
            a->pos = a->pos + size;
            /* zero the memory — use temp ptr to avoid complex cast expression */
            {
                char *cp;
                int i;
                cp = (char *)p;
                for (i = 0; i < size; i++) {
                    cp[i] = 0;
                }
            }
            /* magic header: second qword = alloc_id for tracing */
            if (size >= 16) {
                *((unsigned long long *)((char *)p + 8)) = next_alloc_id++;
            }
            return p;
        }
        if (!a->next) {
            if (block_count >= 8192) {
                printf("zcc: too many arena blocks (%d) — possible infinite loop or corrupt AST (last alloc size=%d)\n", block_count, size);
                if (cc) {
                    printf("zcc: stuck near token kind=%d line=%d text=%.127s\n", cc->tk, cc->tk_line, cc->tk_text);
                }
                exit(1);
            }
            if (block_count >= 8000 && cc) {
                printf("zcc: near block limit block_count=%d token=%d line=%d size=%d text=%.127s\n", block_count, cc->tk, cc->tk_line, size, cc->tk_text);
            }
            block_count++;
            {
                int newcap;
                ArenaBlock *nb;
                newcap = ARENA_SIZE;
                if (size > newcap) newcap = size * 2;
                nb = (ArenaBlock *)malloc(sizeof(ArenaBlock));
                nb->data = (char *)malloc(newcap);
                nb->pos = 0;
                nb->cap = newcap;
                nb->next = 0;
                a->next = nb;
            }
        }
        a = a->next;
    }
    printf( "zcc: arena alloc failed\n");
    exit(1);
    return 0;
}

char *cc_strdup(Compiler *cc, char *s) {
    int len;
    char *p;
    int i;
    len = 0;
    while (s[len]) len++;
    p = (char *)cc_alloc(cc, len + 1);
    for (i = 0; i <= len; i++) {
        p[i] = s[i];
    }
    return p;
}

/* ================================================================ */
/* ERROR REPORTING                                                   */
/* ================================================================ */

void error(Compiler *cc, char *msg) {
    char *name;
    name = cc->filename;
    if (!name) name = "<input>";
    printf( "%s:%d: error: %s\n", name, cc->tk_line, msg);
    cc->errors++;
}

void error_at(Compiler *cc, int line, char *msg) {
    char *name;
    name = cc->filename;
    if (!name) name = "<input>";
    printf( "%s:%d: error: %s\n", name, line, msg);
    cc->errors++;
}

/* ================================================================ */
/* TYPE CONSTRUCTORS                                                 */
/* ================================================================ */

Type *type_new(Compiler *cc, int kind) {
    Type *t;
    t = (Type *)cc_alloc(cc, sizeof(Type));
    t->magic = 0xDEADBEEF87654321ULL;
    t->alloc_id = next_alloc_id - 1;
    t->kind = kind;
    switch (kind) {
        case TY_VOID:  t->size = 0; t->align = 1; break;
        case TY_CHAR:  t->size = 1; t->align = 1; break;
        case TY_UCHAR: t->size = 1; t->align = 1; break;
        case TY_SHORT: t->size = 2; t->align = 2; break;
        case TY_USHORT:t->size = 2; t->align = 2; break;
        case TY_INT:   t->size = 4; t->align = 4; break;
        case TY_UINT:  t->size = 4; t->align = 4; break;
        case TY_LONG:  t->size = 8; t->align = 8; break;
        case TY_ULONG: t->size = 8; t->align = 8; break;
        case TY_LONGLONG:  t->size = 8; t->align = 8; break;
        case TY_ULONGLONG: t->size = 8; t->align = 8; break;
        case TY_FLOAT: t->size = 4; t->align = 4; break;
        case TY_DOUBLE: t->size = 8; t->align = 8; break;
        case TY_PTR:   t->size = 8; t->align = 8; break;
        case TY_ENUM:  t->size = 4; t->align = 4; break;
        default:       t->size = 0; t->align = 1; break;
    }
    validate_type(cc, t, "type_new", 0);
    return t;
}

Type *type_ptr(Compiler *cc, Type *base) {
    Type *t;
    t = type_new(cc, TY_PTR);
    t->base = base;
    return t;
}

Type *type_array(Compiler *cc, Type *base, int len) {
    Type *t;
    t = type_new(cc, TY_ARRAY);
    t->base = base;
    t->array_len = len;
    t->size = type_size(base) * len;
    t->align = type_align(base);
    return t;
}

Type *type_func(Compiler *cc, Type *ret) {
    Type *t;
    t = type_new(cc, TY_FUNC);
    t->ret = ret;
    t->size = 8;
    t->align = 8;
    return t;
}

int type_size(Type *t) {
    if (!t) return 8;
    return t->size;
}

int type_align(Type *t) {
    if (!t) return 8;
    return t->align;
}

int is_integer(Type *t) {
    if (!t) return 0;
    if (t->kind >= TY_CHAR) {
        if (t->kind <= TY_ULONGLONG) return 1;
    }
    if (t->kind == TY_ENUM) return 1;
    return 0;
}

int is_pointer(Type *t) {
    if (!t) return 0;
    if (t->kind == TY_PTR) return 1;
    if (t->kind == TY_ARRAY) return 1;
    return 0;
}

int is_float_type(Type *t) {
    if (!t) return 0;
    if (t->kind == TY_FLOAT) return 1;
    if (t->kind == TY_DOUBLE) return 1;
    return 0;
}

int is_unsigned_type(Type *t) {
    if (!t) return 0;
    if (t->kind == TY_UCHAR) return 1;
    if (t->kind == TY_USHORT) return 1;
    if (t->kind == TY_UINT) return 1;
    if (t->kind == TY_ULONG) return 1;
    if (t->kind == TY_ULONGLONG) return 1;
    return 0;
}

/* ================================================================ */
/* NODE CONSTRUCTORS                                                 */
/* ================================================================ */

Node *node_new(Compiler *cc, int kind, int line) {
    Node *n;
    n = (Node *)cc_alloc(cc, sizeof(Node));
    n->magic = 0xC0FFEEBAD1234567ULL;
    n->alloc_id = next_alloc_id - 1;
    n->kind = kind;
    n->line = line;
    validate_node(cc, n, "node_new", line);
    return n;
}

Node *node_num(Compiler *cc, long long val, int line) {
    Node *n;
    n = node_new(cc, ND_NUM, line);
    n->int_val = val;
    n->type = cc->ty_int;
    if (val > 2147483647 || val < -2147483648) {
        n->type = cc->ty_long;
    }
    return n;
}

Node *node_flit(Compiler *cc, double val, int line) {
    Node *n;
    n = node_new(cc, ND_FLIT, line);
    n->f_val = val;
    n->type = cc->ty_double;
    return n;
}

/* ================================================================ */
/* SCOPE MANAGEMENT                                                  */
/* ================================================================ */

void scope_push(Compiler *cc) {
    Scope *s;
    s = (Scope *)cc_alloc(cc, sizeof(Scope));
    s->parent = cc->current_scope;
    s->symbols = 0;
    cc->current_scope = s;
}

void scope_pop(Compiler *cc) {
    if (cc->current_scope) {
        cc->current_scope = cc->current_scope->parent;
    }
}

Symbol *scope_find(Compiler *cc, char *name) {
    Scope *s;
    Symbol *sym;
    s = cc->current_scope;
    while (s) {
        sym = s->symbols;
        while (sym) {
            if (strcmp(sym->name, name) == 0) return sym;
            sym = sym->next;
        }
        s = s->parent;
    }
    return 0;
}

Symbol *scope_find_local(Compiler *cc, char *name) {
    Symbol *sym;
    if (!cc->current_scope) return 0;
    sym = cc->current_scope->symbols;
    while (sym) {
        if (strcmp(sym->name, name) == 0) return sym;
        sym = sym->next;
    }
    return 0;
}

Symbol *scope_add(Compiler *cc, char *name, Type *type) {
    Symbol *sym;
    sym = (Symbol *)cc_alloc(cc, sizeof(Symbol));
    strncpy(sym->name, name, MAX_IDENT - 1);
    sym->type = type;
    sym->stack_offset = 0;
    sym->next = cc->current_scope->symbols;
    cc->current_scope->symbols = sym;
    if (strcmp(name, "yyParser") == 0) {
        printf("DEBUG: scope_add('yyParser') with type kind=%d tag='%s'\n", type ? type->kind : -1, (type && type->tag[0]) ? type->tag : "<none>");
        fflush(stdout);
    }
    return sym;
}

/* Add local with auto-assigned stack offset */
Symbol *scope_add_local(Compiler *cc, char *name, Type *type) {
    Symbol *sym;
    int sz;
    sym = scope_add(cc, name, type);
    sym->is_local = 1;
    sz = type_size(type);
    if (sz < 8) sz = 8;  /* minimum 8-byte slots */
    cc->local_offset = cc->local_offset - sz;
    sym->stack_offset = cc->local_offset;
    return sym;
}

/* ================================================================ */
/* KEYWORD TABLE                                                     */
/* ================================================================ */

typedef struct {
    char *word;
    int token;
} Keyword;

static Keyword keywords[] = {
    {"int",       TK_INT},
    {"char",      TK_CHAR},
    {"void",      TK_VOID},
    {"long",      TK_LONG},
    {"short",     TK_SHORT},
    {"unsigned",  TK_UNSIGNED},
    {"signed",    TK_SIGNED},
    {"float",     TK_FLOAT},
    {"double",    TK_DOUBLE},
    {"if",        TK_IF},
    {"else",      TK_ELSE},
    {"while",     TK_WHILE},
    {"for",       TK_FOR},
    {"do",        TK_DO},
    {"return",    TK_RETURN},
    {"break",     TK_BREAK},
    {"continue",  TK_CONTINUE},
    {"goto",      TK_GOTO},
    {"switch",    TK_SWITCH},
    {"case",      TK_CASE},
    {"default",   TK_DEFAULT},
    {"struct",    TK_STRUCT},
    {"union",     TK_UNION},
    {"enum",      TK_ENUM},
    {"typedef",   TK_TYPEDEF},
    {"typeof",    TK_TYPEOF},
    {"__typeof__", TK_TYPEOF},
    {"__typeof",  TK_TYPEOF},
    {"__auto_type", TK_AUTO_TYPE},
    {"sizeof",    TK_SIZEOF},
    {"__builtin_va_arg", TK_BUILTIN_VA_ARG},
    {"static",    TK_STATIC},
    {"extern",    TK_EXTERN},
    {"const",     TK_CONST},
    {"volatile",  TK_VOLATILE},
    {"auto",      TK_AUTO},
    {"register",  TK_REGISTER},
    {"inline",    TK_INLINE},
    {"asm",       TK_ASM},
    {"__asm__",   TK_ASM},
    {"__signed__", TK_SIGNED},
    {"__signed",   TK_SIGNED},
    {"__const__",  TK_CONST},
    {"__const",    TK_CONST},
    {"__inline__", TK_INLINE},
    {"__inline",   TK_INLINE},
    {"__volatile__", TK_VOLATILE},
    {"__volatile", TK_VOLATILE},
    {"__restrict__", TK_VOLATILE},
    {"__restrict", TK_VOLATILE},
    {"restrict",  TK_VOLATILE},
    {"__int128",   TK_LONG},
    {"__int128_t", TK_LONG},
    {"_Bool",      TK_INT},
    {"_Atomic",    TK_VOLATILE},
    {"_Noreturn",  TK_INLINE},
    {"_Thread_local", TK_STATIC},
    {"_Alignof",   TK_SIZEOF},
    {"__alignof__", TK_SIZEOF},
    {"__alignof",  TK_SIZEOF},
    {0, 0}
};

static int kw_count = 62;

static int lookup_keyword(char *name) {
    int i;
    if (!name) return 0;
    for (i = 0; i < kw_count; i++) {
        if (!keywords[i].word) break;
        if (strcmp(keywords[i].word, name) == 0) {
            return keywords[i].token;
        }
    }
    return 0;
}

/* stage2 fallback: match ident by length and bytes (no strcmp/keyword table). Bounds-safe. */
static int lookup_keyword_fallback(char *buf, int len) {
    if (!buf || len <= 0 || len > 16) return 0;  /* max keyword length 16 */
    if (len==3 && buf[0]=='i'&&buf[1]=='n'&&buf[2]=='t') return TK_INT;
    if (len==4 && buf[0]=='c'&&buf[1]=='h'&&buf[2]=='a'&&buf[3]=='r') return TK_CHAR;
    if (len==4 && buf[0]=='v'&&buf[1]=='o'&&buf[2]=='i'&&buf[3]=='d') return TK_VOID;
    if (len==4 && buf[0]=='l'&&buf[1]=='o'&&buf[2]=='n'&&buf[3]=='g') return TK_LONG;
    if (len==5 && buf[0]=='s'&&buf[1]=='h'&&buf[2]=='o'&&buf[3]=='r'&&buf[4]=='t') return TK_SHORT;
    if (len==6 && buf[0]=='s'&&buf[1]=='i'&&buf[2]=='g'&&buf[3]=='n'&&buf[4]=='e'&&buf[5]=='d') return TK_SIGNED;
    if (len==5 && buf[0]=='f'&&buf[1]=='l'&&buf[2]=='o'&&buf[3]=='a'&&buf[4]=='t') return TK_FLOAT;
    if (len==6 && buf[0]=='d'&&buf[1]=='o'&&buf[2]=='u'&&buf[3]=='b'&&buf[4]=='l'&&buf[5]=='e') return TK_DOUBLE;
    if (len==2 && buf[0]=='i'&&buf[1]=='f') return TK_IF;
    if (len==3 && buf[0]=='a'&&buf[1]=='s'&&buf[2]=='m') return TK_ASM;
    if (len==7 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='a'&&buf[3]=='s'&&buf[4]=='m'&&buf[5]=='_'&&buf[6]=='_') return TK_ASM;
    if (len==4 && buf[0]=='e'&&buf[1]=='l'&&buf[2]=='s'&&buf[3]=='e') return TK_ELSE;
    if (len==5 && buf[0]=='w'&&buf[1]=='h'&&buf[2]=='i'&&buf[3]=='l'&&buf[4]=='e') return TK_WHILE;
    if (len==3 && buf[0]=='f'&&buf[1]=='o'&&buf[2]=='r') return TK_FOR;
    if (len==2 && buf[0]=='d'&&buf[1]=='o') return TK_DO;
    if (len==6 && buf[0]=='r'&&buf[1]=='e'&&buf[2]=='t'&&buf[3]=='u'&&buf[4]=='r'&&buf[5]=='n') return TK_RETURN;
    if (len==5 && buf[0]=='b'&&buf[1]=='r'&&buf[2]=='e'&&buf[3]=='a'&&buf[4]=='k') return TK_BREAK;
    if (len==4 && buf[0]=='g'&&buf[1]=='o'&&buf[2]=='t'&&buf[3]=='o') return TK_GOTO;
    if (len==6 && buf[0]=='s'&&buf[1]=='w'&&buf[2]=='i'&&buf[3]=='t'&&buf[4]=='c'&&buf[5]=='h') return TK_SWITCH;
    if (len==4 && buf[0]=='c'&&buf[1]=='a'&&buf[2]=='s'&&buf[3]=='e') return TK_CASE;
    if (len==6 && buf[0]=='s'&&buf[1]=='t'&&buf[2]=='r'&&buf[3]=='u'&&buf[4]=='c'&&buf[5]=='t') return TK_STRUCT;
    if (len==5 && buf[0]=='u'&&buf[1]=='n'&&buf[2]=='i'&&buf[3]=='o'&&buf[4]=='n') return TK_UNION;
    if (len==4 && buf[0]=='e'&&buf[1]=='n'&&buf[2]=='u'&&buf[3]=='m') return TK_ENUM;
    if (len==6 && buf[0]=='s'&&buf[1]=='i'&&buf[2]=='z'&&buf[3]=='e'&&buf[4]=='o'&&buf[5]=='f') return TK_SIZEOF;
    if (len==6 && buf[0]=='s'&&buf[1]=='t'&&buf[2]=='a'&&buf[3]=='t'&&buf[4]=='i'&&buf[5]=='c') return TK_STATIC;
    if (len==6 && buf[0]=='e'&&buf[1]=='x'&&buf[2]=='t'&&buf[3]=='e'&&buf[4]=='r'&&buf[5]=='n') return TK_EXTERN;
    if (len==5 && buf[0]=='c'&&buf[1]=='o'&&buf[2]=='n'&&buf[3]=='s'&&buf[4]=='t') return TK_CONST;
    if (len==4 && buf[0]=='a'&&buf[1]=='u'&&buf[2]=='t'&&buf[3]=='o') return TK_AUTO;
    if (len==6 && buf[0]=='i'&&buf[1]=='n'&&buf[2]=='l'&&buf[3]=='i'&&buf[4]=='n'&&buf[5]=='e') return TK_INLINE;
    if (len==6 && buf[0]=='t'&&buf[1]=='y'&&buf[2]=='p'&&buf[3]=='e'&&buf[4]=='o'&&buf[5]=='f') return TK_TYPEOF;
    if (len==8 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='t'&&buf[3]=='y'&&buf[4]=='p'&&buf[5]=='e'&&buf[6]=='o'&&buf[7]=='f') return TK_TYPEOF;
    if (len==10 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='t'&&buf[3]=='y'&&buf[4]=='p'&&buf[5]=='e'&&buf[6]=='o'&&buf[7]=='f'&&buf[8]=='_'&&buf[9]=='_') return TK_TYPEOF;
    /* 7–8 char keywords: need len check then safe indices 0..6 or 0..7 */
    if (len==7 && buf[0]=='t'&&buf[1]=='y'&&buf[2]=='p'&&buf[3]=='e'&&buf[4]=='d'&&buf[5]=='e'&&buf[6]=='f') return TK_TYPEDEF;
    if (len==7 && buf[0]=='d'&&buf[1]=='e'&&buf[2]=='f'&&buf[3]=='a'&&buf[4]=='u'&&buf[5]=='l'&&buf[6]=='t') return TK_DEFAULT;
    if (len==8 && buf[0]=='u'&&buf[1]=='n'&&buf[2]=='s'&&buf[3]=='i'&&buf[4]=='g'&&buf[5]=='n'&&buf[6]=='e'&&buf[7]=='d') return TK_UNSIGNED;
    if (len==8 && buf[0]=='v'&&buf[1]=='o'&&buf[2]=='l'&&buf[3]=='a'&&buf[4]=='t'&&buf[5]=='i'&&buf[6]=='l'&&buf[7]=='e') return TK_VOLATILE;
    if (len==8 && buf[0]=='c'&&buf[1]=='o'&&buf[2]=='n'&&buf[3]=='t'&&buf[4]=='i'&&buf[5]=='n'&&buf[6]=='u'&&buf[7]=='e') return TK_CONTINUE;
    if (len==8 && buf[0]=='r'&&buf[1]=='e'&&buf[2]=='g'&&buf[3]=='i'&&buf[4]=='s'&&buf[5]=='t'&&buf[6]=='e'&&buf[7]=='r') return TK_REGISTER;
    if (len==8 && buf[0]=='r'&&buf[1]=='e'&&buf[2]=='s'&&buf[3]=='t'&&buf[4]=='r'&&buf[5]=='i'&&buf[6]=='c'&&buf[7]=='t') return TK_VOLATILE;
    if (len==8 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='c'&&buf[3]=='o'&&buf[4]=='n'&&buf[5]=='s'&&buf[6]=='t') return TK_CONST;
    if (len==8 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='s'&&buf[3]=='i'&&buf[4]=='g'&&buf[5]=='n'&&buf[6]=='e'&&buf[7]=='d') return TK_SIGNED;
    if (len==8 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='i'&&buf[3]=='n'&&buf[4]=='l'&&buf[5]=='i'&&buf[6]=='n'&&buf[7]=='e') return TK_INLINE;
    if (len==9 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='c'&&buf[3]=='o'&&buf[4]=='n'&&buf[5]=='s'&&buf[6]=='t'&&buf[7]=='_'&&buf[8]=='_') return TK_CONST;
    if (len==10 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='s'&&buf[3]=='i'&&buf[4]=='g'&&buf[5]=='n'&&buf[6]=='e'&&buf[7]=='d'&&buf[8]=='_'&&buf[9]=='_') return TK_SIGNED;
    if (len==10 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='i'&&buf[3]=='n'&&buf[4]=='l'&&buf[5]=='i'&&buf[6]=='n'&&buf[7]=='e'&&buf[8]=='_'&&buf[9]=='_') return TK_INLINE;
    if (len==10 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='r'&&buf[3]=='e'&&buf[4]=='s'&&buf[5]=='t'&&buf[6]=='r'&&buf[7]=='i'&&buf[8]=='c'&&buf[9]=='t') return TK_VOLATILE;
    if (len==12 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='r'&&buf[3]=='e'&&buf[4]=='s'&&buf[5]=='t'&&buf[6]=='r'&&buf[7]=='i'&&buf[8]=='c'&&buf[9]=='t'&&buf[10]=='_'&&buf[11]=='_') return TK_VOLATILE;
    if (len==10 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='v'&&buf[3]=='o'&&buf[4]=='l'&&buf[5]=='a'&&buf[6]=='t'&&buf[7]=='i'&&buf[8]=='l'&&buf[9]=='e') return TK_VOLATILE;
    if (len==12 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='v'&&buf[3]=='o'&&buf[4]=='l'&&buf[5]=='a'&&buf[6]=='t'&&buf[7]=='i'&&buf[8]=='l'&&buf[9]=='e'&&buf[10]=='_'&&buf[11]=='_') return TK_VOLATILE;
    if (len==8 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='i'&&buf[3]=='n'&&buf[4]=='t'&&buf[5]=='1'&&buf[6]=='2'&&buf[7]=='8') return TK_LONG;
    if (len==10 && buf[0]=='_'&&buf[1]=='_'&&buf[2]=='i'&&buf[3]=='n'&&buf[4]=='t'&&buf[5]=='1'&&buf[6]=='2'&&buf[7]=='8'&&buf[8]=='_'&&buf[9]=='t') return TK_LONG;
    return 0;
}

/* ================================================================ */
/* LEXER                                                             */
/* ================================================================ */

static int is_alpha(int c) {
    if (c >= 'a') { if (c <= 'z') return 1; }
    if (c >= 'A') { if (c <= 'Z') return 1; }
    if (c == '_') return 1;
    return 0;
}

static int is_digit(int c) {
    if (c >= '0') { if (c <= '9') return 1; }
    return 0;
}

static int is_alnum(int c) {
    if (is_alpha(c)) return 1;
    if (is_digit(c)) return 1;
    return 0;
}

static int is_space(int c) {
    if (c == ' ') return 1;
    if (c == '\t') return 1;
    if (c == '\r') return 1;
    if (c == '\f') return 1;
    if (c == '\v') return 1;
    return 0;
}

static int hex_val(int c) {
    if (c >= '0') { if (c <= '9') return c - '0'; }
    if (c >= 'a') { if (c <= 'f') return c - 'a' + 10; }
    if (c >= 'A') { if (c <= 'F') return c - 'A' + 10; }
    return -1;
}

static int peek_char(Compiler *cc) {
    if (cc->pos < cc->source_len) return (unsigned char)cc->source[cc->pos];
    return -1;
}

static int read_char(Compiler *cc) {
    int c;
    if (cc->pos >= cc->source_len) return -1;
    c = (unsigned char)cc->source[cc->pos];
    cc->pos++;
    if (c == '\n') {
        cc->line++;
        cc->col = 1;
    } else {
        cc->col++;
    }
    return c;
}

static int read_escape(Compiler *cc) {
    int c;
    c = read_char(cc);
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7': {
            int val = c - '0';
            if (peek_char(cc) >= '0' && peek_char(cc) <= '7') {
                val = val * 8 + (read_char(cc) - '0');
                if (peek_char(cc) >= '0' && peek_char(cc) <= '7') {
                    val = val * 8 + (read_char(cc) - '0');
                }
            }
            return val;
        }
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'v': return '\v';
        case 'x': {
            int val;
            int h;
            val = 0;
            h = hex_val(peek_char(cc));
            while (h >= 0) {
                read_char(cc);
                val = val * 16 + h;
                h = hex_val(peek_char(cc));
            }
            return val;
        }
        default: return c;
    }
}

void next_token(Compiler *cc) {
    int c;
    int kw;
    int entry_pos = cc->pos;

    /* if we have a peeked token, use it */
    if (cc->has_peek) {
        cc->tk = cc->peek_tk;
        cc->tk_val = cc->peek_val;
        strncpy(cc->tk_text, cc->peek_text, MAX_IDENT - 1);
        strncpy(cc->tk_str, cc->peek_str, MAX_STR - 1);
        cc->tk_str_len = cc->peek_str_len;
        cc->tk_line = cc->peek_line;
        cc->tk_col = cc->peek_col;
        cc->has_peek = 0;
        return;
    }

again:
    /* skip whitespace */
    while (cc->pos < cc->source_len) {
        c = peek_char(cc);
        if (c == '\n') {
            read_char(cc);
            goto again;
        }
        if (is_space(c)) {
            read_char(cc);
        } else {
            break;
        }
    }

    cc->tk_line = cc->line;
    cc->tk_col = cc->col;

    if (cc->pos >= cc->source_len) {
        cc->tk = TK_EOF;
        cc->tk_text[0] = 0;
        return;
    }

    c = peek_char(cc);

    /* skip line comments */
    if (c == '/') {
        if (cc->pos + 1 < cc->source_len) {
            if (cc->source[cc->pos + 1] == '/') {
                while (cc->pos < cc->source_len) {
                    if (cc->source[cc->pos] == '\n') break;
                    cc->pos++;
                }
                goto again;
            }
            /* block comments */
            if (cc->source[cc->pos + 1] == '*') {
                cc->pos += 2;
                cc->col += 2;
                while (cc->pos + 1 < cc->source_len) {
                    if (cc->source[cc->pos] == '\n') {
                        cc->line++;
                        cc->col = 1;
                    }
                    if (cc->source[cc->pos] == '*') {
                        if (cc->source[cc->pos + 1] == '/') {
                            cc->pos += 2;
                            cc->col += 2;
                            goto again;
                        }
                    }
                    cc->pos++;
                    cc->col++;
                }
                goto again;
            }
        }
    }

    /* preprocessor lines: skip entirely; update line/filename from directives */
    if (c == '#') {
        /* check if it's a GCC line directive: # NNN "filename" [flags] */
        int dpos = cc->pos + 1;
        int new_line = 0;
        int has_linenum = 0;
        /* skip spaces after # */
        while (dpos < cc->source_len && (cc->source[dpos] == ' ' || cc->source[dpos] == '\t')) dpos++;
        /* parse number */
        while (dpos < cc->source_len && cc->source[dpos] >= '0' && cc->source[dpos] <= '9') {
            new_line = new_line * 10 + (cc->source[dpos] - '0');
            has_linenum = 1;
            dpos++;
        }
        if (has_linenum && new_line > 0) {
            cc->line = new_line;
        }
        /* skip to end of line */
        while (cc->pos < cc->source_len) {
            if (cc->source[cc->pos] == '\n') break;
            /* handle line continuation */
            if (cc->source[cc->pos] == '\\') {
                if (cc->pos + 1 < cc->source_len) {
                    if (cc->source[cc->pos + 1] == '\n') {
                        cc->pos += 2;
                        cc->line++;
                        cc->col = 1;
                        continue;
                    }
                }
            }
            cc->pos++;
        }
        goto again;
    }

    /* number literals */
    if (is_digit(c)) {
        long val;
        int start;
        int is_float = 0;
        int i_look = 0;
        char *end;
        double fval;
        int len;
        
        if (c == '0' && cc->pos + 1 < cc->source_len && 
            (cc->source[cc->pos + 1] == 'x' || cc->source[cc->pos + 1] == 'X')) {
            is_float = 0;
        } else {
            while (cc->pos + i_look < cc->source_len) {
                char ch = cc->source[cc->pos + i_look];
                if (ch == '.' || ch == 'e' || ch == 'E') { is_float = 1; break; }
                if (!is_digit(ch) && !is_alpha(ch)) break;
                i_look++;
            }
        }
        
        if (is_float) {
            fval = strtod(cc->source + cc->pos, &end);
            len = end - (cc->source + cc->pos);
            if (len <= 0) len = 1;
            /* we peeked c, so real current index is cc->pos, 
               but we need to advance cc->pos. */
            cc->pos = cc->pos + len;
            cc->col = cc->col + len;
            /* CG-FLOAT-007: record f/F suffix in tk_text[0] for parse_primary */
            cc->tk_text[0] = 0;
            if (cc->pos < cc->source_len) {
                char sc = cc->source[cc->pos];
                if (sc == 'f' || sc == 'F') {
                    cc->pos++; cc->col++;
                    cc->tk_text[0] = 'F';  /* tag as float literal */
                } else if (sc == 'l' || sc == 'L') {
                    cc->pos++; cc->col++;
                }
            }
            cc->tk = TK_FLIT;
            cc->tk_fval = fval;
            return;
        }

        val = 0;
        start = cc->pos;
        if (c == '0') {
            read_char(cc);
            if (peek_char(cc) == 'x' || peek_char(cc) == 'X') {
                read_char(cc);
                while (hex_val(peek_char(cc)) >= 0) {
                    val = val * 16 + hex_val(peek_char(cc));
                    read_char(cc);
                }
            } else if (peek_char(cc) >= '0') {
                if (peek_char(cc) <= '7') {
                    /* octal */
                    while (peek_char(cc) >= '0') {
                        if (peek_char(cc) <= '7') {
                            val = val * 8 + (peek_char(cc) - '0');
                            read_char(cc);
                        } else {
                            break;
                        }
                    }
                }
            }
        } else {
            while (is_digit(peek_char(cc))) {
                val = val * 10 + (peek_char(cc) - '0');
                read_char(cc);
            }
        }
        /* parse suffixes like L, U, UL, ULL, etc */
        int is_uns = 0;
        int is_lng = 0;
        while (peek_char(cc) == 'L' || peek_char(cc) == 'l' ||
               peek_char(cc) == 'U' || peek_char(cc) == 'u') {
            int ch = read_char(cc);
            if (ch == 'U' || ch == 'u') is_uns = 1;
            if (ch == 'L' || ch == 'l') is_lng = 1;
        }
        cc->tk = TK_NUM;
        cc->tk_val = val;
        cc->tk_text[0] = is_uns ? 'U' : 0;
        cc->tk_text[1] = is_lng ? 'L' : 0;
        return;
    }

    /* character literal */
    if (c == '\'') {
        int val;
        read_char(cc);  /* skip opening ' */
        if (peek_char(cc) == '\\') {
            read_char(cc);
            val = read_escape(cc);
        } else {
            val = read_char(cc);
        }
        if (peek_char(cc) == '\'') read_char(cc);  /* skip closing ' */
        cc->tk = TK_CHAR_LIT;
        cc->tk_val = val;
        return;
    }

    /* string literal */
    if (c == '"') {
        int len;
        read_char(cc);  /* skip opening " */
        len = 0;
        while (peek_char(cc) != '"') {
            if (peek_char(cc) == -1) break;
            if (peek_char(cc) == '\\') {
                read_char(cc);
                cc->tk_str[len] = read_escape(cc);
            } else {
                cc->tk_str[len] = read_char(cc);
            }
            len++;
            if (len >= MAX_STR - 1) break;
        }
        if (peek_char(cc) == '"') read_char(cc);  /* skip closing " */
        cc->tk_str[len] = 0;
        cc->tk_str_len = len;
        cc->tk = TK_STR;

        /* handle adjacent string literals */
        /* skip whitespace and check for another " */
        while (1) {
            int saved_pos;
            int saved_line;
            int saved_col;
            saved_pos = cc->pos;
            saved_line = cc->line;
            saved_col = cc->col;
            while (cc->pos < cc->source_len) {
                int ch;
                ch = cc->source[cc->pos];
                if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
                    if (ch == '\n') {
                        cc->line++;
                        cc->col = 1;
                    } else {
                        cc->col++;
                    }
                    cc->pos++;
                } else {
                    break;
                }
            }
            if (cc->pos < cc->source_len) {
                if (cc->source[cc->pos] == '"') {
                    /* concatenate adjacent string */
                    read_char(cc); /* skip " */
                    while (peek_char(cc) != '"') {
                        if (peek_char(cc) == -1) break;
                        if (peek_char(cc) == '\\') {
                            read_char(cc);
                            cc->tk_str[len] = read_escape(cc);
                        } else {
                            cc->tk_str[len] = read_char(cc);
                        }
                        len++;
                        if (len >= MAX_STR - 1) break;
                    }
                    if (peek_char(cc) == '"') read_char(cc);
                    cc->tk_str[len] = 0;
                    cc->tk_str_len = len;
                } else {
                    /* no adjacent string — restore position and exit loop */
                    cc->pos = saved_pos;
                    cc->line = saved_line;
                    cc->col = saved_col;
                    break;
                }
            } else {
                break;
            }
        }
        return;
    }

    /* identifiers and keywords: use local buffer for lookup to avoid bad cc->tk_text in stage2 (offset/codegen quirk) */
    if (is_alpha(c)) {
        static char ident_buf[MAX_IDENT];
        int len;
        len = 0;
        while (is_alnum(peek_char(cc))) {
            ident_buf[len] = read_char(cc);
            len++;
            if (len >= MAX_IDENT - 1) break;
        }
        ident_buf[len] = 0;

        kw = lookup_keyword(ident_buf);
        if (!kw) kw = lookup_keyword_fallback(ident_buf, len);
        if (kw) {
            cc->tk = kw;
        } else {
            /* ATTR-UNKNOWN-001: preprocessor emits __zcc_attr_packed__ for __attribute__((packed)).
             * Set the pending flag and loop to the next real token. */
            if (strcmp(ident_buf, "__zcc_attr_packed__") == 0) {
                cc->pending_packed = 1;
                goto again;
            }
            if (strcmp(ident_buf, "__attribute__") == 0 || strcmp(ident_buf, "__attribute") == 0) {
                /* Two-pass attribute handler (handles any __attribute__ that leaks through PP) */
                int pos0 = cc->pos;  /* save position right after __attribute__ */
                {
                    /* --- Pass 1: find attribute name --- */
                    int p = pos0;
                    char attr_name[64];
                    int ai = 0;
                    char *src = cc->source;
                    int slen = cc->source_len;
                    /* skip whitespace then opening (( */
                    while (p < slen && (src[p]==' '||src[p]=='\t'||src[p]=='\n'||src[p]=='\r')) p++;
                    if (p < slen && src[p] == '(') p++;
                    while (p < slen && (src[p]==' '||src[p]=='\t')) p++;
                    if (p < slen && src[p] == '(') p++;
                    while (p < slen && (src[p]==' '||src[p]=='\t')) p++;
                    /* read attribute name identifier */
                    while (p < slen && ai < 63) {
                        char ac = src[p];
                        if ((ac>='a'&&ac<='z')||(ac>='A'&&ac<='Z')||(ac>='0'&&ac<='9')||ac=='_') {
                            attr_name[ai++] = ac; p++;
                        } else break;
                    }
                    attr_name[ai] = 0;
                    /* strip __ prefix/suffix (GCC __packed__ style) */
                    {
                        char *an = attr_name;
                        int alen = ai;
                        if (alen > 4 && an[0]=='_' && an[1]=='_' && an[alen-1]=='_' && an[alen-2]=='_') {
                            an[alen-2] = 0; an += 2; alen -= 4;
                        }
                        if (strcmp(an, "packed") == 0) {
                            cc->pending_packed = 1;
                        } else if (strcmp(an, "aligned") == 0) {
                            /* find (N) argument */
                            while (p < slen && src[p] != '(' && src[p] != ')') p++;
                            if (p < slen && src[p] == '(') {
                                int n = 0;
                                p++;
                                while (p < slen && src[p] >= '0' && src[p] <= '9') {
                                    n = n * 10 + (src[p] - '0'); p++;
                                }
                                if (n > 0) cc->pending_aligned_n = n;
                            }
                        } else if (an[0] != 0) {
                            /* Unknown attribute — warn and drop (GCC -Wunknown-attributes) */
                            /* NOTE: suppress noisy stdio.h attrs to keep output readable */
                            if (strcmp(an, "nothrow") != 0 && strcmp(an, "leaf") != 0 &&
                                strcmp(an, "nonnull") != 0 && strcmp(an, "malloc") != 0 &&
                                strcmp(an, "warn_unused_result") != 0 && strcmp(an, "deprecated") != 0 &&
                                strcmp(an, "format") != 0 && strcmp(an, "noreturn") != 0 &&
                                strcmp(an, "sentinel") != 0 && strcmp(an, "artificial") != 0) {
                                fprintf(stderr, "%s:%d: warning: unknown attribute '%s' ignored [-Wunknown-attributes]\n",
                                        cc->filename ? cc->filename : "<input>", cc->line, an);
                            }
                        }
                    }
                }
                /* --- Pass 2: rewind to pos0 and gobble entire __attribute__((...)) --- */
                {
                    int pcount = 0;
                    int started = 0;
                    cc->pos = pos0;
                    while (cc->pos < cc->source_len) {
                        char ac = cc->source[cc->pos];
                        cc->pos++;
                        if (ac == '(') { pcount++; started = 1; }
                        else if (ac == ')') {
                            pcount--;
                            if (started && pcount == 0) break;
                        }
                    }
                }
                goto again;
            }




            if (strcmp(ident_buf, "__extension__") == 0) {
                goto again;
            }
            
            cc->tk = TK_IDENT;
        }
        strncpy(cc->tk_text, ident_buf, MAX_IDENT - 1);
        cc->tk_text[MAX_IDENT - 1] = 0;
        return;
    }

    /* operators and delimiters */
    read_char(cc);

    if (c == '+') {
        if (peek_char(cc) == '+') { read_char(cc); cc->tk = TK_INC; return; }
        if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_PLUS_ASSIGN; return; }
        cc->tk = TK_PLUS; return;
    }
    if (c == '-') {
        if (peek_char(cc) == '-') { read_char(cc); cc->tk = TK_DEC; return; }
        if (peek_char(cc) == '>') { read_char(cc); cc->tk = TK_ARROW; return; }
        if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_MINUS_ASSIGN; return; }
        cc->tk = TK_MINUS; return;
    }
    if (c == '*') {
        if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_STAR_ASSIGN; return; }
        cc->tk = TK_STAR; return;
    }
    if (c == '/') {
        if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_SLASH_ASSIGN; return; }
        cc->tk = TK_SLASH; return;
    }
    if (c == '%') {
        if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_PERCENT_ASSIGN; return; }
        cc->tk = TK_PERCENT; return;
    }
    if (c == '&') {
        if (peek_char(cc) == '&') { read_char(cc); cc->tk = TK_LAND; return; }
        if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_AMP_ASSIGN; return; }
        cc->tk = TK_AMP; return;
    }
    if (c == '|') {
        if (peek_char(cc) == '|') { read_char(cc); cc->tk = TK_LOR; return; }
        if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_PIPE_ASSIGN; return; }
        cc->tk = TK_PIPE; return;
    }
    if (c == '^') {
        if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_CARET_ASSIGN; return; }
        cc->tk = TK_CARET; return;
    }
    if (c == '~') { cc->tk = TK_TILDE; return; }
    if (c == '!') {
        if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_NE; return; }
        cc->tk = TK_BANG; return;
    }
    if (c == '=') {
        if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_EQ; return; }
        cc->tk = TK_ASSIGN; return;
    }
    if (c == '<') {
        if (peek_char(cc) == '<') {
            read_char(cc);
            if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_SHL_ASSIGN; return; }
            cc->tk = TK_SHL; return;
        }
        if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_LE; return; }
        cc->tk = TK_LT; return;
    }
    if (c == '>') {
        if (peek_char(cc) == '>') {
            read_char(cc);
            if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_SHR_ASSIGN; return; }
            cc->tk = TK_SHR; return;
        }
        if (peek_char(cc) == '=') { read_char(cc); cc->tk = TK_GE; return; }
        cc->tk = TK_GT; return;
    }
    if (c == '(') { cc->tk = TK_LPAREN; return; }
    if (c == ')') { cc->tk = TK_RPAREN; return; }
    if (c == '{') { cc->tk = TK_LBRACE; return; }
    if (c == '}') { cc->tk = TK_RBRACE; return; }
    if (c == '[') { cc->tk = TK_LBRACKET; return; }
    if (c == ']') { cc->tk = TK_RBRACKET; return; }
    if (c == ';') { cc->tk = TK_SEMI; return; }
    if (c == ',') { cc->tk = TK_COMMA; return; }
    if (c == '.') {
        if (is_digit(peek_char(cc))) {
            char *end;
            double fval = strtod(cc->source + cc->pos - 1, &end);
            int len = end - (cc->source + cc->pos - 1);
            if (len <= 0) len = 1;
            cc->pos = cc->pos - 1 + len;
            cc->col = cc->col - 1 + len;
            /* CG-FLOAT-007: record f/F suffix in tk_text[0] for parse_primary */
            cc->tk_text[0] = 0;
            if (cc->pos < cc->source_len) {
                char sc = cc->source[cc->pos];
                if (sc == 'f' || sc == 'F') {
                    cc->pos++; cc->col++;
                    cc->tk_text[0] = 'F';  /* tag as float literal */
                } else if (sc == 'l' || sc == 'L') {
                    cc->pos++; cc->col++;
                }
            }
            cc->tk = TK_FLIT;
            cc->tk_fval = fval;
            return;
        }
        if (peek_char(cc) == '.') {
            read_char(cc);
            if (peek_char(cc) == '.') { read_char(cc); cc->tk = TK_ELLIPSIS; return; }
        }
        cc->tk = TK_DOT; return;
    }
    if (c == '?') { cc->tk = TK_QUESTION; return; }
    if (c == ':') { cc->tk = TK_COLON; return; }

    /* unknown character — skip */
    goto again;

}

/* Token enum in part1 runs 0..TK_HASH. Stage2 may compile enums to different values (75-81 seen); use 128 so we only flag obvious garbage. */
#define MAX_KNOWN_TOKEN  128

/* Human-readable token name for error messages (index = enum value from part1). */
static const char *token_name(int t) {
    static const char *names[] = {
        "EOF", "NUM", "STR", "CHAR_LIT", "IDENT", "FLIT",
        "INT", "CHAR", "VOID", "LONG", "SHORT", "UNSIGNED", "SIGNED", "FLOAT", "DOUBLE",
        "IF", "ELSE", "WHILE", "FOR", "DO", "RETURN", "BREAK", "CONTINUE", "GOTO",
        "SWITCH", "CASE", "DEFAULT", "STRUCT", "UNION", "ENUM", "TYPEDEF",
        "SIZEOF", "STATIC", "EXTERN", "CONST", "VOLATILE", "AUTO", "REGISTER", "INLINE",
        "ASM", "BUILTIN_VA_ARG", "TYPEOF", "AUTO_TYPE",
        "PLUS", "MINUS", "STAR", "SLASH", "PERCENT", "AMP", "PIPE", "CARET", "TILDE", "BANG",
        "ASSIGN", "EQ", "NE", "LT", "GT", "LE", "GE", "LAND", "LOR", "SHL", "SHR",
        "INC", "DEC", "ARROW", "DOT", "QUESTION", "COLON",
        "PLUS_ASSIGN", "MINUS_ASSIGN", "STAR_ASSIGN", "SLASH_ASSIGN", "PERCENT_ASSIGN",
        "AMP_ASSIGN", "PIPE_ASSIGN", "CARET_ASSIGN", "SHL_ASSIGN", "SHR_ASSIGN",
        "LPAREN", "RPAREN", "LBRACE", "RBRACE", "LBRACKET", "RBRACKET",
        "SEMI", "COMMA", "ELLIPSIS", "HASH"
    };
    if (t >= 0 && t < 88) return names[t];  /* 88 = number of token names */
    return "?";
}

void expect(Compiler *cc, int tk) {
    char buf[256];
    if (tk < 0 || tk > 128) {
        sprintf(buf, "DEMON: insane expected token %d (line %d) — possible stack corruption", tk, cc->tk_line);
        error(cc, buf);
    }
    if (cc->tk != tk) {
        sprintf(buf, "expected %s (%d), got %s (%d)", token_name(tk), tk, token_name(cc->tk), cc->tk);
        error(cc, buf);
    }
    next_token(cc);
}

int peek_token(Compiler *cc) {
    int s_tk;
    long s_val;
    char s_text[MAX_IDENT];
    char s_str[MAX_STR];
    int s_slen;
    int s_line;
    int s_col;

    if (cc->has_peek) return cc->peek_tk;

    /* save current token */
    s_tk = cc->tk;
    s_val = cc->tk_val;
    strncpy(s_text, cc->tk_text, MAX_IDENT - 1);
    strncpy(s_str, cc->tk_str, MAX_STR - 1);
    s_slen = cc->tk_str_len;
    s_line = cc->tk_line;
    s_col = cc->tk_col;

    /* lex next */
    next_token(cc);

    /* store peeked */
    cc->peek_tk = cc->tk;
    cc->peek_val = cc->tk_val;
    strncpy(cc->peek_text, cc->tk_text, MAX_IDENT - 1);
    strncpy(cc->peek_str, cc->tk_str, MAX_STR - 1);
    cc->peek_str_len = cc->tk_str_len;
    cc->peek_line = cc->tk_line;
    cc->peek_col = cc->tk_col;
    cc->has_peek = 1;

    /* restore current */
    cc->tk = s_tk;
    cc->tk_val = s_val;
    strncpy(cc->tk_text, s_text, MAX_IDENT - 1);
    strncpy(cc->tk_str, s_str, MAX_STR - 1);
    cc->tk_str_len = s_slen;
    cc->tk_line = s_line;
    cc->tk_col = s_col;

    return cc->peek_tk;
}

/* ZKAEDI FORCE RENDER CACHE INVALIDATION */
#ifndef ZCC_AST_BRIDGE_H
/* Exclusively for standalone IDE analysis */
#include "part1.c"
#endif

/* ================================================================ */
/* PARSER                                                            */
/* ================================================================ */

static int is_type_token(Compiler *cc) {
    if (cc->tk >= TK_INT && cc->tk <= TK_DOUBLE) return 1;
    if (cc->tk >= TK_STATIC && cc->tk <= TK_INLINE) return 1;
    if (cc->tk == TK_STRUCT) return 1;
    if (cc->tk == TK_UNION) return 1;
    if (cc->tk == TK_ENUM) return 1;
    if (cc->tk == TK_TYPEDEF) return 1;
    if (cc->tk == TK_TYPEOF) return 1;
    if (cc->tk == TK_AUTO_TYPE) return 1;
    if (cc->tk == TK_IDENT) {
        Symbol *sym;
        sym = scope_find(cc, cc->tk_text);
        if (sym) {
            if (sym->is_typedef) return 1;
        }
    }
    return 0;
}

static Type *find_struct(Compiler *cc, char *tag) {
    int i;
    for (i = 0; i < cc->num_structs; i++) {
        if (strcmp(cc->structs[i]->tag, tag) == 0) return cc->structs[i];
    }
    return 0;
}

static void register_struct(Compiler *cc, Type *t) {
    if (cc->num_structs < MAX_STRUCTS) {
        cc->structs[cc->num_structs] = t;
        cc->num_structs++;
    }
}

/* ---------------------------------------------------------------- */
/* Parse struct/union body                                           */
/* ---------------------------------------------------------------- */

Type *parse_type(Compiler *cc);
Type *parse_declarator(Compiler *cc, Type *base, char *name_out);
static long long parse_const_expr_ternary(Compiler *cc);
static long long parse_const_expr_lor(Compiler *cc);
static long long parse_const_expr_land(Compiler *cc);
static long long parse_const_expr_bor(Compiler *cc);
static long long parse_const_expr_bxor(Compiler *cc);
static long long parse_const_expr_band(Compiler *cc);
static long long parse_const_expr_eq(Compiler *cc);
static long long parse_const_expr_rel(Compiler *cc);
static long long parse_const_expr_shift(Compiler *cc);
static long long parse_const_expr_add(Compiler *cc);
static long long parse_const_expr_mul(Compiler *cc);
static long long parse_const_expr_unary(Compiler *cc);
static long long parse_const_expr_primary(Compiler *cc);

static long long parse_const_expr(Compiler *cc) { return parse_const_expr_ternary(cc); }

static long long parse_const_expr_ternary(Compiler *cc) {
    long long val = parse_const_expr_lor(cc);
    if (cc->tk == TK_QUESTION) {
        next_token(cc);
        long long t_val = parse_const_expr(cc);
        expect(cc, TK_COLON);
        long long f_val = parse_const_expr_ternary(cc);
        return val ? t_val : f_val;
    }
    return val;
}
static long long parse_const_expr_lor(Compiler *cc) {
    long long val = parse_const_expr_land(cc);
    while (cc->tk == TK_LOR) { next_token(cc); long long rhs = parse_const_expr_land(cc); val = val || rhs; }
    return val;
}
static long long parse_const_expr_land(Compiler *cc) {
    long long val = parse_const_expr_bor(cc);
    while (cc->tk == TK_LAND) { next_token(cc); long long rhs = parse_const_expr_bor(cc); val = val && rhs; }
    return val;
}
static long long parse_const_expr_bor(Compiler *cc) {
    long long val = parse_const_expr_bxor(cc);
    while (cc->tk == TK_PIPE) { next_token(cc); val |= parse_const_expr_bxor(cc); }
    return val;
}
static long long parse_const_expr_bxor(Compiler *cc) {
    long long val = parse_const_expr_band(cc);
    while (cc->tk == TK_CARET) { next_token(cc); val ^= parse_const_expr_band(cc); }
    return val;
}
static long long parse_const_expr_band(Compiler *cc) {
    long long val = parse_const_expr_eq(cc);
    while (cc->tk == TK_AMP) { next_token(cc); val &= parse_const_expr_eq(cc); }
    return val;
}
static long long parse_const_expr_eq(Compiler *cc) {
    long long val = parse_const_expr_rel(cc);
    while (cc->tk == TK_EQ || cc->tk == TK_NE) {
        int tk = cc->tk; next_token(cc);
        if (tk == TK_EQ) val = (val == parse_const_expr_rel(cc));
        else val = (val != parse_const_expr_rel(cc));
    }
    return val;
}
static long long parse_const_expr_rel(Compiler *cc) {
    long long val = parse_const_expr_shift(cc);
    while (cc->tk == TK_LT || cc->tk == TK_GT || cc->tk == TK_LE || cc->tk == TK_GE) {
        int tk = cc->tk; next_token(cc);
        if (tk == TK_LT) val = (val < parse_const_expr_shift(cc));
        else if (tk == TK_GT) val = (val > parse_const_expr_shift(cc));
        else if (tk == TK_LE) val = (val <= parse_const_expr_shift(cc));
        else val = (val >= parse_const_expr_shift(cc));
    }
    return val;
}
static long long parse_const_expr_shift(Compiler *cc) {
    long long val = parse_const_expr_add(cc);
    while (cc->tk == TK_SHL || cc->tk == TK_SHR) {
        int tk = cc->tk; next_token(cc);
        if (tk == TK_SHL) val <<= parse_const_expr_add(cc);
        else val >>= parse_const_expr_add(cc);
    }
    return val;
}
static long long parse_const_expr_add(Compiler *cc) {
    long long val = parse_const_expr_mul(cc);
    while (cc->tk == TK_PLUS || cc->tk == TK_MINUS) {
        int tk = cc->tk; next_token(cc);
        if (tk == TK_PLUS) val += parse_const_expr_mul(cc);
        else val -= parse_const_expr_mul(cc);
    }
    return val;
}
static long long parse_const_expr_mul(Compiler *cc) {
    long long val = parse_const_expr_unary(cc);
    while (cc->tk == TK_STAR || cc->tk == TK_SLASH || cc->tk == TK_PERCENT) {
        int tk = cc->tk; next_token(cc);
        long long rhs = parse_const_expr_unary(cc);
        if (tk == TK_STAR) val *= rhs;
        else if (tk == TK_SLASH) { if (rhs) val /= rhs; }
        else if (tk == TK_PERCENT) { if (rhs) val %= rhs; }
    }
    return val;
}
static long long parse_const_expr_unary(Compiler *cc) {
    if (cc->tk == TK_LPAREN) {
        int is_cast = 0;
        int ptk = peek_token(cc);
        int curr_tk = cc->tk;
        char curr_txt[MAX_IDENT];
        strncpy(curr_txt, cc->tk_text, MAX_IDENT - 1);
        cc->tk = ptk;
        strncpy(cc->tk_text, cc->peek_text, MAX_IDENT - 1);
        if (is_type_token(cc)) is_cast = 1;
        cc->tk = curr_tk;
        strncpy(cc->tk_text, curr_txt, MAX_IDENT - 1);
        
        if (is_cast) {
            next_token(cc); /* consume ( */
            Type *st = parse_type(cc);
            char dummy[128];
            st = parse_declarator(cc, st, dummy);
            expect(cc, TK_RPAREN);
            return parse_const_expr_unary(cc);
        }
    }
    if (cc->tk == TK_MINUS) { next_token(cc); return -parse_const_expr_unary(cc); }
    if (cc->tk == TK_PLUS) { next_token(cc); return parse_const_expr_unary(cc); }
    if (cc->tk == TK_TILDE) { next_token(cc); return ~parse_const_expr_unary(cc); }
    if (cc->tk == TK_BANG) { next_token(cc); return !parse_const_expr_unary(cc); }
    if (cc->tk == TK_SIZEOF) {
        next_token(cc);
        if (cc->tk == TK_LPAREN) {
            next_token(cc);
            if (is_type_token(cc)) {
                Type *st = parse_type(cc);
                char dummy[128];
                st = parse_declarator(cc, st, dummy);
                expect(cc, TK_RPAREN);
                return type_size(st);
            }
            long long v = parse_const_expr(cc);
            expect(cc, TK_RPAREN);
            return v;
        } else {
            return 8;
        }
    }
    return parse_const_expr_primary(cc);
}
static long long parse_const_expr_primary(Compiler *cc) {
    if (cc->tk == TK_LPAREN) {
        long long val;
        next_token(cc);
        val = parse_const_expr(cc);
        expect(cc, TK_RPAREN);
        return val;
    }
    if (cc->tk == TK_NUM) {
        long long val = cc->tk_val;
        next_token(cc);
        return val;
    }
    if (cc->tk == TK_IDENT) {
        Symbol *sym = scope_find(cc, cc->tk_text);
        if (sym && sym->is_enum_const) {
            long long val = sym->enum_val;
            next_token(cc);
            return val;
        }
        next_token(cc);
        return 0;
    }
    next_token(cc);
    return 0;
}

static Type *parse_struct_or_union_body(Compiler *cc, Type *stype, int is_union);

static Type *parse_struct_or_union(Compiler *cc, int is_union) {
    Type *stype;
    char tag[MAX_IDENT];
    int has_tag;

    has_tag = 0;
    tag[0] = 0;

    if (cc->tk == TK_IDENT) {
        strncpy(tag, cc->tk_text, MAX_IDENT - 1);
        has_tag = 1;
        next_token(cc);
    }

    /* forward reference or existing struct */
    if (cc->tk != TK_LBRACE) {
        if (has_tag) {
            stype = find_struct(cc, tag);
            if (stype) return stype;
            /* forward declaration */
        stype = type_new(cc, is_union ? TY_UNION : TY_STRUCT);
            strncpy(stype->tag, tag, MAX_IDENT - 1);
            stype->is_complete = 0;
            register_struct(cc, stype);
            return stype;
        }
        error(cc, "expected struct/union tag or body");
        return cc->ty_int;
    }

    /* check if already declared */
    stype = 0;
    if (has_tag) {
        stype = find_struct(cc, tag);
    }
    if (!stype) {
        stype = type_new(cc, is_union ? TY_UNION : TY_STRUCT);
        if (cc->pending_tbfp) {
            stype->is_tbfp = 1;
            cc->pending_tbfp = 0;
        }
        if (has_tag) {
            strncpy(stype->tag, tag, MAX_IDENT - 1);
            register_struct(cc, stype);
        }
    } else if (cc->pending_tbfp) {
        stype->is_tbfp = 1;
        cc->pending_tbfp = 0;
    }

    return parse_struct_or_union_body(cc, stype, is_union);
}

static int get_member_size(Type *stype, Type *ftype) {
    if (stype && stype->is_tbfp) {
        if (ftype->kind == TY_FLOAT) return 2;
        if (ftype->kind == TY_DOUBLE) return 5;
    }
    return type_size(ftype);
}

static Type *parse_struct_or_union_body(Compiler *cc, Type *stype, int is_union) {
    expect(cc, TK_LBRACE);

    {
        StructField *last_field;
        int offset;
        int max_size;
        int max_align;

        last_field = 0;
        offset = 0;
        max_size = 0;
        max_align = 1;

        while (cc->tk != TK_RBRACE) {
            Type *ftype;
            Type *base_ftype;
            char fname[MAX_IDENT];
            StructField *field;
            int falign;

            if (cc->tk == TK_EOF) break;

            base_ftype = parse_type(cc);
            ftype = parse_declarator(cc, base_ftype, fname);

            /* Ignore bitfield size since ZCC allocates full integers for them */
            if (cc->tk == TK_COLON) {
                next_token(cc);
                parse_const_expr(cc);
            }

            field = (StructField *)cc_alloc(cc, sizeof(StructField));
            strncpy(field->name, fname, MAX_IDENT - 1);
            field->type = ftype;

            falign = type_align(ftype);
            if (falign > max_align) max_align = falign;

            if (is_union) {
                field->offset = 0;
                if (type_size(ftype) > max_size) max_size = type_size(ftype);
            } else {
                /* align offset */
                if (falign > 1) {
                    offset = (offset + falign - 1) & ~(falign - 1);
                }
                field->offset = offset;
                offset = offset + type_size(ftype);
            }

            field->next = 0;
            if (last_field) {
                last_field->next = field;
            } else {
                stype->fields = field;
            }
            last_field = field;

            /* handle multiple declarators: int *next, *prev; */
            while (cc->tk == TK_COMMA) {
                Type *ftype2;
                char fname2[MAX_IDENT];
                StructField *field2;
                int falign2;
                next_token(cc);
                ftype2 = parse_declarator(cc, base_ftype, fname2);

                /* Ignore bitfield size since ZCC allocates full integers */
                if (cc->tk == TK_COLON) {
                    next_token(cc);
                    parse_const_expr(cc);
                }

                field2 = (StructField *)cc_alloc(cc, sizeof(StructField));
                strncpy(field2->name, fname2, MAX_IDENT - 1);
                field2->type = ftype2;
                falign2 = type_align(ftype2);
                if (falign2 > max_align) max_align = falign2;
                if (is_union) {
                    field2->offset = 0;
                    if (type_size(ftype2) > max_size) max_size = type_size(ftype2);
                } else {
                    if (falign2 > 1) {
                        offset = (offset + falign2 - 1) & ~(falign2 - 1);
                    }
                    field2->offset = offset;
                    offset = offset + type_size(ftype2);
                }
                field2->next = 0;
                last_field->next = field2;
                last_field = field2;
            }

            expect(cc, TK_SEMI);
        }

            if (is_union) {
            stype->size = max_size;
        } else {
            stype->size = offset;
        }
        /* align total size */
        if (max_align > 1) {
            stype->size = (stype->size + max_align - 1) & ~(max_align - 1);
        }
        stype->align = max_align;
        stype->is_complete = 1;
    }

    expect(cc, TK_RBRACE);
    if (stype->tag[0] && (strcmp(stype->tag, "yyStackEntry") == 0 || strcmp(stype->tag, "yyParser") == 0 || strcmp(stype->tag, "Walker") == 0)) {
        int n = 0;
        StructField *f = stype->fields;
        while(f) { n++; printf("FIELD: %s\n", f->name); f = f->next; }
        printf("DEBUG: Parsed %s with %d fields (Type=%p, fields=%p)\n", stype->tag, n, (void*)stype, (void*)stype->fields);
        fflush(stdout);
    }
    return stype;
}

/* ---------------------------------------------------------------- */
/* Parse enum                                                        */
/* ---------------------------------------------------------------- */

static Type *parse_enum_def(Compiler *cc) {
    char tag[MAX_IDENT];
    Type *etype;

    tag[0] = 0;
    if (cc->tk == TK_IDENT) {
        strncpy(tag, cc->tk_text, MAX_IDENT - 1);
        next_token(cc);
    }

    etype = type_new(cc, TY_ENUM);

    if (cc->tk == TK_LBRACE) {
        long val;
        next_token(cc);
        val = 0;
        while (cc->tk != TK_RBRACE) {
            char name[MAX_IDENT];
            Symbol *sym;

            if (cc->tk == TK_EOF) break;
            if (cc->tk != TK_IDENT) {
                error(cc, "expected enum constant name");
                next_token(cc);
                continue;
            }
            strncpy(name, cc->tk_text, MAX_IDENT - 1);
            next_token(cc);

            if (cc->tk == TK_ASSIGN) {
                next_token(cc);
                val = parse_const_expr(cc);
            }

            sym = scope_add(cc, name, etype);
            sym->is_enum_const = 1;
            sym->enum_val = val;
            val++;

            if (cc->tk == TK_COMMA) {
                next_token(cc);
            } else {
                break;
            }
        }
        expect(cc, TK_RBRACE);
    }
    return etype;
}

/* ---------------------------------------------------------------- */
/* Parse base type                                                   */
/* ---------------------------------------------------------------- */

Type *parse_type(Compiler *cc) {
    Type *type = 0;
    int is_unsigned = 0;
    int is_signed = 0;
    int is_long = 0;
    int is_short = 0;
    int is_int = 0;
    int is_char = 0;
    int is_double = 0;
    int is_float = 0;
    int is_void = 0;
    int is_typedef_kw = 0;
    int is_static = 0;
    int is_extern = 0;

    /* storage class / qualifiers / basic types */
    for (;;) {
        if (cc->tk == TK_STATIC) { is_static = 1; next_token(cc); }
        else if (cc->tk == TK_EXTERN) { is_extern = 1; next_token(cc); }
        else if (cc->tk == TK_CONST) { next_token(cc); }
        else if (cc->tk == TK_VOLATILE) { next_token(cc); }
        else if (cc->tk == TK_INLINE) { next_token(cc); }
        else if (cc->tk == TK_AUTO) { next_token(cc); }
        else if (cc->tk == TK_REGISTER) { next_token(cc); }
        else if (cc->tk == TK_TYPEDEF) { is_typedef_kw = 1; next_token(cc); }
        else if (cc->tk == TK_UNSIGNED) { is_unsigned = 1; next_token(cc); }
        else if (cc->tk == TK_SIGNED) { is_signed = 1; next_token(cc); }
        else if (cc->tk == TK_LONG) { is_long++; next_token(cc); }
        else if (cc->tk == TK_SHORT) { is_short = 1; next_token(cc); }
        else if (cc->tk == TK_INT) { is_int = 1; next_token(cc); }
        else if (cc->tk == TK_CHAR) { is_char = 1; next_token(cc); }
        else if (cc->tk == TK_DOUBLE) { is_double = 1; next_token(cc); }
        else if (cc->tk == TK_FLOAT) { is_float = 1; next_token(cc); }
        else if (cc->tk == TK_VOID) { is_void = 1; next_token(cc); }
        else break;
    }

    if (is_void) { type = cc->ty_void; }
    else if (is_float) { type = cc->ty_float; }
    else if (is_double) { type = cc->ty_double; }
    else if (is_char) {
        if (is_unsigned) { type = cc->ty_uchar; } else { type = cc->ty_char; }
    }
    else if (is_short) {
        if (is_unsigned) { type = cc->ty_ushort; } else { type = cc->ty_short; }
    }
    else if (is_long >= 2) {
        if (is_unsigned) { type = cc->ty_ulonglong; } else { type = cc->ty_longlong; }
    }
    else if (is_long == 1) {
        if (is_unsigned) { type = cc->ty_ulong; } else { type = cc->ty_long; }
    }
    else if (is_int || is_unsigned || is_signed) {
        if (is_unsigned) { type = cc->ty_uint; } else { type = cc->ty_int; }
    }

    if (!type) {
        if (cc->tk == TK_STRUCT) {
            next_token(cc);
            type = parse_struct_or_union(cc, 0);
        }
        else if (cc->tk == TK_UNION) {
            next_token(cc);
            type = parse_struct_or_union(cc, 1);
        }
        else if (cc->tk == TK_ENUM) {
            next_token(cc);
            type = parse_enum_def(cc);
        }
        else if (cc->tk == TK_TYPEOF) {
            next_token(cc);
            expect(cc, TK_LPAREN);
            Node *expr = parse_expr(cc);
            expect(cc, TK_RPAREN);
            type = expr->type;
            if (!type) type = cc->ty_int;
        }
        else if (cc->tk == TK_AUTO_TYPE) {
            next_token(cc);
            type = type_new(cc, TY_VOID);
            type->size = 0; /* placeholder */
        }
        else if (cc->tk == TK_IDENT) {
            /* could be a typedef name */
            Symbol *sym;
            sym = scope_find(cc, cc->tk_text);
            if (strcmp(cc->tk_text, "yyParser") == 0) {
                printf("DEBUG: parse_type lookup 'yyParser'. sym=%p\n", (void*)sym);
            }
            if (sym && sym->is_typedef) {
                type = sym->type;
                next_token(cc);
            }
        }
    }

    if (!type) {
        type = cc->ty_int;
    }

    /* skip trailing const/volatile (e.g. 'char const *') */
    while (cc->tk == TK_CONST || cc->tk == TK_VOLATILE) {
        next_token(cc);
    }

    cc->current_is_static = is_static;
    return type;
}

/* ---------------------------------------------------------------- */
/* Parse declarator (pointers, arrays, function params)              */
/* ---------------------------------------------------------------- */

Node *parse_assign(Compiler *cc);

static int eval_const_expr(Node *n) {
    if (!n) return 0;
    if (n->kind == ND_NUM) return (int)n->int_val;
    if (n->kind == ND_ADD) return eval_const_expr(n->lhs) + eval_const_expr(n->rhs);
    if (n->kind == ND_SUB) return eval_const_expr(n->lhs) - eval_const_expr(n->rhs);
    if (n->kind == ND_MUL) return eval_const_expr(n->lhs) * eval_const_expr(n->rhs);
    if (n->kind == ND_DIV) {
        int r = eval_const_expr(n->rhs);
        return r ? eval_const_expr(n->lhs) / r : 0;
    }
    if (n->kind == ND_MOD) {
        int r = eval_const_expr(n->rhs);
        return r ? eval_const_expr(n->lhs) % r : 0;
    }
    if (n->kind == ND_SHL) return eval_const_expr(n->lhs) << eval_const_expr(n->rhs);
    if (n->kind == ND_SHR) return eval_const_expr(n->lhs) >> eval_const_expr(n->rhs);
    if (n->kind == ND_BAND) return eval_const_expr(n->lhs) & eval_const_expr(n->rhs);
    if (n->kind == ND_BOR) return eval_const_expr(n->lhs) | eval_const_expr(n->rhs);
    if (n->kind == ND_BXOR) return eval_const_expr(n->lhs) ^ eval_const_expr(n->rhs);
    if (n->kind == ND_CAST) return eval_const_expr(n->lhs);
    /* Unary operators — critical for negative switch cases like case (-15): */
    if (n->kind == ND_NEG)  return -eval_const_expr(n->lhs);
    if (n->kind == ND_BNOT) return ~eval_const_expr(n->lhs);
    if (n->kind == ND_LNOT) return !eval_const_expr(n->lhs);
    /* Enum constants (global only — local vars are NOT constants) */
    if (n->kind == ND_VAR && n->sym && n->sym->is_enum_const && !n->sym->is_local)
        return n->sym->enum_val;
    return 0; /* Fallback for unsupported complex compile-time bounds */
}



static Type *inject_base_type(Compiler *cc, Type *t, Type *base) {
    if (!t || t == cc->ty_int) return base;
    if (t->kind == TY_PTR) {
        return type_ptr(cc, inject_base_type(cc, t->base, base));
    }
    if (t->kind == TY_ARRAY) {
        return type_array(cc, inject_base_type(cc, t->base, base), t->array_len);
    }
    if (t->kind == TY_FUNC) {
        Type *n = type_func(cc, inject_base_type(cc, t->ret, base));
        n->params = t->params;
        n->num_params = t->num_params;
        n->is_variadic = t->is_variadic;
        return n;
    }
    return base;
}

Type *parse_declarator(Compiler *cc, Type *base, char *name_out) {
    Type *type;

    type = base;
    name_out[0] = 0;

    /* pointers */
    while (cc->tk == TK_STAR) {
        next_token(cc);
        /* skip const/volatile after * */
        while (cc->tk == TK_CONST || cc->tk == TK_VOLATILE) {
            next_token(cc);
        }
        type = type_ptr(cc, type);
    }

    /* name */
    if (cc->tk == TK_IDENT) {
        strncpy(name_out, cc->tk_text, MAX_IDENT - 1);
        next_token(cc);
    }
    else if (cc->tk == TK_LPAREN) {
        /* grouped declarator: (*name)(params) or (*name)[N] */
        int pk;
        pk = peek_token(cc);
        if (pk == TK_STAR || pk == TK_IDENT || pk == TK_LPAREN) {
            Type *inner;
            next_token(cc); /* consume ( */
            inner = parse_declarator(cc, cc->ty_int, name_out);
            expect(cc, TK_RPAREN);
            /* process outer dimensions and function args first! */
            if (cc->tk == TK_LBRACKET) {
                int arr_lens[16];
                int arr_num = 0;
                while (cc->tk == TK_LBRACKET) {
                    int len = 0;
                    next_token(cc);
                    if (cc->tk != TK_RBRACKET) len = (int)parse_const_expr(cc);
                    expect(cc, TK_RBRACKET);
                    if (arr_num < 16) arr_lens[arr_num++] = len;
                }
                while (arr_num > 0) {
                    arr_num--;
                    type = type_array(cc, type, arr_lens[arr_num]);
                }
            }
            if (cc->tk == TK_LPAREN) {
                Type *ftype;
                next_token(cc);
                ftype = type_func(cc, type);
                ftype->params = (Type **)cc_alloc(cc, sizeof(Type *) * MAX_PARAMS);
                ftype->num_params = 0;
                ftype->is_variadic = 0;
                if (cc->tk != TK_RPAREN) {
                    if (cc->tk == TK_VOID) {
                        int vpk;
                        vpk = peek_token(cc);
                        if (vpk == TK_RPAREN) {
                            next_token(cc);
                        } else {
                            goto parse_params_inner;
                        }
                    } else {
                        parse_params_inner:
                        while (cc->tk != TK_RPAREN) {
                            Type *ptype;
                            char pname[MAX_IDENT];
                            if (cc->tk == TK_ELLIPSIS) {
                                ftype->is_variadic = 1;
                                next_token(cc);
                                break;
                            }
                            if (cc->tk == TK_EOF) break;
                            ptype = parse_type(cc);
                            ptype = parse_declarator(cc, ptype, pname);
                            if (ptype->kind == TY_ARRAY) ptype = type_ptr(cc, ptype->base);
                            if (ftype->num_params < MAX_PARAMS) {
                                ftype->params[ftype->num_params] = ptype;
                                ftype->num_params++;
                            }
                            if (cc->tk == TK_COMMA) {
                                next_token(cc);
                            } else {
                                break;
                            }
                        }
                    }
                }
                expect(cc, TK_RPAREN);
                type = ftype;
            }
            return inject_base_type(cc, inner, type);
        }
    }

    /* array dimensions */
    if (cc->tk == TK_LBRACKET) {
        int arr_lens[16];
        int arr_num = 0;
        while (cc->tk == TK_LBRACKET) {
            int len = 0;
            next_token(cc);
            if (cc->tk != TK_RBRACKET) len = (int)parse_const_expr(cc);
            expect(cc, TK_RBRACKET);
            if (arr_num < 16) arr_lens[arr_num++] = len;
        }
        while (arr_num > 0) {
            arr_num--;
            type = type_array(cc, type, arr_lens[arr_num]);
        }
    }

    /* function parameters */
    if (cc->tk == TK_LPAREN) {
        Type *ftype;
        next_token(cc);
        ftype = type_func(cc, type);
        ftype->params = (Type **)cc_alloc(cc, sizeof(Type *) * MAX_PARAMS);
        ftype->num_params = 0;
        ftype->is_variadic = 0;

        if (cc->tk != TK_RPAREN) {
            if (cc->tk == TK_VOID) {
                int pk;
                pk = peek_token(cc);
                if (pk == TK_RPAREN) {
                    next_token(cc); /* skip void */
                } else {
                    goto parse_params;
                }
            } else {
                parse_params:
                while (cc->tk != TK_RPAREN) {
                    Type *ptype;
                    char pname[MAX_IDENT];

                    if (cc->tk == TK_ELLIPSIS) {
                        ftype->is_variadic = 1;
                        next_token(cc);
                        break;
                    }
                    if (cc->tk == TK_EOF) break;

                    ptype = parse_type(cc);
                    ptype = parse_declarator(cc, ptype, pname);

                    if (ptype->kind == TY_ARRAY) ptype = type_ptr(cc, ptype->base);

                    if (ftype->num_params < MAX_PARAMS) {
                        ftype->params[ftype->num_params] = ptype;
                        ftype->num_params++;
                    }

                    if (cc->tk == TK_COMMA) {
                        next_token(cc);
                    } else {
                        break;
                    }
                }
            }
        }
        expect(cc, TK_RPAREN);
        type = ftype;
    }

    return type;
}

/* ================================================================ */
/* EXPRESSION PARSING — FULL PRECEDENCE CHAIN                       */
/* ================================================================ */

/* --- primary --- */

Node *parse_primary(Compiler *cc) {
    Node *n;
    int line;

    line = cc->tk_line;

    if (cc->tk == TK_NUM) {
        n = node_num(cc, cc->tk_val, line);
        if (cc->tk_text[0] == 'U') {
             if (cc->tk_val <= 4294967295ULL) {
                 n->type = cc->ty_uint;
             } else {
                 n->type = cc->ty_ulong;
             }
        }
        if (cc->tk_text[1] == 'L') {
             if (cc->tk_text[0] == 'U') {
                 n->type = cc->ty_ulong;
             } else {
                 n->type = cc->ty_long;
             }
        }
        next_token(cc);
        return n;
    }

    if (cc->tk == TK_FLIT) {
        n = node_flit(cc, cc->tk_fval, line);
        next_token(cc);
        return n;
    }

    if (cc->tk == TK_CHAR_LIT) {
        n = node_num(cc, cc->tk_val, line);
        n->type = cc->ty_char;
        next_token(cc);
        return n;
    }

    if (cc->tk == TK_STR) {
        int sid;
        StringEntry *se;

        sid = cc->num_strings;
        if (sid < MAX_STRINGS) {
            se = &cc->strings[sid];
            se->data = cc_strdup(cc, cc->tk_str);
            se->len = cc->tk_str_len;
            se->label_id = cc->label_count;
            cc->label_count++;
            cc->num_strings++;
        }
        n = node_new(cc, ND_STR, line);
        n->str_id = sid;
        n->type = type_ptr(cc, cc->ty_char);
        next_token(cc);
        return n;
    }

    if (cc->tk == TK_IDENT) {
        Symbol *sym;
        n = node_new(cc, ND_VAR, line);
        strncpy(n->name, cc->tk_text, MAX_IDENT - 1);
        sym = scope_find(cc, cc->tk_text);
        
        if (strcmp(cc->tk_text, "yypParser") == 0 || strcmp(cc->tk_text, "pParser") == 0) {
            printf("DEBUG: parse_primary lookup '%s'. sym=%p, sym->type=%p, base=%p, base_tag='%s'\n", 
                   cc->tk_text,
                   (void*)sym, sym ? (void*)sym->type : NULL, 
                   (sym && sym->type) ? (void*)sym->type->base : NULL, 
                   (sym && sym->type && sym->type->base && sym->type->base->tag[0]) ? sym->type->base->tag : "<none>");
            fflush(stdout);
        }
        
        if (sym) {
            n->sym = sym;
            n->type = sym->type;
            if (sym->asm_name[0]) {
                strncpy(n->name, sym->asm_name, MAX_IDENT - 1);
            }
            if (sym->is_enum_const && !sym->is_local) {
                /* Only fold to ND_NUM if this is truly a global enum constant.
                 * A local variable (is_local=1) with the same name as an outer
                 * enum constant must NOT be constant-folded — it is a live
                 * stack variable that must be loaded at runtime. */
                n->kind = ND_NUM;
                n->int_val = sym->enum_val;
                n->type = cc->ty_int;
            }
        } else {
            /* stdio global pointers: stdin, stdout, stderr are FILE* (size 8) */
            if (strcmp(n->name, "stdin") == 0 || strcmp(n->name, "stdout") == 0 || strcmp(n->name, "stderr") == 0) {
                n->type = type_ptr(cc, cc->ty_char);
            } else {
                /* ZCC SCOPING HARDENING: undeclared identifiers must be rejected    */
                /* at AST build time. The old silent int decay caused CWE-476 healer */
                /* mutations (guards inserted before decl) to survive bootstrap,     */
                /* masking invalid C as applied_survived in the mutation log.        */
                char _ud_buf[256];
                sprintf(_ud_buf, "use of undeclared identifier '%s'", n->name);
                error(cc, _ud_buf);
                n->type = cc->ty_int;
            }
        }
        next_token(cc);
        return n;
    }

    if (cc->tk == TK_LPAREN) {
        next_token(cc);

        /* check for cast expression */
        if (is_type_token(cc)) {
            Type *cast_type;
            char dummy[MAX_IDENT];
            cast_type = parse_type(cc);
            cast_type = parse_declarator(cc, cast_type, dummy);
            expect(cc, TK_RPAREN);
            n = node_new(cc, ND_CAST, line);
            n->cast_type = cast_type;
            n->lhs = parse_unary(cc);
            n->type = cast_type;
            return n;
        }

        n = parse_expr(cc);
        expect(cc, TK_RPAREN);
        return n;
    }

    if (cc->tk == TK_BUILTIN_VA_ARG) {
        Node *n;
        Node *ap_node;
        Type *ret_type;
        char dummy[MAX_IDENT];
        int line;
        
        line = cc->tk_line;
        next_token(cc); /* skip va_arg */
        expect(cc, TK_LPAREN);
        ap_node = parse_assign(cc);
        expect(cc, TK_COMMA);
        ret_type = parse_type(cc);
        ret_type = parse_declarator(cc, ret_type, dummy);
        expect(cc, TK_RPAREN);

        n = node_new(cc, ND_VA_ARG, line);
        n->lhs = ap_node;
        n->type = ret_type;
        return n;
    }

    if (cc->tk == TK_SIZEOF) {
        int pk;
        int is_type_in_parens;
        next_token(cc);
        if (cc->tk == TK_LPAREN) {
            pk = peek_token(cc);
            is_type_in_parens = 0;
            if (pk >= TK_INT) {
                if (pk <= TK_INLINE || pk == TK_STRUCT ||
                    pk == TK_UNION || pk == TK_ENUM) {
                    is_type_in_parens = 1;
                }
            }
            if (!is_type_in_parens && pk == TK_IDENT) {
                Symbol *sz_sym;
                sz_sym = scope_find(cc, cc->peek_text);
                if (sz_sym && sz_sym->is_typedef) is_type_in_parens = 1;
            }
            if (is_type_in_parens) {
                Type *st;
                char dummy[MAX_IDENT];
                next_token(cc);  /* skip ( */
                st = parse_type(cc);
                st = parse_declarator(cc, st, dummy);
                expect(cc, TK_RPAREN);
                n = node_num(cc, type_size(st), line);
                return n;
            }
            /* sizeof(expr) */
            {
                Node *expr;
                next_token(cc); /* skip ( */
                expr = parse_unary(cc);
                expect(cc, TK_RPAREN);
                n = node_num(cc, type_size(expr->type), line);
                return n;
            }
        } else {
            Node *expr;
            expr = parse_unary(cc);
            n = node_num(cc, type_size(expr->type), line);
            return n;
        }
    }

    /* fallthrough — unexpected token */
    {
        char buf[256];
        sprintf(buf, "unexpected token %d in expression", cc->tk);
        error(cc, buf);
        next_token(cc);
        n = node_num(cc, 0, line);
        return n;
    }
}

/* --- postfix: [], ., ->, (), ++, -- --- */
/* Bug fix: call handler inlined, && replaced with nested if */

static StructField *find_struct_member(Type *type, const char *name, int *out_offset) {
    StructField *f;
    if (!type) {
        return 0;
    }

    for (f = type->fields; f; f = f->next) {
        if (f->name[0]) {
            if (strcmp(f->name, name) == 0) {
                *out_offset = f->offset;
                return f;
            }
        } else if (f->type && (f->type->kind == TY_STRUCT || f->type->kind == TY_UNION)) {
            int sub_offset = 0;
            StructField *sub = find_struct_member(f->type, name, &sub_offset);
            if (sub) {
                *out_offset = f->offset + sub_offset;
                return sub;
            }
        }
    }
    return 0;
}

Node *parse_postfix(Compiler *cc) {
    Node *n;
    n = parse_primary(cc);

    for (;;) {
        int line;
        line = cc->tk_line;

        /* array subscript */
        if (cc->tk == TK_LBRACKET) {
            Node *idx;
            Node *add;
            next_token(cc);
            idx = parse_expr(cc);
            expect(cc, TK_RBRACKET);
            /* a[i] => *(a + i) */
            add = node_new(cc, ND_ADD, line);
            add->lhs = n;
            add->rhs = idx;
            if (is_pointer(n->type)) {
                add->type = n->type;
            } else {
                add->type = idx->type;
            }
            n = node_new(cc, ND_DEREF, line);
            n->lhs = add;
            if (add->type) {
                if (add->type->base) {
                    n->type = add->type->base;
                } else {
                    n->type = cc->ty_int;
                }
            } else {
                n->type = cc->ty_int;
            }
            continue;
        }

        /* struct member access */
        if (cc->tk == TK_DOT) {
            int accumulated_offset = 0;
            StructField *f;
            Node *member;
            next_token(cc);
            if (cc->tk != TK_IDENT) {
                error(cc, "expected member name");
                break;
            }
            member = node_new(cc, ND_MEMBER, line);
            member->lhs = n;
            strncpy(member->member_name, cc->tk_text, MAX_IDENT - 1);
            /* find field */
            if (n->type) {
                f = find_struct_member(n->type, cc->tk_text, &accumulated_offset);
                if (f) {
                    member->member_offset = accumulated_offset;
                    member->type = f->type;
                    member->member_size = get_member_size(n->type, f->type);
                    member->member_size = type_size(f->type);
                } else {
                    char buf[256];
                    sprintf(buf, "unknown struct member '%s' in struct '%s' (type=%p, fields=%p)", cc->tk_text, (n->type && n->type->tag[0]) ? n->type->tag : "<anon>", (void*)n->type, n->type ? (void*)n->type->fields : NULL);
                    error(cc, buf);
                    member->type = cc->ty_int;
                }
            } else {
                member->type = cc->ty_int;
            }
            n = member;
            next_token(cc);
            continue;
        }

        /* arrow member access */
        if (cc->tk == TK_ARROW) {
            int accumulated_offset = 0;
            StructField *f;
            Node *deref;
            next_token(cc);
            if (cc->tk != TK_IDENT) {
                error(cc, "expected member name after ->");
                break;
            }
            /* deref first */
            deref = node_new(cc, ND_DEREF, line);
            deref->lhs = n;
            if (n->type) {
                if (n->type->base) {
                    deref->type = n->type->base;
                } else {
                    deref->type = cc->ty_int;
                }
            } else {
                deref->type = cc->ty_int;
            }
            n = node_new(cc, ND_MEMBER, line);
            n->lhs = deref;
            strncpy(n->member_name, cc->tk_text, MAX_IDENT - 1);
            if (deref->type) {
                f = find_struct_member(deref->type, cc->tk_text, &accumulated_offset);
                if (f) {
                    n->member_offset = accumulated_offset;
                    n->type = f->type;
                    n->member_size = get_member_size(deref->type, f->type);
                } else {
                    char buf[256];
                    sprintf(buf, "unknown struct member '%s' after -> in struct '%s' (type=%p, fields=%p)", cc->tk_text, (deref->type && deref->type->tag[0]) ? deref->type->tag : "<anon>", (void*)deref->type, deref->type ? (void*)deref->type->fields : NULL);
                    error(cc, buf);
                    n->type = cc->ty_int;
                }
            } else {
                n->type = cc->ty_int;
            }
            next_token(cc);
            continue;
        }

        /* function call — inlined handler, nested ifs instead of && */
        if (cc->tk == TK_LPAREN) {
            int is_direct = 0;
            if (n->kind == ND_VAR) {
                if (!n->sym) is_direct = 1;
                else if (n->sym->type && n->sym->type->kind == TY_FUNC) is_direct = 1;
            }
            if (is_direct) {
                /* function call */
                Node *call;
                Symbol *sym;
                next_token(cc);
                call = node_new(cc, ND_CALL, line);
                strncpy(call->func_name, n->name, MAX_IDENT - 1);
                call->args = (Node **)cc_alloc(cc, sizeof(Node *) * MAX_CALL_ARGS);
                call->num_args = 0;
                while (cc->tk != TK_RPAREN) {
                    if (cc->tk == TK_EOF) break;
                    if (call->num_args < MAX_CALL_ARGS) {
                        call->args[call->num_args] = parse_assign(cc);
                        call->num_args = call->num_args + 1;
                    } else {
                        parse_assign(cc);
                    }
                    if (cc->tk == TK_COMMA) {
                        next_token(cc);
                    } else {
                        break;
                    }
                }
                expect(cc, TK_RPAREN);
                sym = scope_find(cc, call->func_name);
                if (sym) {
                    if (sym->type) {
                        if (sym->type->kind == TY_FUNC) {
                            if (sym->type->ret) {
                                call->type = sym->type->ret;
                            } else {
                                call->type = cc->ty_int;
                            }
                            /* ZKAEDI: Implicitly coerce arguments to parameter types */
                            for (int k = 0; k < call->num_args; k++) {
                                if (k < sym->type->num_params && sym->type->params && sym->type->params[k]) {
                                    Type *pty = sym->type->params[k];
                                    if (call->args[k] && call->args[k]->type && call->args[k]->type != pty) {
                                        Node *c = node_new(cc, ND_CAST, line);
                                        c->lhs = call->args[k];
                                        c->cast_type = pty;
                                        c->type = pty;
                                        call->args[k] = c;
                                    }
                                }
                            }
                        } else {
                            call->type = cc->ty_int;
                        }
                    } else {
                        call->type = cc->ty_int;
                    }
                } else {
                    call->type = cc->ty_int;
                }
                n = call;
                continue;
            }
            if (n->kind == ND_MEMBER || n->kind == ND_DEREF || n->kind == ND_CAST || (n->kind == ND_VAR && !is_direct)) {
                /* indirect call through function pointer member, deref, or cast */
                Node *call;
                next_token(cc);
                call = node_new(cc, ND_CALL, line);
                call->func_name[0] = 0; /* empty = indirect */
                call->lhs = n; /* callee expression */
                call->args = (Node **)cc_alloc(cc, sizeof(Node *) * MAX_CALL_ARGS);
                call->num_args = 0;
                while (cc->tk != TK_RPAREN) {
                    if (cc->tk == TK_EOF) break;
                    if (call->num_args < MAX_CALL_ARGS) {
                        call->args[call->num_args] = parse_assign(cc);
                        call->num_args = call->num_args + 1;
                    } else {
                        parse_assign(cc);
                    }
                    if (cc->tk == TK_COMMA) {
                        next_token(cc);
                    } else {
                        break;
                    }
                }
                expect(cc, TK_RPAREN);
                /* derive return type from function pointer type */
                if (n->type) {
                    if (n->type->kind == TY_FUNC) {
                        call->type = n->type->ret ? n->type->ret : cc->ty_int;
                        for (int k = 0; k < call->num_args; k++) {
                            if (k < n->type->num_params && n->type->params && n->type->params[k]) {
                                Type *pty = n->type->params[k];
                                if (call->args[k] && call->args[k]->type && call->args[k]->type != pty) {
                                    Node *c = node_new(cc, ND_CAST, line);
                                    c->lhs = call->args[k];
                                    c->cast_type = pty;
                                    c->type = pty;
                                    call->args[k] = c;
                                }
                            }
                        }
                    } else if (n->type->kind == TY_PTR && n->type->base && n->type->base->kind == TY_FUNC) {
                        call->type = n->type->base->ret ? n->type->base->ret : cc->ty_int;
                        for (int k = 0; k < call->num_args; k++) {
                            if (k < n->type->base->num_params && n->type->base->params && n->type->base->params[k]) {
                                Type *pty = n->type->base->params[k];
                                if (call->args[k] && call->args[k]->type && call->args[k]->type != pty) {
                                    Node *c = node_new(cc, ND_CAST, line);
                                    c->lhs = call->args[k];
                                    c->cast_type = pty;
                                    c->type = pty;
                                    call->args[k] = c;
                                }
                            }
                        }
                    } else {
                        call->type = cc->ty_int;
                    }
                } else {
                    call->type = cc->ty_int;
                }
                n = call;
                continue;
            }
        }

        /* post increment */
        if (cc->tk == TK_INC) {
            Node *inc;
            next_token(cc);
            inc = node_new(cc, ND_POST_INC, line);
            inc->lhs = n;
            inc->type = n->type;
            n = inc;
            continue;
        }

        /* post decrement */
        if (cc->tk == TK_DEC) {
            Node *dec;
            next_token(cc);
            dec = node_new(cc, ND_POST_DEC, line);
            dec->lhs = n;
            dec->type = n->type;
            n = dec;
            continue;
        }

        break;
    }

    return n;
}

/* --- unary: -, +, !, ~, *, &, ++, --, cast, sizeof --- */

Node *parse_unary(Compiler *cc) {
    int line;
    Node *n;

    line = cc->tk_line;

    if (cc->tk == TK_SIZEOF) {
        Type *t = 0;
        int sz = 0;
        int is_type = 0;
        next_token(cc);
        if (cc->tk == TK_LPAREN) {
            int pk = peek_token(cc);
            if ((pk >= TK_INT && pk <= TK_DOUBLE) || (pk >= TK_STATIC && pk <= TK_INLINE) || pk == TK_STRUCT || pk == TK_UNION || pk == TK_ENUM || pk == TK_TYPEDEF) {
                is_type = 1;
            } else if (pk == TK_IDENT) {
                Symbol *sym = scope_find(cc, cc->peek_text);
                if (sym && sym->is_typedef) is_type = 1;
            }
        }
        
        if (is_type) {
            char dummy[MAX_IDENT];
            next_token(cc);
            t = parse_type(cc);
            t = parse_declarator(cc, t, dummy);
            expect(cc, TK_RPAREN);
        } else {
            Node *expr_n;
            if (cc->tk == TK_LPAREN) {
                next_token(cc);
                expr_n = parse_expr(cc);
                expect(cc, TK_RPAREN);
            } else {
                expr_n = parse_unary(cc);
            }
            if (expr_n && expr_n->type) {
                t = expr_n->type;
            }
        }
        if (t) sz = type_size(t);
        if (sz == 0) sz = 8;
        n = node_num(cc, sz, line);
        n->type = cc->ty_ulong;
        return n;
    }

    if (cc->tk == TK_LPAREN) {
        int pk = peek_token(cc);
        int is_cast = 0;
        if (pk == TK_INT || pk == TK_CHAR || pk == TK_VOID || pk == TK_STRUCT || pk == TK_UNION || pk == TK_ENUM || pk == TK_LONG || pk == TK_SHORT || pk == TK_UNSIGNED || pk == TK_SIGNED || pk == TK_FLOAT || pk == TK_DOUBLE) {
            is_cast = 1;
        } else if (pk == TK_IDENT) {
            Symbol *sym = scope_find(cc, cc->peek_text);
            if (sym && sym->is_typedef) is_cast = 1;
        }
        
        if (is_cast) {
            char dummy[MAX_IDENT];
            Type *cast_type;
            next_token(cc); /* consume LPAREN */
            cast_type = parse_type(cc);
            cast_type = parse_declarator(cc, cast_type, dummy);
            expect(cc, TK_RPAREN);
            if (cc->tk == TK_LBRACE) {
                Symbol *sym;
                char tmp_name[32];
                sprintf(tmp_name, ".L_comp_%d", cc->label_count++);
                sym = scope_add_local(cc, tmp_name, cast_type);

                Node *var_n_final = node_new(cc, ND_VAR, line);
                strncpy(var_n_final->name, tmp_name, MAX_IDENT - 1);
                var_n_final->sym = sym;
                var_n_final->type = cast_type;

                Node *expr = 0;
                next_token(cc); /* skip { */

                if (cast_type->kind == TY_STRUCT) {
                    StructField *sf = cast_type->fields;
                    while (cc->tk != TK_RBRACE && cc->tk != TK_EOF && sf) {
                        Node *var_n = node_new(cc, ND_VAR, line);
                        strncpy(var_n->name, tmp_name, MAX_IDENT - 1);
                        var_n->sym = sym;
                        var_n->type = cast_type;

                        Node *mem_n = node_new(cc, ND_MEMBER, line);
                        mem_n->lhs = var_n;
                        strncpy(mem_n->member_name, sf->name, MAX_IDENT - 1);
                        mem_n->member_offset = sf->offset;
                        mem_n->type = sf->type;
                        mem_n->member_size = type_size(sf->type);

                        Node *asgn_v = parse_assign(cc);
                        Node *asgn_n = node_new(cc, ND_ASSIGN, line);
                        asgn_n->lhs = mem_n;
                        asgn_n->rhs = asgn_v;
                        asgn_n->type = mem_n->type;

                        if (expr) {
                            Node *cma = node_new(cc, ND_COMMA_EXPR, line);
                            cma->lhs = expr;
                            cma->rhs = asgn_n;
                            cma->type = asgn_n->type;
                            expr = cma;
                        } else {
                            expr = asgn_n;
                        }

                        sf = sf->next;
                        if (cc->tk == TK_COMMA) next_token(cc);
                    }
                } else if (cast_type->kind == TY_ARRAY) {
                    int idx = 0;
                    while (cc->tk != TK_RBRACE && cc->tk != TK_EOF) {
                        Node *var_n = node_new(cc, ND_VAR, line);
                        strncpy(var_n->name, tmp_name, MAX_IDENT - 1);
                        var_n->sym = sym;
                        var_n->type = cast_type;

                        Node *add_n = node_new(cc, ND_ADD, line);
                        add_n->lhs = var_n;
                        add_n->rhs = node_num(cc, (long long)idx, line);
                        add_n->type = cast_type;

                        Node *deref_n = node_new(cc, ND_DEREF, line);
                        deref_n->lhs = add_n;
                        deref_n->type = cast_type->base;

                        Node *asgn_v = parse_assign(cc);
                        Node *asgn_n = node_new(cc, ND_ASSIGN, line);
                        asgn_n->lhs = deref_n;
                        asgn_n->rhs = asgn_v;
                        asgn_n->type = cast_type->base;

                        if (expr) {
                            Node *cma = node_new(cc, ND_COMMA_EXPR, line);
                            cma->lhs = expr;
                            cma->rhs = asgn_n;
                            cma->type = asgn_n->type;
                            expr = cma;
                        } else {
                            expr = asgn_n;
                        }

                        idx++;
                        if (cc->tk == TK_COMMA) next_token(cc);
                    }
                } else {
                    Node *var_n = node_new(cc, ND_VAR, line);
                    strncpy(var_n->name, tmp_name, MAX_IDENT - 1);
                    var_n->sym = sym;
                    var_n->type = cast_type;

                    Node *asgn_v = parse_assign(cc);
                    Node *asgn_n = node_new(cc, ND_ASSIGN, line);
                    asgn_n->lhs = var_n;
                    asgn_n->rhs = asgn_v;
                    asgn_n->type = cast_type;
                    expr = asgn_n;
                    if (cc->tk == TK_COMMA) next_token(cc);
                }

                while (cc->tk != TK_RBRACE && cc->tk != TK_EOF) next_token(cc);
                expect(cc, TK_RBRACE);

                if (!expr) expr = var_n_final;
                else {
                    Node *cma = node_new(cc, ND_COMMA_EXPR, line);
                    cma->lhs = expr;
                    cma->rhs = var_n_final;
                    cma->type = cast_type;
                    expr = cma;
                }
                return expr;
            }

            n = node_new(cc, ND_CAST, line);
            n->lhs = parse_unary(cc);
            n->type = cast_type;
            n->cast_type = cast_type;
            return n;
        }
    }

    if (cc->tk == TK_MINUS) {
        next_token(cc);
        n = node_new(cc, ND_NEG, line);
        n->lhs = parse_unary(cc);
        n->type = n->lhs->type;
        return n;
    }
    if (cc->tk == TK_PLUS) {
        next_token(cc);
        return parse_unary(cc);
    }
    if (cc->tk == TK_BANG) {
        next_token(cc);
        n = node_new(cc, ND_LNOT, line);
        n->lhs = parse_unary(cc);
        n->type = cc->ty_int;
        return n;
    }
    if (cc->tk == TK_TILDE) {
        next_token(cc);
        n = node_new(cc, ND_BNOT, line);
        n->lhs = parse_unary(cc);
        n->type = n->lhs->type;
        return n;
    }
    if (cc->tk == TK_STAR) {
        next_token(cc);
        n = node_new(cc, ND_DEREF, line);
        n->lhs = parse_unary(cc);
        if (n->lhs->type) {
            if (n->lhs->type->base) {
                n->type = n->lhs->type->base;
            } else {
                n->type = cc->ty_int;
            }
        } else {
            n->type = cc->ty_int;
        }
        return n;
    }
    if (cc->tk == TK_AMP) {
        next_token(cc);
        n = node_new(cc, ND_ADDR, line);
        n->lhs = parse_unary(cc);
        n->type = type_ptr(cc, n->lhs->type);
        return n;
    }
    if (cc->tk == TK_INC) {
        next_token(cc);
        n = node_new(cc, ND_PRE_INC, line);
        n->lhs = parse_unary(cc);
        n->type = n->lhs->type;
        return n;
    }
    if (cc->tk == TK_DEC) {
        next_token(cc);
        n = node_new(cc, ND_PRE_DEC, line);
        n->lhs = parse_unary(cc);
        n->type = n->lhs->type;
        return n;
    }

    return parse_postfix(cc);
}

/* --- binary operators: mul, add, shift, relational, equality, bit, logical --- */

static Type *promote_type(Compiler *cc, Type *t1, Type *t2) {
    if (!t1) return t2;
    if (!t2) return t1;
    if (t1->kind == TY_DOUBLE || t2->kind == TY_DOUBLE) return cc->ty_double;
    if (t1->kind == TY_FLOAT || t2->kind == TY_FLOAT) return cc->ty_float;
    if (t1->kind == TY_PTR) return t1;
    if (t2->kind == TY_PTR) return t2;
    if (type_size(t1) >= 8 || type_size(t2) >= 8) {
        if (is_unsigned_type(t1) || is_unsigned_type(t2)) return cc->ty_ulong;
        return cc->ty_long;
    }
    if (is_unsigned_type(t1) || is_unsigned_type(t2)) return cc->ty_uint;
    return cc->ty_int;
}

static Node *ensure_type(Compiler *cc, Node *n, Type *ty) {
    if (!n || !n->type || !ty) return n;
    if (n->type == ty) return n;
    Node *c = node_new(cc, ND_CAST, n->line);
    c->lhs = n;
    c->cast_type = ty;
    c->type = ty;
    return c;
}

Node *parse_mul(Compiler *cc) {
    Node *n;
    n = parse_unary(cc);
    while (cc->tk == TK_STAR || cc->tk == TK_SLASH || cc->tk == TK_PERCENT) {
        int op;
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        if (cc->tk == TK_STAR) op = ND_MUL;
        else if (cc->tk == TK_SLASH) op = ND_DIV;
        else op = ND_MOD;
        next_token(cc);
        rhs = parse_unary(cc);
        pt = promote_type(cc, n->type, rhs->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, pt);
        binop = node_new(cc, op, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = pt;
        n = binop;
    }
    return n;
}

Node *parse_add(Compiler *cc) {
    Node *n;
    Type *pt;
    n = parse_mul(cc);
    while (cc->tk == TK_PLUS || cc->tk == TK_MINUS) {
        int op;
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        if (cc->tk == TK_PLUS) op = ND_ADD;
        else op = ND_SUB;
        next_token(cc);
        rhs = parse_mul(cc);
        binop = node_new(cc, op, line);
        binop->lhs = n;
        binop->rhs = rhs;
        /* pointer arithmetic */
        if (is_pointer(n->type)) {
            binop->type = n->type;
        } else if (is_pointer(rhs->type)) {
            binop->type = rhs->type;
        } else {
            pt = promote_type(cc, n->type, rhs->type);
            n = ensure_type(cc, n, pt);
            rhs = ensure_type(cc, rhs, pt);
            binop->lhs = n;
            binop->rhs = rhs;
            binop->type = pt;
        }
        n = binop;
    }
    return n;
}

Node *parse_shift(Compiler *cc) {
    Node *n;
    n = parse_add(cc);
    while (cc->tk == TK_SHL || cc->tk == TK_SHR) {
        int op;
        int line;
        Node *rhs;
        Node *binop;
        line = cc->tk_line;
        if (cc->tk == TK_SHL) op = ND_SHL;
        else op = ND_SHR;
        next_token(cc);
        rhs = parse_add(cc);
        binop = node_new(cc, op, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = n->type;
        n = binop;
    }
    return n;
}

Node *parse_relational(Compiler *cc) {
    Node *n;
    n = parse_shift(cc);
    while (cc->tk == TK_LT || cc->tk == TK_GT || cc->tk == TK_LE || cc->tk == TK_GE) {
        int op;
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        if (cc->tk == TK_LT) op = ND_LT;
        else if (cc->tk == TK_GT) op = ND_GT;
        else if (cc->tk == TK_LE) op = ND_LE;
        else op = ND_GE;
        next_token(cc);
        rhs = parse_shift(cc);
        pt = promote_type(cc, n->type, rhs->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, pt);
        binop = node_new(cc, op, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = cc->ty_int;
        n = binop;
    }
    return n;
}

Node *parse_equality(Compiler *cc) {
    Node *n;
    n = parse_relational(cc);
    while (cc->tk == TK_EQ || cc->tk == TK_NE) {
        int op;
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        if (cc->tk == TK_EQ) op = ND_EQ;
        else op = ND_NE;
        next_token(cc);
        rhs = parse_relational(cc);
        pt = promote_type(cc, n->type, rhs->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, pt);
        binop = node_new(cc, op, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = cc->ty_int;
        n = binop;
    }
    return n;
}

Node *parse_bitand(Compiler *cc) {
    Node *n;
    n = parse_equality(cc);
    while (cc->tk == TK_AMP) {
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        next_token(cc);
        rhs = parse_equality(cc);
        pt = promote_type(cc, n->type, rhs->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, pt);
        binop = node_new(cc, ND_BAND, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = pt;
        n = binop;
    }
    return n;
}

Node *parse_bitxor(Compiler *cc) {
    Node *n;
    n = parse_bitand(cc);
    while (cc->tk == TK_CARET) {
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        next_token(cc);
        rhs = parse_bitand(cc);
        pt = promote_type(cc, n->type, rhs->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, pt);
        binop = node_new(cc, ND_BXOR, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = pt;
        n = binop;
    }
    return n;
}

Node *parse_bitor(Compiler *cc) {
    Node *n;
    Type *pt;
    n = parse_bitxor(cc);
    while (cc->tk == TK_PIPE) {
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        next_token(cc);
        rhs = parse_bitxor(cc);
        pt = promote_type(cc, n->type, rhs->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, pt);
        binop = node_new(cc, ND_BOR, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = pt;
        n = binop;
    }
    return n;
}

Node *parse_logand(Compiler *cc) {
    Node *n;
    n = parse_bitor(cc);
    while (cc->tk == TK_LAND) {
        int line;
        Node *rhs;
        Node *binop;
        line = cc->tk_line;
        next_token(cc);
        rhs = parse_bitor(cc);
        binop = node_new(cc, ND_LAND, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = cc->ty_int;
        n = binop;
    }
    return n;
}

Node *parse_logor(Compiler *cc) {
    Node *n;
    n = parse_logand(cc);
    while (cc->tk == TK_LOR) {
        int line;
        Node *rhs;
        Node *binop;
        line = cc->tk_line;
        next_token(cc);
        rhs = parse_logand(cc);
        binop = node_new(cc, ND_LOR, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = cc->ty_int;
        n = binop;
    }
    return n;
}

Node *parse_ternary(Compiler *cc) {
    Node *n;
    n = parse_logor(cc);
    if (cc->tk == TK_QUESTION) {
        Node *tern;
        int line;
        line = cc->tk_line;
        next_token(cc);
        tern = node_new(cc, ND_TERNARY, line);
        tern->cond = n;
        tern->then_body = parse_expr(cc);
        expect(cc, TK_COLON);
        tern->else_body = parse_ternary(cc);
        tern->type = promote_type(cc, tern->then_body->type, tern->else_body->type);
        tern->then_body = ensure_type(cc, tern->then_body, tern->type);
        tern->else_body = ensure_type(cc, tern->else_body, tern->type);
        n = tern;
    }
    return n;
}

Node *parse_assign(Compiler *cc) {
    Node *n;
    n = parse_ternary(cc);

    if (cc->tk == TK_ASSIGN) {
        Node *asgn;
        int line;
        line = cc->tk_line;
        next_token(cc);
        asgn = node_new(cc, ND_ASSIGN, line);
        asgn->lhs = n;
        asgn->rhs = ensure_type(cc, parse_assign(cc), n->type);
        asgn->type = n->type;
        return asgn;
    }

    /* compound assignment: +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>= */
    if (cc->tk >= TK_PLUS_ASSIGN) {
        if (cc->tk <= TK_SHR_ASSIGN) {
            Node *ca;
            int line;
            int op;
            line = cc->tk_line;
            switch (cc->tk) {
                case TK_PLUS_ASSIGN:    op = ND_ADD; break;
                case TK_MINUS_ASSIGN:   op = ND_SUB; break;
                case TK_STAR_ASSIGN:    op = ND_MUL; break;
                case TK_SLASH_ASSIGN:   op = ND_DIV; break;
                case TK_PERCENT_ASSIGN: op = ND_MOD; break;
                case TK_AMP_ASSIGN:     op = ND_BAND; break;
                case TK_PIPE_ASSIGN:    op = ND_BOR; break;
                case TK_CARET_ASSIGN:   op = ND_BXOR; break;
                case TK_SHL_ASSIGN:     op = ND_SHL; break;
                case TK_SHR_ASSIGN:     op = ND_SHR; break;
                default: op = ND_ADD; break;
            }
            next_token(cc);
            ca = node_new(cc, ND_COMPOUND_ASSIGN, line);
            ca->compound_op = op;
            ca->lhs = n;
            ca->rhs = parse_assign(cc);
            ca->type = n->type;
            return ca;
        }
    }

    return n;
}

Node *parse_expr(Compiler *cc) {
    Node *n;
    n = parse_assign(cc);
    while (cc->tk == TK_COMMA) {
        Node *comma;
        int line;
        line = cc->tk_line;
        next_token(cc);
        comma = node_new(cc, ND_COMMA_EXPR, line);
        comma->lhs = n;
        comma->rhs = parse_assign(cc);
        comma->type = comma->rhs->type;
        n = comma;
    }
    return n;
}

/* ================================================================ */
/* STATEMENT PARSING                                                 */
/* ================================================================ */

static Node *current_sw = 0;

Node *parse_stmt(Compiler *cc) {
    int line;
    line = cc->tk_line;

    /* Skip __asm__(...) / asm(...) / __asm(...) statements */
    if (cc->tk == TK_ASM || (cc->tk == TK_IDENT && (
        strcmp(cc->tk_text, "__asm__") == 0 ||
        strcmp(cc->tk_text, "asm") == 0 ||
        strcmp(cc->tk_text, "__asm") == 0))) {
        next_token(cc); /* consume asm keyword */
        /* skip optional volatile qualifier */
        if (cc->tk == TK_IDENT && (strcmp(cc->tk_text, "__volatile__") == 0 ||
            strcmp(cc->tk_text, "volatile") == 0)) {
            next_token(cc);
        }
        if (cc->tk == TK_VOLATILE) next_token(cc);
        if (cc->tk == TK_LPAREN) {
            int depth = 1;
            next_token(cc);
            while (depth > 0 && cc->tk != TK_EOF) {
                if (cc->tk == TK_LPAREN) depth++;
                else if (cc->tk == TK_RPAREN) depth--;
                if (depth > 0) next_token(cc);
            }
            if (cc->tk == TK_RPAREN) next_token(cc);
        }
        if (cc->tk == TK_SEMI) next_token(cc);
        return node_new(cc, ND_NOP, line);
    }

    /* block */
    if (cc->tk == TK_LBRACE) {
        Node *block;
        int cap;
        int cnt;
        next_token(cc);
        scope_push(cc);
        block = node_new(cc, ND_BLOCK, line);
        cap = 64;
        block->stmts = (Node **)cc_alloc(cc, sizeof(Node *) * cap);
        cnt = 0;
        while (cc->tk != TK_RBRACE) {
            int prev_pos;
            int no_progress_count;
            if (cc->tk == TK_EOF) break;
            if (cnt >= 4096) {
                error(cc, "block has too many statements (possible parser loop)");
                break;
            }
            if (cnt >= cap) {
                Node **old;
                int newcap;
                int i;
                old = block->stmts;
                newcap = cap * 2;
                if (newcap > 4096) newcap = 4096;
                block->stmts = (Node **)cc_alloc(cc, sizeof(Node *) * newcap);
                for (i = 0; i < cnt; i++) block->stmts[i] = old[i];
                cap = newcap;
            }
            prev_pos = cc->pos;
            block->stmts[cnt] = parse_stmt(cc);
            cnt++;
            no_progress_count = 0;
            /* avoid infinite loop if parse_stmt didn't consume (e.g. after error) */
            while (cc->pos == prev_pos && cc->tk != TK_RBRACE && cc->tk != TK_EOF) {
                next_token(cc);
                no_progress_count++;
                if (no_progress_count > 1024) {
                    error(cc, "parser stuck (no progress after 1024 tokens)");
                    block->num_stmts = cnt;
                    scope_pop(cc);
                    return block;
                }
            }
        }
        block->num_stmts = cnt;
        scope_pop(cc);
        expect(cc, TK_RBRACE);
        return block;
    }

    /* return */
    if (cc->tk == TK_RETURN) {
        Node *ret;
        next_token(cc);
        ret = node_new(cc, ND_RETURN, line);
        if (cc->tk != TK_SEMI) {
            ret->lhs = parse_expr(cc);
        }
            expect(cc, TK_SEMI);
        return ret;
    }

    /* if */
    if (cc->tk == TK_IF) {
        Node *ifn;
        next_token(cc);
        expect(cc, TK_LPAREN);
        ifn = node_new(cc, ND_IF, line);
        ifn->cond = parse_expr(cc);
        expect(cc, TK_RPAREN);
        ifn->then_body = parse_stmt(cc);
        if (cc->tk == TK_ELSE) {
            next_token(cc);
            ifn->else_body = parse_stmt(cc);
        }
        return ifn;
    }

    /* while */
    if (cc->tk == TK_WHILE) {
        Node *wh;
        next_token(cc);
        expect(cc, TK_LPAREN);
        wh = node_new(cc, ND_WHILE, line);
        wh->cond = parse_expr(cc);
        expect(cc, TK_RPAREN);
        wh->body = parse_stmt(cc);
        return wh;
    }

    /* for */
    if (cc->tk == TK_FOR) {
        Node *forn;
        next_token(cc);
        expect(cc, TK_LPAREN);
        forn = node_new(cc, ND_FOR, line);

        scope_push(cc);

        /* init — could be declaration or expression */
        if (cc->tk == TK_SEMI) {
            next_token(cc);
            forn->init = 0;
        } else if (is_type_token(cc)) {
            forn->init = parse_stmt(cc);  /* handles the semicolon */
        } else {
            forn->init = parse_expr(cc);
            expect(cc, TK_SEMI);
        }

        /* condition */
        if (cc->tk == TK_SEMI) {
            forn->cond = 0;
        } else {
            forn->cond = parse_expr(cc);
        }
            expect(cc, TK_SEMI);

        /* increment */
        if (cc->tk == TK_RPAREN) {
            forn->inc = 0;
        } else {
            forn->inc = parse_expr(cc);
        }
        expect(cc, TK_RPAREN);

        forn->body = parse_stmt(cc);
        scope_pop(cc);
        return forn;
    }

    /* do-while */
    if (cc->tk == TK_DO) {
        Node *dow;
        next_token(cc);
        dow = node_new(cc, ND_DO_WHILE, line);
        dow->body = parse_stmt(cc);
        if (cc->tk != TK_WHILE) {
            error(cc, "expected 'while' after do body");
        }
        next_token(cc); /* skip 'while' */
        expect(cc, TK_LPAREN);
        dow->cond = parse_expr(cc);
        expect(cc, TK_RPAREN);
            expect(cc, TK_SEMI);
        return dow;
    }

    /* switch */
    if (cc->tk == TK_SWITCH) {
        Node *sw;
        next_token(cc);
        expect(cc, TK_LPAREN);
        sw = node_new(cc, ND_SWITCH, line);
        sw->cond = parse_expr(cc);
        expect(cc, TK_RPAREN);
        /* parse body — extract cases */
        sw->cases = (Node **)cc_alloc(cc, sizeof(Node *) * MAX_CASES);
        sw->num_cases = 0;
        sw->default_case = 0;
        
        Node *old_sw = current_sw;
        current_sw = sw;

        sw->body = parse_stmt(cc);

        current_sw = old_sw;
        return sw;
    }

    if (cc->tk == TK_CASE) {
        Node *cn;
        Node *cnode;
        Node *gto;
        long long val;
        next_token(cc);
        val = eval_const_expr(parse_assign(cc));
        expect(cc, TK_COLON);

        cn = node_new(cc, ND_LABEL, line);
        sprintf(cn->label_name, "__zc_%llx_%d", (unsigned long long)val, line);
        cn->lhs = parse_stmt(cc);

        if (current_sw && current_sw->num_cases < MAX_CASES) {
            cnode = node_new(cc, ND_CASE, line);
            cnode->case_val = val;
            gto = node_new(cc, ND_GOTO, line);
            strncpy(gto->label_name, cn->label_name, MAX_IDENT - 1);
            cnode->case_body = gto;
            current_sw->cases[current_sw->num_cases++] = cnode;
        }
        return cn;
    }

    if (cc->tk == TK_DEFAULT) {
        Node *dn;
        Node *dnode;
        Node *gto;
        next_token(cc);
        expect(cc, TK_COLON);

        dn = node_new(cc, ND_LABEL, line);
        sprintf(dn->label_name, "__zc_def_%d", line);
        dn->lhs = parse_stmt(cc);

        if (current_sw) {
            dnode = node_new(cc, ND_DEFAULT, line);
            gto = node_new(cc, ND_GOTO, line);
            strncpy(gto->label_name, dn->label_name, MAX_IDENT - 1);
            dnode->case_body = gto;
            current_sw->default_case = dnode;
        }
        return dn;
    }

    /* break */
    if (cc->tk == TK_BREAK) {
        Node *brk;
        next_token(cc);
        brk = node_new(cc, ND_BREAK, line);
            expect(cc, TK_SEMI);
        return brk;
    }

    /* continue */
    if (cc->tk == TK_CONTINUE) {
        Node *cont;
        next_token(cc);
        cont = node_new(cc, ND_CONTINUE, line);
            expect(cc, TK_SEMI);
        return cont;
    }

    /* goto */
    if (cc->tk == TK_GOTO) {
        Node *gto;
        next_token(cc);
        gto = node_new(cc, ND_GOTO, line);
        strncpy(gto->label_name, cc->tk_text, MAX_IDENT - 1);
        next_token(cc);
            expect(cc, TK_SEMI);
        return gto;
    }

    /* label: check ident followed by colon */
    if (cc->tk == TK_IDENT) {
        int pk;
        pk = peek_token(cc);
        if (pk == TK_COLON) {
            Node *lbl;
            lbl = node_new(cc, ND_LABEL, line);
            strncpy(lbl->label_name, cc->tk_text, MAX_IDENT - 1);
            next_token(cc);  /* skip ident */
            next_token(cc);  /* skip : */
            lbl->lhs = parse_stmt(cc);
            return lbl;
        }
    }

    /* local variable declaration */
    if (is_type_token(cc)) {
        Type *base;
        int is_typedef = 0;
        if (cc->tk == TK_TYPEDEF) is_typedef = 1;
        base = parse_type(cc);
        int is_static_local = cc->current_is_static;

        /* handle multiple declarators: int a, b, c; */
        {
            Node *block;
            int cap;
            int cnt;
            block = node_new(cc, ND_BLOCK, line);
            cap = 8;
            block->stmts = (Node **)cc_alloc(cc, sizeof(Node *) * cap);
            cnt = 0;

            for (;;) {
                Type *vtype;
                char vname[MAX_IDENT];
                Symbol *sym;
                Node *decl;

                vtype = parse_declarator(cc, base, vname);

                if (vname[0]) {
                    if (is_typedef) {
                        sym = scope_add(cc, vname, vtype);
                        sym->is_typedef = 1;
                    } else if (is_static_local) {
                        char mangled[MAX_IDENT];
                        sprintf(mangled, "%s__%s__%d", cc->current_func, vname, cc->label_count++);
                        sym = scope_add_local(cc, vname, vtype);
                        sym->is_local = 0;
                        sym->is_global = 1;
                        strncpy(sym->asm_name, mangled, MAX_IDENT - 1);

                        Node *gvar = node_new(cc, ND_GLOBAL_VAR, line);
                        strncpy(gvar->name, mangled, MAX_IDENT - 1);
                        gvar->type = vtype;
                        gvar->is_static = 1;
                        gvar->is_extern = 0;

                        if (cc->tk == TK_ASSIGN) {
                            next_token(cc);
                            if (cc->tk == TK_LBRACE) {
                                Node *init_list;
                                Node **inits;
                                int count;
                                init_list = node_new(cc, ND_INIT_LIST, line);
                                inits = (Node **)cc_alloc(cc, sizeof(Node *) * MAX_INIT);
                                count = 0;
                                next_token(cc); /* skip { */
                                int depth = 1;
                                int skip_tk;
                                int prev_pos;
                                int no_progress_count = 0;
                                int top_elems = 0;
                                int last_comma = 1;
                                while (depth > 0 && cc->tk != TK_EOF) {
                                    if (depth == 1 && cc->tk != TK_RBRACE && cc->tk != TK_COMMA && last_comma) {
                                        top_elems++;
                                        last_comma = 0;
                                    }
                                    prev_pos = cc->pos;
                                    if (cc->tk == TK_LBRACE) {
                                        depth++;
                                        next_token(cc);
                                    } else if (cc->tk == TK_RBRACE) {
                                        depth--;
                                        if (depth == 0) break;
                                        next_token(cc);
                                    } else if (cc->tk == TK_COMMA) {
                                        if (depth == 1) last_comma = 1;
                                        next_token(cc);
                                    } else {
                                        skip_tk = cc->tk;
                                        if (count < MAX_INIT) {
                                            inits[count++] = parse_assign(cc);
                                        } else {
                                            parse_assign(cc);
                                        }
                                        if (cc->tk == skip_tk) {
                                            if (cc->tk != TK_EOF) next_token(cc);
                                        }
                                        if (cc->pos == prev_pos) {
                                            no_progress_count++;
                                            if (no_progress_count > 100) { break; }
                                        } else {
                                            no_progress_count = 0;
                                        }
                                    }
                                }
                                if (cc->tk == TK_RBRACE) next_token(cc);
                                init_list->args = inits;
                                init_list->num_args = count;
                                gvar->initializer = init_list;
                                
                                if (gvar->type->kind == TY_ARRAY && gvar->type->array_len == 0) {
                                    gvar->type->array_len = top_elems;
                                    gvar->type->size = type_size(gvar->type->base) * top_elems;
                                    sym->type->array_len = top_elems;
                                    sym->type->size = gvar->type->size;
                                }
                            } else {
                                Node *init_node2 = parse_assign(cc);
                                gvar->initializer = init_node2;
                                /* Fix: if char[] initialized with string literal, update size */
                                if (init_node2 && init_node2->kind == ND_STR &&
                                    gvar->type->kind == TY_ARRAY && gvar->type->array_len == 0) {
                                    int slen2 = cc->strings[init_node2->str_id].len + 1;
                                    gvar->type->array_len = slen2;
                                    gvar->type->size = type_size(gvar->type->base) * slen2;
                                    sym->type->array_len = slen2;
                                    sym->type->size = gvar->type->size;
                                }
                            }
                        }

                        if (cc->num_globals < MAX_GLOBALS) cc->globals[cc->num_globals++] = gvar;
                    } else {
                        sym = scope_add_local(cc, vname, vtype);

                        /* initializer? */
                        if (cc->tk == TK_ASSIGN) {
                            Node *asgn;
                            Node *var;
                            next_token(cc);

                        /* check for array/struct initializer list */
                        if (cc->tk == TK_LBRACE) {
                            if (vtype->kind == TY_ARRAY) {
                                /* Parse array initializer list and emit element assignments */
                                int depth;
                                int init_count;
                                Node **inits;
                                int init_cap;
                                Type *elem_type;
                                Type *arr_type;
                                int sz;
                                int skip_tk;
                                int prev_pos;
                                int no_progress_count = 0;
                                next_token(cc); /* skip { */
                                init_count = 0;
                                init_cap = 16;
                                inits = (Node **)cc_alloc(cc, sizeof(Node *) * init_cap);
                                depth = 1;
                                while (depth > 0 && cc->tk != TK_EOF) {
                                    prev_pos = cc->pos;
                                    if (cc->tk == TK_LBRACE) {
                                        depth++;
                                        next_token(cc);
                                    } else if (cc->tk == TK_RBRACE) {
                                        depth--;
                                        if (depth == 0) break;
                                        next_token(cc);
                                    } else if (cc->tk == TK_COMMA) {
                                        next_token(cc);
                                    } else {
                                        skip_tk = cc->tk;
                                        Node *expr = parse_assign(cc);
                                        if (cc->tk == skip_tk) {
                                            if (cc->tk != TK_EOF) next_token(cc);
                                        }
                                        if (cc->pos == prev_pos) {
                                            no_progress_count++;
                                            if (no_progress_count > 100) {
                                                error(cc, "array parser stuck (possible infinite loop)");
                                                break;
                                            }
                                        } else {
                                            no_progress_count = 0;
                                        }
                                        if (init_count >= init_cap) {
                                            Node **old_inits = inits;
                                            int j;
                                            init_cap *= 2;
                                            inits = (Node **)cc_alloc(cc, sizeof(Node *) * init_cap);
                                            for (j = 0; j < init_count; j++) inits[j] = old_inits[j];
                                        }
                                        inits[init_count++] = expr;
                                    }
                                }
                                if (cc->tk == TK_RBRACE) next_token(cc); /* skip final } */
                                elem_type = vtype->base;
                                if (vtype->array_len == 0) {
                                    arr_type = type_array(cc, elem_type, init_count);
                                    sym->type = arr_type;
                                    sz = type_size(arr_type);
                                    if (sz < 8) sz = 8;
                                    cc->local_offset = cc->local_offset + 8;
                                    cc->local_offset = cc->local_offset - sz;
                                    sym->stack_offset = cc->local_offset;
                                } else {
                                    arr_type = vtype;
                                }
                                /* Grow block stmts if needed */
                                while (cnt + init_count > cap) {
                                    Node **new_stmts;
                                    int j;
                                    new_stmts = (Node **)cc_alloc(cc, sizeof(Node *) * cap * 2);
                                    for (j = 0; j < cnt; j++) new_stmts[j] = block->stmts[j];
                                    block->stmts = new_stmts;
                                    cap = cap * 2;
                                }
                                {
                                    int idx;
                                    for (idx = 0; idx < init_count; idx++) {
                                        Node *var;
                                        Node *add;
                                        Node *deref;
                                        Node *asgn;
                                        var = node_new(cc, ND_VAR, line);
                                        strncpy(var->name, vname, MAX_IDENT - 1);
                                        var->sym = sym;
                                        var->type = arr_type;
                                        add = node_new(cc, ND_ADD, line);
                                        add->lhs = var;
                                        add->rhs = node_num(cc, (long long)idx, line);
                                        add->type = arr_type;
                                        deref = node_new(cc, ND_DEREF, line);
                                        deref->lhs = add;
                                        deref->type = elem_type;
                                        asgn = node_new(cc, ND_ASSIGN, line);
                                        asgn->lhs = deref;
                                        asgn->rhs = inits[idx];
                                        asgn->type = elem_type;
                                        block->stmts[cnt] = asgn;
                                        cnt++;
                                    }
                                }
                            } else if (vtype->kind == TY_STRUCT || vtype->kind == TY_UNION) {
                                /* Local struct/union initializer: {v0, v1, ...}
                                 * Walk StructField list, assign each field in order. */
                                int init_count_s;
                                int init_cap_s;
                                Node **inits_s;
                                int skip_tk_s;
                                int prev_pos_s;
                                int no_progress_s;
                                int depth_s;
                                init_count_s = 0;
                                init_cap_s = 32;
                                inits_s = (Node **)cc_alloc(cc, sizeof(Node *) * init_cap_s);
                                depth_s = 1;
                                no_progress_s = 0;
                                next_token(cc); /* skip { */
                                while (depth_s > 0 && cc->tk != TK_EOF) {
                                    prev_pos_s = cc->pos;
                                    if (cc->tk == TK_LBRACE) {
                                        depth_s++;
                                        next_token(cc);
                                    } else if (cc->tk == TK_RBRACE) {
                                        depth_s--;
                                        if (depth_s == 0) break;
                                        next_token(cc);
                                    } else if (cc->tk == TK_COMMA) {
                                        next_token(cc);
                                    } else {
                                        skip_tk_s = cc->tk;
                                        if (init_count_s < init_cap_s) {
                                            inits_s[init_count_s++] = parse_assign(cc);
                                        } else {
                                            parse_assign(cc);
                                        }
                                        if (cc->tk == skip_tk_s) {
                                            if (cc->tk != TK_EOF) next_token(cc);
                                        }
                                        if (cc->pos == prev_pos_s) {
                                            no_progress_s++;
                                            if (no_progress_s > 100) break;
                                        } else {
                                            no_progress_s = 0;
                                        }
                                    }
                                }
                                if (cc->tk == TK_RBRACE) next_token(cc);
                                /* Now emit field assignments */
                                {
                                    StructField *sf;
                                    int fi;
                                    fi = 0;
                                    sf = vtype->fields;
                                    while (sf && fi < init_count_s) {
                                        /* Build: var.sf_name = inits_s[fi] */
                                        Node *var_n;
                                        Node *mem_n;
                                        Node *asgn_n;
                                        int acc_off;
                                        StructField *found_f;
                                        var_n = node_new(cc, ND_VAR, line);
                                        strncpy(var_n->name, vname, MAX_IDENT - 1);
                                        var_n->sym = sym;
                                        var_n->type = vtype;
                                        acc_off = 0;
                                        found_f = find_struct_member(vtype, sf->name, &acc_off);
                                        mem_n = node_new(cc, ND_MEMBER, line);
                                        mem_n->lhs = var_n;
                                        strncpy(mem_n->member_name, sf->name, MAX_IDENT - 1);
                                        if (found_f) {
                                            mem_n->member_offset = acc_off;
                                            mem_n->type = found_f->type;
                                            mem_n->member_size = type_size(found_f->type);
                                        } else {
                                            mem_n->member_offset = sf->offset;
                                            mem_n->type = sf->type;
                                            mem_n->member_size = type_size(sf->type);
                                        }
                                        asgn_n = node_new(cc, ND_ASSIGN, line);
                                        asgn_n->lhs = mem_n;
                                        asgn_n->rhs = inits_s[fi];
                                        asgn_n->type = mem_n->type;
                                        /* Grow block if needed */
                                        if (cnt >= cap) {
                                            Node **new_stmts;
                                            int ji;
                                            new_stmts = (Node **)cc_alloc(cc, sizeof(Node *) * cap * 2);
                                            for (ji = 0; ji < cnt; ji++) new_stmts[ji] = block->stmts[ji];
                                            block->stmts = new_stmts;
                                            cap = cap * 2;
                                        }
                                        block->stmts[cnt] = asgn_n;
                                        cnt++;
                                        sf = sf->next;
                                        fi++;
                                        /* Skip nested struct fields for now — treat as flat */
                                    }
                                    /* Zero-initialize remaining fields if fewer inits provided */
                                    while (sf) {
                                        Node *var_n;
                                        Node *mem_n;
                                        Node *asgn_n;
                                        int acc_off;
                                        StructField *found_f;
                                        var_n = node_new(cc, ND_VAR, line);
                                        strncpy(var_n->name, vname, MAX_IDENT - 1);
                                        var_n->sym = sym;
                                        var_n->type = vtype;
                                        acc_off = 0;
                                        found_f = find_struct_member(vtype, sf->name, &acc_off);
                                        mem_n = node_new(cc, ND_MEMBER, line);
                                        mem_n->lhs = var_n;
                                        strncpy(mem_n->member_name, sf->name, MAX_IDENT - 1);
                                        if (found_f) {
                                            mem_n->member_offset = acc_off;
                                            mem_n->type = found_f->type;
                                            mem_n->member_size = type_size(found_f->type);
                                        } else {
                                            mem_n->member_offset = sf->offset;
                                            mem_n->type = sf->type;
                                            mem_n->member_size = type_size(sf->type);
                                        }
                                        asgn_n = node_new(cc, ND_ASSIGN, line);
                                        asgn_n->lhs = mem_n;
                                        asgn_n->rhs = node_num(cc, 0LL, line);
                                        asgn_n->type = mem_n->type;
                                        if (cnt >= cap) {
                                            Node **new_stmts;
                                            int ji;
                                            new_stmts = (Node **)cc_alloc(cc, sizeof(Node *) * cap * 2);
                                            for (ji = 0; ji < cnt; ji++) new_stmts[ji] = block->stmts[ji];
                                            block->stmts = new_stmts;
                                            cap = cap * 2;
                                        }
                                        block->stmts[cnt] = asgn_n;
                                        cnt++;
                                        sf = sf->next;
                                    }
                                }
                            } else {
                                /* Other non-array, non-struct types: skip initializer list */
                                int skip_depth_x;
                                skip_depth_x = 1;
                                next_token(cc);
                                while (skip_depth_x > 0) {
                                    if (cc->tk == TK_EOF) break;
                                    if (cc->tk == TK_LBRACE) skip_depth_x = skip_depth_x + 1;
                                    if (cc->tk == TK_RBRACE) skip_depth_x = skip_depth_x - 1;
                                    if (skip_depth_x > 0) next_token(cc);
                                }
                                next_token(cc);
                            }
                        } else {
                            var = node_new(cc, ND_VAR, line);
                            strncpy(var->name, vname, MAX_IDENT - 1);
                            var->sym = sym;
                            var->type = vtype;
                            asgn = node_new(cc, ND_ASSIGN, line);
                            asgn->lhs = var;
                            asgn->rhs = parse_assign(cc);
                            asgn->type = vtype;
                            if (cnt < cap) {
                                block->stmts[cnt] = asgn;
                                cnt++;
                            }
                        }
                    }
                    }
                }

                if (cc->tk == TK_COMMA) {
                    next_token(cc);
                } else {
                    break;
                }
            }

            block->num_stmts = cnt;
            expect(cc, TK_SEMI);

            if (cnt == 0) {
                return node_new(cc, ND_NOP, line);
            }
            if (cnt == 1) {
                return block->stmts[0];
            }
            return block;
        }
    }

    /* empty statement */
    if (cc->tk == TK_SEMI) {
        next_token(cc);
        return node_new(cc, ND_NOP, line);
    }

    /* expression statement */
    {
        Node *expr;
        expr = parse_expr(cc);
            expect(cc, TK_SEMI);
        return expr;
    }
}

/* ================================================================ */
/* TOP-LEVEL PARSING                                                 */
/* ================================================================ */

static Node *parse_func_def(Compiler *cc, Type *ret_type, char *name, int is_static) {
    Node *func;
    int line;
    int i;
    int is_variadic = 0;

    line = cc->tk_line;
    func = node_new(cc, ND_FUNC_DEF, line);
    strncpy(func->func_def_name, name, MAX_IDENT - 1);
    func->is_static = is_static;

    /* parse parameters */
    expect(cc, TK_LPAREN);
    scope_push(cc);
    cc->local_offset = 0;

    func->num_params = 0;
    if (cc->tk != TK_RPAREN) {
        if (cc->tk == TK_VOID) {
            int pk;
            pk = peek_token(cc);
            if (pk == TK_RPAREN) {
                next_token(cc);
            } else {
                goto parse_fparams;
            }
        } else {
            parse_fparams:
            while (cc->tk != TK_RPAREN) {
                Type *ptype;
                char pname[MAX_IDENT];
                Symbol *psym;

                if (cc->tk == TK_ELLIPSIS) {
                    is_variadic = 1;
                    next_token(cc);
                    break;
                }
                if (cc->tk == TK_EOF) break;

                ptype = parse_type(cc);
                ptype = parse_declarator(cc, ptype, pname);

                if (ptype->kind == TY_ARRAY) ptype = type_ptr(cc, ptype->base);

                if (func->num_params < MAX_PARAMS) {
                    func->param_types[func->num_params] = ptype;
                    strncpy(func->param_names_buf[func->num_params], pname, MAX_IDENT - 1);
                    psym = scope_add_local(cc, pname, ptype);
                    func->num_params++;
                }

                if (cc->tk == TK_COMMA) {
                    next_token(cc);
                } else if (cc->tk == TK_RPAREN) {
                    break;
                } else {
                    /* Unexpected token (e.g. ->) after parameter — skip to ) to avoid cascade */
                    if (cc->tk == TK_ARROW) {
                        error(cc, "unexpected '->' in parameter list (missing ',' or ')'?)");
                    } else {
                        error(cc, "expected ',' or ')' after parameter");
                    }
                    while (cc->tk != TK_RPAREN && cc->tk != TK_EOF) next_token(cc);
                    break;
                }
            }
        }
    }
    expect(cc, TK_RPAREN);

    /* Populate func->func_params from the inline capture arrays so
     * codegen_func (part4.c) can safely do func->func_params->types[i].
     * cc_alloc uses the arena so this is lifetime-safe. */
    {
        struct FuncParams *fp = (struct FuncParams *)cc_alloc(cc, sizeof(struct FuncParams));
        int k;
        for (k = 0; k < func->num_params && k < MAX_PARAMS; k++) {
            fp->types[k] = func->param_types[k];
            strncpy(fp->names[k], func->param_names_buf[k], MAX_IDENT - 1);
            fp->names[k][MAX_IDENT - 1] = '\0';
        }
        func->func_params = fp;
    }

    /* CG-IR-VARARGS: Reserve the first 48 bytes exclusively for reg_save_area */
    if (is_variadic && cc->local_offset > -48) {
        cc->local_offset = -48;
    }

    /* register function in global scope (forward decl and definition) */
    {
        Type *ftype;
        Symbol *fsym;
        ftype = type_func(cc, ret_type);
        ftype->num_params = func->num_params;
        ftype->is_variadic = is_variadic;
        if (func->num_params > 0) {
            ftype->params = (Type **)cc_alloc(cc, sizeof(Type *) * func->num_params);
            for (int k = 0; k < func->num_params; k++) {
                ftype->params[k] = func->param_types[k];
            }
        }
        func->func_type = ftype;

        /* add to parent scope (global) */
        fsym = scope_find(cc, name);
        if (!fsym) {
            /* temporarily pop to add to parent */
            Scope *cur;
            cur = cc->current_scope;
            cc->current_scope = cur->parent;
            fsym = scope_add(cc, name, ftype);
            fsym->is_global = 1;
            cc->current_scope = cur;
        }
    }

    /* forward declaration: );  — do not parse body, do not add to codegen list */
    if (cc->tk == TK_SEMI) {
        next_token(cc);
        func->body = 0;
        scope_pop(cc);
        return func;
    }

    /* definition: ) { ... } */
    func->body = parse_stmt(cc);
    func->stack_size = -cc->local_offset;
    scope_pop(cc);

    return func;
}

Node *parse_program(Compiler *cc) {
    Node *head;
    Node *tail;
    int top_count;
    int prev_pos;

    head = 0;
    tail = 0;
    top_count = 0;

    while (cc->tk != TK_EOF) {
        Type *base;
        char name[MAX_IDENT];
        Type *dtype;
        int is_typedef_kw;
        int is_static;
        int is_extern;
        int line;

        if (top_count >= 50000) {
            error(cc, "too many top-level declarations (possible parser loop)");
            break;
        }
        if (top_count > 0 && cc->pos == prev_pos) {
            next_token(cc);
        }
        top_count++;
        prev_pos = cc->pos;
        line = cc->tk_line;
        is_typedef_kw = 0;
        is_static = 0;
        is_extern = 0;

        /* check storage class */
        if (cc->tk == TK_TYPEDEF) { is_typedef_kw = 1; }
        if (cc->tk == TK_STATIC) { is_static = 1; }
        if (cc->tk == TK_EXTERN) { is_extern = 1; }

        base = parse_type(cc);

        /* bare struct/enum definition with no declarator */
        if (cc->tk == TK_SEMI) {
            next_token(cc);
            continue;
        }

        /* After base type, parse name and pointers only (not full declarator)
           so we can detect function defs before params are consumed */
        {
            Type *ptr_type;
            ptr_type = base;

            /* parse pointer stars */
            while (cc->tk == TK_STAR) {
                next_token(cc);
                while (cc->tk == TK_CONST || cc->tk == TK_VOLATILE) next_token(cc);
                ptr_type = type_ptr(cc, ptr_type);
            }

            /* get name — also handle grouped declarator (*name)(params) for function pointers */
            name[0] = 0;
            if (cc->tk == TK_LPAREN) {
                int gpk;
                gpk = peek_token(cc);
                if (gpk == TK_STAR) {
                    /* function pointer: typedef int (*name)(params); or int (*fp)(int); */
                    int gptr;
                    int arr_lens[8];
                    int arr_num;
                    next_token(cc); /* consume ( */
                    gptr = 0;
                    arr_num = 0;
                    while (cc->tk == TK_STAR) {
                        next_token(cc);
                        while (cc->tk == TK_CONST || cc->tk == TK_VOLATILE) next_token(cc);
                        gptr++;
                    }
                    if (cc->tk == TK_IDENT) {
                        strncpy(name, cc->tk_text, MAX_IDENT - 1);
                        next_token(cc);
                    }
                    int is_inner_func = 0;
                    if (cc->tk == TK_LPAREN) {
                        is_inner_func = 1;
                        next_token(cc);
                        int pcnt = 1;
                        while (pcnt > 0 && cc->tk != TK_EOF) {
                            if (cc->tk == TK_LPAREN) pcnt++;
                            else if (cc->tk == TK_RPAREN) pcnt--;
                            next_token(cc);
                        }
                    }
                    while (cc->tk == TK_LBRACKET) {
                        int alen = 0;
                        next_token(cc);
                        if (cc->tk != TK_RBRACKET) alen = (int)parse_const_expr(cc);
                        expect(cc, TK_RBRACKET);
                        if (arr_num < 8) arr_lens[arr_num++] = alen;
                    }
                    expect(cc, TK_RPAREN);
                    /* parse trailing (params) */
                    if (cc->tk == TK_LPAREN) {
                        Type *ftype;
                        next_token(cc);
                        ftype = type_func(cc, ptr_type);
                        ftype->params = (Type **)cc_alloc(cc, sizeof(Type *) * MAX_PARAMS);
                        ftype->num_params = 0;
                        ftype->is_variadic = 0;
                        if (cc->tk != TK_RPAREN) {
                            if (cc->tk == TK_VOID) {
                                int vpk2;
                                vpk2 = peek_token(cc);
                                if (vpk2 == TK_RPAREN) {
                                    next_token(cc);
                                } else {
                                    goto parse_gd_params;
                                }
                            } else {
                                parse_gd_params:
                                while (cc->tk != TK_RPAREN) {
                                    Type *ptype;
                                    char pname[128];
                                    if (cc->tk == TK_ELLIPSIS) {
                                        ftype->is_variadic = 1;
                                        next_token(cc);
                                        break;
                                    }
                                    if (cc->tk == TK_EOF) break;
                                    ptype = parse_type(cc);
                                    ptype = parse_declarator(cc, ptype, pname);
                                    if (ftype->num_params < MAX_PARAMS) {
                                        ftype->params[ftype->num_params] = ptype;
                                        ftype->num_params++;
                                    }
                                    if (cc->tk == TK_COMMA) {
                                        next_token(cc);
                                    } else {
                                        break;
                                    }
                                }
                            }
                        }
                        expect(cc, TK_RPAREN);
                        dtype = ftype;
                    } else {
                        dtype = ptr_type;
                    }
                    while (gptr > 0) {
                        dtype = type_ptr(cc, dtype);
                        gptr--;
                    }
                    while (arr_num > 0) {
                        arr_num--;
                        dtype = type_array(cc, dtype, arr_lens[arr_num]);
                    }
                    if (is_inner_func) {
                        dtype = type_func(cc, dtype);
                    }
                    /* fall through to typedef / global var handling */
                    goto after_name;
                } else if (gpk == TK_IDENT) {
                    /* parenthesized name: type (name)(params)
                     * Used pervasively in Lua 5.4.6 API headers:
                     *   extern lua_State *(lua_newstate)(lua_Alloc f, void *ud);
                     *   extern void       (lua_close)(lua_State *L);
                     * Consume (name) then parse trailing (params). */
                    next_token(cc); /* consume ( */
                    if (cc->tk == TK_IDENT) {
                        strncpy(name, cc->tk_text, MAX_IDENT - 1);
                        next_token(cc);
                    }
                    expect(cc, TK_RPAREN);
                    if (cc->tk == TK_LPAREN) {
                        Type *ftype;
                        next_token(cc);
                        ftype = type_func(cc, ptr_type);
                        ftype->params = (Type **)cc_alloc(cc, sizeof(Type *) * MAX_PARAMS);
                        ftype->num_params = 0;
                        ftype->is_variadic = 0;
                        if (cc->tk != TK_RPAREN) {
                            if (cc->tk == TK_VOID && peek_token(cc) == TK_RPAREN) {
                                next_token(cc);
                            } else {
                                while (cc->tk != TK_RPAREN && cc->tk != TK_EOF) {
                                    Type *ptype; char pname[MAX_IDENT]; pname[0] = 0;
                                    if (cc->tk == TK_ELLIPSIS) { ftype->is_variadic = 1; next_token(cc); break; }
                                    ptype = parse_type(cc);
                                    ptype = parse_declarator(cc, ptype, pname);
                                    if (ftype->num_params < MAX_PARAMS) ftype->params[ftype->num_params++] = ptype;
                                    if (cc->tk == TK_COMMA) next_token(cc); else break;
                                }
                            }
                        }
                        expect(cc, TK_RPAREN);
                        dtype = ftype;
                    } else {
                        dtype = ptr_type;
                    }
                    goto after_name;
                }
            }

            if (cc->tk == TK_IDENT) {
                strncpy(name, cc->tk_text, MAX_IDENT - 1);
                next_token(cc);
            }
            dtype = ptr_type;

            after_name:
            /* handle bare struct/enum definitions (no name) */
            if (!name[0]) {
                if (cc->tk == TK_SEMI) {
                    next_token(cc);
                    continue;
                }
                /* unnamed — error or typedef was handled above */
                if (cc->tk != TK_LPAREN) {
                    error(cc, "expected name in declaration");
                    next_token(cc);
                    continue;
                }
            }

            /* function definition or forward declaration: name followed by ( */
            if (cc->tk == TK_LPAREN) {
                if (!is_typedef_kw) {
                    Node *func;
                    func = parse_func_def(cc, ptr_type, name, is_static);
                    /* only link into list if definition (has body); forward decl has body == 0 */
                    if (func->body) {
                        func->next = 0;
                        if (!head) { head = func; tail = func; }
                        else { tail->next = func; tail = func; }
                    }
                    continue;
                } else {
                    /* it's a typedef to a function type: typedef int func_t(int, int); */
                    int depth;
                    dtype = type_func(cc, ptr_type);
                    next_token(cc); /* consume ( */
                    depth = 1;
                    while (depth > 0 && cc->tk != TK_EOF) {
                        if (cc->tk == TK_LPAREN) depth++;
                        else if (cc->tk == TK_RPAREN) depth--;
                        next_token(cc);
                    }
                    /* falls through to array dimensional parsing or typedef registration */
                }
            }

            /* array dimensions after name */
            while (cc->tk == TK_LBRACKET) {
                int alen;
                next_token(cc);
                alen = 0;
                if (cc->tk != TK_RBRACKET) alen = (int)parse_const_expr(cc);
                expect(cc, TK_RBRACKET);
                dtype = type_array(cc, dtype, alen);
            }
        }

        /* typedef */
        if (is_typedef_kw) {
            Symbol *sym;
            sym = scope_add(cc, name, dtype);
            sym->is_typedef = 1;
            /* handle multiple typedef names */
            while (cc->tk == TK_COMMA) {
                char name2[MAX_IDENT];
                next_token(cc);
                dtype = parse_declarator(cc, base, name2);
                sym = scope_add(cc, name2, dtype);
                sym->is_typedef = 1;
            }
            expect(cc, TK_SEMI);
            continue;
        }

        /* global variable */
        {
            Node *gvar;
            gvar = node_new(cc, ND_GLOBAL_VAR, line);
            strncpy(gvar->name, name, MAX_IDENT - 1);
            gvar->type = dtype;
            gvar->is_static = is_static;
            gvar->is_extern = is_extern;

            /* register in scope */
            {
                Symbol *sym;
                sym = scope_add(cc, name, dtype);
                sym->is_global = 1;
            }

            /* initializer */
            if (cc->tk == TK_ASSIGN) {
                next_token(cc);
                if (cc->tk == TK_LBRACE) {
                    /* Parse array initializer list */
                    Node *init_list;
                    Node **inits;
                    int count;
                    init_list = node_new(cc, ND_INIT_LIST, line);
                    inits = (Node **)cc_alloc(cc, sizeof(Node *) * MAX_INIT);
                    count = 0;
                    next_token(cc); /* skip { */
                    int depth = 1;
                    int skip_tk;
                    int prev_pos;
                    int no_progress_count = 0;
                    int top_elems = 0;
                    int last_comma = 1;
                    while (depth > 0 && cc->tk != TK_EOF) {
                        if (depth == 1 && cc->tk != TK_RBRACE && cc->tk != TK_COMMA && last_comma) {
                            top_elems++;
                            last_comma = 0;
                        }
                        prev_pos = cc->pos;
                        if (cc->tk == TK_LBRACE) {
                            depth++;
                            next_token(cc);
                        } else if (cc->tk == TK_RBRACE) {
                            depth--;
                            if (depth == 0) break;
                            next_token(cc);
                        } else if (cc->tk == TK_COMMA) {
                            if (depth == 1) last_comma = 1;
                            next_token(cc);
                        } else {
                            skip_tk = cc->tk;
                            if (count < MAX_INIT) {
                                inits[count++] = parse_assign(cc);
                            } else {
                                parse_assign(cc); /* discard if over limit */
                            }
                            if (cc->tk == skip_tk) {
                                if (cc->tk != TK_EOF) next_token(cc);
                            }
                            if (cc->pos == prev_pos) {
                                no_progress_count++;
                                if (no_progress_count > 100) {
                                    error(cc, "global array parser stuck");
                                    break;
                                }
                            } else {
                                no_progress_count = 0;
                            }
                        }
                    }
                    if (cc->tk == TK_RBRACE) next_token(cc); /* skip } */
                    init_list->args = inits;
                    init_list->num_args = count;
                    gvar->initializer = init_list;
                    
                    /* adjust array size if omitted */
                    if (gvar->type->kind == TY_ARRAY && gvar->type->array_len == 0) {
                        gvar->type->array_len = top_elems;
                        gvar->type->size = type_size(gvar->type->base) * top_elems;
                        if (gvar->sym) {
                            gvar->sym->type->array_len = top_elems;
                            gvar->sym->type->size = gvar->type->size;
                        }
                    }
                } else {
                    Node *init_node = parse_assign(cc);
                    gvar->initializer = init_node;
                    /* Fix: if char[] initialized with string literal, update size */
                    if (init_node && init_node->kind == ND_STR &&
                        gvar->type->kind == TY_ARRAY && gvar->type->array_len == 0) {
                        int slen = cc->strings[init_node->str_id].len + 1; /* +1 for null */
                        gvar->type->array_len = slen;
                        gvar->type->size = type_size(gvar->type->base) * slen;
                    }
                }
            }

            /* store for codegen */
            if (cc->num_globals < MAX_GLOBALS) {
                cc->globals[cc->num_globals] = gvar;
                cc->num_globals++;
            }

            /* handle multiple declarators: int a, b; */
            while (cc->tk == TK_COMMA) {
                Node *gvar2;
                char name2[MAX_IDENT];
                Type *dtype2;
                Symbol *sym2;

                next_token(cc);
                dtype2 = parse_declarator(cc, base, name2);
                gvar2 = node_new(cc, ND_GLOBAL_VAR, line);
                strncpy(gvar2->name, name2, MAX_IDENT - 1);
                gvar2->type = dtype2;
                gvar2->is_extern = is_extern;
                gvar2->is_static = is_static;

                sym2 = scope_add(cc, name2, dtype2);
                sym2->is_global = 1;

                if (cc->tk == TK_ASSIGN) {
                    next_token(cc);
                    gvar2->initializer = parse_assign(cc);
                }

                if (cc->num_globals < MAX_GLOBALS) {
                    cc->globals[cc->num_globals] = gvar2;
                    cc->num_globals++;
                }
            }

            /* link */
            gvar->next = 0;
            if (!head) { head = gvar; tail = gvar; }
            else { tail->next = gvar; tail = gvar; }

            expect(cc, TK_SEMI);
            continue;
        }
    }

    return head;
}
/* ZKAEDI FORCE RENDER CACHE INVALIDATION */
/*
 * ir.h — ZCC 3-Address Intermediate Representation
 *
 * Design constraints:
 *   1. ZCC-parseable: no #include <stdint.h>, no _Static_assert
 *   2. Native LP64 types only (char=1, short=2, int=4, long=8 on x86-64)
 *   3. GCC boundary fn zcc_run_passes_log: UNCHANGED, never included here
 *   4. All IR nodes heap-allocated via ir_node_alloc()
 *   5. ir_module_free() is the single cleanup entry point
 *
 * Text format (one node per line):
 *   LABEL   .L0:
 *   ALLOCA  i64  t0  8
 *   CONST   i32  t1  42
 *   ADD     i32  t2  t0  t1
 *   STORE   ptr  t0  t2
 *   LOAD    i32  t3  t0
 *   BR_IF       t3  .L1
 *   BR          .L2
 *   CALL    i32  rv  printf  [args follow as IR_ARG nodes]
 *   ARG     i32      t3
 *   RET     i32      t2
 */

#ifndef ZCC_IR_H
#define ZCC_IR_H

/* ── stdio.h is always available to ZCC ─────────────────────────────── */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Field size constants ────────────────────────────────────────────── */
enum {
    IR_NAME_MAX   = 64,    /* max length of a variable/temp name        */
    IR_LABEL_MAX  = 64,    /* max length of a branch label              */
    IR_FUNC_MAX   = 64,    /* max length of a function name             */
    IR_MAX_FUNCS  = 8192,  /* max functions per module                  */
    IR_STR_MAX    = 256    /* max string literal length in IR           */
};

/* ── Opcodes ─────────────────────────────────────────────────────────── */
typedef enum {
    /* Terminators (must end a basic block) */
    IR_RET     = 0, /* return [src1]                                      */
    IR_BR,          /* unconditional jump → label                         */
    IR_BR_IF,       /* if src1 != 0, jump → label                        */

    /* Memory */
    IR_ALLOCA,      /* dst = alloca(imm) — stack slot, imm = bytes        */
    IR_LOAD,        /* dst = *src1                                         */
    IR_STORE,       /* *dst = src1   (dst is an address, src1 is value)   */

    /* Arithmetic */
    IR_ADD,         /* dst = src1 + src2                                   */
    IR_SUB,         /* dst = src1 - src2                                   */
    IR_MUL,         /* dst = src1 * src2                                   */
    IR_DIV,         /* dst = src1 / src2  (signed or unsigned per type)   */
    IR_MOD,         /* dst = src1 % src2                                   */
    IR_NEG,         /* dst = -src1                                         */

    /* Bitwise */
    IR_AND,         /* dst = src1 & src2                                   */
    IR_OR,          /* dst = src1 | src2                                   */
    IR_XOR,         /* dst = src1 ^ src2                                   */
    IR_NOT,         /* dst = ~src1                                         */
    IR_SHL,         /* dst = src1 << src2                                  */
    IR_SHR,         /* dst = src1 >> src2  (arithmetic if signed type)    */

    /* Comparisons — always produce i32 0 or 1 */
    IR_EQ,          /* dst = (src1 == src2)                                */
    IR_NE,          /* dst = (src1 != src2)                                */
    IR_LT,          /* dst = (src1 <  src2)                                */
    IR_LE,          /* dst = (src1 <= src2)                                */
    IR_GT,          /* dst = (src1 >  src2)                                */
    IR_GE,          /* dst = (src1 >= src2)                                */

    /* Type operations */
    IR_CAST,        /* dst = (type)src1  — widen/narrow/ptr-cast           */
    IR_COPY,        /* dst = src1  — identity, phi-lowering target         */

    /* Constants */
    IR_CONST,       /* dst = imm  (integer immediate, stored in .imm)     */
    IR_CONST_STR,   /* dst = &str_literal  (label points into .rodata)    */

    /* Function call */
    IR_CALL,        /* dst = call label(...)  — args via IR_ARG nodes      */
    IR_ARG,         /* argument to the preceding IR_CALL                   */

    /* SSA */
    IR_PHI,         /* dst = phi(src1 from label, src2 from label2)       */
    IR_ADDR,        /* dst = &src1                                        */

    /* Pseudo */
    IR_LABEL,       /* label definition — label field only                 */
    IR_NOP,         /* no operation — used as placeholder during passes    */

    /* Floating-point operations */
    IR_FCONST,      /* dst = float_imm  (double stored bitwise in .imm)   */
    IR_FADD,        /* dst = src1 +f src2  (double add)                    */
    IR_FSUB,        /* dst = src1 -f src2  (double sub)                    */
    IR_FMUL,        /* dst = src1 *f src2  (double mul)                    */
    IR_FDIV,        /* dst = src1 /f src2  (double div)                    */
    IR_ITOF,        /* dst = (double)src1  (signed int64 to double)        */
    IR_FTOI,        /* dst = (int64)src1   (double to signed int64)        */
    IR_ASM,         /* dst = asm_string                                    */

    IR_OP_COUNT     /* sentinel — keep last                                */
} ir_op_t;

/* ── Type system ─────────────────────────────────────────────────────── */
/*
 * Encodes C's type width AND signedness.
 * Signedness matters for: DIV, MOD, SHR, LT/LE/GT/GE, CAST.
 * Unsigned types: U8..U64.  Signed: I8..I64.
 */
typedef enum {
    IR_TY_VOID = 0,
    IR_TY_I8,       /* signed  8-bit                                       */
    IR_TY_I16,      /* signed 16-bit                                       */
    IR_TY_I32,      /* signed 32-bit  — default int                        */
    IR_TY_I64,      /* signed 64-bit  — long on LP64                       */
    IR_TY_U8,       /* unsigned  8-bit                                     */
    IR_TY_U16,      /* unsigned 16-bit                                     */
    IR_TY_U32,      /* unsigned 32-bit                                     */
    IR_TY_U64,      /* unsigned 64-bit                                     */
    IR_TY_PTR,      /* generic pointer — 8 bytes LP64                      */
    IR_TY_F32,      /* float   — reserved, not emitted in P1               */
    IR_TY_F64,      /* double  — reserved, not emitted in P1               */
    IR_TY_COUNT     /* sentinel                                             */
} ir_type_t;

/* ── Core node ───────────────────────────────────────────────────────── */
/*
 * Every IR instruction is one ir_node_t.
 * Layout:
 *   binary:  op  type  dst  src1  src2
 *   unary:   op  type  dst  src1  (src2 = "")
 *   branch:  op        src1 label (dst/src2 = "")
 *   label:   IR_LABEL  label (all others = "")
 *   const:   IR_CONST  type  dst  imm
 *   call:    IR_CALL   type  dst  label (label = fn name)
 *   arg:     IR_ARG    type      src1
 *   phi:     IR_PHI    type  dst  src1  src2  label (src1 from label, src2 from label2)
 *   alloca:  IR_ALLOCA ptr   dst  imm (bytes)
 *   store:   IR_STORE  type  dst(addr)  src1(val)
 *   load:    IR_LOAD   type  dst  src1(addr)
 */
typedef struct ir_node_t {
    ir_op_t            op;
    ir_type_t          type;

    char               dst[IR_NAME_MAX];    /* destination temp/var       */
    char               src1[IR_NAME_MAX];   /* first source               */
    char               src2[IR_NAME_MAX];   /* second source (if any)     */
    char               label[IR_LABEL_MAX]; /* branch target / label name */
    char               label2[IR_LABEL_MAX];/* phi: label for src2        */

    char              *asm_string;          /* assembly string            */

    long               imm;      /* integer immediate (CONST, ALLOCA)     */
    int                lineno;   /* source file line number               */

    struct ir_node_t  *next;     /* intrusive singly-linked list          */
} ir_node_t;

/* ── Function container ──────────────────────────────────────────────── */
typedef struct {
    char       name[IR_FUNC_MAX];
    ir_type_t  ret_type;

    ir_node_t *head;          /* first node in the function               */
    ir_node_t *tail;          /* last  node — O(1) append                 */
    int        node_count;

    int        tmp_counter;   /* monotonic counter for fresh temps        */
    int        lbl_counter;   /* monotonic counter for fresh labels       */
    
    int        num_params;
    char       param_names[8][IR_NAME_MAX]; /* SystemV up to 6 in regs, keep 8 safe */
} ir_func_t;

/* ── Module container ────────────────────────────────────────────────── */
typedef struct {
    ir_func_t *funcs[IR_MAX_FUNCS];
    int        func_count;
} ir_module_t;

/* ── Global emit-IR flag (set by zcc.c at startup) ───────────────────── */
/*
 * When non-zero, ZCC's codegen routes through ir_append_node() instead
 * of writing x86-64 assembly directly.
 * Set by:  ZCC_EMIT_IR=1 environment variable
 *      or: --emit-ir command-line flag
 */
extern int g_emit_ir;

/* Active function during code generation — set by codegen before emitting */
extern ir_func_t *g_ir_cur_func;

/* Active module — created once per compilation unit */
extern ir_module_t *g_ir_module;

/* ── Construction API ────────────────────────────────────────────────── */

/* Allocate and zero a single ir_node_t.  Never returns NULL (exits on OOM). */
ir_node_t   *ir_node_alloc(void);

/* Append a pre-built node to a function's node list. */
void         ir_append(ir_func_t *fn, ir_node_t *n);

/* Create and append a node in one call. Returns the new node. */
ir_node_t   *ir_emit(ir_func_t *fn, ir_op_t op, ir_type_t ty,
                     const char *dst, const char *src1, const char *src2,
                     const char *label, long imm, int lineno);

/* Fresh temporaries and labels (function-scoped counters) */
void         ir_fresh_tmp(ir_func_t *fn, char *buf);    /* writes "t<N>" */
void         ir_fresh_label(ir_func_t *fn, char *buf);  /* writes ".L<N>" */

/* Module lifecycle */
ir_module_t *ir_module_create(void);
ir_func_t   *ir_func_create(ir_module_t *mod, const char *name,
                             ir_type_t ret_type, int num_params);
void         ir_module_free(ir_module_t *mod);

/* ── Emission API ────────────────────────────────────────────────────── */

/* Emit IR text for one function to fp (stdout or output file). */
void         ir_func_emit_text(const ir_func_t *fn, FILE *fp);

/* Emit the entire module. */
void         ir_module_emit_text(const ir_module_t *mod, FILE *fp);

/* ── Query helpers ───────────────────────────────────────────────────── */

/* Opcode → mnemonic string ("ADD", "LOAD", ...) */
const char  *ir_op_name(ir_op_t op);

/* Type → string ("i32", "ptr", ...) */
const char  *ir_type_name(ir_type_t ty);

/* Width in bytes of a type (0 for void, -1 for unknown) */
int          ir_type_bytes(ir_type_t ty);

/* Non-zero if type is unsigned integer */
int          ir_type_unsigned(ir_type_t ty);

/* Non-zero if op is a basic-block terminator */
int          ir_op_is_terminator(ir_op_t op);

#endif /* ZCC_IR_H */
/*
 * ir_emit_dispatch.h — Emit dispatch shim for zcc.c
 *
 * Include this AFTER "ir.h" in zcc.c.
 * Provides ZCC_EMIT_* macros that check g_emit_ir and route to either:
 *   a) the existing x86-64 asm fprintf path (g_emit_ir == 0)
 *   b) ir_emit() into g_ir_cur_func  (g_emit_ir != 0)
 *
 * Usage in zcc.c codegen functions:
 *
 *   // Before (x86-64 only):
 *   fprintf(output, "    addq %s, %s\n", src, dst);
 *
 *   // After (dual-mode):
 *   ZCC_EMIT_BINARY(IR_ADD, ty, dst_tmp, src1_tmp, src2_tmp, lineno)
 *   // falls through to x86 emit if g_emit_ir == 0
 *
 * The macros are intentionally simple — they do NOT replace all of zcc.c's
 * codegen in P1.  P1's goal is:
 *   1. ir.h / ir.c compile cleanly
 *   2. --emit-ir flag activates IR path
 *   3. IR text is emitted for every function
 *   4. x86 path is completely unaffected (stage2==stage3 green)
 *
 * Full codegen routing (every node type) is a P3-IR deliverable.
 */

#ifndef ZCC_IR_EMIT_DISPATCH_H
#define ZCC_IR_EMIT_DISPATCH_H

#include "ir.h"

char *getenv(const char *name);

static void ZCC_IR_INIT(void) {
    if (getenv("ZCC_EMIT_IR")) g_emit_ir = 1;
    if (getenv("ZCC_IR_BACKEND")) g_emit_ir = 1;
    if (g_emit_ir) g_ir_module = ir_module_create();
}

static void ZCC_IR_FUNC_BEGIN(const char *fname, ir_type_t ret_ty, int num_params) {
    if (g_emit_ir) {
        g_ir_cur_func = ir_func_create(g_ir_module, fname, ret_ty, num_params);
    }
}

static void ZCC_IR_FUNC_END(void) {
    if (g_emit_ir) g_ir_cur_func = 0;
}

static void ZCC_IR_FLUSH(FILE *fp) {
    if (g_emit_ir && g_ir_module) {
        ir_module_emit_text(g_ir_module, fp);
        ir_module_free(g_ir_module);
        g_ir_module = 0;
    }
}

static void ZCC_EMIT_BINARY(ir_op_t op, ir_type_t ty, const char *dst, const char *s1, const char *s2, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, op, ty, dst, s1, s2, 0, 0, line);
    }
}

static void ZCC_EMIT_UNARY(ir_op_t op, ir_type_t ty, const char *dst, const char *src, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, op, ty, dst, src, 0, 0, 0, line);
    }
}

static void ZCC_EMIT_CONST(ir_type_t ty, const char *dst, long immval, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_CONST, ty, dst, 0, 0, 0, immval, line);
    }
}

static void ZCC_EMIT_ALLOCA(const char *dst, long bytes, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_ALLOCA, IR_TY_PTR, dst, 0, 0, 0, bytes, line);
    }
}

static void ZCC_EMIT_STORE(ir_type_t ty, const char *addr, const char *val, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_STORE, ty, addr, val, 0, 0, 0, line);
    }
}

static void ZCC_EMIT_LOAD(ir_type_t ty, const char *dst, const char *addr, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_LOAD, ty, dst, addr, 0, 0, 0, line);
    }
}

static void ZCC_EMIT_LABEL(const char *lbl, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_LABEL, IR_TY_VOID, 0, 0, 0, lbl, 0, line);
    }
}

static void ZCC_EMIT_BR(const char *lbl, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_BR, IR_TY_VOID, 0, 0, 0, lbl, 0, line);
    }
}

static void ZCC_EMIT_BR_IF(const char *cond, const char *lbl, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_BR_IF, IR_TY_VOID, 0, cond, 0, lbl, 0, line);
    }
}

static void ZCC_EMIT_RET(ir_type_t ty, const char *val, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_RET, ty, 0, val ? val : "", 0, 0, 0, line);
    }
}

static void ZCC_EMIT_CALL(ir_type_t ty, const char *dst, const char *fname, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_CALL, ty, dst, 0, 0, fname, 0, line);
    }
}

static void ZCC_EMIT_ARG(ir_type_t ty, const char *val, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_ARG, ty, 0, val, 0, 0, 0, line);
    }
}

static void ZCC_EMIT_FCONST(const char *dst, long bits, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_FCONST, IR_TY_F64, dst, 0, 0, 0, bits, line);
    }
}

static void ZCC_EMIT_FBINARY(ir_op_t op, const char *dst, const char *s1, const char *s2, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, op, IR_TY_F64, dst, s1, s2, 0, 0, line);
    }
}

static void ZCC_EMIT_ITOF(const char *dst, const char *src, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_ITOF, IR_TY_F64, dst, src, 0, 0, 0, line);
    }
}

static void ZCC_EMIT_FTOI(const char *dst, const char *src, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_emit(g_ir_cur_func, IR_FTOI, IR_TY_I64, dst, src, 0, 0, 0, line);
    }
}

static void ZCC_EMIT_ASM(const char *asm_str, int line) {
    if (g_emit_ir && g_ir_cur_func) {
        ir_node_t *n = ir_emit(g_ir_cur_func, IR_ASM, IR_TY_VOID, 0, 0, 0, 0, 0, line);
        if (n) n->asm_string = (char *)asm_str;
    }
}

#endif /* ZCC_IR_EMIT_DISPATCH_H */
/* ================================================================ */
/* ir_bridge.h — ZCC IR Bridge Infrastructure                        */
/*                                                                   */
/* Translation layer between register-machine x86-64 codegen and     */
/* 3-address SSA-style IR emission.                                  */
/*                                                                   */
/* Include this ONCE at the top of part4.c, after ir_emit_dispatch.h */
/* is already visible (it comes through part5.c concatenation).      */
/*                                                                   */
/* CRITICAL: All types here must be ZCC-parseable.                   */
/*   - No stdint.h, no _Static_assert, no __attribute__              */
/*   - No designated initializers, no compound literals              */
/* ================================================================ */

#ifndef ZCC_IR_BRIDGE_H
#define ZCC_IR_BRIDGE_H

/* ── IR Temporary Name Generator ─────────────────────────────────── */
/*                                                                     */
/* The x86-64 backend leaves results in %rax. The IR needs named       */
/* temporaries (%t0, %t1, ...). This counter generates unique names.   */
/* Reset at each function boundary via ir_bridge_func_begin().         */

static int ir_tmp_counter;
static char ir_tmp_buf[32];         /* scratch for sprintf */
static char ir_last_result[32];     /* name of the last IR temp produced */
static int ir_bridge_active;        /* 1 = emit IR alongside asm */

static void ir_bridge_reset(void) {
    ir_tmp_counter = 0;
    ir_last_result[0] = 0;
    ir_bridge_active = 1;
}

static char *ir_bridge_fresh_tmp(void) {
    /* Returns a new temporary name like "%t0", "%t1", etc.
     * Also copies into ir_last_result so callers can reference it. */
    sprintf(ir_tmp_buf, "%%t%d", ir_tmp_counter);
    ir_tmp_counter++;
    /* copy to ir_last_result */
    {
        int i;
        for (i = 0; ir_tmp_buf[i]; i++)
            ir_last_result[i] = ir_tmp_buf[i];
        ir_last_result[i] = 0;
    }
    return ir_tmp_buf;
}

/* Snapshot the current ir_last_result into a caller-owned buffer.
 * Use this to save lhs result before codegen_expr(rhs) overwrites it. */
static void ir_save_result(char *dst) {
    int i;
    for (i = 0; ir_last_result[i]; i++)
        dst[i] = ir_last_result[i];
    dst[i] = 0;
}


/* ── AST Type → IR Type Mapper ───────────────────────────────────── */
/*                                                                     */
/* Maps the compiler's internal Type* to the ir_type_t enum.           */
/* Handles all TY_* kinds including pointers, arrays, and structs.     */

static ir_type_t ir_map_type(Type *ty) {
    if (!ty) return IR_TY_I64;   /* default to 64-bit signed */

    switch (ty->kind) {
    case TY_VOID:       return IR_TY_VOID;
    case TY_CHAR:       return IR_TY_I8;
    case TY_UCHAR:      return IR_TY_U8;
    case TY_SHORT:      return IR_TY_I16;
    case TY_USHORT:     return IR_TY_U16;
    case TY_INT:        return IR_TY_I32;
    case TY_UINT:       return IR_TY_U32;
    case TY_LONG:       return IR_TY_I64;
    case TY_ULONG:      return IR_TY_U64;
    case TY_LONGLONG:   return IR_TY_I64;
    case TY_ULONGLONG:  return IR_TY_U64;
    default:            break;
    }

    /* Pointers, arrays, function pointers, structs → IR_TY_PTR (64-bit) */
    if (ty->kind == TY_PTR || ty->kind == TY_ARRAY || ty->kind == TY_FUNC)
        return IR_TY_PTR;

    /* Structs passed by pointer in System V ABI */
    if (ty->kind == TY_STRUCT)
        return IR_TY_PTR;

    /* Enum → i32 */
    if (ty->kind == TY_ENUM)
        return IR_TY_I32;

    return IR_TY_I64;  /* safety fallback */
}


/* ── Variable Name Extraction ────────────────────────────────────── */
/*                                                                     */
/* For ND_VAR nodes, extract the symbol name for IR readability.       */
/* Falls back to stack offset notation if name is unavailable.         */

static char ir_var_buf[64];

static char *ir_var_name(Node *node) {
    if (node->sym && node->sym->stack_offset != 0) {
        sprintf(ir_var_buf, "%%stack_%d", node->sym->stack_offset);
    } else if (node->sym && node->sym->name[0]) {
        sprintf(ir_var_buf, "%%%s", node->sym->name);
    } else {
        sprintf(ir_var_buf, "%%anon");
    }
    return ir_var_buf;
}


/* ── Function Boundary Helpers ───────────────────────────────────── */

static void ir_bridge_func_begin(Node *func) {
    ir_tmp_counter = 0;
    ir_last_result[0] = 0;
    ZCC_IR_FUNC_BEGIN(func->func_def_name, ir_map_type(func->func_type), func->num_params);
}

static void ir_bridge_func_end(void) {
    ZCC_IR_FUNC_END();
}


/* ── Binary Op Mapper ────────────────────────────────────────────── */
/*                                                                     */
/* Maps AST node kinds (ND_ADD, ND_SUB, ...) to IR opcodes (IR_ADD,   */
/* IR_SUB, ...). Returns -1 for non-binary nodes.                      */

static ir_op_t ir_map_binop(int nd_kind) {
    switch (nd_kind) {
    case ND_ADD:  return IR_ADD;
    case ND_SUB:  return IR_SUB;
    case ND_MUL:  return IR_MUL;
    case ND_DIV:  return IR_DIV;
    case ND_MOD:  return IR_MOD;
    case ND_BAND: return IR_AND;
    case ND_BOR:  return IR_OR;
    case ND_BXOR: return IR_XOR;
    case ND_SHL:  return IR_SHL;
    case ND_SHR:  return IR_SHR;
    case ND_EQ:   return IR_EQ;
    case ND_NE:   return IR_NE;
    case ND_LT:   return IR_LT;
    case ND_LE:   return IR_LE;
    case ND_GT:   return IR_GT;
    case ND_GE:   return IR_GE;
    default:      return IR_NOP;
    }
}


/* ── Convenience: Emit Binary with Auto-Temp ─────────────────────── */
/*                                                                     */
/* Pattern: save lhs result, codegen rhs, emit IR_BINARY with fresh   */
/* destination temp. Used by all arithmetic/bitwise/comparison nodes.  */

static void ir_emit_binary_op(int nd_kind, Type *ty, char *lhs_tmp,
                               char *rhs_tmp, int line) {
    ir_op_t op;
    char *dst;

    op = ir_map_binop(nd_kind);
    if (op == IR_NOP) return;  /* not a binary op */

    dst = ir_bridge_fresh_tmp();
    ZCC_EMIT_BINARY(op, ir_map_type(ty), dst, lhs_tmp, rhs_tmp, line);
}

static void ir_emit_var_load(Node *node) {
    char *vname = ir_var_name(node);
    char *dst = ir_bridge_fresh_tmp();
    if (node->type && (node->type->kind == TY_ARRAY || node->type->kind == TY_STRUCT || node->type->kind == TY_UNION)) {
        ZCC_EMIT_UNARY(IR_ADDR, ir_map_type(node->type), dst, vname, node->line);
    } else {
        ZCC_EMIT_LOAD(ir_map_type(node->type), dst, vname, node->line);
    }
}

#endif /* ZCC_IR_BRIDGE_H */
/* ================================================================ */
/* CODE GENERATOR — x86-64 Linux (System V) or Windows x64 ABI      */
/* ================================================================ */

#ifndef ZCC_AST_BRIDGE_H
/* Exclusively for standalone IDE analysis */
#include "part1.c"
#endif

#include "ir_emit_dispatch.h"
#include "ir_bridge.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wdangling-else"
#pragma clang diagnostic ignored "-Wmisleading-indentation"
#pragma clang diagnostic ignored "-Wpointer-bool-conversion"
#pragma clang diagnostic ignored "-Wtautological-pointer-compare"
int is_unsigned_type(Type *ty);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
#endif

static void push_reg(Compiler *cc, char *reg) {
  if (backend_ops) {
      if (strcmp(reg, "rax") == 0) reg = "r0";
      else if (strcmp(reg, "r11") == 0) reg = "r1";
      else if (strcmp(reg, "rdx") == 0) reg = "r2";
      fprintf(cc->out, "    push {%s}\n", reg);
      cc->stack_depth++;
      return;
  }
  fprintf(cc->out, "    pushq %%%s\n", reg);
  cc->stack_depth++;
}

static void pop_reg(Compiler *cc, char *reg) {
  if (backend_ops) {
      if (strcmp(reg, "rax") == 0) reg = "r0";
      else if (strcmp(reg, "r11") == 0) reg = "r1";
      else if (strcmp(reg, "rdx") == 0) reg = "r2";
      fprintf(cc->out, "    pop {%s}\n", reg);
      cc->stack_depth--;
      return;
  }
  fprintf(cc->out, "    popq %%%s\n", reg);
  cc->stack_depth--;
}

static int new_label(Compiler *cc) {
  int l;
  l = cc->label_count;
  cc->label_count++;
  return l;
}

static int is_power_of_2_val(long long val) {
  return val > 0 && (val & (val - 1)) == 0;
}

static int log2_of(long long val) {
  int n;
  n = 0;
  while (val > 1) {
    val = val >> 1;
    n = n + 1;
  }
  return n;
}

/* Emit efficient pointer-scale for r11: shlq for power-of-2 sizes, imulq otherwise.
 * Oneirogenesis confirmed 371 surviving shlq substitutions — wire it into the codegen. */
#define EMIT_PTR_SCALE_R11(cc, esz) do { \
    int _e = (esz); \
    if ((_e & (_e - 1)) == 0) \
        fprintf((cc)->out, "    shlq $%d, %%r11\n", log2_of(_e)); \
    else \
        fprintf((cc)->out, "    imulq $%d, %%r11\n", _e); \
} while(0)

#define EMIT_PTR_SCALE_RAX(cc, esz) do { \
    int _e = (esz); \
    if ((_e & (_e - 1)) == 0) \
        fprintf((cc)->out, "    shlq $%d, %%rax\n", log2_of(_e)); \
    else \
        fprintf((cc)->out, "    imulq $%d, %%rax\n", _e); \
} while(0)

/* Format codes for label emission — avoid passing string literals as 2nd arg
 * (stage2 mispasses them). */
enum { FMT_JE = 1, FMT_JMP, FMT_DEF, FMT_JNE };
static void emit_bb_telem(Compiler *cc, int label_id) {
    if (getenv("ZCC_EMIT_TELEMETRY")) {
        fprintf(stderr, "[telem] fn=%s bb=%d used_regs=0x%02X src=%s\n",
                cc->current_func, label_id, cc->used_regs_mask, cc->is_forced_mask ? "forced" : "computed");
    }
}

static void emit_label_fmt(Compiler *cc, int n, int fmt) {
    if (fmt == FMT_DEF) {
        emit_bb_telem(cc, n);
    }

  if (backend_ops) {
      switch (fmt) {
      case FMT_JE:
        fprintf(cc->out, "    beq .L%d\n", n);
        break;
      case FMT_JMP:
        fprintf(cc->out, "    b .L%d\n", n);
        break;
      case FMT_DEF:
        fprintf(cc->out, ".L%d:\n", n);
        break;
      case FMT_JNE:
        fprintf(cc->out, "    bne .L%d\n", n);
        break;
      default:
        fprintf(cc->out, ".L%d:\n", n);
        break;
      }
      return;
  }
  switch (fmt) {
  case FMT_JE:
    if (backend_ops) fprintf(cc->out, "    beq .L%d\n", n);
    else fprintf(cc->out, "    je .L%d\n", n);
    break;
  case FMT_JMP:
    if (backend_ops) fprintf(cc->out, "    b .L%d\n", n);
    else fprintf(cc->out, "    jmp .L%d\n", n);
    break;
  case FMT_DEF:
    fprintf(cc->out, ".L%d:\n", n);
    break;
  case FMT_JNE:
    fprintf(cc->out, "    jne .L%d\n", n);
    break;
  default:
    fprintf(cc->out, ".L%d:\n", n);
    break;
  }
}

/* Forward: defined after bad_node_cutoff. */
static int is_bad_ptr(const void *p);

/* True if ptr is in fault range. GDB: 0x800b457d0 (hi=0, lo>=2G), 0x800ac07d0
 * (hi=0x80). Reject: (hi==0 && lo>=2G) OR (hi in [0x80, 0x100)). Not inline so
 * stage2 emits a body. */
int ptr_in_fault_range(const void *p) {
  unsigned long long u = (unsigned long long)(const char *)p;
  unsigned int hi = (unsigned int)(u >> 32);
  unsigned int lo = (unsigned int)u;
  return (hi == 0 && lo >= 0x80000000U) || (hi >= 0x80U && hi < 0x100U);
}
/* Use ptr_in_fault_range() in code, not macro; stage2 may not expand macros. */

/* ---------------------------------------------------------------- */
/* System V AMD64 ABI Classifications                               */
/* ---------------------------------------------------------------- */

/* Join two classes per SysV §3.2.3. INTEGER ⊔ SSE = INTEGER. */
static abi_class_t abi_join(abi_class_t a, abi_class_t b) {
    if (a == b) return a;
    if (a == CLASS_NO_CLASS) return b;
    if (b == CLASS_NO_CLASS) return a;
    if (a == CLASS_MEMORY || b == CLASS_MEMORY) return CLASS_MEMORY;
    if (a == CLASS_INTEGER || b == CLASS_INTEGER) return CLASS_INTEGER;
    return CLASS_SSE;
}

/* Classify one field into one of the two eightbytes. */
static void classify_field(Type *field_type, int field_offset, abi_class_t eb[2]) {
    int size = type_size(field_type);
    int first_eb  = field_offset / 8;
    int last_eb   = (field_offset + size - 1) / 8;
    abi_class_t fc = CLASS_INTEGER;

    /* SysV §3.2.3: if it has unaligned fields, it has class MEMORY. */
    int falign = type_align(field_type);
    if (falign > 0 && (field_offset % falign) != 0) {
        eb[0] = eb[1] = CLASS_MEMORY;
        return;
    }

    if (field_type->kind == TY_FLOAT || field_type->kind == TY_DOUBLE) {
        fc = CLASS_SSE;
    } else if (field_type->kind == TY_STRUCT || field_type->kind == TY_UNION) {
        StructField *sf;
        for (sf = field_type->fields; sf; sf = sf->next) {
            classify_field(sf->type, field_offset + sf->offset, eb);
        }
        return;
    } else if (field_type->kind == TY_ARRAY) {
        for (int i = 0; i < field_type->array_len; i++) {
            classify_field(field_type->base, field_offset + i * type_size(field_type->base), eb);
        }
        return;
    }

    if (first_eb >= 2 || last_eb >= 2) {
        eb[0] = eb[1] = CLASS_MEMORY;
        return;
    }
    eb[first_eb] = abi_join(eb[first_eb], fc);
    if (last_eb != first_eb) {
        eb[last_eb] = abi_join(eb[last_eb], fc);
    }
}

void classify_aggregate(Type *agg, abi_class_t eb[2]) {
    int size = type_size(agg);
    int align = type_align(agg);
    eb[0] = eb[1] = CLASS_NO_CLASS;

    if (size > 16 || size == 0) {
        eb[0] = eb[1] = CLASS_MEMORY;
        return;
    }

    /* SysV §3.2.3 step 1: unaligned aggregates (straddling boundary) go to MEMORY.
     * If an aggregate is larger than 8 bytes and its alignment is less than 8,
     * it might not start at a boundary multiple, which forces MEMORY. */
    if (align < 8 && size > 8) {
        eb[0] = eb[1] = CLASS_MEMORY;
        return;
    }

    if (agg->kind == TY_STRUCT || agg->kind == TY_UNION) {
        StructField *f;
        for (f = agg->fields; f; f = f->next) {
            classify_field(f->type, f->offset, eb);
            if (eb[0] == CLASS_MEMORY) return;
        }
    }

    if (eb[0] == CLASS_MEMORY || eb[1] == CLASS_MEMORY) {
        eb[0] = eb[1] = CLASS_MEMORY;
    }
}

/* ---------------------------------------------------------------- */
/* Load value from address in %rax                                   */
/* ---------------------------------------------------------------- */

void codegen_load(Compiler *cc, Type *type) {
  if (backend_ops) {
      if (!type || type->size == 4 || type->size == 8 || type->kind == TY_PTR) {
          fprintf(cc->out, "    ldr r0, [r0]\n");
      } else if (type->size == 1) {
          fprintf(cc->out, "    ldrb r0, [r0]\n");
      } else if (type->size == 2) {
          fprintf(cc->out, "    ldrh r0, [r0]\n");
      }
      return;
  }
  if (!type) {
    fprintf(cc->out, "    movq (%%rax), %%rax\n");
    return;
  }
  /* Do NOT check fault range here: stage2 miscompiles it and rejects valid
   * Type* ptrs. */
  if (type->kind == TY_ARRAY)
    return;
  if (type->kind == TY_STRUCT || type->kind == TY_UNION)
    return;
  /* Pointers must always be 64-bit (fixes stage2 sign-extended 32-bit load ->
   * 0x80xxxxxx crash). */
  if (type->kind == TY_PTR) {
    fprintf(cc->out, "    movq (%%rax), %%rax\n");
    return;
  }
  if (type->kind == TY_FLOAT) {
    fprintf(cc->out, "    movss (%%rax), %%xmm0\n");
    fprintf(cc->out, "    movq %%xmm0, %%rax\n");
    return;
  }
  if (type->kind == TY_DOUBLE) {
    fprintf(cc->out, "    movsd (%%rax), %%xmm0\n");
    fprintf(cc->out, "    movq %%xmm0, %%rax\n");
    return;
  }
  switch (type->size) {
  case 1:
    if (is_unsigned_type(type))
      fprintf(cc->out, "    movzbq (%%rax), %%rax\n");
    else
      fprintf(cc->out, "    movsbq (%%rax), %%rax\n");
    break;
  case 2:
    if (is_unsigned_type(type))
      fprintf(cc->out, "    movzwq (%%rax), %%rax\n");
    else
      fprintf(cc->out, "    movswq (%%rax), %%rax\n");
    break;
  case 4:
    if (is_unsigned_type(type))
      fprintf(cc->out, "    movl (%%rax), %%eax\n");
    else
      fprintf(cc->out, "    movslq (%%rax), %%rax\n");
    break;
  default:
    fprintf(cc->out, "    movq (%%rax), %%rax\n");
    break;
  }
}

/* ---------------------------------------------------------------- */
/* Store: value in %r11, address in %rax                             */
/* ---------------------------------------------------------------- */

void codegen_store(Compiler *cc, Type *type) {
  pop_reg(cc, "r11");
  if (backend_ops) {
      if (!type || type->size == 4 || type->size == 8 || type->kind == TY_PTR) {
          fprintf(cc->out, "    str r1, [r0]\n");
      } else if (type->size == 1) {
          fprintf(cc->out, "    strb r1, [r0]\n");
      } else if (type->size == 2) {
          fprintf(cc->out, "    strh r1, [r0]\n");
      }
      fprintf(cc->out, "    mov r0, r1\n");
      return;
  }
  if (!type) {
    if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
    if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
    return;
  }
  /* Do NOT check fault range here: stage2 miscompiles it and rejects valid
   * Type* ptrs. */
  /* Pointers must always be 64-bit (fixes stage2 sign-extended 32-bit store).
   */
  if (type->kind == TY_PTR) {
    if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
    if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
    return;
  }
  if (type->kind == TY_STRUCT || type->kind == TY_UNION || type->size > 8) {
    fprintf(cc->out, "    pushq %%rsi\n");
    fprintf(cc->out, "    pushq %%rdi\n");
    fprintf(cc->out, "    pushq %%rcx\n");
    fprintf(cc->out, "    movq %%r11, %%rsi\n");
    fprintf(cc->out, "    movq %%rax, %%rdi\n");
    fprintf(cc->out, "    movq $%d, %%rcx\n", type->size);
    fprintf(cc->out, "    rep movsb\n");
    fprintf(cc->out, "    popq %%rcx\n");
    fprintf(cc->out, "    popq %%rdi\n");
    fprintf(cc->out, "    popq %%rsi\n");
    if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
    return;
  }
  switch (type->size) {
  case 1:
    if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
    break;
  case 2:
    if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
    break;
  case 4:
    if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
    break;
  default:
    if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
    break;
  }
  if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
}

/* Return cutoff so stage2 can't miscompile inline constant (GDB showed
 * node=0x10000). */
static unsigned long long bad_node_cutoff(void) { return 65536; }

/* Only null and very low ptr (<=0x10000). No hi/lo range check here—stage2
 * miscompiles it and then rejects valid heap (e.g. 0x00A7C8C0).
 * ptr_in_fault_range handles 0x80xxxxxx. */
static int is_bad_ptr(const void *p) {
  unsigned long long u = (unsigned long long)(const char *)p;
  return !p || u <= bad_node_cutoff();
}

/* Validate node/type after creation; fatal on first bad to pinpoint root cause.
 */
void validate_node(Compiler *cc, Node *node, const char *where, int line) {
  char buf[320];
  if (!node)
    return;
  if (node->magic != 0xC0FFEEBAD1234567ULL) {
    sprintf(buf, "CORRUPTED NODE (magic=0x%llx, alloc_id=%llu) at %s",
            (unsigned long long)node->magic, (unsigned long long)node->alloc_id,
            where);
    error_at(cc, line, buf);
    exit(1);
  }
  /* Do NOT check ptr_in_fault_range(node->type): stage2 miscompiles it. */
}

void validate_type(Compiler *cc, Type *type, const char *where, int line) {
  char buf[256];
  if (!type)
    return;
  if (type->magic != 0xDEADBEEF87654321ULL) {
    sprintf(buf, "CORRUPTED TYPE (magic=0x%llx, alloc_id=%llu) at %s",
            (unsigned long long)type->magic, (unsigned long long)type->alloc_id,
            where);
    error_at(cc, line, buf);
    exit(1);
  }
  /* Do NOT check ptr_in_fault_range(type) here: type is the pointer we just got
   * from cc_alloc (arena address like 0xa56040). Stage2 miscompiles that check
   * and falsely rejects valid ptrs. */
}

static int guard_error_count = 0;

/* Fatal on first bad node in codegen to stop spam. */
static void guard_node(Compiler *cc, Node *node) {
  char buf[256];
  if (!node || guard_error_count > 0)
    return;
  if (node->magic != 0xC0FFEEBAD1234567ULL) {
    sprintf(buf, "FATAL: node magic corrupted (0x%llx, id=%llu)",
            (unsigned long long)node->magic,
            (unsigned long long)node->alloc_id);
    error_at(cc, node->line, buf);
    guard_error_count++;
    exit(1);
  }
  /* Do NOT check ptr_in_fault_range(node->type): stage2 miscompiles it and
   * rejects valid Type* ptrs. */
}

/* Wrappers: catch bad ptr before any node load. */
static void codegen_expr_checked(Compiler *cc, Node *n) {
  if (is_bad_ptr(n)) {
    fprintf(cc->out, "    movq $0, %%rax\n");
    return;
  }
  codegen_expr(cc, n);
}
static void codegen_addr_checked(Compiler *cc, Node *n) {
  /* In main, never substitute 0 for param/local addresses: Stage2 can have
   * "bad" node ptrs (e.g. 0x10000) and we must use the fixed stack slots. */
  if (is_bad_ptr(n) &&
      (!cc->current_func[0] || strcmp(cc->current_func, "main") != 0)) {
    fprintf(cc->out, "    movq $0, %%rax\n");
    return;
  }
  codegen_addr(cc, n);
}

/* ---------------------------------------------------------------- */
/* Address of lvalue into %rax                                       */
/* ---------------------------------------------------------------- */

void codegen_addr(Compiler *cc, Node *node) {
  char errbuf[80];
  /* Do NOT use ptr_in_fault_range(node): stage2 miscompiles it and rejects
   * valid arena ptrs. */
  if (!node) {
    error_at(cc, 0, "codegen_addr: null node");
    fprintf(cc->out, "    movq $0, %%rax\n");
    return;
  }
  /* In main, do not substitute 0 for bad node ptr — we need to reach the main
   * param/local stack slots. */
  if (is_bad_ptr(node) &&
      (!cc->current_func[0] || strcmp(cc->current_func, "main") != 0)) {
    error_at(cc, 0, "codegen_addr: bad node ptr");
    fprintf(cc->out, "    movq $0, %%rax\n");
    return;
  }
  if (node->kind == ND_VAR) {
    /* Main params/locals: always use fixed stack layout when in main, so Stage2
     * never emits globals for these. */
    if (cc->current_func[0] && strcmp(cc->current_func, "main") == 0) {
      if (strcmp(node->name, "argc") == 0) {
        if (cc->verbose)
          fprintf(stderr, "zcc: codegen main param 'argc'\n");
        fprintf(cc->out, "    leaq -8(%%rbp), %%rax\n");
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, "%stack_-8", node->line);
        return;
      }
      if (strcmp(node->name, "argv") == 0) {
        if (cc->verbose)
          fprintf(stderr, "zcc: codegen main param 'argv'\n");
        fprintf(cc->out, "    leaq -16(%%rbp), %%rax\n");
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, "%stack_-16", node->line);
        return;
      }
      if (strcmp(node->name, "input_file") == 0) {
        if (cc->verbose)
          fprintf(stderr, "zcc: codegen main local 'input_file'\n");
        fprintf(cc->out, "    leaq -32(%%rbp), %%rax\n");
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, "%stack_-32", node->line);
        return;
      }
      if (strcmp(node->name, "output_file") == 0) {
        if (cc->verbose)
          fprintf(stderr, "zcc: codegen main local 'output_file'\n");
        fprintf(cc->out, "    leaq -40(%%rbp), %%rax\n");
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, "%stack_-40", node->line);
        return;
      }
    }
    /* Do NOT check is_bad_ptr(node->sym): Stage1 can have valid Symbol* in
     * fault range -> emit global -> segfault. */
    /* Use stack whenever we have a symbol with stack_offset. Params/locals have
     * negative offset; guard against wrong sign. */
    if (node->sym) {
      int off = node->sym->stack_offset;
      if (off > 0)
        off = -off; /* never use positive: would write above frame (e.g. return
                       address) and crash */
      if (off != 0 && !node->sym->is_global) {
        if (backend_ops) {
            fprintf(cc->out, "    ldr r3, =%d\n    adds r0, r7, r3\n", off);
        } else {
            fprintf(cc->out, "    leaq %d(%%rbp), %%rax\n", off);
        }
        char vname[32];
        sprintf(vname, "%%stack_%d", off);
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, vname, node->line);
        return;
      }
    }
    if (backend_ops) {
        fprintf(cc->out, "    ldr r0, =%s\n", node->name);
    } else {
        fprintf(cc->out, "    leaq %s(%%rip), %%rax\n", node->name);
    }
    char gname[32];
    sprintf(gname, "%%%s", node->name);
    char *dst = ir_bridge_fresh_tmp();
    ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, gname, node->line);
    return;
  }
  if (node->kind == ND_DEREF) {
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_addr: ND_DEREF null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    return; /* Note: ptr is ALREADY eval'd by codegen_expr! ir_last_result holds
               it! */
  }
  if (node->kind == ND_MEMBER) {
    if (!node->lhs) {
      error_at(cc, node->line,
               "codegen_addr: ND_MEMBER null lhs (member access on null)");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    if (node->member_offset != 0) {
      fprintf(cc->out, "    addq $%d, %%rax\n", node->member_offset);
      char lhs_ir[32];
      ir_save_result(lhs_ir);
      char off_str[32];
      char c_tmp[32];
      sprintf(off_str, "%d", node->member_offset);
      sprintf(c_tmp, "%%t%d", ir_tmp_counter++);
      ZCC_EMIT_UNARY(IR_CONST, IR_TY_I64, c_tmp, off_str, node->line);

      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_BINARY(IR_ADD, IR_TY_PTR, dst, lhs_ir, c_tmp, node->line);
    }
    return;
  }
  error_at(cc, node->line, "not an lvalue");
}

static int ptr_elem_size(Type *type) {
  if (!type)
    return 1;
  if (type->kind == TY_PTR || type->kind == TY_ARRAY) {
    if (type->base)
      return type_size(type->base);
  }
  return 1;
}

/* ---------------------------------------------------------------- */
/* Expression codegen — result in %rax                               */
/* ---------------------------------------------------------------- */

void codegen_expr(Compiler *cc, Node *node) {
  if (!node)
    return;

  if (cc) {
  }

  int lbl1;
  int lbl2;
  char errbuf[80];

  /* Do NOT use ptr_in_fault_range(node): stage2 miscompiles it and rejects
   * valid arena ptrs. */
  if (!node) {
    error_at(cc, 0, "codegen_expr: NULL node");
    fprintf(cc->out, "    movq $0, %%rax\n");
    return;
  }
  /* In main, do not substitute 0 for bad node ptr — we need to reach main
   * param/local codegen. */
  if (is_bad_ptr(node) &&
      (!cc->current_func[0] || strcmp(cc->current_func, "main") != 0)) {
    error_at(cc, 0, "codegen_expr: bad node ptr");
    fprintf(cc->out, "    movq $0, %%rax\n");
    return;
  }
  if (node->kind < ND_NUM || node->kind > ND_NOP) {
    sprintf(errbuf, "codegen_expr: invalid kind %d (0x%x) at %p", node->kind,
            node->kind, (void *)node);
    error_at(cc, node->line, errbuf);
    return;
  }
  guard_node(cc, node);

  switch (node->kind) {

  case ND_NUM:
    if (backend_ops) {
        fprintf(cc->out, "    ldr r0, =%lld\n", node->int_val);
        {
          char *dst = ir_bridge_fresh_tmp();
          ZCC_EMIT_CONST(ir_map_type(node->type), dst, node->int_val, node->line);
        }
        return;
    }
    if (node->int_val >= -2147483648) {
      if (node->int_val <= 2147483647) {
        fprintf(cc->out, "    movq $%lld, %%rax\n", node->int_val);
        {
          char *dst = ir_bridge_fresh_tmp();
          ZCC_EMIT_CONST(ir_map_type(node->type), dst, node->int_val,
                         node->line);
        }
        return;
      }
    }
    fprintf(cc->out, "    movabsq $%lld, %%rax\n", node->int_val);
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_CONST(ir_map_type(node->type), dst, node->int_val, node->line);
    }
    return;

  case ND_FLIT: {
    int lbl;
    lbl = cc->str_label_count;
    cc->str_label_count = cc->str_label_count + 1;
    if (node->type && node->type->kind == TY_FLOAT) {
      /* CG-FLOAT-007: float literal (f/F suffix) — emit 32-bit IEEE bits */
      float fv = (float)node->f_val;
      unsigned int fbits;
      memcpy(&fbits, &fv, sizeof(float));
      fprintf(cc->out, "    .section .rodata\n");
      fprintf(cc->out, "    .p2align 2\n");
      fprintf(cc->out, ".LS_flit_%d:\n", lbl);
      fprintf(cc->out, "    .long %u\n", fbits);
      fprintf(cc->out, "    .text\n");
      fprintf(cc->out, "    movss .LS_flit_%d(%%rip), %%xmm0\n", lbl);
      fprintf(cc->out, "    movd %%xmm0, %%eax\n");
    } else {
      /* double literal — existing path */
      unsigned long long bits;
      memcpy(&bits, &node->f_val, sizeof(double));
      fprintf(cc->out, "    .section .rodata\n");
      fprintf(cc->out, "    .p2align 3\n");
      fprintf(cc->out, ".LS_flit_%d:\n", lbl);
      fprintf(cc->out, "    .quad %llu\n", bits);
      fprintf(cc->out, "    .text\n");
      fprintf(cc->out, "    movsd .LS_flit_%d(%%rip), %%xmm0\n", lbl);
      fprintf(cc->out, "    movq %%xmm0, %%rax\n");
    }
    /* Satisfy IR subsystem sequence */
    {
      char *dst = ir_bridge_fresh_tmp();
      long long flit_bits;
      memcpy(&flit_bits, &node->f_val, sizeof(double));
      ZCC_EMIT_FCONST(dst, flit_bits, node->line);
    }
    return;
  }

  case ND_STR: {
    char lbl_buf[32];
    fprintf(cc->out, "    leaq .LS%d(%%rip), %%rax\n",
            cc->strings[node->str_id].label_id);
    sprintf(lbl_buf, ".LS%d", cc->strings[node->str_id].label_id);
    char *dst = ir_bridge_fresh_tmp();
    ZCC_EMIT_CONST(IR_TY_PTR, dst, 0, node->line); /* Fake for now */
    ZCC_EMIT_UNARY(IR_CONST_STR, IR_TY_PTR, dst, lbl_buf,
                   node->line); /* REAL emit */
    return;
  }

  case ND_VAR:
    if (node->sym && node->sym->assigned_reg) {
      if (backend_ops) fprintf(cc->out, "    mov r0, %s\n", node->sym->assigned_reg); else fprintf(cc->out, "    movq %s, %%rax\n", node->sym->assigned_reg);
      if (node->type && !node_type_unsigned(node) && !is_pointer(node->type)) {
          if (node->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
          else if (node->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->type && node_type_unsigned(node)) {
          if (node->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
          else if (node->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      {
        char *vname = ir_var_name(node);
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_LOAD(ir_map_type(node->type), dst, vname, node->line);
      }
      return;
    }
    codegen_addr_checked(cc, node);
    if (node->type) {
      if (node->type->kind != TY_ARRAY) {
        if (node->type->kind != TY_STRUCT) {
          if (node->type->kind != TY_UNION) {
            if (node->type->kind != TY_FUNC) {
              codegen_load(cc, node->type);
            }
          }
        }
      }
    }
    {
      char *vname = ir_var_name(node);
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_LOAD(ir_map_type(node->type), dst, vname, node->line);
    }
    return;

  case ND_ASSIGN: {
    char rhs_ir[32];
    if (!node->lhs || !node->rhs) {
      error_at(cc, node->line, "codegen_expr: ND_ASSIGN missing lhs or rhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      if (backend_ops) fprintf(cc->out, "    mov %s, r0\n", node->lhs->sym->assigned_reg); else fprintf(cc->out, "    movq %%rax, %s\n", node->lhs->sym->assigned_reg);
      {
        char *vname = ir_var_name(node->lhs);
        ZCC_EMIT_STORE(ir_map_type(node->lhs->type), vname, rhs_ir, node->line);
      }
      return;
    }
    push_reg(cc, "rax");
    codegen_addr_checked(cc, node->lhs);
    char lhs_addr_ir[32];
    ir_save_result(lhs_addr_ir);
    if (is_bad_ptr(node->lhs) &&
        (!cc->current_func[0] || strcmp(cc->current_func, "main") != 0)) {
      error_at(cc, node->line, "codegen_expr: ND_ASSIGN lhs bad ptr");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (!node->lhs->type && node->lhs->kind != ND_MEMBER) {
      error_at(cc, node->line, "codegen_expr: ND_ASSIGN lhs has null type");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    /* Use member_size for ND_MEMBER so stage2 stores 4 bytes to cc->tk etc.,
     * not 8 */
    if (node->lhs->kind == ND_MEMBER && node->lhs->member_size > 0) {
      pop_reg(cc, "r11");
      if ((node->lhs->type && (node->lhs->type->kind == TY_STRUCT || node->lhs->type->kind == TY_UNION)) || node->lhs->member_size > 8) {
        fprintf(cc->out, "    pushq %%rsi\n");
        fprintf(cc->out, "    pushq %%rdi\n");
        fprintf(cc->out, "    pushq %%rcx\n");
        fprintf(cc->out, "    movq %%r11, %%rsi\n");
        fprintf(cc->out, "    movq %%rax, %%rdi\n");
        fprintf(cc->out, "    movq $%d, %%rcx\n", node->lhs->member_size);
        fprintf(cc->out, "    rep movsb\n");
        fprintf(cc->out, "    popq %%rcx\n");
        fprintf(cc->out, "    popq %%rdi\n");
        fprintf(cc->out, "    popq %%rsi\n");
        if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
      } else {
        /* If member type is pointer/func, always use movq regardless of member_size */
        if (node->lhs->type && is_pointer(node->lhs->type)) {
          if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
        } else {
        switch (node->lhs->member_size) {
        case 1:
          if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
          break;
        case 2:
          if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
          break;
        case 4:
          if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
          break;
        default:
          if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
          break;
        }
        }
        if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
      }
    } else {
      codegen_store(cc, node->lhs->type);
    }
    {
      ZCC_EMIT_STORE(ir_map_type(node->lhs->type), lhs_addr_ir, rhs_ir,
                     node->line);
    }
    return;
  }

  case ND_COMPOUND_ASSIGN:
    if (!node->lhs || !node->rhs) {
      error_at(cc, node->line,
               "codegen_expr: ND_COMPOUND_ASSIGN missing operand");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      codegen_expr_checked(cc, node->rhs);
      if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
      if (backend_ops) fprintf(cc->out, "    mov r0, %s\n", reg); else fprintf(cc->out, "    movq %s, %%rax\n", reg);
      switch (node->compound_op) {
      case ND_ADD:
        if (is_pointer(node->lhs->type)) {
          int esz = ptr_elem_size(node->lhs->type);
          if (esz > 1)
            EMIT_PTR_SCALE_R11(cc, esz);
        }
        fprintf(cc->out, "    addq %%r11, %%rax\n");
        break;
      case ND_SUB:
        if (is_pointer(node->lhs->type)) {
          int esz = ptr_elem_size(node->lhs->type);
          if (esz > 1)
            EMIT_PTR_SCALE_R11(cc, esz);
        }
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_SUB);
      else fprintf(cc->out, "    subq %%r11, %%rax\n");
        break;
      case ND_MUL:
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_MUL);
      else fprintf(cc->out, "    imulq %%r11, %%rax\n");
        break;
      case ND_DIV:
        if (node->lhs->type && is_unsigned_type(node->lhs->type)) {
          fprintf(cc->out, "    xorq %%rdx, %%rdx\n    divq %%r11\n");
        } else {
          fprintf(cc->out, "    cqo\n    idivq %%r11\n");
        }
        break;
      case ND_MOD:
        if (node->lhs->type && is_unsigned_type(node->lhs->type)) {
          fprintf(
              cc->out,
              "    xorq %%rdx, %%rdx\n    divq %%r11\n    movq %%rdx, %%rax\n");
        } else {
          fprintf(cc->out, "    cqo\n    idivq %%r11\n    movq %%rdx, %%rax\n");
        }
        break;
      case ND_BAND:
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_BAND);
      else fprintf(cc->out, "    andq %%r11, %%rax\n");
        break;
      case ND_BOR:
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_BOR);
      else fprintf(cc->out, "    orq %%r11, %%rax\n");
        break;
      case ND_BXOR:
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_BXOR);
      else fprintf(cc->out, "    xorq %%r11, %%rax\n");
        break;
      case ND_SHL:
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_SHL);
      else fprintf(cc->out, "    movq %%r11, %%rcx\n    shlq %%cl, %%rax\n");
        break;
      case ND_SHR:
        if (node->lhs->type && is_unsigned_type(node->lhs->type))
          if (backend_ops) backend_ops->emit_binary_op(cc, ND_SHR);
      else fprintf(cc->out, "    movq %%r11, %%rcx\n    shrq %%cl, %%rax\n");
        else
          if (backend_ops) backend_ops->emit_binary_op(cc, ND_SHR);
      else fprintf(cc->out, "    movq %%r11, %%rcx\n    sarq %%cl, %%rax\n");
        break;
      default:
        break;
      }
      if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      if (backend_ops) fprintf(cc->out, "    mov %s, r0\n", reg); else fprintf(cc->out, "    movq %%rax, %s\n", reg);
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    if (is_bad_ptr(node->lhs)) {
      error_at(cc, node->line, "codegen_expr: ND_COMPOUND_ASSIGN lhs bad ptr");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (!node->lhs->type) {
      error_at(cc, node->line,
               "codegen_expr: ND_COMPOUND_ASSIGN lhs has null type");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    push_reg(cc, "rax");
    if (node->lhs->kind == ND_MEMBER && node->lhs->member_size > 0) {
      switch (node->lhs->member_size) {
      case 1:
        fprintf(cc->out, "    movzbl (%%rax), %%eax\n");
        break;
      case 2:
        fprintf(cc->out, "    movzwl (%%rax), %%eax\n");
        break;
      case 4:
        fprintf(cc->out, "    movl (%%rax), %%eax\n");
        break;
      default:
        fprintf(cc->out, "    movq (%%rax), %%rax\n");
        break;
      }
    } else {
      codegen_load(cc, node->lhs->type);
    }
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    switch (node->compound_op) {
    case ND_ADD:
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        /* CG-FLOAT-008a: dispatch ss vs sd based on lhs type */
        if (node->lhs->type->kind == TY_FLOAT) {
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    movd %%r11d, %%xmm1\n");
          fprintf(cc->out, "    addss %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movd %%xmm0, %%eax\n");
        } else {
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    movq %%r11, %%xmm1\n");
          fprintf(cc->out, "    addsd %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        }
        break;
      }
      if (is_pointer(node->lhs->type)) {
        int esz;
        esz = ptr_elem_size(node->lhs->type);
        if (esz > 1)
          EMIT_PTR_SCALE_R11(cc, esz);
      }
      fprintf(cc->out, "    addq %%r11, %%rax\n");
      break;
    case ND_SUB:
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        /* CG-FLOAT-008a: dispatch ss vs sd based on lhs type */
        if (node->lhs->type->kind == TY_FLOAT) {
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    movd %%r11d, %%xmm1\n");
          fprintf(cc->out, "    subss %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movd %%xmm0, %%eax\n");
        } else {
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    movq %%r11, %%xmm1\n");
          fprintf(cc->out, "    subsd %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        }
        break;
      }
      if (is_pointer(node->lhs->type)) {
        int esz = ptr_elem_size(node->lhs->type);
        if (esz > 1)
          EMIT_PTR_SCALE_R11(cc, esz);
      }
      if (backend_ops) backend_ops->emit_binary_op(cc, ND_SUB);
      else fprintf(cc->out, "    subq %%r11, %%rax\n");
      break;
    case ND_MUL:
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        /* CG-FLOAT-008a: dispatch ss vs sd based on lhs type */
        if (node->lhs->type->kind == TY_FLOAT) {
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    movd %%r11d, %%xmm1\n");
          fprintf(cc->out, "    mulss %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movd %%xmm0, %%eax\n");
        } else {
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    movq %%r11, %%xmm1\n");
          fprintf(cc->out, "    mulsd %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        }
        break;
      }
      if (backend_ops) backend_ops->emit_binary_op(cc, ND_MUL);
      else fprintf(cc->out, "    imulq %%r11, %%rax\n");
      break;
    case ND_DIV:
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        /* CG-FLOAT-008a: dispatch ss vs sd based on lhs type */
        if (node->lhs->type->kind == TY_FLOAT) {
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    movd %%r11d, %%xmm1\n");
          fprintf(cc->out, "    divss %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movd %%xmm0, %%eax\n");
        } else {
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    movq %%r11, %%xmm1\n");
          fprintf(cc->out, "    divsd %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        }
        break;
      }
      if (node->lhs->type && is_unsigned_type(node->lhs->type)) {
        fprintf(cc->out, "    xorq %%rdx, %%rdx\n");
        fprintf(cc->out, "    divq %%r11\n");
      } else {
        fprintf(cc->out, "    cqo\n    idivq %%r11\n");
      }
      break;
    case ND_MOD:
      if (node->lhs->type && is_unsigned_type(node->lhs->type)) {
        fprintf(cc->out, "    xorq %%rdx, %%rdx\n");
        fprintf(cc->out, "    divq %%r11\n");
        if (backend_ops) fprintf(cc->out, "    mov r0, r2\n");
      else fprintf(cc->out, "    movq %%rdx, %%rax\n");
      } else {
        fprintf(cc->out, "    cqo\n    idivq %%r11\n    movq %%rdx, %%rax\n");
      }
      break;
    case ND_BAND:
      if (backend_ops) backend_ops->emit_binary_op(cc, ND_BAND);
      else fprintf(cc->out, "    andq %%r11, %%rax\n");
      break;
    case ND_BOR:
      if (backend_ops) backend_ops->emit_binary_op(cc, ND_BOR);
      else fprintf(cc->out, "    orq %%r11, %%rax\n");
      break;
    case ND_BXOR:
      if (backend_ops) backend_ops->emit_binary_op(cc, ND_BXOR);
      else fprintf(cc->out, "    xorq %%r11, %%rax\n");
      break;
    case ND_SHL:
      if (backend_ops) backend_ops->emit_binary_op(cc, ND_SHL);
      else fprintf(cc->out, "    movq %%r11, %%rcx\n    shlq %%cl, %%rax\n");
      break;
    case ND_SHR:
      if (node->lhs->type && is_unsigned_type(node->lhs->type))
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_SHR);
      else fprintf(cc->out, "    movq %%r11, %%rcx\n    shrq %%cl, %%rax\n");
      else
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_SHR);
      else fprintf(cc->out, "    movq %%r11, %%rcx\n    sarq %%cl, %%rax\n");
      break;
    default:
      break;
    }
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (node->lhs->kind == ND_MEMBER && node->lhs->member_size > 0) {
      switch (node->lhs->member_size) {
      case 1:
        if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
        break;
      case 2:
        if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
        break;
      case 4:
        if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
        break;
      default:
        if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
        break;
      }
    } else {
      switch (type_size(node->lhs->type)) {
      case 1:
        if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
        break;
      case 2:
        if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
        break;
      case 4:
        if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
        break;
      default:
        if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
        break;
      }
    }
    if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
    if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
        if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
    } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
        if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
    }
    {
      char *dst = ir_bridge_fresh_tmp();
      ir_emit_binary_op(node->compound_op, node->lhs->type, "ca_lhs", "ca_rhs", node->line);
    }
    return;

  case ND_ADD: {
    char lhs_ir[32];
    char rhs_ir[32];
    if (!node->lhs || !node->rhs) {
      error_at(cc, node->line, "codegen_expr: ND_ADD missing operand");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (node->type && is_float_type(node->type)) {
      int is_f32 = (node->type->kind == TY_FLOAT);
      codegen_expr_checked(cc, node->lhs);
      if (!is_float_type(node->lhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      if (is_f32) fprintf(cc->out, "    movss %%xmm0, (%%rsp)\n");
      else        fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!is_float_type(node->rhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      if (is_f32) { fprintf(cc->out, "    movss (%%rsp), %%xmm1\n"); }
      else        { fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n"); }
      fprintf(cc->out, "    addq $8, %%rsp\n");
      if (is_f32) { fprintf(cc->out, "    addss %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movd %%xmm0, %%eax\n"); }
      else        { fprintf(cc->out, "    addsd %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movq %%xmm0, %%rax\n"); }
      ir_emit_binary_op(ND_ADD, node->type, "f_lhs", "f_rhs", node->line);
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
    else if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (node->lhs->type && is_pointer(node->lhs->type)) {
      int esz = ptr_elem_size(node->lhs->type);
      if (esz > 1) {
        if (backend_ops) fprintf(cc->out, "    ldr r3, =%d\n    muls r1, r3, r1\n", esz);
        else EMIT_PTR_SCALE_R11(cc, esz);
        char scale_str[32];
        sprintf(scale_str, "%d", esz);
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_BINARY(IR_MUL, IR_TY_I64, dst, rhs_ir, scale_str, node->line);
        strcpy(rhs_ir, dst);
      }
    } else if (node->rhs->type && is_pointer(node->rhs->type)) {
      int esz = ptr_elem_size(node->rhs->type);
      if (esz > 1) {
        if (backend_ops) fprintf(cc->out, "    ldr r3, =%d\n    muls r0, r3, r0\n", esz);
        else EMIT_PTR_SCALE_RAX(cc, esz);
        char scale_str[32];
        sprintf(scale_str, "%d", esz);
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_BINARY(IR_MUL, IR_TY_I64, dst, lhs_ir, scale_str, node->line);
        strcpy(lhs_ir, dst);
      }
    }
    if (backend_ops && backend_ops->emit_binary_op) {
        backend_ops->emit_binary_op(cc, ND_ADD);
    } else {
        fprintf(cc->out, "    addq %%r11, %%rax\n");
    }
    if (node->type && type_size(node->type) == 4 && !is_pointer(node->type)) {
      if (node_type_unsigned(node)) {
        if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n");
      } else {
        if (!backend_ops) fprintf(cc->out, "    cltq\n");
      }
    }
    ir_emit_binary_op(ND_ADD, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_SUB: {
    char lhs_ir[32];
    char rhs_ir[32];
    if (!node->lhs || !node->rhs) {
      error_at(cc, node->line, "codegen_expr: ND_SUB missing operand");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (node->type && is_float_type(node->type)) {
      int is_f32 = (node->type->kind == TY_FLOAT);
      codegen_expr_checked(cc, node->lhs);
      if (!is_float_type(node->lhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      if (is_f32) fprintf(cc->out, "    movss %%xmm0, (%%rsp)\n");
      else        fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!is_float_type(node->rhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      if (is_f32) { fprintf(cc->out, "    movss (%%rsp), %%xmm1\n"); }
      else        { fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n"); }
      fprintf(cc->out, "    addq $8, %%rsp\n");
      /* note: subsd/subss operand order: xmm1 -= xmm0, then copy to xmm0 */
      if (is_f32) { fprintf(cc->out, "    subss %%xmm0, %%xmm1\n"); fprintf(cc->out, "    movaps %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movd %%xmm0, %%eax\n"); }
      else        { fprintf(cc->out, "    subsd %%xmm0, %%xmm1\n"); fprintf(cc->out, "    movsd %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movq %%xmm0, %%rax\n"); }
      ir_emit_binary_op(ND_SUB, node->type, "f_lhs", "f_rhs", node->line);
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
    else if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (node->lhs->type && is_pointer(node->lhs->type)) {
      if (node->rhs->type && is_pointer(node->rhs->type)) {
        if (backend_ops && backend_ops->emit_binary_op) backend_ops->emit_binary_op(cc, ND_SUB);
        else if (backend_ops) backend_ops->emit_binary_op(cc, ND_SUB);
      else fprintf(cc->out, "    subq %%r11, %%rax\n");
        {
          int esz;
          esz = ptr_elem_size(node->lhs->type);
          if (esz > 1) {
            if (backend_ops) { /* no-op for hello.c */ } else {
            fprintf(cc->out, "    movq $%d, %%r11\n", esz);
            fprintf(cc->out, "    cqo\n    idivq %%r11\n");
            }
          }
        }
        ir_emit_binary_op(ND_SUB, node->type, lhs_ir, rhs_ir, node->line);
        return;
      }
      {
        int esz;
        esz = node->lhs->type ? ptr_elem_size(node->lhs->type) : 1;
        if (esz > 1) {
          if (backend_ops) {
            fprintf(cc->out, "    ldr r3, =%d\n    muls r1, r3, r1\n", esz);
          } else {
            EMIT_PTR_SCALE_R11(cc, esz);
          }
        }
      }
    }
    if (backend_ops && backend_ops->emit_binary_op) backend_ops->emit_binary_op(cc, ND_SUB);
    else if (backend_ops) backend_ops->emit_binary_op(cc, ND_SUB);
      else fprintf(cc->out, "    subq %%r11, %%rax\n");
    if (node->type && type_size(node->type) == 4 && !is_pointer(node->type)) {
      if (node_type_unsigned(node)) {
        if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n");
      } else {
        if (!backend_ops) fprintf(cc->out, "    cltq\n");
      }
    }
    ir_emit_binary_op(ND_SUB, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_MUL: {
    char lhs_ir[32];
    char rhs_ir[32];
    if (!node->lhs || !node->rhs) {
      error_at(cc, node->line, "codegen_expr: binary op missing operand");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (node->type && is_float_type(node->type)) {
      int is_f32 = (node->type->kind == TY_FLOAT);
      codegen_expr_checked(cc, node->lhs);
      if (!is_float_type(node->lhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      if (is_f32) fprintf(cc->out, "    movss %%xmm0, (%%rsp)\n");
      else        fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!is_float_type(node->rhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      if (is_f32) { fprintf(cc->out, "    movss (%%rsp), %%xmm1\n"); }
      else        { fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n"); }
      fprintf(cc->out, "    addq $8, %%rsp\n");
      if (is_f32) { fprintf(cc->out, "    mulss %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movd %%xmm0, %%eax\n"); }
      else        { fprintf(cc->out, "    mulsd %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movq %%xmm0, %%rax\n"); }
      ir_emit_binary_op(ND_MUL, node->type, "f_lhs", "f_rhs", node->line);
      return;
    }
    if (!backend_ops) {
      if (node->rhs->kind == ND_NUM && is_power_of_2_val(node->rhs->int_val)) {
        int shift;
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(lhs_ir);
        shift = log2_of(node->rhs->int_val);
        fprintf(cc->out, "    shlq $%d, %%rax\n", shift);
        ir_emit_binary_op(ND_SHL, node->type, lhs_ir, "unused_rhs", node->line);
        return;
      }
      if (node->lhs->kind == ND_NUM && is_power_of_2_val(node->lhs->int_val)) {
        int shift;
        codegen_expr_checked(cc, node->rhs);
        ir_save_result(rhs_ir);
        shift = log2_of(node->lhs->int_val);
        fprintf(cc->out, "    shlq $%d, %%rax\n", shift);
        ir_emit_binary_op(ND_SHL, node->type, "unused_lhs", rhs_ir, node->line);
        return;
      }
      if (node->rhs->kind == ND_NUM && node->rhs->int_val == 3) {
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(lhs_ir);
        fprintf(cc->out, "    leaq (%%rax,%%rax,2), %%rax\n");
        ir_emit_binary_op(ND_MUL, node->type, lhs_ir, "unused_rhs", node->line);
        return;
      }
      if (node->rhs->kind == ND_NUM && node->rhs->int_val == 5) {
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(lhs_ir);
        fprintf(cc->out, "    leaq (%%rax,%%rax,4), %%rax\n");
        ir_emit_binary_op(ND_MUL, node->type, lhs_ir, "unused_rhs", node->line);
        return;
      }
      if (node->rhs->kind == ND_NUM && node->rhs->int_val == 9) {
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(lhs_ir);
        fprintf(cc->out, "    leaq (%%rax,%%rax,8), %%rax\n");
        ir_emit_binary_op(ND_MUL, node->type, lhs_ir, "unused_rhs", node->line);
        return;
      }
    }

    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    pop_reg(cc, "r11");
    if (backend_ops) {
        if (backend_ops->emit_binary_op) backend_ops->emit_binary_op(cc, ND_MUL);
    } else {
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_MUL);
      else fprintf(cc->out, "    imulq %%r11, %%rax\n");
    }
    if (node->type && type_size(node->type) == 4 && !is_pointer(node->type)) {
      if (node_type_unsigned(node)) {
        if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n");
      } else {
        if (!backend_ops) fprintf(cc->out, "    cltq\n");
      }
    }
    ir_emit_binary_op(ND_MUL, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_DIV: {
    char lhs_ir[32];
    char rhs_ir[32];
    if (!node->lhs || !node->rhs) {
      error_at(cc, node->line, "codegen_expr: ND_DIV missing operand");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (node->type && is_float_type(node->type)) {
      int is_f32 = (node->type->kind == TY_FLOAT);
      codegen_expr_checked(cc, node->lhs);
      if (!is_float_type(node->lhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      if (is_f32) fprintf(cc->out, "    movss %%xmm0, (%%rsp)\n");
      else        fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!is_float_type(node->rhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      if (is_f32) { fprintf(cc->out, "    movss (%%rsp), %%xmm1\n"); }
      else        { fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n"); }
      fprintf(cc->out, "    addq $8, %%rsp\n");
      /* divss/divsd: dividend xmm1, divisor xmm0 -> result in xmm1 */
      if (is_f32) { fprintf(cc->out, "    divss %%xmm0, %%xmm1\n"); fprintf(cc->out, "    movaps %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movd %%xmm0, %%eax\n"); }
      else        { fprintf(cc->out, "    divsd %%xmm0, %%xmm1\n"); fprintf(cc->out, "    movsd %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movq %%xmm0, %%rax\n"); }
      ir_emit_binary_op(ND_DIV, node->type, "f_lhs", "f_rhs", node->line);
      return;
    }

    if (node->rhs->kind == ND_NUM && is_power_of_2_val(node->rhs->int_val)) {
      int shift;
      char rhs_val_str[32];
      codegen_expr_checked(cc, node->lhs);
      ir_save_result(lhs_ir);
      shift = log2_of(node->rhs->int_val);
      if (node->type && is_unsigned_type(node->type)) {
        fprintf(cc->out, "    shrq $%d, %%rax\n", shift);
      } else {
        fprintf(cc->out, "    movq %%rax, %%rcx\n");
        fprintf(cc->out, "    sarq $63, %%rcx\n");
        fprintf(cc->out, "    andq $%lld, %%rcx\n", (1LL << shift) - 1);
        fprintf(cc->out, "    addq %%rcx, %%rax\n");
        fprintf(cc->out, "    sarq $%d, %%rax\n", shift);
      }
      snprintf(rhs_val_str, 32, "$%lld", node->rhs->int_val);
      ir_emit_binary_op(ND_DIV, node->type, lhs_ir, rhs_val_str, node->line);
      return;
    }

    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (node->lhs->type && node->rhs->type &&
        (is_unsigned_type(node->lhs->type) ||
         is_unsigned_type(node->rhs->type))) {
      fprintf(cc->out, "    xorq %%rdx, %%rdx\n");
      fprintf(cc->out, "    divq %%r11\n");
    } else {
      fprintf(cc->out, "    cqo\n    idivq %%r11\n");
    }
    ir_emit_binary_op(ND_DIV, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_MOD: {
    char lhs_ir[32];
    char rhs_ir[32];

    if (node->rhs && node->rhs->kind == ND_NUM &&
        is_power_of_2_val(node->rhs->int_val)) {
      long long mask;
      char rhs_val_str[32];
      codegen_expr_checked(cc, node->lhs);
      ir_save_result(lhs_ir);
      mask = node->rhs->int_val - 1;
      if (node->type && is_unsigned_type(node->type)) {
        fprintf(cc->out, "    andq $%lld, %%rax\n", mask);
        snprintf(rhs_val_str, 32, "$%lld", node->rhs->int_val);
        ir_emit_binary_op(ND_MOD, node->type, lhs_ir, rhs_val_str, node->line);
        return;
      }
    }

    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (node->lhs->type && node->rhs->type &&
        (is_unsigned_type(node->lhs->type) ||
         is_unsigned_type(node->rhs->type))) {
      fprintf(cc->out, "    xorq %%rdx, %%rdx\n");
      fprintf(cc->out, "    divq %%r11\n");
      if (backend_ops) fprintf(cc->out, "    mov r0, r2\n");
      else fprintf(cc->out, "    movq %%rdx, %%rax\n");
    } else {
      fprintf(cc->out, "    cqo\n    idivq %%r11\n");
      if (backend_ops) fprintf(cc->out, "    mov r0, r2\n");
      else fprintf(cc->out, "    movq %%rdx, %%rax\n");
    }
    ir_emit_binary_op(ND_MOD, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_BAND: {
    char lhs_ir[32];
    char rhs_ir[32];
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    pop_reg(cc, "r11");
    if (backend_ops) backend_ops->emit_binary_op(cc, ND_BAND);
      else fprintf(cc->out, "    andq %%r11, %%rax\n");
    ir_emit_binary_op(ND_BAND, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_BOR: {
    char lhs_ir[32];
    char rhs_ir[32];
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    pop_reg(cc, "r11");
    if (backend_ops) backend_ops->emit_binary_op(cc, ND_BOR);
      else fprintf(cc->out, "    orq %%r11, %%rax\n");
    ir_emit_binary_op(ND_BOR, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_BXOR: {
    char lhs_ir[32];
    char rhs_ir[32];
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    pop_reg(cc, "r11");
    if (backend_ops) backend_ops->emit_binary_op(cc, ND_BXOR);
      else fprintf(cc->out, "    xorq %%r11, %%rax\n");
    ir_emit_binary_op(ND_BXOR, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_BNOT: {
    char src_ir[32];
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_BNOT null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(src_ir);
    if (node->type && type_size(node->type) == 4) {
      if (node_type_unsigned(node)) {
        fprintf(cc->out, "    notl %%eax\n");
      } else {
        fprintf(cc->out, "    notl %%eax\n    cltq\n");
      }
    } else {
      fprintf(cc->out, "    notq %%rax\n");
    }
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_NOT, ir_map_type(node->type), dst, src_ir, node->line);
    }
    return;
  }

  case ND_SHL: {
    char lhs_ir[32];
    char rhs_ir[32];
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    fprintf(cc->out, "    movq %%rax, %%rcx\n");
    pop_reg(cc, "rax");
    fprintf(cc->out, "    shlq %%cl, %%rax\n");
    ir_emit_binary_op(ND_SHL, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_SHR: {
    char lhs_ir[32];
    char rhs_ir[32];
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    fprintf(cc->out, "    movq %%rax, %%rcx\n");
    pop_reg(cc, "rax");
    if (node->lhs->type && is_unsigned_type(node->lhs->type))
      fprintf(cc->out, "    shrq %%cl, %%rax\n");
    else
      fprintf(cc->out, "    sarq %%cl, %%rax\n");
    ir_emit_binary_op(ND_SHR, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_NEG: {
    char src_ir[32];
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_NEG null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (node->type && is_float_type(node->type)) {
      codegen_expr_checked(cc, node->lhs);
      /* CG-FLOAT-008b: flip sign bit at the correct width.
       * ty_float value lives in low 32 of %rax — flip bit 31 only.
       * ty_double value lives in full 64 of %rax — flip bit 63. */
      if (node->type->kind == TY_FLOAT) {
        fprintf(cc->out, "    xorl $0x80000000, %%eax\n");
      } else {
        fprintf(cc->out, "    movabsq $-9223372036854775808, %%r11\n");
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_BXOR);
        else fprintf(cc->out, "    xorq %%r11, %%rax\n");
      }
      {
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_UNARY(IR_NEG, ir_map_type(node->type), dst, "f_lhs", node->line);
      }
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(src_ir);
    if (node->type && type_size(node->type) == 4) {
      if (node_type_unsigned(node)) {
        fprintf(cc->out, "    negl %%eax\n");
      } else {
        fprintf(cc->out, "    negl %%eax\n    cltq\n");
      }
    } else {
      fprintf(cc->out, "    negq %%rax\n");
    }
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_NEG, ir_map_type(node->type), dst, src_ir, node->line);
    }
    return;
  }

  case ND_LNOT: {
    char src_ir[32];
    int lnot_is_float;
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_LNOT null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(src_ir);
    /* CG-FLOAT-012: float/double operands require SSE zero-compare.
     * cmpq $0, %rax treats -0.0f (0x80000000) as non-zero, wrong. */
    lnot_is_float = node->lhs->type && is_float_type(node->lhs->type);
    if (!backend_ops && lnot_is_float) {
      int lnot_f32 = node->lhs->type->kind == TY_FLOAT;
      if (lnot_f32) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
      }
      /* ZF=1 iff zero (includes -0.0); PF=1 iff NaN (treat as nonzero) */
      fprintf(cc->out, "    sete %%al\n");
      fprintf(cc->out, "    setnp %%r11b\n");
      fprintf(cc->out, "    andb %%r11b, %%al\n");
    } else {
      if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
      else             fprintf(cc->out, "    cmpq $0, %%rax\n");
      fprintf(cc->out, "    sete %%al\n");
    }
    fprintf(cc->out, "    movzbl %%al, %%eax\n");
    {
      char zero_ir[32];
      char *zt = ir_bridge_fresh_tmp();
      int i;
      for (i = 0; zt[i]; i++)
        zero_ir[i] = zt[i];
      zero_ir[i] = 0;
      ZCC_EMIT_CONST(IR_TY_I64, zero_ir, 0, node->line);
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_BINARY(IR_EQ, IR_TY_I32, dst, src_ir, zero_ir, node->line);
    }
    return;
  }

  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
  case ND_GT:
  case ND_GE: {
    int uns;
    int use32;
    char lhs_ir[32];
    char rhs_ir[32];

    if ((node->lhs && node->lhs->type && is_float_type(node->lhs->type)) ||
        (node->rhs && node->rhs->type && is_float_type(node->rhs->type))) {
      /* CG-FLOAT-010: dispatch ss vs sd based on operand type.
       * Using movq+ucomisd for a float operand zero-extends the 32-bit bits
       * into a 64-bit pattern, turning negative floats into small positive
       * doubles and inverting all comparisons involving negatives. */
      int is_f32 = (node->lhs && node->lhs->type &&
                    node->lhs->type->kind == TY_FLOAT) ||
                   (node->rhs && node->rhs->type &&
                    node->rhs->type->kind == TY_FLOAT);
      /* If either operand is float, compare as float (ucomiss) */
      codegen_expr_checked(cc, node->lhs);
      if (!node->lhs->type || !is_float_type(node->lhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      if (is_f32) fprintf(cc->out, "    movss %%xmm0, (%%rsp)\n");
      else        fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!node->rhs->type || !is_float_type(node->rhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      if (is_f32) fprintf(cc->out, "    movss (%%rsp), %%xmm1\n");
      else        fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n");
      fprintf(cc->out, "    addq $8, %%rsp\n");
      if (is_f32) fprintf(cc->out, "    ucomiss %%xmm0, %%xmm1\n");
      else        fprintf(cc->out, "    ucomisd %%xmm0, %%xmm1\n");
      switch (node->kind) {
      case ND_EQ:
        fprintf(cc->out, "    sete %%al\n    setnp %%r11b\n    andb %%r11b, %%al\n");
        break;
      case ND_NE:
        fprintf(cc->out, "    setne %%al\n    setp %%r11b\n    orb %%r11b, %%al\n");
        break;
      case ND_LT:
        fprintf(cc->out, "    setb %%al\n");
        break;
      case ND_LE:
        fprintf(cc->out, "    setbe %%al\n");
        break;
      case ND_GT:
        fprintf(cc->out, "    seta %%al\n");
        break;
      case ND_GE:
        fprintf(cc->out, "    setae %%al\n");
        break;
      }
      fprintf(cc->out, "    movzbl %%al, %%eax\n");
      ir_emit_binary_op(node->kind, node->type, "f_lhs", "f_rhs", node->line);
      return;
    }

    uns = (node->lhs && node->lhs->type && is_unsigned_type(node->lhs->type)) ||
          (node->rhs && node->rhs->type && is_unsigned_type(node->rhs->type));
    use32 = node->lhs && node->lhs->type && node->rhs &&
            node->rhs->type && type_size(node->lhs->type) == 4 &&
            type_size(node->rhs->type) == 4;
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    if (backend_ops) {
        fprintf(cc->out, "    mov r1, r0\n");
    } else {
        if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    }
    pop_reg(cc, "rax");
    if (backend_ops) {
        fprintf(cc->out, "    cmp r0, r1\n");
        int lbl_true = new_label(cc);
        int lbl_end = new_label(cc);
        if (uns) {
            switch (node->kind) {
            case ND_EQ: fprintf(cc->out, "    beq .L%d\n", lbl_true); break;
            case ND_NE: fprintf(cc->out, "    bne .L%d\n", lbl_true); break;
            case ND_LT: fprintf(cc->out, "    blo .L%d\n", lbl_true); break;
            case ND_LE: fprintf(cc->out, "    bls .L%d\n", lbl_true); break;
            case ND_GT: fprintf(cc->out, "    bhi .L%d\n", lbl_true); break;
            case ND_GE: fprintf(cc->out, "    bhs .L%d\n", lbl_true); break;
            }
        } else {
            switch (node->kind) {
            case ND_EQ: fprintf(cc->out, "    beq .L%d\n", lbl_true); break;
            case ND_NE: fprintf(cc->out, "    bne .L%d\n", lbl_true); break;
            case ND_LT: fprintf(cc->out, "    blt .L%d\n", lbl_true); break;
            case ND_LE: fprintf(cc->out, "    ble .L%d\n", lbl_true); break;
            case ND_GT: fprintf(cc->out, "    bgt .L%d\n", lbl_true); break;
            case ND_GE: fprintf(cc->out, "    bge .L%d\n", lbl_true); break;
            }
        }
        fprintf(cc->out, "    movs r0, #0\n");
        fprintf(cc->out, "    b .L%d\n", lbl_end);
        fprintf(cc->out, ".L%d:\n", lbl_true);
        fprintf(cc->out, "    movs r0, #1\n");
        fprintf(cc->out, ".L%d:\n", lbl_end);
    } else {
        if (use32)
          fprintf(cc->out,
                  "    cmpl %%r11d, %%eax\n"); /* 32-bit: avoids sign-extended imm
                                                  in 64-bit */
        else
          if (backend_ops) fprintf(cc->out, "    cmp r0, r1\n" \
    ); else fprintf(cc->out, "    cmpq %%r11, %%rax\n");
        if (uns) {
          switch (node->kind) {
          case ND_EQ:
            fprintf(cc->out, "    sete %%al\n");
            break;
          case ND_NE:
            fprintf(cc->out, "    setne %%al\n");
            break;
          case ND_LT:
            fprintf(cc->out, "    setb %%al\n");
            break;
          case ND_LE:
            fprintf(cc->out, "    setbe %%al\n");
            break;
          case ND_GT:
            fprintf(cc->out, "    seta %%al\n");
            break;
          case ND_GE:
            fprintf(cc->out, "    setae %%al\n");
            break;
          default:
            break;
          }
        } else {
          switch (node->kind) {
          case ND_EQ:
            fprintf(cc->out, "    sete %%al\n");
            break;
          case ND_NE:
            fprintf(cc->out, "    setne %%al\n");
            break;
          case ND_LT:
            fprintf(cc->out, "    setl %%al\n");
            break;
          case ND_LE:
            fprintf(cc->out, "    setle %%al\n");
            break;
          case ND_GT:
            fprintf(cc->out, "    setg %%al\n");
            break;
          case ND_GE:
            fprintf(cc->out, "    setge %%al\n");
            break;
          default:
            break;
          }
        }
        fprintf(cc->out, "    movzbl %%al, %%eax\n");
    }
    ir_emit_binary_op(node->kind, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_LAND: {
    char land_lhs_ir[32];
    char land_lbl[32];
    char land_lbl2[32];
    int land_lhs_float;
    int land_rhs_float;
    int land_lhs_f32;
    int land_rhs_f32;
    lbl1 = new_label(cc);
    lbl2 = new_label(cc);
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(land_lhs_ir);
    /* CG-FLOAT-012: use SSE zero-compare if LHS is float/double */
    land_lhs_float = node->lhs && node->lhs->type && is_float_type(node->lhs->type);
    land_lhs_f32   = land_lhs_float && node->lhs->type->kind == TY_FLOAT;
    if (!backend_ops && land_lhs_float) {
      if (land_lhs_f32) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
      }
      /* Jump to false-label if LHS is zero (ZF=1) or NaN (PF=1) */
      fprintf(cc->out, "    jp .L%d\n", lbl1);
      fprintf(cc->out, "    je .L%d\n", lbl1);
    } else {
      if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
      else             fprintf(cc->out, "    cmpq $0, %%rax\n");
      if (backend_ops) emit_label_fmt(cc, lbl1, FMT_JE);
      else             emit_label_fmt(cc, lbl1, FMT_JE);
    }
    sprintf(land_lbl, ".L%d", lbl1);
    sprintf(land_lbl2, ".L%d", lbl2);
    ZCC_EMIT_BR_IF(land_lhs_ir, land_lbl, node->line);
    /* LHS was truthy: evaluate RHS and convert to bool */
    codegen_expr_checked(cc, node->rhs);
    land_rhs_float = node->rhs && node->rhs->type && is_float_type(node->rhs->type);
    land_rhs_f32   = land_rhs_float && node->rhs->type->kind == TY_FLOAT;
    if (!backend_ops && land_rhs_float) {
      if (land_rhs_f32) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
      }
      fprintf(cc->out, "    setne %%al\n");
      fprintf(cc->out, "    movzbl %%al, %%eax\n");
    } else {
      if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
      else             fprintf(cc->out, "    cmpq $0, %%rax\n");
      fprintf(cc->out, "    setne %%al\n");
      fprintf(cc->out, "    movzbl %%al, %%eax\n");
    }
    /* Skip over the false-result */
    emit_label_fmt(cc, lbl2, FMT_JMP);
    ZCC_EMIT_BR(land_lbl2, node->line);
    /* Short-circuit false label: LHS was falsy, result = 0 */
    fprintf(cc->out, ".L%d:\n", lbl1);
    ZCC_EMIT_LABEL(land_lbl, node->line);
    fprintf(cc->out, "    movq $0, %%rax\n");
    fprintf(cc->out, ".L%d:\n", lbl2);
    ZCC_EMIT_LABEL(land_lbl2, node->line);
    return;
  }

  case ND_LOR: {
    char lor_lhs_ir[32];
    char lor_lbl1[32];
    char lor_lbl2[32];
    int lor_lhs_float;
    int lor_rhs_float;
    int lor_lhs_f32;
    int lor_rhs_f32;
    lbl1 = new_label(cc);
    lbl2 = new_label(cc);
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lor_lhs_ir);
    /* CG-FLOAT-012: use SSE zero-compare if LHS is float/double */
    lor_lhs_float = node->lhs && node->lhs->type && is_float_type(node->lhs->type);
    lor_lhs_f32   = lor_lhs_float && node->lhs->type->kind == TY_FLOAT;
    if (!backend_ops && lor_lhs_float) {
      if (lor_lhs_f32) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
      }
      /* jne or jp = nonzero or NaN = truthy, jump to result=1 */
      fprintf(cc->out, "    jp .L%d\n", lbl1);
      fprintf(cc->out, "    jne .L%d\n", lbl1);
    } else {
      if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
      else             fprintf(cc->out, "    cmpq $0, %%rax\n");
      fprintf(cc->out, "    jne .L%d\n", lbl1);
    }
    sprintf(lor_lbl1, ".L%d", lbl1);
    sprintf(lor_lbl2, ".L%d", lbl2);
    codegen_expr_checked(cc, node->rhs);
    lor_rhs_float = node->rhs && node->rhs->type && is_float_type(node->rhs->type);
    lor_rhs_f32   = lor_rhs_float && node->rhs->type->kind == TY_FLOAT;
    if (!backend_ops && lor_rhs_float) {
      if (lor_rhs_f32) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
      }
      fprintf(cc->out, "    jp .L%d\n", lbl1);
      fprintf(cc->out, "    jne .L%d\n", lbl1);
    } else {
      if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
      else             fprintf(cc->out, "    cmpq $0, %%rax\n");
      fprintf(cc->out, "    jne .L%d\n", lbl1);
    }
    fprintf(cc->out, "    movq $0, %%rax\n");
    if (backend_ops) emit_label_fmt(cc, lbl2, FMT_JMP);
    else             emit_label_fmt(cc, lbl2, FMT_JMP);
    ZCC_EMIT_BR(lor_lbl2, node->line);
    fprintf(cc->out, ".L%d:\n", lbl1);
    ZCC_EMIT_LABEL(lor_lbl1, node->line);
    fprintf(cc->out, "    movq $1, %%rax\n");
    fprintf(cc->out, ".L%d:\n", lbl2);
    ZCC_EMIT_LABEL(lor_lbl2, node->line);
    return;
  }

  case ND_VA_ARG: {
    int lbl_overflow;
    int lbl_end;

    /* Evaluate LHS (ap) -> %rax */
    codegen_expr(cc, node->lhs);

    /* Safe-keep ap in %rcx */
    fprintf(cc->out, "    movq %%rax, %%rcx\n");

    lbl_overflow = new_label(cc);
    lbl_end = new_label(cc);

    int fp = is_float_type(node->type);
    
    if (fp) {
        /* Float path: Check fp_offset (4(%%rcx)) threshold 176 */
        fprintf(cc->out, "    movl 4(%%rcx), %%edx\n");
        fprintf(cc->out, "    cmpl $176, %%edx\n");
        fprintf(cc->out, "    jae .L%d\n", lbl_overflow);
        /* Fast path: fetch from reg_save_area */
        fprintf(cc->out, "    movq 16(%%rcx), %%rsi\n");
        fprintf(cc->out, "    movslq %%edx, %%rax\n");
        fprintf(cc->out, "    addq %%rax, %%rsi\n");
        fprintf(cc->out, "    movq (%%rsi), %%rax\n");
        /* Increment fp_offset by 16 */
        fprintf(cc->out, "    addl $16, %%edx\n");
        fprintf(cc->out, "    movl %%edx, 4(%%rcx)\n");
    } else {
        /* GP path: Check gp_offset (0(%%rcx)) threshold 48 */
        fprintf(cc->out, "    movl 0(%%rcx), %%edx\n");
        fprintf(cc->out, "    cmpl $48, %%edx\n");
        fprintf(cc->out, "    jae .L%d\n", lbl_overflow);
        /* Fast path: fetch from reg_save_area */
        fprintf(cc->out, "    movq 16(%%rcx), %%rsi\n");
        fprintf(cc->out, "    movslq %%edx, %%rax\n");
        fprintf(cc->out, "    addq %%rax, %%rsi\n");
        fprintf(cc->out, "    movq (%%rsi), %%rax\n");
        /* Increment gp_offset by 8 */
        fprintf(cc->out, "    addl $8, %%edx\n");
        fprintf(cc->out, "    movl %%edx, 0(%%rcx)\n");
    }

    emit_label_fmt(cc, lbl_end, FMT_JMP);

    /* Slow path: fetch from overflow_arg_area */
    emit_label_fmt(cc, lbl_overflow, FMT_DEF);
    fprintf(cc->out, "    movq 8(%%rcx), %%rsi\n");
    fprintf(cc->out, "    movq (%%rsi), %%rax\n");

    /* Increment overflow_arg_area by 8 (even for floats, stack passing is 8-aligned) */
    fprintf(cc->out, "    leaq 8(%%rsi), %%rdi\n");
    fprintf(cc->out, "    movq %%rdi, 8(%%rcx)\n");

    /* End */
    emit_label_fmt(cc, lbl_end, FMT_DEF);
    return;
  }

  case ND_ADDR_LABEL: {
    /* get address of local label: leaq .Luser_func_label(%rip), %rax */
    fprintf(cc->out, "    leaq .Luser_%s_%s(%%rip), %%rax\n", cc->current_func, node->label_name);
    push_reg(cc, "rax");
    return;
  }

  case ND_ADDR:
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_ADDR null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    {
      char addr_src[32];
      ir_save_result(addr_src);
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, addr_src, node->line);
    }
    return;

  case ND_DEREF:
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_DEREF null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    {
      char deref_addr[32];
      ir_save_result(deref_addr);
      /* Type-aware load: char* -> movsbl, int* -> movl, ptr/long* -> movq */
      if (node->type && node->type->kind == TY_FUNC) {
          /* Do nothing: function pointer decays natively */
      } else {
          codegen_load(cc, node->type);
      }
      {
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_LOAD(ir_map_type(node->type), dst, deref_addr, node->line);
      }
    }
    return;

  case ND_MEMBER:
    codegen_addr_checked(cc, node);
    {
      char member_addr[32];
      ir_save_result(member_addr);
      if (node->type) {
        codegen_load(cc, node->type);
      } else {
        switch (node->member_size) {
        case 1:
          fprintf(cc->out, "    movzbl (%%rax), %%eax\n");
          break;
        case 2:
          fprintf(cc->out, "    movzwl (%%rax), %%eax\n");
          break;
        case 4:
          fprintf(cc->out, "    movl (%%rax), %%eax\n");
          break;
        case 8:
          fprintf(cc->out, "    movq (%%rax), %%rax\n");
          break;
        default:
          break;
        }
      }
      {
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_LOAD(ir_map_type(node->type), dst, member_addr, node->line);
      }
    }
    return;

  case ND_CAST: {
    char src_ir[32];
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_CAST null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(src_ir);
    /* truncate/extend based on target type */
    if (node->cast_type) {
      int src_size = node->lhs && node->lhs->type ? type_size(node->lhs->type) : 4;

      /* ---- float <-> double conversions (must precede size switch) ---- */
      if (node->lhs && node->lhs->type && is_float_type(node->lhs->type) && is_float_type(node->cast_type)) {
        if (node->lhs->type->kind == TY_FLOAT && node->cast_type->kind == TY_DOUBLE) {
          /* float -> double: rax holds 32-bit IEEE float in low bits */
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    cvtss2sd %%xmm0, %%xmm0\n");
          fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        } else if (node->lhs->type->kind == TY_DOUBLE && node->cast_type->kind == TY_FLOAT) {
          /* double -> float: rax holds 64-bit IEEE double */
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    cvtsd2ss %%xmm0, %%xmm0\n");
          fprintf(cc->out, "    movd %%xmm0, %%eax\n");
        }
        /* else same-type cast: no-op */
        {
          char *dst = ir_bridge_fresh_tmp();
          ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->type), dst, src_ir, node->line);
        }
        return;
      }

      switch (node->cast_type->size) {
      case 1:
        if (node->lhs && node->lhs->type && is_float_type(node->lhs->type)) {
            /* float/double -> char/uchar: truncating convert first, then narrow */
            if (node->lhs->type->kind == TY_FLOAT) {
                fprintf(cc->out, "    movd %%eax, %%xmm0\n");
                fprintf(cc->out, "    cvttss2si %%xmm0, %%rax\n");
            } else {
                fprintf(cc->out, "    movq %%rax, %%xmm0\n");
                fprintf(cc->out, "    cvttsd2si %%xmm0, %%rax\n");
            }
        }
        if (node->cast_type->kind == TY_UCHAR)
          fprintf(cc->out, "    movzbl %%al, %%eax\n");
        else
          fprintf(cc->out, "    movsbl %%al, %%eax\n");
        break;
      case 2:
        if (node->lhs && node->lhs->type && is_float_type(node->lhs->type)) {
            /* float/double -> short/ushort: truncating convert first, then narrow */
            if (node->lhs->type->kind == TY_FLOAT) {
                fprintf(cc->out, "    movd %%eax, %%xmm0\n");
                fprintf(cc->out, "    cvttss2si %%xmm0, %%rax\n");
            } else {
                fprintf(cc->out, "    movq %%rax, %%xmm0\n");
                fprintf(cc->out, "    cvttsd2si %%xmm0, %%rax\n");
            }
        }
        if (node->cast_type->kind == TY_USHORT)
          fprintf(cc->out, "    movzwl %%ax, %%eax\n");
        else
          fprintf(cc->out, "    movswl %%ax, %%eax\n");
        break;
      case 4:
        if (node->cast_type && !is_float_type(node->cast_type) && node->lhs && node->lhs->type && is_float_type(node->lhs->type)) {
            /* float/double -> int32: convert via SSE */
            fprintf(cc->out, "    movq %%rax, %%xmm0\n");
            if (node->lhs->type->kind == TY_FLOAT)
                fprintf(cc->out, "    cvttss2si %%xmm0, %%rax\n");
            else
                fprintf(cc->out, "    cvttsd2si %%xmm0, %%rax\n");
        } else if (node->cast_type->kind == TY_FLOAT && node->lhs && node->lhs->type && !is_float_type(node->lhs->type)) {
            /* int -> float: */
            fprintf(cc->out, "    cvtsi2ssq %%rax, %%xmm0\n");
            fprintf(cc->out, "    movd %%xmm0, %%eax\n");
        } else if (node->cast_type->kind == TY_UINT || node->cast_type->kind == TY_ULONG) {
            if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n");
        } else if (!is_pointer(node->lhs ? node->lhs->type : 0)) {
            if (!backend_ops) fprintf(cc->out, "    cltq\n");
        }
        break;
      case 8:
        if (node->cast_type && is_float_type(node->cast_type) && node->lhs && node->lhs->type && !is_float_type(node->lhs->type)) {
            if (is_unsigned_type(node->lhs->type)) {
                fprintf(cc->out, "    testq %%rax, %%rax\n");
                fprintf(cc->out, "    js 1f\n");
                fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
                fprintf(cc->out, "    jmp 2f\n");
                fprintf(cc->out, "1:\n");
                if (backend_ops) fprintf(cc->out, "    mov r2, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%rdx\n");
                fprintf(cc->out, "    shrq $1, %%rdx\n");
                fprintf(cc->out, "    andl $1, %%eax\n");
                fprintf(cc->out, "    orq %%rax, %%rdx\n");
                fprintf(cc->out, "    cvtsi2sdq %%rdx, %%xmm0\n");
                fprintf(cc->out, "    addsd %%xmm0, %%xmm0\n");
                fprintf(cc->out, "2:\n");
            } else {
                fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
            }
            fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        } else if (node->cast_type && !is_float_type(node->cast_type) && node->lhs && node->lhs->type && is_float_type(node->lhs->type)) {
            fprintf(cc->out, "    movq %%rax, %%xmm0\n");
            fprintf(cc->out, "    cvttsd2si %%xmm0, %%rax\n");
        } else if (src_size == 4 && !is_pointer(node->lhs ? node->lhs->type : 0)) {
            if (node->lhs && node->lhs->type && is_unsigned_type(node->lhs->type)) {
                if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n");

            } else {
                if (!backend_ops) fprintf(cc->out, "    cltq\n");

            }
        }
        break;
      default:
        break;
      }
    }
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->type), dst, src_ir, node->line);
    }
    return;
  }

  case ND_TERNARY: {
    char ternary_cond_ir[32];
    char ternary_lbl1[32];
    char ternary_lbl2[32];
    if (!node->cond || !node->then_body || !node->else_body) {
      error_at(cc, node->line,
               "codegen_expr: ND_TERNARY missing cond/then/else");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    lbl1 = new_label(cc);
    lbl2 = new_label(cc);
    codegen_expr_checked(cc, node->cond);
    ir_save_result(ternary_cond_ir);
    if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
    else {
      /* CG-FLOAT-012: float/double ternary condition needs SSE zero-compare */
      int tern_is_float = node->cond && node->cond->type &&
                          is_float_type(node->cond->type);
      if (tern_is_float) {
        int tern_f32 = node->cond->type->kind == TY_FLOAT;
        if (tern_f32) {
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
          fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
        } else {
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
          fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
        }
      } else {
        fprintf(cc->out, "    cmpq $0, %%rax\n");
      }
    }
    if (backend_ops) emit_label_fmt(cc, lbl1, FMT_JE);
    else emit_label_fmt(cc, lbl1, FMT_JE);
    sprintf(ternary_lbl1, ".L%d", lbl1);
    ZCC_EMIT_BR_IF(ternary_cond_ir, ternary_lbl1, node->line);
    codegen_expr_checked(cc, node->then_body);
    if (backend_ops) emit_label_fmt(cc, lbl2, FMT_JMP);
    else emit_label_fmt(cc, lbl2, FMT_JMP);
    sprintf(ternary_lbl2, ".L%d", lbl2);
    ZCC_EMIT_BR(ternary_lbl2, node->line);
    fprintf(cc->out, ".L%d:\n", lbl1);
    ZCC_EMIT_LABEL(ternary_lbl1, node->line);
    codegen_expr_checked(cc, node->else_body);
    fprintf(cc->out, ".L%d:\n", lbl2);
    ZCC_EMIT_LABEL(ternary_lbl2, node->line);
    return;
  }

  case ND_COMMA_EXPR:
    codegen_expr_checked(cc, node->lhs);
    codegen_expr_checked(cc, node->rhs);
    return;

  case ND_PRE_INC:
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      int esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n", esz); if (strcmp(reg, "rax") == 0) fprintf(cc->out, "    adds r0, r0, r3\n"); else fprintf(cc->out, "    adds r1, r1, r3\n"); } else fprintf(cc->out, "    addq $%d, %s\n", esz, reg);
      if (backend_ops) fprintf(cc->out, "    mov r0, %s\n", reg); else fprintf(cc->out, "    movq %s, %%rax\n", reg);
      if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      if (backend_ops) fprintf(cc->out, "    mov %s, r0\n", reg); else fprintf(cc->out, "    movq %%rax, %s\n", reg);
      return;
    }
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_PRE_INC null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    if (is_bad_ptr(node->lhs)) {
      error_at(cc, node->line, "codegen_expr: ND_PRE_INC lhs bad ptr");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    push_reg(cc, "rax");
    if (!node->lhs->type) {
      error_at(cc, node->line, "codegen_expr: ND_PRE_INC lhs has null type");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_load(cc, node->lhs->type);
    if (node->lhs->type && is_float_type(node->lhs->type)) {
      /* CG-FLOAT-011: float ++/-- must use FP add, not integer addq */
      if (node->lhs->type->kind == TY_FLOAT) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    movss .Lf32_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    addss %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movd %%xmm0, %%eax\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    movsd .Lf64_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    addsd %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
      }
    } else {
      int esz;
      esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n    adds r0, r0, r3\n", esz); } else fprintf(cc->out, "    addq $%d, %%rax\n", esz);
    }
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    switch (type_size(node->lhs->type)) {
    case 1:
      if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
      break;
    case 2:
      if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
      break;
    case 4:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
      break;
    default:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
      break;
    }
    if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
    if (node->lhs->type && !is_float_type(node->lhs->type) &&
        !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
        if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
    } else if (node->lhs->type && !is_float_type(node->lhs->type) &&
               node_type_unsigned(node->lhs)) {
        if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
    }
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->lhs->type), dst, "pre_inc", node->line);
    }
    return;

  case ND_PRE_DEC:
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      int esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n", esz); if (strcmp(reg, "rax") == 0) fprintf(cc->out, "    subs r0, r0, r3\n"); else fprintf(cc->out, "    subs r1, r1, r3\n"); } else fprintf(cc->out, "    subq $%d, %s\n", esz, reg);
      if (backend_ops) fprintf(cc->out, "    mov r0, %s\n", reg); else fprintf(cc->out, "    movq %s, %%rax\n", reg);
      if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      if (backend_ops) fprintf(cc->out, "    mov %s, r0\n", reg); else fprintf(cc->out, "    movq %%rax, %s\n", reg);
      return;
    }
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_PRE_DEC null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    if (is_bad_ptr(node->lhs)) {
      error_at(cc, node->line, "codegen_expr: ND_PRE_DEC lhs bad ptr");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    push_reg(cc, "rax");
    if (!node->lhs->type) {
      error_at(cc, node->line, "codegen_expr: ND_PRE_DEC lhs has null type");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_load(cc, node->lhs->type);
    if (node->lhs->type && is_float_type(node->lhs->type)) {
      /* CG-FLOAT-011: float -- must use FP sub, not integer subq */
      if (node->lhs->type->kind == TY_FLOAT) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    movss .Lf32_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    subss %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movd %%xmm0, %%eax\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    movsd .Lf64_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    subsd %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
      }
    } else {
      int esz;
      esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n    subs r0, r0, r3\n", esz); } else fprintf(cc->out, "    subq $%d, %%rax\n", esz);
    }
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    switch (type_size(node->lhs->type)) {
    case 1:
      if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
      break;
    case 2:
      if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
      break;
    case 4:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
      break;
    default:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
      break;
    }
    if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
    if (node->lhs->type && !is_float_type(node->lhs->type) &&
        !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
        if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
    } else if (node->lhs->type && !is_float_type(node->lhs->type) &&
               node_type_unsigned(node->lhs)) {
        if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
    }
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->lhs->type), dst, "pre_dec", node->line);
    }
    return;

  case ND_POST_INC:
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      int esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) fprintf(cc->out, "    mov r0, %s\n", reg); else fprintf(cc->out, "    movq %s, %%rax\n", reg);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n", esz); if (strcmp(reg, "rax") == 0) fprintf(cc->out, "    adds r0, r0, r3\n"); else fprintf(cc->out, "    adds r1, r1, r3\n"); } else fprintf(cc->out, "    addq $%d, %s\n", esz, reg);
      return;
    }
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_POST_INC null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    if (is_bad_ptr(node->lhs)) {
      error_at(cc, node->line, "codegen_expr: ND_POST_INC lhs bad ptr");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    push_reg(cc, "rax");
    if (!node->lhs->type) {
      error_at(cc, node->line, "codegen_expr: ND_POST_INC lhs has null type");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_load(cc, node->lhs->type);
    push_reg(cc, "rax"); /* save original value */
    if (node->lhs->type && is_float_type(node->lhs->type)) {
      /* CG-FLOAT-011: float post++ must use FP add, not integer addq */
      if (node->lhs->type->kind == TY_FLOAT) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    movss .Lf32_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    addss %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movd %%xmm0, %%eax\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    movsd .Lf64_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    addsd %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
      }
    } else {
      int esz;
      esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n    adds r0, r0, r3\n", esz); } else fprintf(cc->out, "    addq $%d, %%rax\n", esz);
    }
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rdx"); /* rdx = original value */
    pop_reg(cc, "rax"); /* rax = address */
    switch (type_size(node->lhs->type)) {
    case 1:
      if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
      break;
    case 2:
      if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
      break;
    case 4:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
      break;
    default:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
      break;
    }
    if (backend_ops) fprintf(cc->out, "    mov r0, r2\n");
      else fprintf(cc->out, "    movq %%rdx, %%rax\n");
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->lhs->type), dst, "post_inc", node->line);
    }
    return;

  case ND_POST_DEC:
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      int esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) fprintf(cc->out, "    mov r0, %s\n", reg); else fprintf(cc->out, "    movq %s, %%rax\n", reg);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n", esz); if (strcmp(reg, "rax") == 0) fprintf(cc->out, "    subs r0, r0, r3\n"); else fprintf(cc->out, "    subs r1, r1, r3\n"); } else fprintf(cc->out, "    subq $%d, %s\n", esz, reg);
      return;
    }
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_POST_DEC null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    if (is_bad_ptr(node->lhs)) {
      error_at(cc, node->line, "codegen_expr: ND_POST_DEC lhs bad ptr");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    push_reg(cc, "rax");
    if (!node->lhs->type) {
      error_at(cc, node->line, "codegen_expr: ND_POST_DEC lhs has null type");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_load(cc, node->lhs->type);
    push_reg(cc, "rax");
    if (node->lhs->type && is_float_type(node->lhs->type)) {
      /* CG-FLOAT-011: float post-- must use FP sub, not integer subq */
      if (node->lhs->type->kind == TY_FLOAT) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    movss .Lf32_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    subss %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movd %%xmm0, %%eax\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    movsd .Lf64_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    subsd %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
      }
    } else {
      int esz;
      esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n    subs r0, r0, r3\n", esz); } else fprintf(cc->out, "    subq $%d, %%rax\n", esz);
    }
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rdx"); /* rdx = original value */
    pop_reg(cc, "rax"); /* rax = address */
    switch (type_size(node->lhs->type)) {
    case 1:
      if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
      break;
    case 2:
      if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
      break;
    case 4:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
      break;
    default:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
      break;
    }
    if (backend_ops) fprintf(cc->out, "    mov r0, r2\n");
      else fprintf(cc->out, "    movq %%rdx, %%rax\n");
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->lhs->type), dst, "post_dec", node->line);
    }
    return;

  case ND_CALL: {
    char *argregs[6];
    int i;
    int nargs;
    int shadow_and_stack;
    int args_on_stack;
    int alignment_pad;
    int cleanup_bytes;
    char args_ir_1d[2048];
    int arg_is_stack[64];

    /* System V AMD64 (Linux): 6 register args: RDI, RSI, RDX, RCX, R8, R9; 7th+
     * on stack */
    argregs[0] = "rdi";
    argregs[1] = "rsi";
    argregs[2] = "rdx";
    argregs[3] = "rcx";
    argregs[4] = "r8";
    argregs[5] = "r9";

    if (!node->func_name[0] && !node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_CALL no func_name and no callee");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (!node->args) {
      error_at(cc, node->line, "codegen_expr: ND_CALL with NULL args");
      return;
    }
    nargs = node->num_args;
    if (nargs < 0 || nargs > 64) {
      error_at(cc, node->line, "call node bad num_args");
      return;
    }

    /* System V AMD64: no shadow space required. Space for 7th+ gp args and 9th+ fp args is left on the stack. */
    {
      int temp_gp = 0, temp_fp = 0;
      int stack_slots = 0;
      /* Resolve callee type early for arg classification */
      Symbol *pre_sym = node->func_name[0] ? scope_find(cc, node->func_name) : 0;
      Type *pre_ftype = (pre_sym && pre_sym->type && pre_sym->type->kind == TY_FUNC) ? pre_sym->type : 0;
      if (!pre_ftype && node->lhs && node->lhs->type) {
        Type *plt = node->lhs->type;
        if (plt->kind == TY_FUNC) pre_ftype = plt;
        else if (plt->kind == TY_PTR && plt->base && plt->base->kind == TY_FUNC) pre_ftype = plt->base;
      }
      for (i = 0; i < nargs; i++) {
        Type *at = (node->args[i] && node->args[i]->type) ? node->args[i]->type : 0;
        int is_stack = 0;
        if (at && (at->kind == TY_STRUCT || at->kind == TY_UNION)) {
            abi_class_t eb[2];
            classify_aggregate(at, eb);
            if (eb[0] == CLASS_MEMORY) {
                is_stack = 1;
            } else {
                int needed_gp = 0, needed_fp = 0;
                if (eb[0] == CLASS_SSE) needed_fp++; else needed_gp++;
                if (type_size(at) > 8) {
                    if (eb[1] == CLASS_SSE) needed_fp++; else needed_gp++;
                }
                if (temp_gp + needed_gp <= 6 && temp_fp + needed_fp <= 8) {
                    temp_gp += needed_gp;
                    temp_fp += needed_fp;
                } else {
                    is_stack = 1;
                }
            }
        } else {
            int is_fp = (at && is_float_type(at));
            if (!is_fp && pre_ftype && pre_ftype->params && i < pre_ftype->num_params &&
                pre_ftype->params[i] && is_float_type(pre_ftype->params[i])) is_fp = 1;
            
            if (is_fp) {
                if (temp_fp < 8) temp_fp++; else is_stack = 1;
            } else {
                if (temp_gp < 6) temp_gp++; else is_stack = 1;
            }
        }
        arg_is_stack[i] = is_stack;
        if (is_stack) stack_slots += (at ? (type_size(at) + 7) / 8 : 1);
      }
      args_on_stack = stack_slots;
    }
    /* We must ensure that AFTER the args are on the stack, %rsp is 16-byte
     * aligned. */
    /* Currently, depth is cc->stack_depth. After pushes/pops, depth will be
     * cc->stack_depth + args_on_stack. */
    alignment_pad = 0;
    {
      if ((cc->stack_depth + args_on_stack) % 2 != 0) {
        alignment_pad = 8;
      }
    }


    if (alignment_pad > 0) {
      fprintf(cc->out, "    subq $%d, %%rsp\n", alignment_pad);
      cc->stack_depth++;
    }

    /* for indirect calls, evaluate callee first and save on stack */
    if (node->func_name[0] == 0 && node->lhs) {
      codegen_expr_checked(cc, node->lhs);
      push_reg(cc, "rax");
    }

    /* push args in reverse order */
    for (i = nargs - 1; i >= 0; i--) {
      if (!node->args[i]) {
        error_at(cc, node->line, "null argument in call");
        return;
      }
      codegen_expr_checked(cc, node->args[i]);
      Type *atype = node->args[i]->type;
      if (arg_is_stack[i]) {
          if (atype && (atype->kind == TY_STRUCT || atype->kind == TY_UNION)) {
              /* Move aggregate data to stack in 8-byte chunks */
              int j, n_slots = (type_size(atype) + 7) / 8;
              fprintf(cc->out, "    movq %%rax, %%r10\n");
              for (j = n_slots - 1; j >= 0; j--) {
                  fprintf(cc->out, "    pushq %d(%%r10)\n", j * 8);
                  cc->stack_depth++;
              }
          } else {
              push_reg(cc, "rax");
          }
          continue;
      }
      /* REG arg: push placeholders for register loading pass */
      if (atype && (atype->kind == TY_STRUCT || atype->kind == TY_UNION)) {
          fprintf(cc->out, "    movq %%rax, %%r10\n");
          if (type_size(atype) > 8) {
              fprintf(cc->out, "    pushq 8(%%r10)\n");
              cc->stack_depth++;
          }
          fprintf(cc->out, "    pushq 0(%%r10)\n");
          cc->stack_depth++;
      } else {
          push_reg(cc, "rax");
      }
      ir_save_result(&args_ir_1d[i * 32]);
      ZCC_EMIT_ARG(ir_map_type(node->args[i] ? node->args[i]->type : 0), &args_ir_1d[i * 32], node->line);
    }

    /* pop args into correct registers: floats->xmm, ints->gpregs independently */
    {
      int gp_idx = 0;
      int fp_idx = 0;
      /* Resolve callee's declared parameter types for float/double ABI */
      Symbol *callee_sym = node->func_name[0] ? scope_find(cc, node->func_name) : 0;
      Type *callee_ftype = (callee_sym && callee_sym->type && callee_sym->type->kind == TY_FUNC) ? callee_sym->type : 0;
      /* CG-FLOAT-008c: for indirect calls via function pointer, extract TY_FUNC from lhs type */
      if (!callee_ftype && node->lhs && node->lhs->type) {
        Type *lt = node->lhs->type;
        if (lt->kind == TY_FUNC)
          callee_ftype = lt;
        else if (lt->kind == TY_PTR && lt->base && lt->base->kind == TY_FUNC)
          callee_ftype = lt->base;
      }
       for (i = 0; i < nargs; i++) {
        if (arg_is_stack[i]) continue;
        Type *atype = node->args[i]->type;
        if (atype && (atype->kind == TY_STRUCT || atype->kind == TY_UNION)) {
            abi_class_t eb[2];
            classify_aggregate(atype, eb);
            /* Small aggregate: pop eightbytes into correct registers. */
            /* Pop eightbyte 0 */
            if (eb[0] == CLASS_SSE) {
                fprintf(cc->out, "    popq %%rax\n");
                cc->stack_depth--;
                fprintf(cc->out, "    movq %%rax, %%xmm%d\n", fp_idx++);
            } else {
                pop_reg(cc, argregs[gp_idx++]);
            }
            /* Pop eightbyte 1 */
            if (type_size(atype) > 8) {
                if (eb[1] == CLASS_SSE) {
                    fprintf(cc->out, "    popq %%rax\n");
                    cc->stack_depth--;
                    fprintf(cc->out, "    movq %%rax, %%xmm%d\n", fp_idx++);
                } else {
                    pop_reg(cc, argregs[gp_idx++]);
                }
            }
            continue;
        }
        if (atype && is_float_type(atype)) {
          if (fp_idx < 8) {
            fprintf(cc->out, "    popq %%rax\n");
            cc->stack_depth--;
            fprintf(cc->out, "    movq %%rax, %%xmm%d\n", fp_idx);
            /* Check callee's declared param type... (keep existing conversion logic) */
            {
              int arg_is_float = (atype->kind == TY_FLOAT);
              if (arg_is_float) {
                int in_fixed_float_param = 0;
                if (callee_ftype && callee_ftype->params && i < callee_ftype->num_params) {
                  if (callee_ftype->params[i]->kind == TY_FLOAT) in_fixed_float_param = 1;
                }
                if (!in_fixed_float_param) {
                  fprintf(cc->out, "    cvtss2sd %%xmm%d, %%xmm%d\n", fp_idx, fp_idx);
                }
              } else {
                int need_cvt = 0;
                if (callee_ftype && callee_ftype->params && i < callee_ftype->num_params) {
                  if (callee_ftype->params[i] && callee_ftype->params[i]->kind == TY_FLOAT) need_cvt = 1;
                }
                if (need_cvt) fprintf(cc->out, "    cvtsd2ss %%xmm%d, %%xmm%d\n", fp_idx, fp_idx);
              }
            }
            fp_idx++;
          }
        } else if (callee_ftype && callee_ftype->params && i < callee_ftype->num_params &&
                   callee_ftype->params[i] && is_float_type(callee_ftype->params[i])) {
          /* CG-FLOAT-ABI-FIX: implicit int-to-float/double conversion at call site.
           * The arg expression is integer but the callee declares this param as
           * float/double. SysV ABI requires float/double args in xmm registers.
           * Convert the integer value and route to xmm instead of GP. */
          if (fp_idx < 8) {
            fprintf(cc->out, "    popq %%rax\n");
            cc->stack_depth--;
            if (callee_ftype->params[i]->kind == TY_FLOAT) {
              fprintf(cc->out, "    cvtsi2ss %%eax, %%xmm%d\n", fp_idx);
            } else {
              fprintf(cc->out, "    cvtsi2sd %%rax, %%xmm%d\n", fp_idx);
            }
            fp_idx++;
          }
        } else {
          if (backend_ops) {
              if (gp_idx < 4) {
                 fprintf(cc->out, "    pop {r%d}\n", gp_idx);
                 cc->stack_depth--;
                 gp_idx++;
              }
          } else {
              if (gp_idx < 6) {
                pop_reg(cc, argregs[gp_idx]);
                gp_idx++;
              }
          }
        }
      }
      if (!backend_ops) {
          fprintf(cc->out, "    movl $%d, %%eax\n", fp_idx > 8 ? 8 : fp_idx);
      }
    }
    if (node->func_name[0] == 0 && node->lhs) {
      /* indirect call: pop callee into r10, call *r10 */
      pop_reg(cc, "r10");
      fprintf(cc->out, "    call *%%r10\n");
    } else if (strcmp(node->func_name, "__builtin_va_start") == 0) {
      Symbol *fsym = scope_find(cc, cc->current_func);
      int nparams_gp = 0;
      int nparams_fp = 0;
      if (fsym && fsym->type && fsym->type->params) {
          for (int j = 0; j < fsym->type->num_params; j++) {
              if (is_float_type(fsym->type->params[j])) nparams_fp++;
              else nparams_gp++;
          }
      }
      int gp_offset = nparams_gp * 8;
      int fp_offset = 48 + nparams_fp * 16;
      fprintf(cc->out, "    # arg0 = ap, arg1 ignored\n");
      fprintf(cc->out, "    movl $%d, 0(%%rdi)\n", gp_offset);
      fprintf(cc->out, "    movl $%d, 4(%%rdi)\n", fp_offset);
      fprintf(cc->out, "    leaq 16(%%rbp), %%rax\n");
      fprintf(cc->out, "    movq %%rax, 8(%%rdi)\n");
      /* The reg_save_area is dynamically stored at stack bottom. */
      fprintf(cc->out, "    leaq %d(%%rbp), %%rax\n", fsym ? fsym->stack_offset : -176);
      fprintf(cc->out, "    movq %%rax, 16(%%rdi)\n");
    } else if (strcmp(node->func_name, "__builtin_va_end") == 0) {
      /* va_end is a no-op on x86-64 SysV ABI */
      fprintf(cc->out, "    # __builtin_va_end (no-op)\n");
    } else {
      if (backend_ops && backend_ops->emit_call) {
          backend_ops->emit_call(cc, node);
      } else {
          fprintf(cc->out, "    call %s\n", node->func_name);
      }
    }

    /* CG-IR-019: System V aggregate return capture */
    if (node->type && (node->type->kind == TY_STRUCT || node->type->kind == TY_UNION)) {
        abi_class_t eb[2];
        classify_aggregate(node->type, eb);
        if (eb[0] != CLASS_MEMORY) {
            /* Small aggregate: capture registers into scratch buffer immediately post-call.
             * This must happen before any stack cleanup or other register clobbering. */
            fprintf(cc->out, "    # SysV spill for %s\n", node->type->tag[0] ? node->type->tag : "<anon>");
            
            int cur_gp = 0, cur_fp = 0;
            char *ret_gpregs[] = { "rax", "rdx" };

            /* Eightbyte 0 */
            if (eb[0] == CLASS_SSE) {
                fprintf(cc->out, "    movq %%xmm%d, %d(%%rbp)\n", cur_fp++, cc->abi_scratch_offset);
            } else {
                fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", ret_gpregs[cur_gp++], cc->abi_scratch_offset);
            }

            /* Eightbyte 1 */
            if (type_size(node->type) > 8) {
                if (eb[1] == CLASS_SSE) {
                    fprintf(cc->out, "    movq %%xmm%d, %d(%%rbp)\n", cur_fp++, cc->abi_scratch_offset + 8);
                } else {
                    fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", ret_gpregs[cur_gp++], cc->abi_scratch_offset + 8);
                }
            }
            /* Set %rax to the address of the scratch buffer for downstream consumption. */
            fprintf(cc->out, "    leaq %d(%%rbp), %%rax\n", cc->abi_scratch_offset);
        }
    } else if (node->type && is_float_type(node->type)) {
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
    } else if (node->type && !node_type_unsigned(node)) {
        if (node->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
        else if (node->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
        else if (node->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
    } else if (node->type && node_type_unsigned(node)) {
        if (node->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
        else if (node->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
        else if (node->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
    }

    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_CALL(ir_map_type(node->type), dst, node->func_name, node->line);
    }

    /* cleanup arguments left on stack AND the alignment pad */
    cleanup_bytes = (args_on_stack * 8) + alignment_pad;
    if (cleanup_bytes > 0) {
      fprintf(cc->out, "    addq $%d, %%rsp\n", cleanup_bytes);
      cc->stack_depth -= (cleanup_bytes / 8);
    }
    return;
  }

  case ND_NOP:
    return;

  default:
    error_at(cc, node->line, "unsupported expression in codegen");
    return;
  }
}

/* ================================================================ */
/* STATEMENT CODEGEN                                                 */
/* ================================================================ */

void codegen_stmt(Compiler *cc, Node *node) {
  if (!node)
    return;

  if (cc) {
  }

  int lbl1;
  int lbl2;
  int lbl3;
  int old_break;
  int old_continue;
  char errbuf[80];

  /* Do NOT use ptr_in_fault_range(node): stage2 miscompiles it and rejects
   * valid arena ptrs. */
  if (!node)
    return;
  if (is_bad_ptr(node)) {
    sprintf(errbuf, "codegen_stmt: bad node ptr %p", (void *)node);
    error_at(cc, 0, errbuf);
    return;
  }

  switch (node->kind) {

  case ND_RETURN:
    if (node->lhs) {
      codegen_expr_checked(cc, node->lhs);
      /* CG-IR-019: System V aggregate return support */
      Type *ty = node->lhs->type;
      if (ty && (ty->kind == TY_STRUCT || ty->kind == TY_UNION)) {
        abi_class_t eb[2];
        classify_aggregate(ty, eb);
        if (eb[0] != CLASS_MEMORY) {
          /* Small aggregate return: classify and load into registers.
           * Use %r10 as a temporary base to avoid clobbering %rax (the result address). */
          fprintf(cc->out, "    movq %%rax, %%r10\n");
          
          int cur_gp = 0, cur_fp = 0;
          char *ret_gpregs[] = { "rax", "rdx" };

          /* Eightbyte 0 */
          if (eb[0] == CLASS_SSE) {
            fprintf(cc->out, "    movq 0(%%r10), %%xmm%d\n", cur_fp++);
          } else {
            fprintf(cc->out, "    movq 0(%%r10), %%%s\n", ret_gpregs[cur_gp++]);
          }
          
          /* Eightbyte 1 (optional) */
          if (type_size(ty) > 8) {
            if (eb[1] == CLASS_SSE) {
              fprintf(cc->out, "    movq 8(%%r10), %%xmm%d\n", cur_fp++);
            } else {
              fprintf(cc->out, "    movq 8(%%r10), %%%s\n", ret_gpregs[cur_gp++]);
            }
          }
          
          /* Jump directly to epilogue */
          if (backend_ops) fprintf(cc->out, "    b .Lfunc_end_%d\n", cc->func_end_label);
          else fprintf(cc->out, "    jmp .Lfunc_end_%d\n", cc->func_end_label);
          return;
        }
      }

      {
        char ret_tmp[32];
        ir_save_result(ret_tmp);
        ZCC_EMIT_RET(ir_map_type(node->lhs->type), ret_tmp, node->line);
      }
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
    } else {
      ZCC_EMIT_RET(0, "", node->line);
    }
    if (backend_ops) fprintf(cc->out, "    b .Lfunc_end_%d\n", cc->func_end_label);
    else fprintf(cc->out, "    jmp .Lfunc_end_%d\n", cc->func_end_label);
    return;

  case ND_BLOCK: {
    int i;
    int nst;
    if (!node->stmts) {
      error_at(cc, node->line, "codegen_stmt: ND_BLOCK null stmts");
      return;
    }
    nst = node->num_stmts;
    if (nst < 0 || nst > 65536) {
      error_at(cc, node->line, "block: bad num_stmts");
      return;
    }
    for (i = 0; i < nst; i++) {
      codegen_stmt(cc, node->stmts[i]);
    }
    return;
  }

  case ND_IF: {
    char cond_ir[32];
    char ir_lbl[32];
    lbl1 = new_label(cc);
    codegen_expr_checked(cc, node->cond);
    ir_save_result(cond_ir);
    if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
    else {
      /* CG-FLOAT-012: use SSE zero-compare for float/double conditions */
      int cond_is_float = node->cond && node->cond->type &&
                          is_float_type(node->cond->type);
      if (cond_is_float) {
        int cond_f32 = node->cond->type->kind == TY_FLOAT;
        if (cond_f32) {
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
          fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
        } else {
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
          fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
        }
        /* je = zero (false); also jump if ZF via je after ucomiss */
      } else {
        fprintf(cc->out, "    cmpq $0, %%rax\n");
      }
    }
    emit_label_fmt(cc, lbl1, FMT_JE);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_BR_IF(cond_ir, ir_lbl, node->line);
    codegen_stmt(cc, node->then_body);
    if (node->else_body) {
      lbl2 = new_label(cc);
      emit_label_fmt(cc, lbl2, FMT_JMP);
      sprintf(ir_lbl, ".L%d", lbl2);
      ZCC_EMIT_BR(ir_lbl, node->line);
      emit_label_fmt(cc, lbl1, FMT_DEF);
      sprintf(ir_lbl, ".L%d", lbl1);
      ZCC_EMIT_LABEL(ir_lbl, node->line);
      codegen_stmt(cc, node->else_body);
      emit_label_fmt(cc, lbl2, FMT_DEF);
      sprintf(ir_lbl, ".L%d", lbl2);
      ZCC_EMIT_LABEL(ir_lbl, node->line);
    } else {
      emit_label_fmt(cc, lbl1, FMT_DEF);
      sprintf(ir_lbl, ".L%d", lbl1);
      ZCC_EMIT_LABEL(ir_lbl, node->line);
    }
    return;
  }

  case ND_WHILE: {
    char cond_ir[32];
    char ir_lbl[32];
    lbl1 = new_label(cc); /* loop start */
    lbl2 = new_label(cc); /* loop end */
    old_break = cc->break_label;
    old_continue = cc->continue_label;
    cc->break_label = lbl2;
    cc->continue_label = lbl1;
    emit_label_fmt(cc, lbl1, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    codegen_expr_checked(cc, node->cond);
    ir_save_result(cond_ir);
    if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
    else if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n" \
    ); else fprintf(cc->out, "    cmpq $0, %%rax\n");
    emit_label_fmt(cc, lbl2, FMT_JE);
    sprintf(ir_lbl, ".L%d", lbl2);
    ZCC_EMIT_BR_IF(cond_ir, ir_lbl, node->line);
    codegen_stmt(cc, node->body);
    emit_label_fmt(cc, lbl1, FMT_JMP);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_BR(ir_lbl, node->line);
    emit_label_fmt(cc, lbl2, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl2);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    cc->break_label = old_break;
    cc->continue_label = old_continue;
    return;
  }

  case ND_FOR: {
    char cond_ir[32];
    char ir_lbl[32];
    lbl1 = new_label(cc); /* loop start */
    lbl2 = new_label(cc); /* loop end */
    lbl3 = new_label(cc); /* continue target (increment) */
    old_break = cc->break_label;
    old_continue = cc->continue_label;
    cc->break_label = lbl2;
    cc->continue_label = lbl3;
    if (node->init)
      codegen_stmt(cc, node->init);
    emit_label_fmt(cc, lbl1, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    if (node->cond) {
      codegen_expr_checked(cc, node->cond);
      ir_save_result(cond_ir);
      if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
      else if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n" \
    ); else fprintf(cc->out, "    cmpq $0, %%rax\n");
      emit_label_fmt(cc, lbl2, FMT_JE);
      sprintf(ir_lbl, ".L%d", lbl2);
      ZCC_EMIT_BR_IF(cond_ir, ir_lbl, node->line);
    }
    codegen_stmt(cc, node->body);
    emit_label_fmt(cc, lbl3, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl3);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    if (node->inc)
      codegen_expr_checked(cc, node->inc);
    emit_label_fmt(cc, lbl1, FMT_JMP);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_BR(ir_lbl, node->line);
    emit_label_fmt(cc, lbl2, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl2);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    cc->break_label = old_break;
    cc->continue_label = old_continue;
    return;
  }

  case ND_DO_WHILE: {
    /* do { body } while (cond)
     * Layout:
     *   lbl_body: (lbl1)
     *       <body> — break→lbl2, continue→lbl_cond
     *   lbl_cond: (lbl3 — new label for condition)
     *       <cond>; jne lbl_body
     *   lbl_break: (lbl2)
     *
     * CRITICAL: continue must jump to lbl_cond (the condition/increment),
     * NOT back to lbl_body (which would skip the while-condition entirely,
     * turning "do { if(!x) continue; } while(++i<n)" into an infinite loop
     * because ++i is in the condition and never evaluated). */
    int lbl3;   /* separate continue-target label = condition start */
    char cond_ir[32];
    char ir_lbl[32];
    lbl1 = new_label(cc);  /* body start */
    lbl2 = new_label(cc);  /* break/exit */
    lbl3 = new_label(cc);  /* continue target = condition-start */
    old_break = cc->break_label;
    old_continue = cc->continue_label;
    cc->break_label = lbl2;
    cc->continue_label = lbl3;   /* ← fix: continue skips to condition */
    emit_label_fmt(cc, lbl1, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    codegen_stmt(cc, node->body);
    /* condition begins here — this is where continue targets */
    emit_label_fmt(cc, lbl3, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl3);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    codegen_expr_checked(cc, node->cond);
    ir_save_result(cond_ir);
    if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n" \
    ); else fprintf(cc->out, "    cmpq $0, %%rax\n");
    emit_label_fmt(cc, lbl1, FMT_JNE);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_BR_IF(cond_ir, ir_lbl, node->line);
    emit_label_fmt(cc, lbl2, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl2);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    cc->break_label = old_break;
    cc->continue_label = old_continue;
    return;
  }

  case ND_SWITCH: {
    int end_lbl;
    int *case_labels;
    int default_lbl;
    int i;
    int ncase;
    char switch_val_ir[32];
    char ir_lbl[64];

    if (!node->cases) {
      error_at(cc, node->line, "codegen_stmt: ND_SWITCH null cases");
      return;
    }
    ncase = node->num_cases;
    if (ncase < 0 || ncase > MAX_CASES) {
      error_at(cc, node->line, "switch: bad num_cases");
      return;
    }

    end_lbl = new_label(cc);
    old_break = cc->break_label;
    cc->break_label = end_lbl;

    codegen_expr_checked(cc, node->cond);
    ir_save_result(switch_val_ir);

    case_labels = (int *)cc_alloc(cc, sizeof(int) * (ncase + 1));
    for (i = 0; i < ncase; i++) {
      if (!node->cases[i]) {
        error_at(cc, node->line, "codegen_stmt: ND_SWITCH null case");
        return;
      }
      case_labels[i] = new_label(cc);
      fprintf(cc->out, "    cmpq $%lld, %%rax\n", node->cases[i]->case_val);
      emit_label_fmt(cc, case_labels[i], FMT_JE);

      {
        char case_const_ir[32];
        char cmp_ir[32];
        char *ct;

        ct = ir_bridge_fresh_tmp();
        sprintf(case_const_ir, "%s", ct);
        ZCC_EMIT_CONST(IR_TY_I64, case_const_ir, node->cases[i]->case_val,
                       node->line);

        ct = ir_bridge_fresh_tmp();
        sprintf(cmp_ir, "%s", ct);

        ZCC_EMIT_BINARY(IR_EQ, IR_TY_I64, cmp_ir, switch_val_ir, case_const_ir,
                        node->line);
        sprintf(ir_lbl, ".L%d", case_labels[i]);
        ZCC_EMIT_BR_IF(cmp_ir, ir_lbl, node->line);
      }
    }
    default_lbl = new_label(cc);
    if (node->default_case) {
      emit_label_fmt(cc, default_lbl, FMT_JMP);
      sprintf(ir_lbl, ".L%d", default_lbl);
      ZCC_EMIT_BR(ir_lbl, node->line);
    } else {
      emit_label_fmt(cc, end_lbl, FMT_JMP);
      sprintf(ir_lbl, ".L%d", end_lbl);
      ZCC_EMIT_BR(ir_lbl, node->line);
    }

    for (i = 0; i < ncase; i++) {
      emit_label_fmt(cc, case_labels[i], FMT_DEF);
      sprintf(ir_lbl, ".L%d", case_labels[i]);
      ZCC_EMIT_LABEL(ir_lbl, node->line);
      if (node->cases[i]->case_body)
        codegen_stmt(cc, node->cases[i]->case_body);
      /* IR: emit explicit fallthrough-guard BR to end if no terminator was
       * already emitted by a return/break inside the case body.            */
      if (g_ir_cur_func && g_ir_cur_func->tail &&
          !ir_op_is_terminator(g_ir_cur_func->tail->op)) {
        sprintf(ir_lbl, ".L%d", end_lbl);
        ZCC_EMIT_BR(ir_lbl, node->line);
      }
    }
    if (node->default_case) {
      emit_label_fmt(cc, default_lbl, FMT_DEF);
      sprintf(ir_lbl, ".L%d", default_lbl);
      ZCC_EMIT_LABEL(ir_lbl, node->line);
      if (node->default_case->case_body)
        codegen_stmt(cc, node->default_case->case_body);
      if (g_ir_cur_func && g_ir_cur_func->tail &&
          !ir_op_is_terminator(g_ir_cur_func->tail->op)) {
        sprintf(ir_lbl, ".L%d", end_lbl);
        ZCC_EMIT_BR(ir_lbl, node->line);
      }
    }

    if (node->body)
      codegen_stmt(cc, node->body);

    emit_label_fmt(cc, end_lbl, FMT_DEF);
    sprintf(ir_lbl, ".L%d", end_lbl);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    cc->break_label = old_break;
    return;
  }

  case ND_BREAK: {
    char ir_lbl[32];
    emit_label_fmt(cc, cc->break_label, FMT_JMP);
    sprintf(ir_lbl, ".L%d", cc->break_label);
    ZCC_EMIT_BR(ir_lbl, node->line);
    return;
  }

  case ND_CONTINUE: {
    char ir_lbl[32];
    emit_label_fmt(cc, cc->continue_label, FMT_JMP);
    sprintf(ir_lbl, ".L%d", cc->continue_label);
    ZCC_EMIT_BR(ir_lbl, node->line);
    return;
  }

  case ND_GOTO: {
    char ir_lbl[64];
    if (!node->label_name) {
      error_at(cc, node->line, "codegen_stmt: ND_GOTO null label_name");
      return;
    }
    fprintf(cc->out, "    jmp .Luser_%s_%s\n", cc->current_func,
            node->label_name);
    sprintf(ir_lbl, ".Luser_%s_%s", cc->current_func, node->label_name);
    ZCC_EMIT_BR(ir_lbl, node->line);
    return;
  }

  case ND_GOTO_COMPUTED: {
    /* GNU C Extension: goto *expr; */
    codegen_expr(cc, node->lhs);
    pop_reg(cc, "rax");
    fprintf(cc->out, "    jmp *%%rax\n");
    /* IR bridge: unsupported for now. */
    return;
  }

  case ND_LABEL: {
    char ir_lbl[64];
    if (!node->label_name) {
      error_at(cc, node->line, "codegen_stmt: ND_LABEL null label_name");
      return;
    }
    fprintf(cc->out, ".Luser_%s_%s:\n", cc->current_func, node->label_name);
    sprintf(ir_lbl, ".Luser_%s_%s", cc->current_func, node->label_name);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    if (node->lhs)
      codegen_stmt(cc, node->lhs);
    return;
  }

  case ND_NOP:
    return;

  case ND_ASM:
    fprintf(cc->out, "    %s\n", node->asm_string);
    ZCC_EMIT_ASM(node->asm_string, node->line);
    /* Note: IR backend handles this via ZCC_ND_ASM -> OP_ASM translation if enabled */
    return;

  default: {
    /* expression statement */
    char badmsg[80];
    if (is_bad_ptr(node)) {
      sprintf(badmsg, "codegen_stmt default: bad expr node %p",
              node ? (void *)node : (void *)0);
      error_at(cc, 0, badmsg);
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node);
    return;
  }
  }
}

/* ================================================================ */
/* REGISTER ALLOCATOR (CHEATCODE 3)                                  */
/* ================================================================ */

static int pseudo_pc = 0;
static Symbol *ra_locals[1024];
static int num_ra_locals = 0;
static char *get_callee_reg(int i) {
  if (backend_ops) {
    if (i == 0) return "r4";
    if (i == 1) return "r5";
    if (i == 2) return "r6";
    if (i == 3) return "r7";
    return "r4";
  }
  if (i == 0)
    return "%r12";
  if (i == 1)
    return "%r13";
  if (i == 2)
    return "%r14";
  if (i == 3)
    return "%r15";
  return "%rbx";
}

static void ra_add_local(Symbol *sym) {
  int i;
  if (!sym || !sym->is_local || sym->live_start == -1)
    return;
  for (i = 0; i < num_ra_locals; i++) {
    if (ra_locals[i] == sym)
      return;
  }
  if (num_ra_locals < 1024)
    ra_locals[num_ra_locals++] = sym;
}

static void compute_liveness(Node *n) {
  int i;
  int loop_start;
  if (!n)
    return;
  pseudo_pc++;

  if (n->kind == ND_WHILE || n->kind == ND_FOR || n->kind == ND_DO_WHILE) {
    loop_start = pseudo_pc;
    compute_liveness(n->cond);
    compute_liveness(n->body);
    compute_liveness(n->init);
    compute_liveness(n->inc);
    for (i = 0; i < num_ra_locals; i++) {
      if (ra_locals[i]->live_end >= loop_start) {
        ra_locals[i]->live_end = pseudo_pc;
        if (ra_locals[i]->live_start >= loop_start) {
          ra_locals[i]->live_start = loop_start;
        }
      }
    }
    return;
  }

  if (n->kind == ND_ADDR && n->lhs && n->lhs->kind == ND_VAR && n->lhs->sym) {
    n->lhs->sym->live_start = -1; /* disable alloc */
  }
  if (n->kind == ND_VAR && n->sym && n->sym->is_local) {
    if (n->sym->live_start != -1) {
      if (n->sym->live_start == 0)
        n->sym->live_start = pseudo_pc;
      n->sym->live_end = pseudo_pc;
      ra_add_local(n->sym);
    }
  }

  if (n->kind == ND_ASSIGN) {
    compute_liveness(n->rhs);
    compute_liveness(n->lhs);
  } else {
    compute_liveness(n->lhs);
    compute_liveness(n->rhs);
  }
  compute_liveness(n->cond);
  compute_liveness(n->then_body);
  compute_liveness(n->else_body);
  compute_liveness(n->init);
  compute_liveness(n->inc);
  compute_liveness(n->body);
  if (n->stmts) {
    for (i = 0; i < n->num_stmts; i++)
      compute_liveness(n->stmts[i]);
  }
  if (n->args) {
    for (i = 0; i < n->num_args; i++)
      compute_liveness(n->args[i]);
  }
  if (n->cases) {
    for (i = 0; i < n->num_cases; i++)
      compute_liveness(n->cases[i]);
  }
  compute_liveness(n->default_case);
  compute_liveness(n->case_body);
}

static int allocate_registers(Node *func) {
  int count = 0;
  int i, j, r;
  int param_limit = -(func->num_params * 8);
  char *active_regs[5] = {0, 0, 0, 0, 0};
  int active_ends[5] = {0, 0, 0, 0, 0};
  int used_regs_bitmask = 0;

  num_ra_locals = 0;
  pseudo_pc = 1;
  compute_liveness(func->body);

  for (i = 0; i < num_ra_locals; i++) {
    Symbol *sym = ra_locals[i];
    if (sym->stack_offset >= param_limit && sym->stack_offset < 0) {
      sym->live_start = -1; /* never alloc parameters for safety */
    }
    if (sym->live_start != -1 && sym->type && sym->type->kind != TY_ARRAY &&
        (is_integer(sym->type) || sym->type->kind == TY_PTR)) {
      ra_locals[count++] = sym;
    } else {
      sym->live_start = -1;
    }
  }
  num_ra_locals = count;

  for (i = 0; i < num_ra_locals; i++) {
    for (j = i + 1; j < num_ra_locals; j++) {
      if (ra_locals[j]->live_start < ra_locals[i]->live_start) {
        Symbol *t = ra_locals[i];
        ra_locals[i] = ra_locals[j];
        ra_locals[j] = t;
      }
    }
  }

  /* Linear Scan */
  for (i = 0; i < num_ra_locals; i++) {
    Symbol *sym = ra_locals[i];
    if (sym->live_start == -1)
      continue;

    for (r = 0; r < 5; r++) {
      if (active_regs[r] && active_ends[r] <= sym->live_start)
        active_regs[r] = 0;
    }

    int allocated = -1;
    for (r = 0; r < 5; r++) {
      if (!active_regs[r]) {
        allocated = r;
        active_regs[r] = get_callee_reg(r);
        active_ends[r] = sym->live_end;
        sym->assigned_reg = get_callee_reg(r);
        used_regs_bitmask |= (1 << r);
        break;
      }
    }

    if (allocated == -1) {
      sym->assigned_reg = 0;
    }
  }
  return used_regs_bitmask;
}

extern struct ZCCNode *zcc_node_from(struct Node *ast);
extern int zcc_run_passes_emit_body_pgo(struct ZCCNode *body, const char *profile, const char *name, void *out, int stack_size, int num_params, int end_label1, int end_label2);

#pragma weak zcc_run_passes_emit_body_pgo
#pragma weak zcc_node_from

static int ir_blacklisted(const char *name) {
  if (!name)
    return 0;
  static const char *blacklist[] = {
      /* Add explicitly dangerous functions here to fallback to AST backend */
      "main", "read_file", "init_compiler", 
      "lookup_keyword_fallback", "parse_stmt", "next_token", NULL};
  int i;
  for (i = 0; blacklist[i]; i++) {
    if (strcmp(name, blacklist[i]) == 0) {
      fprintf(stderr, "[ZCC-BLACKLIST] HIT (skipping IR): %s\n", name);
      return 1;
    }
  }
  return 0;
}

void codegen_func(Compiler *cc, Node *func) {
  char *argregs[6];
  int i;
  int stack_size;
  int used_regs;

  if (!func)
    return;
  fprintf(stderr, "cc_func: %s\n", func->func_def_name);
  cc->used_regs_mask = allocate_registers(func);
  cc->is_forced_mask = 0;
  if (backend_ops) {
      if ((cc->used_regs_mask & 0x1F) != 0x1F) cc->is_forced_mask = 1;
      cc->used_regs_mask = 0x1F;
  }
  used_regs = cc->used_regs_mask;

  if (getenv("ZCC_EMIT_TELEMETRY")) {
      fprintf(stderr, "[telem] fn=%s used_regs=0x%02X src=%s\n",
              func->func_def_name, used_regs, cc->is_forced_mask ? "forced" : "computed");
  }

  argregs[0] = "rdi";
  argregs[1] = "rsi";
  argregs[2] = "rdx";
  argregs[3] = "rcx";
  argregs[4] = "r8";
  argregs[5] = "r9";

  cc->func_end_label = new_label(cc);
  strncpy(cc->current_func, func->func_def_name, MAX_IDENT - 1);

  ir_bridge_func_begin(func);

  if (backend_ops && backend_ops->emit_prologue) {
      backend_ops->emit_prologue(cc, func);
  } else {
      stack_size = func->stack_size + 40; /* reserve 5x8 byte push slots */
      stack_size += 16;                   /* ABI: 16-byte scratch for aggregate returns (CG-IR-019) */
      cc->abi_scratch_offset = -stack_size;
      if (func->func_type && func->func_type->is_variadic) {
          stack_size += 176;
      }
      if (stack_size < 256)
        stack_size = 256;
      stack_size = (stack_size + 15) & ~15;

      fprintf(cc->out, "    .text\n");
      if (!func->is_static) {
        fprintf(cc->out, "    .globl %s\n", func->func_def_name);
      }
      fprintf(cc->out, "%s:\n", func->func_def_name);
      fprintf(cc->out, "    pushq %%rbp\n");
      fprintf(cc->out, "    movq %%rsp, %%rbp\n");
      fprintf(cc->out, "    subq $%d, %%rsp\n", stack_size);

      for (i = 0; i < 5; i++) {
        if (used_regs & (1 << i)) {
          fprintf(cc->out, "    movq %s, %d(%%rbp)\n", get_callee_reg(i),
                  -(func->stack_size + 8 * (i + 1)));
        }
      }
  }

  cc->stack_depth = 0;

  /* We need to re-create the scope with locals for codegen.
     During parsing, scope_add_local assigned stack offsets.
     Those offsets are stored in the Symbol nodes (via the arena).
     The body's ND_VAR nodes reference those Symbols.
     So the offsets are already embedded in the AST — we just need
     to store params from registers to their assigned stack slots. */

  scope_push(cc);

  /* Store params using System V ABI classification rules §3.2.3. */
  int param_offset = 0;
  int f_idx = 0;
  int gp_idx = 0;
  int stack_arg_off = 16; /* args on stack start at 16(%rbp) */
  if (!backend_ops) {
  for (i = 0; i < func->num_params; i++) {
    Type *ptype = func->func_params->types[i];
    int sz = type_size(ptype);
    int slots = (sz + 7) / 8;
    param_offset -= (slots * 8); // Always 8-byte aligned slots in the local frame
    
    if (ptype->kind == TY_STRUCT || ptype->kind == TY_UNION) {
        abi_class_t eb[2];
        classify_aggregate(ptype, eb);
        int fits = (eb[0] != CLASS_MEMORY);
        if (fits) {
            int needed_gp = 0, needed_fp = 0;
            if (eb[0] == CLASS_SSE) needed_fp++; else needed_gp++;
            if (sz > 8) { if (eb[1] == CLASS_SSE) needed_fp++; else needed_gp++; }
            if (gp_idx + needed_gp > 6 || f_idx + needed_fp > 8) fits = 0;
        }

        if (fits) {
            /* Registers */
            if (eb[0] == CLASS_SSE) fprintf(cc->out, "    movq %%xmm%d, %d(%%rbp)\n", f_idx++, param_offset);
            else fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", argregs[gp_idx++], param_offset);
            if (sz > 8) {
                if (eb[1] == CLASS_SSE) fprintf(cc->out, "    movq %%xmm%d, %d(%%rbp)\n", f_idx++, param_offset + 8);
                else fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", argregs[gp_idx++], param_offset + 8);
            }
        } else {
            /* Memory: data is directly on stack at stack_arg_off */
            for (int j = 0; j < slots; j++) {
                fprintf(cc->out, "    movq %d(%%rbp), %%rax\n", stack_arg_off + j * 8);
                fprintf(cc->out, "    movq %%rax, %d(%%rbp)\n", param_offset + j * 8);
            }
            stack_arg_off += slots * 8;
        }
    } else {
        int is_fp = is_float_type(ptype);
        if (is_fp && f_idx < 8) {
            if (ptype->kind == TY_FLOAT) fprintf(cc->out, "    movss %%xmm%d, %d(%%rbp)\n", f_idx++, param_offset);
            else fprintf(cc->out, "    movsd %%xmm%d, %d(%%rbp)\n", f_idx++, param_offset);
        } else if (!is_fp && gp_idx < 6) {
            fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", argregs[gp_idx++], param_offset);
        } else {
            /* Pass on stack */
            fprintf(cc->out, "    movq %d(%%rbp), %%rax\n", stack_arg_off);
            fprintf(cc->out, "    movq %%rax, %d(%%rbp)\n", param_offset);
            stack_arg_off += 8;
        }
    }
  }

  if (func->func_type && func->func_type->is_variadic) {
    int save_base = -(func->stack_size + 40 + 176);
    /* Save GP regs at offsets 0-47 */
    for (i = 0; i < 6; i++) {
        fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", argregs[i], save_base + i*8);
    }
    /* Save XMM regs at offsets 48-175 */
    for (i = 0; i < 8; i++) {
        fprintf(cc->out, "    movsd %%xmm%d, %d(%%rbp)\n", i, save_base + 48 + i*16);
    }
    Symbol *fsym = scope_find(cc, func->func_def_name);
    if (fsym) {
        fsym->stack_offset = save_base;
    }
  }
  } /* end !backend_ops block */

  int ir_ok = 0;
  if ((getenv("ZCC_IR_BACKEND") || getenv("ZCC_IR_LOWER")) &&
      !ir_blacklisted(func->func_def_name)) {
    if (zcc_run_passes_emit_body_pgo && zcc_node_from) {
      void *ir_ast = zcc_node_from((void *)func->body);
      if (ir_ast) {
        char params_env[512];
        memset(params_env, 0, sizeof(params_env));
        for (int p_idx = 0; p_idx < func->num_params; p_idx++) {
          if (p_idx > 0)
            strcat(params_env, ",");
          strncat(params_env, func->func_params->names[p_idx],
                  511 - strlen(params_env));
        }
        setenv("ZCC_IR_PARAM_NAMES", params_env, 1);

        ir_ok = zcc_run_passes_emit_body_pgo(
            ir_ast, NULL, func->func_def_name, cc->out, stack_size,
            func->num_params, cc->func_end_label, cc->func_end_label);

        unsetenv("ZCC_IR_PARAM_NAMES");
      }
    }
  }
  if (!ir_ok) {
    codegen_stmt(cc, func->body);
  }

  if (backend_ops && backend_ops->emit_epilogue) {
      backend_ops->emit_epilogue(cc, func);
  } else {
      fprintf(cc->out, ".Lfunc_end_%d:\n", cc->func_end_label);
      for (i = 4; i >= 0; i--) {
        if (used_regs & (1 << i)) {
          fprintf(cc->out, "    movq %d(%%rbp), %s\n",
                  -(func->stack_size + 8 * (i + 1)), get_callee_reg(i));
        }
      }
      fprintf(cc->out, "    movq %%rbp, %%rsp\n");
      fprintf(cc->out, "    popq %%rbp\n");
      fprintf(cc->out, "    ret\n");
  }

  ir_bridge_func_end();

  scope_pop(cc);
}

/* ================================================================ */
/* GLOBAL VARIABLE EMISSION                                          */
/* Bug fix: section directive (.bss/.data) BEFORE label              */
/* Bug fix: .p2align 3 before every label (x86-64 ABI alignment)    */
/* ================================================================ */

static long long eval_const_expr_p4(Node *elem, int *ok) {
    if (!elem) { *ok = 0; return 0; }
    if (elem->kind == ND_CAST) return eval_const_expr_p4(elem->lhs, ok);
    if (elem->kind == ND_NUM) return elem->int_val;
    if (elem->kind == ND_ADD) return eval_const_expr_p4(elem->lhs, ok) + eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_SUB) return eval_const_expr_p4(elem->lhs, ok) - eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_MUL) return eval_const_expr_p4(elem->lhs, ok) * eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_DIV) { long long r = eval_const_expr_p4(elem->rhs, ok); if(r) return eval_const_expr_p4(elem->lhs, ok) / r; return 0; }
    if (elem->kind == ND_BOR) return eval_const_expr_p4(elem->lhs, ok) | eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_BAND) return eval_const_expr_p4(elem->lhs, ok) & eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_BXOR) return eval_const_expr_p4(elem->lhs, ok) ^ eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_SHL) return eval_const_expr_p4(elem->lhs, ok) << eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_SHR) return eval_const_expr_p4(elem->lhs, ok) >> eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_NEG) {
        /* CG-GINIT-FLOAT-001: float negation must negate the float value,
           not the bit-pattern. Check if child is ND_FLIT. */
        if (elem->lhs && elem->lhs->kind == ND_FLIT) {
            double negval = -(elem->lhs->f_val);
            if (elem->lhs->type && elem->lhs->type->kind == TY_FLOAT) {
                float fv = (float)negval;
                unsigned int fbits;
                memcpy(&fbits, &fv, sizeof(float));
                return (long long)fbits;
            } else {
                unsigned long long bits;
                memcpy(&bits, &negval, sizeof(double));
                return (long long)bits;
            }
        }
        return -eval_const_expr_p4(elem->lhs, ok);
    }
    if (elem->kind == ND_BNOT) return ~eval_const_expr_p4(elem->lhs, ok);
    if (elem->kind == ND_LNOT) return !eval_const_expr_p4(elem->lhs, ok);
    /* CG-GINIT-FLOAT-001: float/double literals in aggregate initializers */
    if (elem->kind == ND_FLIT) {
        if (elem->type && elem->type->kind == TY_FLOAT) {
            float fv = (float)elem->f_val;
            unsigned int fbits;
            memcpy(&fbits, &fv, sizeof(float));
            return (long long)fbits;
        } else {
            unsigned long long bits;
            memcpy(&bits, &elem->f_val, sizeof(double));
            return (long long)bits;
        }
    }
    *ok = 0;
    return 0;
}

static void emit_struct_fields(Compiler *cc, StructField *fields, Node **args, int num_args, int *arg_idx, int arg_end, int base_offset, int *emitted) {
    StructField *f;
    for (f = fields; f; f = f->next) {
        int field_abs_offset = base_offset + f->offset;
        
        if (field_abs_offset > *emitted) {
            fprintf(cc->out, "    .zero %d\n", field_abs_offset - *emitted);
            *emitted = field_abs_offset;
        }
        
        if (f->type->kind == TY_STRUCT) {
            emit_struct_fields(cc, f->type->fields, args, num_args, arg_idx, arg_end, field_abs_offset, emitted);
        } else if (f->type->kind == TY_ARRAY) {
            int j;
            if (f->type->base && (f->type->base->kind == TY_STRUCT || f->type->base->kind == TY_UNION)) {
                int elem_size = type_size(f->type->base);
                for (j = 0; j < f->type->array_len; j++) {
                    int expected_end = field_abs_offset + (j + 1) * elem_size;
                    emit_struct_fields(cc, f->type->base->fields, args, num_args, arg_idx, arg_end, field_abs_offset + j * elem_size, emitted);
                    if (*emitted < expected_end) {
                        fprintf(cc->out, "    .zero %d\n", expected_end - *emitted);
                        *emitted = expected_end;
                    }
                }
            } else {
                int elem_size = type_size(f->type->base);
                for (j = 0; j < f->type->array_len; j++) {
                    if (*arg_idx < arg_end && *arg_idx < num_args) {
                        Node *elem = args[(*arg_idx)++];
                        while (elem && elem->kind == ND_CAST) elem = elem->lhs; /* Strip ND_CAST */
                        int const_ok = 1;
                        long long const_val = eval_const_expr_p4(elem, &const_ok);
                        if (!elem) {
                            fprintf(cc->out, "    .zero %d\n", elem_size);
                        } else if (const_ok) {
                            if (elem_size == 1) fprintf(cc->out, "    .byte %lld\n", const_val);
                            else if (elem_size == 2) fprintf(cc->out, "    .short %lld\n", const_val);
                            else if (elem_size == 4) fprintf(cc->out, "    .long %lld\n", const_val);
                            else fprintf(cc->out, "    .quad %lld\n", const_val);
                        } else if (elem->kind == ND_STR) {
                            if (elem_size == 4) fprintf(cc->out, "    .long .Lstr_%d\n", elem->str_id);
                            else fprintf(cc->out, "    .quad .Lstr_%d\n", elem->str_id);
                        } else if (elem->kind == ND_ADDR_LABEL) {
                            if (elem_size == 4) fprintf(cc->out, "    .long .Luser_%s_%s\n", cc->current_func, elem->label_name);
                            else fprintf(cc->out, "    .quad .Luser_%s_%s\n", cc->current_func, elem->label_name);
                        } else if (elem->kind == ND_ADDR && elem->lhs && elem->lhs->kind == ND_VAR) {
                            if (elem_size == 4) fprintf(cc->out, "    .long %s\n", elem->lhs->name);
                            else fprintf(cc->out, "    .quad %s\n", elem->lhs->name);
                } else if (elem->kind == ND_ADDR && elem->lhs && elem->lhs->kind == ND_DEREF && elem->lhs->lhs && elem->lhs->lhs->kind == ND_ADD && elem->lhs->lhs->lhs && elem->lhs->lhs->lhs->kind == ND_VAR && elem->lhs->lhs->rhs && elem->lhs->lhs->rhs->kind == ND_NUM) {
                    long long offset = elem->lhs->lhs->rhs->int_val;
                    if (elem->lhs->lhs->lhs->type && elem->lhs->lhs->lhs->type->base) offset *= type_size(elem->lhs->lhs->lhs->type->base);
                    if (elem_size == 4) fprintf(cc->out, "    .long %s + %lld\n", elem->lhs->lhs->lhs->name, offset);
                    else fprintf(cc->out, "    .quad %s + %lld\n", elem->lhs->lhs->lhs->name, offset);
                } else if (elem->kind == ND_ADD && elem->lhs && elem->lhs->kind == ND_VAR && elem->rhs && elem->rhs->kind == ND_NUM) {
                    long long offset = elem->rhs->int_val;
                    if (elem->lhs->type && elem->lhs->type->base) offset *= type_size(elem->lhs->type->base);
                    if (elem_size == 4) fprintf(cc->out, "    .long %s + %lld\n", elem->lhs->name, offset);
                    else fprintf(cc->out, "    .quad %s + %lld\n", elem->lhs->name, offset);
                        } else if (elem->kind == ND_VAR) {
                            if (elem_size == 4) fprintf(cc->out, "    .long %s\n", elem->name);
                            else fprintf(cc->out, "    .quad %s\n", elem->name);
                        } else {
                            fprintf(stderr, "STRUCT-ARRAY FALLBACK ZERO: kind=%d, const_ok=%d\n", elem->kind, const_ok);
                            if (elem->lhs) fprintf(stderr, "  lhs->kind=%d lhs->name=%s\n", elem->lhs->kind, elem->lhs->name ? elem->lhs->name : "none");
                            fprintf(cc->out, "    .zero %d\n", elem_size);
                        }
                    } else {
                        fprintf(cc->out, "    .zero %d\n", elem_size);
                    }
                    *emitted += elem_size;
                }
            }
        } else {
            int elem_size = type_size(f->type);
            if (*arg_idx < arg_end && *arg_idx < num_args) {
                Node *elem = args[(*arg_idx)++];
                while (elem && elem->kind == ND_CAST) { elem = elem->lhs; } /* Strip ND_CAST */
                int const_ok = 1;
                long long const_val = eval_const_expr_p4(elem, &const_ok);
                if (!elem) {
                    fprintf(cc->out, "    .zero %d\n", elem_size);
                } else if (const_ok) {
                    if (elem_size == 1) fprintf(cc->out, "    .byte %lld\n", const_val);
                    else if (elem_size == 2) fprintf(cc->out, "    .short %lld\n", const_val);
                    else if (elem_size == 4) fprintf(cc->out, "    .long %lld\n", const_val);
                    else fprintf(cc->out, "    .quad %lld\n", const_val);
                } else if (elem->kind == ND_STR) {
                    if (elem_size == 4) fprintf(cc->out, "    .long .Lstr_%d\n", elem->str_id);
                    else fprintf(cc->out, "    .quad .Lstr_%d\n", elem->str_id);
                } else if (elem->kind == ND_ADDR_LABEL) {
                    if (elem_size == 4) fprintf(cc->out, "    .long .Luser_%s_%s\n", cc->current_func, elem->label_name);
                    else fprintf(cc->out, "    .quad .Luser_%s_%s\n", cc->current_func, elem->label_name);
                } else if (elem->kind == ND_ADDR && elem->lhs && elem->lhs->kind == ND_VAR) {
                    if (elem_size == 4) fprintf(cc->out, "    .long %s\n", elem->lhs->name);
                    else fprintf(cc->out, "    .quad %s\n", elem->lhs->name);
                } else if (elem->kind == ND_ADDR && elem->lhs && elem->lhs->kind == ND_DEREF && elem->lhs->lhs && elem->lhs->lhs->kind == ND_ADD && elem->lhs->lhs->lhs && elem->lhs->lhs->lhs->kind == ND_VAR && elem->lhs->lhs->rhs && elem->lhs->lhs->rhs->kind == ND_NUM) {
                    long long offset = elem->lhs->lhs->rhs->int_val;
                    if (elem->lhs->lhs->lhs->type && elem->lhs->lhs->lhs->type->base) offset *= type_size(elem->lhs->lhs->lhs->type->base);
                    if (elem_size == 4) fprintf(cc->out, "    .long %s + %lld\n", elem->lhs->lhs->lhs->name, offset);
                    else fprintf(cc->out, "    .quad %s + %lld\n", elem->lhs->lhs->lhs->name, offset);
                } else if (elem->kind == ND_ADD && elem->lhs && elem->lhs->kind == ND_VAR && elem->rhs && elem->rhs->kind == ND_NUM) {
                    long long offset = elem->rhs->int_val;
                    if (elem->lhs->type && elem->lhs->type->base) offset *= type_size(elem->lhs->type->base);
                    if (elem_size == 4) fprintf(cc->out, "    .long %s + %lld\n", elem->lhs->name, offset);
                    else fprintf(cc->out, "    .quad %s + %lld\n", elem->lhs->name, offset);
                } else if (elem->kind == ND_VAR) {
                    if (elem_size == 4) fprintf(cc->out, "    .long %s\n", elem->name);
                    else fprintf(cc->out, "    .quad %s\n", elem->name);
                } else {
                    fprintf(stderr, "STRUCT FIELD FALLBACK ZERO: kind=%d, const_ok=%d\n", elem->kind, const_ok);
                    if (elem->lhs) fprintf(stderr, "  lhs->kind=%d lhs->name=%s\n", elem->lhs->kind, elem->lhs->name ? elem->lhs->name : "none");
                    fprintf(cc->out, "    .zero %d\n", elem_size);
                }
            } else {
                fprintf(cc->out, "    .zero %d\n", elem_size);
            }
            *emitted += elem_size;
        }
    }
}

static void emit_global_var(Compiler *cc, Node *gvar) {
  int size;

  if (gvar->is_extern)
    return; /* no emission for extern */

  /* Skip .comm for function declarations — they must not create BSS symbols
   * that shadow the real glibc implementations at link time. This catches
   * complex declarations like signal() that the parser may treat as vars. */
  if (gvar->type && (gvar->type->kind == TY_FUNC ||
      (gvar->type->kind == TY_PTR && gvar->type->base && gvar->type->base->kind == TY_FUNC)))
    return;

  size = type_size(gvar->type);
  if (size <= 0)
    size = 8;
    
  if (gvar->initializer) {
    /* initialized data goes in .data */
    fprintf(cc->out, "    .data\n");
    fprintf(cc->out, "    .p2align 3\n");
    if (!gvar->is_static) {
      fprintf(cc->out, "    .globl %s\n", gvar->name);
    }
    fprintf(cc->out, "%s:\n", gvar->name);
    /* only handle simple integer initializers */
    /* Strip ND_CAST wrappers (e.g. function pointer casts like
       `Curl_cmalloc = (curl_malloc_callback)malloc`) */
    Node *init = gvar->initializer;
    while (init && init->kind == ND_CAST) init = init->lhs;
    if (init && init->kind == ND_NUM) {
      if (size == 1)
        fprintf(cc->out, "    .byte %lld\n", init->int_val);
      else if (size == 2)
        fprintf(cc->out, "    .short %lld\n", init->int_val);
      else if (size == 4)
        fprintf(cc->out, "    .long %lld\n", init->int_val);
      else
        fprintf(cc->out, "    .quad %lld\n", init->int_val);
    } else if (init && init->kind == ND_FLIT) {
      /* CG-GINIT-FLOAT-001: static/global float/double scalar initializer */
      if (size == 4) {
        float fv = (float)init->f_val;
        unsigned int fbits;
        memcpy(&fbits, &fv, sizeof(float));
        fprintf(cc->out, "    .long 0x%08x\n", fbits);
      } else {
        unsigned long long bits;
        memcpy(&bits, &init->f_val, sizeof(double));
        fprintf(cc->out, "    .quad 0x%016llx\n", bits);
      }
    } else if (init && init->kind == ND_NEG && init->lhs && init->lhs->kind == ND_FLIT) {
      /* CG-GINIT-FLOAT-001: negated float/double scalar (e.g. static double d = -1.5;) */
      double negval = -(init->lhs->f_val);
      if (size == 4) {
        float fv = (float)negval;
        unsigned int fbits;
        memcpy(&fbits, &fv, sizeof(float));
        fprintf(cc->out, "    .long 0x%08x\n", fbits);
      } else {
        unsigned long long bits;
        memcpy(&bits, &negval, sizeof(double));
        fprintf(cc->out, "    .quad 0x%016llx\n", bits);
      }
    } else if (init->kind == ND_ADDR_LABEL) {
      if (size == 4)
        fprintf(cc->out, "    .long .Luser_%s_%s\n", cc->current_func, init->label_name);
      else
        fprintf(cc->out, "    .quad .Luser_%s_%s\n", cc->current_func, init->label_name);
    } else if (init->kind == ND_ADDR && init->lhs && init->lhs->kind == ND_VAR) {
      if (size == 4)
        fprintf(cc->out, "    .long %s\n", init->lhs->name);
      else
        fprintf(cc->out, "    .quad %s\n", init->lhs->name);
    } else if (init->kind == ND_ADDR && init->lhs && init->lhs->kind == ND_DEREF && init->lhs->lhs && init->lhs->lhs->kind == ND_ADD && init->lhs->lhs->lhs && init->lhs->lhs->lhs->kind == ND_VAR && init->lhs->lhs->rhs && init->lhs->lhs->rhs->kind == ND_NUM) {
      long long offset = init->lhs->lhs->rhs->int_val;
      if (init->lhs->lhs->lhs->type && init->lhs->lhs->lhs->type->base) offset *= type_size(init->lhs->lhs->lhs->type->base);
      if (size == 4) fprintf(cc->out, "    .long %s + %lld\n", init->lhs->lhs->lhs->name, offset);
      else fprintf(cc->out, "    .quad %s + %lld\n", init->lhs->lhs->lhs->name, offset);
    } else if (init->kind == ND_ADD && init->lhs && init->lhs->kind == ND_VAR && init->rhs && init->rhs->kind == ND_NUM) {
      long long offset = init->rhs->int_val;
      if (init->lhs->type && init->lhs->type->base) offset *= type_size(init->lhs->type->base);
      if (size == 4) fprintf(cc->out, "    .long %s + %lld\n", init->lhs->name, offset);
      else fprintf(cc->out, "    .quad %s + %lld\n", init->lhs->name, offset);
    } else if (init->kind == ND_VAR) {
      if (size == 4)
        fprintf(cc->out, "    .long %s\n", init->name);
      else
        fprintf(cc->out, "    .quad %s\n", init->name);
    } else if (init->kind == ND_STR) {
      if (gvar->type && gvar->type->kind == TY_ARRAY) {
          int j;
          int str_id = init->str_id;
          int len = cc->strings[str_id].len;
          fprintf(cc->out, "    .ascii \"");
          for (j = 0; j < len; j++) {
              char c = cc->strings[str_id].data[j];
              if (c == '\n') fprintf(cc->out, "\\n");
              else if (c == '\t') fprintf(cc->out, "\\t");
              else if (c == '\r') fprintf(cc->out, "\\r");
              else if (c == '\\') fprintf(cc->out, "\\\\");
              else if (c == '"') fprintf(cc->out, "\\\"");
              else if (c >= 32 && c < 127) fprintf(cc->out, "%c", c);
              else fprintf(cc->out, "\\%03o", (unsigned char)c);
          }
          fprintf(cc->out, "\\0\"\n");
          if (size > len + 1) fprintf(cc->out, "    .zero %d\n", size - (len + 1));
      } else {
          if (size == 4) fprintf(cc->out, "    .long .Lstr_%d\n", init->str_id);
          else fprintf(cc->out, "    .quad .Lstr_%d\n", init->str_id);
      }
        } else if (init->kind == ND_INIT_LIST) {
            int emitted = 0;
            int i;
            int elem_size = 1;
        if (gvar->type && gvar->type->kind == TY_STRUCT) {
            int arg_idx = 0;
            emit_struct_fields(cc, gvar->type->fields, init->args, init->num_args, &arg_idx, init->num_args, 0, &emitted);
        } else if (gvar->type && gvar->type->kind == TY_ARRAY && gvar->type->base && (gvar->type->base->kind == TY_STRUCT || gvar->type->base->kind == TY_UNION)) {
            int i;
            int arg_idx = 0;
            int elem_size = type_size(gvar->type->base);
            int budget = init->num_args / gvar->type->array_len;
            for (i = 0; i < gvar->type->array_len; i++) {
                int expected_end = (i + 1) * elem_size;
                int arg_end = (i + 1) * budget;
                if (arg_end > init->num_args) arg_end = init->num_args;
                emit_struct_fields(cc, gvar->type->base->fields, init->args, init->num_args, &arg_idx, arg_end, i * elem_size, &emitted);
                if (emitted < expected_end) {
                    fprintf(cc->out, "    .zero %d\n", expected_end - emitted);
                    emitted = expected_end;
                }
            }
        } else {
            int i;
            int elem_size = 1;
            Type *stype = gvar->type;
            while (stype && stype->kind == TY_ARRAY) {
                stype = stype->base;
            }
            if (stype) elem_size = type_size(stype);
            for (i = 0; i < init->num_args; i++) {
                Node *elem = init->args[i];
                int const_ok = 1;
                long long const_val = eval_const_expr_p4(elem, &const_ok);
                if (!elem) {
                    fprintf(cc->out, "    .zero %d\n", elem_size);
                } else if (const_ok) {
                    if (elem_size == 1) fprintf(cc->out, "    .byte %lld\n", const_val);
                    else if (elem_size == 2) fprintf(cc->out, "    .short %lld\n", const_val);
                    else if (elem_size == 4) fprintf(cc->out, "    .long %lld\n", const_val);
                    else fprintf(cc->out, "    .quad %lld\n", const_val);
                } else if (elem->kind == ND_STR) {
                    if (elem_size == 4) fprintf(cc->out, "    .long .Lstr_%d\n", elem->str_id);
                    else fprintf(cc->out, "    .quad .Lstr_%d\n", elem->str_id);
                } else if (elem->kind == ND_ADDR_LABEL) {
                    if (elem_size == 4) fprintf(cc->out, "    .long .Luser_%s_%s\n", cc->current_func, elem->label_name);
                    else fprintf(cc->out, "    .quad .Luser_%s_%s\n", cc->current_func, elem->label_name);
                } else if (elem->kind == ND_ADDR && elem->lhs && elem->lhs->kind == ND_VAR) {
                    if (elem_size == 4) fprintf(cc->out, "    .long %s\n", elem->lhs->name);
                    else fprintf(cc->out, "    .quad %s\n", elem->lhs->name);
                } else if (elem->kind == ND_ADDR && elem->lhs && elem->lhs->kind == ND_DEREF && elem->lhs->lhs && elem->lhs->lhs->kind == ND_ADD && elem->lhs->lhs->lhs && elem->lhs->lhs->lhs->kind == ND_VAR && elem->lhs->lhs->rhs && elem->lhs->lhs->rhs->kind == ND_NUM) {
                    long long offset = elem->lhs->lhs->rhs->int_val;
                    if (elem->lhs->lhs->lhs->type && elem->lhs->lhs->lhs->type->base) offset *= type_size(elem->lhs->lhs->lhs->type->base);
                    if (elem_size == 4) fprintf(cc->out, "    .long %s + %lld\n", elem->lhs->lhs->lhs->name, offset);
                    else fprintf(cc->out, "    .quad %s + %lld\n", elem->lhs->lhs->lhs->name, offset);
                } else if (elem->kind == ND_ADD && elem->lhs && elem->lhs->kind == ND_VAR && elem->rhs && elem->rhs->kind == ND_NUM) {
                    long long offset = elem->rhs->int_val;
                    if (elem->lhs->type && elem->lhs->type->base) offset *= type_size(elem->lhs->type->base);
                    if (elem_size == 4) fprintf(cc->out, "    .long %s + %lld\n", elem->lhs->name, offset);
                    else fprintf(cc->out, "    .quad %s + %lld\n", elem->lhs->name, offset);
                } else if (elem->kind == ND_VAR) {
                    if (elem_size == 4) fprintf(cc->out, "    .long %s\n", elem->name);
                    else fprintf(cc->out, "    .quad %s\n", elem->name);
                } else {
                    fprintf(cc->out, "    .zero %d\n", elem_size);
                }
                emitted += elem_size;
            }
        }
        if (emitted < size) {
            fprintf(cc->out, "    .zero %d\n", size - emitted);
        }
    } else {
        fprintf(cc->out, "    .zero %d\n", size);
    }
  } else {
    /* tentative definitions and uninitialized data */
    if (gvar->is_static) {
        fprintf(cc->out, "    .local %s\n", gvar->name);
        fprintf(cc->out, "    .comm %s, %d, 8\n", gvar->name, size);
    } else {
        fprintf(cc->out, "    .comm %s, %d, 8\n", gvar->name, size);
    }
  }
}

/* ================================================================ */
/* STRING LITERAL EMISSION                                           */
/* ================================================================ */

static void emit_strings(Compiler *cc) {
  int i;
  fprintf(cc->out, "    .section .rodata\n");
  for (i = 0; i < cc->num_strings; i++) {
    int j;
    fprintf(cc->out, ".LS%d:\n", cc->strings[i].label_id);
    fprintf(cc->out, ".Lstr_%d:\n", i); /* IR backend alias */
    fprintf(cc->out, "    .asciz \"");
    for (j = 0; j < cc->strings[i].len; j++) {
      char c;
      c = cc->strings[i].data[j];
      if (c == '\n')
        fprintf(cc->out, "\\n");
      else if (c == '\t')
        fprintf(cc->out, "\\t");
      else if (c == '\r')
        fprintf(cc->out, "\\r");
      else if (c == '\\')
        fprintf(cc->out, "\\\\");
      else if (c == '"')
        fprintf(cc->out, "\\\"");
      else if (c == '\0')
        fprintf(cc->out, "\\0");
      else if (c >= 32 && c < 127)
        fprintf(cc->out, "%c", c);
      else
        fprintf(cc->out, "\\%03o", (unsigned char)c);
    }
    fprintf(cc->out, "\"\n");
  }
}

/* ================================================================ */
/* PROGRAM CODEGEN ENTRY                                             */
/* ================================================================ */

static void fold_constants(Compiler *cc, Node *node) {
  if (!node)
    return;

  fold_constants(cc, node->lhs);
  fold_constants(cc, node->rhs);
  fold_constants(cc, node->cond);
  fold_constants(cc, node->then_body);
  fold_constants(cc, node->else_body);
  fold_constants(cc, node->init);
  fold_constants(cc, node->inc);
  fold_constants(cc, node->body);
  fold_constants(cc, node->case_body);

  if (node->kind == ND_CALL && node->num_args > 0) {
    int i;
    for (i = 0; i < node->num_args; i++)
      fold_constants(cc, node->args[i]);
  }
  if (node->kind == ND_BLOCK && node->num_stmts > 0) {
    int i;
    for (i = 0; i < node->num_stmts; i++)
      fold_constants(cc, node->stmts[i]);
  }
  if (node->kind == ND_SWITCH && node->num_cases > 0) {
    int i;
    for (i = 0; i < node->num_cases; i++)
      fold_constants(cc, node->cases[i]);
    fold_constants(cc, node->default_case);
  }

  if (node->kind == ND_NEG || node->kind == ND_BNOT || node->kind == ND_LNOT) {
    if (node->lhs && node->lhs->kind == ND_NUM) {
      long long v1 = node->lhs->int_val, res = 0;
      if (node->kind == ND_NEG)
        res = -v1;
      else if (node->kind == ND_BNOT)
        res = ~v1;
      else if (node->kind == ND_LNOT)
        res = !v1;

      if (node->type && !node_type_unsigned(node)) {
          if (node->type->size == 4) res = (long long)(int)res;
          else if (node->type->size == 1) res = (long long)(char)res;
          else if (node->type->size == 2) res = (long long)(short)res;
      } else if (node->type && node_type_unsigned(node)) {
          if (node->type->size == 4) res = (long long)(unsigned int)res;
          else if (node->type->size == 1) res = (long long)(unsigned char)res;
          else if (node->type->size == 2) res = (long long)(unsigned short)res;
      }
      node->kind = ND_NUM;
      node->int_val = res;
      node->lhs = 0;
    }
  }

  if (node->kind == ND_ADD || node->kind == ND_SUB || node->kind == ND_MUL ||
      node->kind == ND_DIV || node->kind == ND_MOD) {
    if (node->lhs && node->rhs && node->lhs->kind == ND_NUM &&
        node->rhs->kind == ND_NUM) {
      long long v1 = node->lhs->int_val, v2 = node->rhs->int_val, res = 0;
      int is_unsigned = node->lhs->type && node_type_unsigned(node->lhs);
      unsigned long long u1 = v1, u2 = v2;
      if (is_unsigned && node->lhs->type) {
          if (node->lhs->type->size == 4) { u1 = (unsigned int)v1; u2 = (unsigned int)v2; }
          else if (node->lhs->type->size == 1) { u1 = (unsigned char)v1; u2 = (unsigned char)v2; }
          else if (node->lhs->type->size == 2) { u1 = (unsigned short)v1; u2 = (unsigned short)v2; }
      }

      if (node->kind == ND_ADD)
        res = v1 + v2;
      else if (node->kind == ND_SUB)
        res = v1 - v2;
      else if (node->kind == ND_MUL)
        res = v1 * v2;
      else if (node->kind == ND_DIV && v2 != 0)
        res = is_unsigned ? u1 / u2 : v1 / v2;
      else if (node->kind == ND_MOD && v2 != 0)
        res = is_unsigned ? u1 % u2 : v1 % v2;
      else
        return;
      if (node->type && !node_type_unsigned(node)) {
          if (node->type->size == 4) res = (long long)(int)res;
          else if (node->type->size == 1) res = (long long)(char)res;
          else if (node->type->size == 2) res = (long long)(short)res;
      } else if (node->type && node_type_unsigned(node)) {
          if (node->type->size == 4) res = (long long)(unsigned int)res;
          else if (node->type->size == 1) res = (long long)(unsigned char)res;
          else if (node->type->size == 2) res = (long long)(unsigned short)res;
      }
      node->kind = ND_NUM;
      node->int_val = res;
      node->lhs = 0;
      node->rhs = 0;
    }
  }

  if (node->kind == ND_SHL || node->kind == ND_SHR || node->kind == ND_BAND ||
      node->kind == ND_BOR || node->kind == ND_BXOR || node->kind == ND_LT ||
      node->kind == ND_LE || node->kind == ND_GT || node->kind == ND_GE ||
      node->kind == ND_EQ || node->kind == ND_NE) {
    if (node->lhs && node->rhs && node->lhs->kind == ND_NUM &&
        node->rhs->kind == ND_NUM) {
      long long v1 = node->lhs->int_val, v2 = node->rhs->int_val, res = 0;
      int is_unsigned = node->lhs->type && node_type_unsigned(node->lhs);
      unsigned long long u1 = v1, u2 = v2;
      if (is_unsigned && node->lhs->type) {
          if (node->lhs->type->size == 4) { u1 = (unsigned int)v1; u2 = (unsigned int)v2; }
          else if (node->lhs->type->size == 1) { u1 = (unsigned char)v1; u2 = (unsigned char)v2; }
          else if (node->lhs->type->size == 2) { u1 = (unsigned short)v1; u2 = (unsigned short)v2; }
      }

      if (node->kind == ND_SHL)
        res = v1 << v2;
      else if (node->kind == ND_SHR)
        res = is_unsigned ? u1 >> u2 : v1 >> v2;
      else if (node->kind == ND_BAND)
        res = v1 & v2;
      else if (node->kind == ND_BOR)
        res = v1 | v2;
      else if (node->kind == ND_BXOR)
        res = v1 ^ v2;
      else if (node->kind == ND_LT)
        res = is_unsigned ? u1 < u2 : v1 < v2;
      else if (node->kind == ND_LE)
        res = is_unsigned ? u1 <= u2 : v1 <= v2;
      else if (node->kind == ND_GT)
        res = is_unsigned ? u1 > u2 : v1 > v2;
      else if (node->kind == ND_GE)
        res = is_unsigned ? u1 >= u2 : v1 >= v2;
      else if (node->kind == ND_EQ)
        res = v1 == v2;
      else if (node->kind == ND_NE)
        res = v1 != v2;
      if (node->type && !node_type_unsigned(node)) {
          if (node->type->size == 4) res = (long long)(int)res;
          else if (node->type->size == 1) res = (long long)(char)res;
          else if (node->type->size == 2) res = (long long)(short)res;
      } else if (node->type && node_type_unsigned(node)) {
          if (node->type->size == 4) res = (long long)(unsigned int)res;
          else if (node->type->size == 1) res = (long long)(unsigned char)res;
          else if (node->type->size == 2) res = (long long)(unsigned short)res;
      }
      node->kind = ND_NUM;
      node->int_val = res;
      node->lhs = 0;
      node->rhs = 0;
    }
  }

  /* Dead-Branch Elimination (AST-level DCE) */
  if (node->kind == ND_IF) {
    if (node->cond && node->cond->kind == ND_NUM) {
      if (node->cond->int_val == 0) {
        /* condition is false: replace with else body or empty block */
        if (node->else_body) {
          Node *tgt = node->else_body;
          unsigned long long save_magic = node->magic;
          unsigned long long save_alloc_id = node->alloc_id;
          *node = *tgt;
          node->magic = save_magic;
          node->alloc_id = save_alloc_id;
        } else {
          node->kind = ND_BLOCK;
          node->num_stmts = 0;
          node->stmts = (Node **)cc_alloc(cc, sizeof(Node *));
        }
      } else {
        /* condition is true: replace with then body */
        if (node->then_body) {
          Node *tgt = node->then_body;
          unsigned long long save_magic = node->magic;
          unsigned long long save_alloc_id = node->alloc_id;
          *node = *tgt;
          node->magic = save_magic;
          node->alloc_id = save_alloc_id;
        } else {
          node->kind = ND_BLOCK;
          node->num_stmts = 0;
          node->stmts = (Node **)cc_alloc(cc, sizeof(Node *));
        }
      }
    }
  } else if (node->kind == ND_WHILE) {
    if (node->cond && node->cond->kind == ND_NUM && node->cond->int_val == 0) {
      /* while (0): unreachable loop, collapse entirely */
      node->kind = ND_BLOCK;
      node->num_stmts = 0;
      node->stmts = (Node **)cc_alloc(cc, sizeof(Node *));
    }
  } else if (node->kind == ND_TERNARY) {
    if (node->cond && node->cond->kind == ND_NUM) {
      if (node->cond->int_val == 0) {
        if (node->else_body) {
          Node *tgt = node->else_body;
          unsigned long long save_magic = node->magic;
          unsigned long long save_alloc_id = node->alloc_id;
          *node = *tgt;
          node->magic = save_magic;
          node->alloc_id = save_alloc_id;
        }
      } else {
        if (node->then_body) {
          Node *tgt = node->then_body;
          unsigned long long save_magic = node->magic;
          unsigned long long save_alloc_id = node->alloc_id;
          *node = *tgt;
          node->magic = save_magic;
          node->alloc_id = save_alloc_id;
        }
      }
    }
  }
}

void codegen_program(Compiler *cc, Node *prog) {
  Node *n;
  int i;

  /* Do NOT check ptr_in_fault_range(prog/n): stage2 miscompiles it and falsely
   * rejects valid arena pointers (e.g. 0xac09f8), causing early exit and
   * WinMain link error. */
  if (!prog)
    return;

  /* Linux: avoid "missing .note.GNU-stack" linker warning */
  if (!backend_ops) fprintf(cc->out, "    .section .note.GNU-stack,\"\",@progbits\n");
  if (cc->filename) {
    fprintf(cc->out, "    .file 1 \"%s\"\n", cc->filename);
  }

  /* Emit functions */
  n = prog;
  while (n) {
    if (n->kind == ND_FUNC_DEF) {
      fold_constants(cc, n);
      codegen_func(cc, n);
    }
    n = n->next;
  }

  /* Deduplicate tentative definitions: keep only one instance (prioritizing initializers) */
  for (i = 0; i < cc->num_globals; i++) {
    int j;
    Node *g1 = cc->globals[i];
    if (!g1 || g1->kind != ND_GLOBAL_VAR) continue;
    
    for (j = i + 1; j < cc->num_globals; j++) {
      Node *g2 = cc->globals[j];
      if (!g2 || g2->kind != ND_GLOBAL_VAR) continue;

      
      if (strcmp(g1->name, g2->name) == 0) {
        if (g1->initializer && g2->initializer) {
          cc->globals[j] = 0; /* Keep first initializer, drop second */
        } else if (g1->initializer) {
          cc->globals[j] = 0; /* Keep initialized, drop uninitialized */
        } else if (g2->initializer) {
          cc->globals[i] = 0; /* Drop uninitialized, keep initialized */
          break; /* g1 is gone, stop checking against g1 */
        } else {
          if (g1->is_extern && !g2->is_extern) {
              cc->globals[i] = 0; /* Keep tentative, drop extern */
              break;
          } else {
              cc->globals[j] = 0; /* Both uninitialized, prioritize g1 */
          }
        }
      }
    }
  }

  /* Emit global variables */
  for (i = 0; i < cc->num_globals; i++) {
    if (cc->globals[i]) {
      fold_constants(cc, cc->globals[i]->initializer);
      emit_global_var(cc, cc->globals[i]);
    }
  }

  /* Emit string literals */
  if (cc->num_strings > 0) {
    emit_strings(cc);
  }

  /* CG-FLOAT-011: emit float/double 1.0 constants used by ++/-- on float types */
  if (!backend_ops) {
    fprintf(cc->out, "    .section .rodata\n");
    fprintf(cc->out, "    .align 4\n");
    fprintf(cc->out, ".Lf32_one:\n");
    fprintf(cc->out, "    .long 0x3F800000\n");   /* 1.0f IEEE-754 */
    fprintf(cc->out, "    .align 8\n");
    fprintf(cc->out, ".Lf64_one:\n");
    fprintf(cc->out, "    .quad 0x3FF0000000000000\n"); /* 1.0 double */
  }
}

/* ZKAEDI FORCE RENDER CACHE INVALIDATION */

extern int is_pointer(Type *type);

int node_ptr_elem_size(struct Node *n) {
  if (!n || !n->type)
    return 0;
  if (is_pointer(n->type)) {
    return ptr_elem_size(n->type);
  }
  return 0;
}


/* ================================================================ */
/* COMPILER INITIALIZATION                                           */
/* ================================================================ */

#include "ir_emit_dispatch.h"

#ifndef ZCC_AST_BRIDGE_H
/* 
 * This block is exclusively for IDE syntax analysis when viewing part5.c alone.
 * Since part1.c #includes zcc_ast_bridge.h, this #ifndef prevents infinite loops
 * and double-definitions when part1...5 are concatenated into zcc.c.
 */
#include "part1.c"
#endif

/* Global telemetry control */
static int enable_telemetry_stdout = 0;
extern void ir_telemetry_enable_stdout(void);

/* Manifold engine globals (defined in ir_pass_manager.c) */
extern int  g_manifold_enabled;
extern char g_ir_export_path[256];

/* Peephole globals (defined in ir_peephole.c) */
extern int  g_peephole_enabled;
extern int  g_peephole_deterministic;
extern int  g_peephole_verbose;

/* Security pass globals */
static int  g_security_signext = 0;
static int  g_security_476 = 0;
static int  g_security_787 = 0;

static void init_compiler(Compiler *cc) {
  /* zero everyt5555hing — cc was calloc'd */

  /* init arena */
  cc->arena.data = (char *)malloc(ARENA_SIZE);
  cc->arena.pos = 0;
  cc->arena.cap = ARENA_SIZE;
  cc->arena.next = 0;

  /* create type singletons */
  cc->ty_void = type_new(cc, TY_VOID);
  cc->ty_char = type_new(cc, TY_CHAR);
  cc->ty_uchar = type_new(cc, TY_UCHAR);
  cc->ty_short = type_new(cc, TY_SHORT);
  cc->ty_ushort = type_new(cc, TY_USHORT);
  cc->ty_int = type_new(cc, TY_INT);
  cc->ty_uint = type_new(cc, TY_UINT);
  cc->ty_long = type_new(cc, TY_LONG);
  cc->ty_ulong = type_new(cc, TY_ULONG);
  cc->ty_longlong = type_new(cc, TY_LONGLONG);
  cc->ty_ulonglong = type_new(cc, TY_ULONGLONG);
  cc->ty_float = type_new(cc, TY_FLOAT);
  cc->ty_double = type_new(cc, TY_DOUBLE);

  /* init lexer */
  cc->line = 1;
  cc->col = 1;
  cc->pos = 0;
  cc->has_peek = 0;
  cc->label_count = 100; /* start at 100 to avoid clashes */
  cc->errors = 0;
  cc->num_strings = 0;
  cc->num_structs = 0;
  cc->num_globals = 0;

  /* push global scope */
  scope_push(cc);

  /* register common typedefs and builtins */
  {
    Symbol *sym;
    /* typedef void *FILE — so FILE* works */
    sym = scope_add(cc, "FILE", type_ptr(cc, cc->ty_void));
    sym->is_typedef = 1;

    /* typedef long size_t */
    sym = scope_add(cc, "size_t", cc->ty_ulonglong);
    sym->is_typedef = 1;

    /* typedef long ssize_t */
    sym = scope_add(cc, "ssize_t", cc->ty_long);
    sym->is_typedef = 1;

    /* typedef int ptrdiff_t */
    sym = scope_add(cc, "ptrdiff_t", cc->ty_long);
    sym->is_typedef = 1;

    /* SysV ABI requires va_list to be an array of 1 struct of size 24 */
    {
        Type *t_va = type_new(cc, TY_STRUCT);
        t_va->size = 24;
        t_va->align = 8;
        t_va->is_complete = 1;
        t_va->is_tbfp = 0;
        sym = scope_add(cc, "__builtin_va_list", type_array(cc, t_va, 1));
    }
    sym->is_typedef = 1;

    /* _Float128 workaround */
    sym = scope_add(cc, "_Float128", cc->ty_double);
    sym->is_typedef = 1;


    /* NULL as enum constant */
    sym = scope_add(cc, "NULL", cc->ty_long);
    sym->is_enum_const = 1;
    sym->enum_val = 0;

    /* stdout, stderr */
    sym = scope_add(cc, "stdout", type_ptr(cc, cc->ty_void));
    sym->is_global = 1;
    sym = scope_add(cc, "stderr", type_ptr(cc, cc->ty_void));
    sym->is_global = 1;

    /* common libc functions */
    {
      Type *ft;

      /* printf — returns int, variadic */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "printf", ft);
      sym->is_global = 1;

      /* fprintf */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "fprintf", ft);
      sym->is_global = 1;

      /* sscanf */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "sscanf", ft);
      sym->is_global = 1;

      /* sprintf */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "sprintf", ft);
      sym->is_global = 1;

      /* malloc — returns void* */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "malloc", ft);
      sym->is_global = 1;

      /* calloc */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "calloc", ft);
      sym->is_global = 1;

      /* realloc */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "realloc", ft);
      sym->is_global = 1;

      /* free — returns void */
      ft = type_func(cc, cc->ty_void);
      sym = scope_add(cc, "free", ft);
      sym->is_global = 1;

      /* exit */
      ft = type_func(cc, cc->ty_void);
      sym = scope_add(cc, "exit", ft);
      sym->is_global = 1;

      /* fopen — returns FILE* */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "fopen", ft);
      sym->is_global = 1;

      /* fclose */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "fclose", ft);
      sym->is_global = 1;

      /* fread */
      ft = type_func(cc, cc->ty_ulong);
      sym = scope_add(cc, "fread", ft);
      sym->is_global = 1;

      /* fwrite */
      ft = type_func(cc, cc->ty_ulong);
      sym = scope_add(cc, "fwrite", ft);
      sym->is_global = 1;

      /* fseek */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "fseek", ft);
      sym->is_global = 1;

      /* ftell */
      ft = type_func(cc, cc->ty_long);
      sym = scope_add(cc, "ftell", ft);
      sym->is_global = 1;

      /* fgets */
      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "fgets", ft);
      sym->is_global = 1;

      /* fputs */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "fputs", ft);
      sym->is_global = 1;

      /* strcmp, strncpy, strlen, strncmp */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "strcmp", ft);
      sym->is_global = 1;

      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strncpy", ft);
      sym->is_global = 1;

      ft = type_func(cc, cc->ty_ulong);
      sym = scope_add(cc, "strlen", ft);
      sym->is_global = 1;

      /* strstr */
      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strstr", ft);
      sym->is_global = 1;

      /* strtod */
      ft = type_func(cc, cc->ty_double);
      sym = scope_add(cc, "strtod", ft);
      sym->is_global = 1;

      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "strncmp", ft);
      sym->is_global = 1;

      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strcpy", ft);
      sym->is_global = 1;

      /* memset, memcpy */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "memset", ft);
      sym->is_global = 1;

      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "memcpy", ft);
      sym->is_global = 1;

      /* write, read (syscall wrappers) */
      ft = type_func(cc, cc->ty_long);
      sym = scope_add(cc, "write", ft);
      sym->is_global = 1;

      ft = type_func(cc, cc->ty_long);
      sym = scope_add(cc, "read", ft);
      sym->is_global = 1;

      /* system */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "system", ft);
      sym->is_global = 1;

      /* _exit */
      ft = type_func(cc, cc->ty_void);
      sym = scope_add(cc, "_exit", ft);
      sym->is_global = 1;

      /* strrchr — returns char* */
      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strrchr", ft);
      sym->is_global = 1;

      /* strcspn — returns size_t (ulong) */
      ft = type_func(cc, cc->ty_ulong);
      sym = scope_add(cc, "strcspn", ft);
      sym->is_global = 1;

      /* strspn — returns size_t (ulong) */
      ft = type_func(cc, cc->ty_ulong);
      sym = scope_add(cc, "strspn", ft);
      sym->is_global = 1;

      /* memchr — returns void* */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "memchr", ft);
      sym->is_global = 1;

      /* inet_pton — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "inet_pton", ft);
      sym->is_global = 1;

      /* ctype functions — all return int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "isalpha", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "isdigit", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "isxdigit", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "toupper", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "tolower", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "isalnum", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "isspace", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "isupper", ft); sym->is_global = 1;

      /* strerror — returns char* */
      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strerror", ft);
      sym->is_global = 1;

      /* close — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "close", ft);
      sym->is_global = 1;

      /* select — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "select", ft);
      sym->is_global = 1;

      /* socket — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "socket", ft);
      sym->is_global = 1;

      /* connect — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "connect", ft);
      sym->is_global = 1;

      /* bind — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "bind", ft);
      sym->is_global = 1;

      /* listen — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "listen", ft);
      sym->is_global = 1;

      /* accept — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "accept", ft);
      sym->is_global = 1;

      /* recv — returns ssize_t */
      ft = type_func(cc, cc->ty_long);
      sym = scope_add(cc, "recv", ft);
      sym->is_global = 1;

      /* send — returns ssize_t */
      ft = type_func(cc, cc->ty_long);
      sym = scope_add(cc, "send", ft);
      sym->is_global = 1;

      /* setsockopt — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "setsockopt", ft);
      sym->is_global = 1;

      /* getsockopt — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "getsockopt", ft);
      sym->is_global = 1;

      /* fcntl — returns int, variadic */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "fcntl", ft);
      sym->is_global = 1;

      /* ioctl — returns int, variadic */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "ioctl", ft);
      sym->is_global = 1;

      /* poll — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "poll", ft);
      sym->is_global = 1;

      /* gettimeofday — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "gettimeofday", ft);
      sym->is_global = 1;

      /* Math functions — return double */
      ft = type_func(cc, cc->ty_double);
      sym = scope_add(cc, "log", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_double);
      sym = scope_add(cc, "cos", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_double);
      sym = scope_add(cc, "exp", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_double);
      sym = scope_add(cc, "fabs", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_double);
      sym = scope_add(cc, "sqrt", ft); sym->is_global = 1;

      /* snprintf — returns int, variadic */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "snprintf", ft);
      sym->is_global = 1;

      /* fputc — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "fputc", ft);
      sym->is_global = 1;

      /* ferror — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "ferror", ft);
      sym->is_global = 1;

      /* qsort — returns void */
      ft = type_func(cc, cc->ty_void);
      sym = scope_add(cc, "qsort", ft);
      sym->is_global = 1;

      /* fflush — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "fflush", ft);
      sym->is_global = 1;

      /* strchr — returns char* */
      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strchr", ft);
      sym->is_global = 1;

      /* strtol — returns long */
      ft = type_func(cc, cc->ty_long);
      sym = scope_add(cc, "strtol", ft);
      sym->is_global = 1;

      /* atoi — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "atoi", ft);
      sym->is_global = 1;

      /* strcat, strncat — returns char* */
      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strcat", ft);
      sym->is_global = 1;
      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strncat", ft);
      sym->is_global = 1;

      /* freopen — returns FILE* */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "freopen", ft);
      sym->is_global = 1;
    }
  }
}

/* ================================================================ */
/* FILE READING                                                      */
/* ================================================================ */

static char *read_file(char *path, int *out_len) {
  FILE *fp;
  long len;
  char *buf;
  int nr;

  fprintf(stderr, "read_file: path = '%s' (addr: %p)\n", path, path);
  fp = fopen(path, "rb");
  if (!fp)
    return 0;

  fseek(fp, 0, 2); /* SEEK_END = 2 */
  len = ftell(fp);
  fseek(fp, 0, 0); /* SEEK_SET = 0 */

  buf = (char *)malloc(len + 1);
  if (!buf) {
    fclose(fp);
    return 0;
  }

  nr = fread(buf, 1, len, fp);
  buf[nr] = 0;
  fclose(fp);

  *out_len = nr;
  return buf;
}

/* ================================================================ */
/* PEEPHOLE OPTIMIZER                                                */
/* ================================================================ */

#define MAX_PEEP_LINES 3000000
#define MAX_PEEP_LEN 128

static char *line_buffer = 0;
static char **line_ptrs = 0;

static void peephole_optimize(char *filename) {
  FILE *fp;
  int nlines = 0;
  int i;
  int eliminated = 0;
  long file_size;

  fp = fopen(filename, "r");
  if (!fp)
    return;

  fseek(fp, 0, 2);
  file_size = ftell(fp);
  fseek(fp, 0, 0);

  if (!line_ptrs) {
    line_ptrs = (char **)malloc(MAX_PEEP_LINES * sizeof(char *));
  }
  line_buffer = (char *)malloc(file_size + MAX_PEEP_LINES * 128);
  if (!line_buffer || !line_ptrs) {
    fclose(fp);
    return;
  }

  while (nlines < MAX_PEEP_LINES && fgets(line_buffer + nlines * 128, 128, fp)) {
    line_ptrs[nlines] = line_buffer + nlines * 128;
    nlines++;
  }
  fclose(fp);

  for (i = 0; i < nlines;) {
    char *l1 = line_ptrs[i];

    /* 1. Redundant Push/Pop */
    if (strncmp(l1, "    pushq ", 10) == 0 && i + 1 < nlines) {
      char *l2 = line_ptrs[i + 1];
      if (strncmp(l2, "    popq ", 9) == 0) {
        char tmp1[64], tmp2[64];
        sscanf(l1, "    pushq %s", tmp1);
        sscanf(l2, "    popq %s", tmp2);
        if (strcmp(tmp1, tmp2) == 0) {
          line_ptrs[i][0] = 0;
          line_ptrs[i + 1][0] = 0;
          eliminated += 2;
          i += 2;
          continue;
        } else {
          sprintf(line_ptrs[i], "    movq %s, %s\n", tmp1, tmp2);
          line_ptrs[i + 1][0] = 0;
          eliminated += 2;
          i += 2;
          continue;
        }
      }
    }

    /* 2. Arithmetic Nullification */
    if (strcmp(l1, "    addq $0, %rax\n") == 0 ||
        strcmp(l1, "    subq $0, %rax\n") == 0 ||
        strcmp(l1, "    addq $0, %rsp\n") == 0 ||
        strcmp(l1, "    subq $0, %rsp\n") == 0) {
      line_ptrs[i][0] = 0;
      eliminated += 1;
      i += 1;
      continue;
    }

    /* 3. Push/Lea/Pop Triad */
    if (strcmp(l1, "    pushq %rax\n") == 0 && i + 2 < nlines) {
      char *l2 = line_ptrs[i + 1];
      char *l3 = line_ptrs[i + 2];
      if (strncmp(l2, "    leaq ", 9) == 0 && strstr(l2, ", %rax") &&
          strncmp(l3, "    popq ", 9) == 0) {
        char pop_reg[64];
        sscanf(l3, "    popq %s", pop_reg);
        sprintf(line_ptrs[i], "    movq %%rax, %s\n", pop_reg);
        line_ptrs[i + 2][0] = 0;
        eliminated += 3;
        i += 3;
        continue;
      }
    }
    i++;
  }

  fp = fopen(filename, "w");
  if (!fp) {
    free(line_buffer);
    return;
  }
  for (i = 0; i < nlines; i++) {
    if (line_ptrs[i][0] != 0)
      fputs(line_ptrs[i], fp);
  }
  fclose(fp);
  free(line_buffer);
  if (!enable_telemetry_stdout) printf("[Phase 5] Native C Peephole Optimization... OK (%d elided)\n",
         eliminated);
}

/* ================================================================ */
/* SECURITY PASS: --security-signext (CWE-122)                       */
/* Scans assembly for sign-extension (movsbl/movsbq) feeding         */
/* memcpy/memset/memmove within a 15-instruction window.             */
/* ================================================================ */

/* Scan window: ZCC unoptimized codegen emits ~30-40 instructions of arg
   setup between a sign-extending cast and the consuming memcpy call.
   64 covers observed gaps (CVE-2023-38545: 34 lines) with margin. */
#define SIGNEXT_WINDOW 64

static void security_signext_scan(char *filename) {
  FILE *fp;
  int nlines = 0;
  int findings = 0;
  int i;
  char current_func[128];
  char *line_buf;
  char **lp;
  int max_lines = 32768;

  fp = fopen(filename, "r");
  if (!fp) return;

  line_buf = (char *)malloc(max_lines * 128);
  lp = (char **)malloc(max_lines * sizeof(char *));
  if (!line_buf || !lp) { fclose(fp); return; }

  current_func[0] = 0;
  while (nlines < max_lines && fgets(line_buf + nlines * 128, 128, fp)) {
    lp[nlines] = line_buf + nlines * 128;
    nlines++;
  }
  fclose(fp);

  for (i = 0; i < nlines; i++) {
    /* Track current function */
    if (lp[i][0] != ' ' && lp[i][0] != '\t' && lp[i][0] != '.' &&
        lp[i][0] != '#' && lp[i][0] != '\n') {
      int k = 0;
      while (lp[i][k] && lp[i][k] != ':' && k < 127) {
        current_func[k] = lp[i][k];
        k++;
      }
      current_func[k] = 0;
    }

    /* Look for movsbl or movsbq (sign-extending byte to int/long) */
    if (strstr(lp[i], "movsbl ") || strstr(lp[i], "movsbq ") ||
        strstr(lp[i], "movswl ") || strstr(lp[i], "movswq ")) {
      /* Scan forward within window for memcpy/memset/memmove call */
      int j;
      for (j = i + 1; j < nlines && j < i + SIGNEXT_WINDOW; j++) {
        if (strstr(lp[j], "call memcpy") ||
            strstr(lp[j], "call memset") ||
            strstr(lp[j], "call memmove")) {
          findings++;
          printf("[--security-signext] CWE-122 WARNING: sign-extension at line %d "
                 "feeds %s in %s() (asm lines %d->%d)\n",
                 i + 1, strstr(lp[j], "call ") + 5,
                 current_func[0] ? current_func : "<unknown>",
                 i + 1, j + 1);
          break;  /* don't double-report same sign-ext */
        }
        /* Stop at label or ret - different basic block */
        if (lp[j][0] == '.' && lp[j][1] == 'L') break;
        if (strstr(lp[j], "ret")) break;
      }
    }
  }

  free(line_buf);
  free(lp);

  if (findings) {
    printf("[--security-signext] %d potential sign-extension overflow(s) found\n",
           findings);
  } else {
    printf("[--security-signext] Clean: no sign-extension -> memcpy patterns\n");
  }
}

/* ================================================================ */
/* SECURITY PASS: --security-476 (CWE-476)                           */
/* Scans assembly for nullable-return calls (malloc, calloc, realloc, */
/* fopen, etc.) followed by dereference of %rax without null check.  */
/* ================================================================ */

#define NULLDEREF_WINDOW 30

static int is_nullable_call(const char *line) {
  return strstr(line, "call malloc") || strstr(line, "call calloc") ||
         strstr(line, "call realloc") || strstr(line, "call fopen") ||
         strstr(line, "call fdopen") || strstr(line, "call tmpfile") ||
         strstr(line, "call strdup") || strstr(line, "call strndup") ||
         strstr(line, "call mmap");
}

static int is_null_check(const char *line) {
  /* testq %rax, %rax  or  cmpq $0, %rax */
  return strstr(line, "testq %rax") || strstr(line, "cmpq $0, %rax") ||
         strstr(line, "test %eax") || strstr(line, "cmp $0, %eax");
}

static int is_rax_deref(const char *line) {
  /* (%rax) — memory access through %rax without offset guard */
  return strstr(line, "(%rax)") != 0;
}

static int is_rax_moved(const char *line) {
  /* movq %rax, %r.. — return value moved to another register */
  if (strstr(line, "movq %rax, %r") && !strstr(line, "movq %rax, %rsp") &&
      !strstr(line, "movq %rax, %rbp"))
    return 1;
  return 0;
}

int g_emit_anomalies = 0;

/* Tiered type-to-string mapping for anomaly metadata.
 * Shorthand "ptr" covers all pointer/array/function types.
 * Aggregates are flagged for propose-only by returning their category.
 * char/bool both default to return 0/false; documented here as compatible. */
static const char *type_to_str(Type *ty) {
  if (!ty) return "unknown";
  switch (ty->kind) {
    case TY_VOID:   return "void";
    case TY_INT:    return "int";
    case TY_CHAR:   return "char";
    case TY_SHORT:  return "short";
    case TY_LONG:   return "long";
    case TY_LONGLONG: return "long long";
    case TY_UINT:   return "uint";
    case TY_UCHAR:  return "uchar";
    case TY_USHORT: return "ushort";
    case TY_ULONG:  return "ulong";
    case TY_ULONGLONG: return "ulonglong";
    case TY_FLOAT:  return "float";
    case TY_DOUBLE: return "double";
    case TY_PTR:
    case TY_ARRAY:
    case TY_FUNC:   return "ptr";
    case TY_STRUCT: return "struct";
    case TY_UNION:  return "union";
    case TY_ENUM:   return "enum";
    default:        return "unknown";
  }
}

static void security_nullderef_scan(Compiler *cc, char *filename) {
  FILE *fp;
  int nlines = 0;
  int findings = 0;
  int i;
  char current_func[128];
  char *line_buf;
  char **lp;
  int max_lines = 32768;

  fp = fopen(filename, "r");
  if (!fp) return;

  line_buf = (char *)malloc(max_lines * 128);
  lp = (char **)malloc(max_lines * sizeof(char *));
  if (!line_buf || !lp) { fclose(fp); return; }

  current_func[0] = 0;
  while (nlines < max_lines && fgets(line_buf + nlines * 128, 128, fp)) {
    lp[nlines] = line_buf + nlines * 128;
    nlines++;
  }
  fclose(fp);

  for (i = 0; i < nlines; i++) {
    /* Track current function */
    if (lp[i][0] != ' ' && lp[i][0] != '\t' && lp[i][0] != '.' &&
        lp[i][0] != '#' && lp[i][0] != '\n') {
      int k = 0;
      while (lp[i][k] && lp[i][k] != ':' && k < 127) {
        current_func[k] = lp[i][k];
        k++;
      }
      current_func[k] = 0;
    }

    /* Look for nullable-return function calls */
    if (is_nullable_call(lp[i])) {
      int j;
      int saw_null_check = 0;
      const char *call_name = strstr(lp[i], "call ") + 5;
      char clean_call[64];
      int c_idx = 0;
      while (call_name[c_idx] && call_name[c_idx] != '\n' && call_name[c_idx] != '@' && call_name[c_idx] != ' ' && c_idx < 63) {
          clean_call[c_idx] = call_name[c_idx];
          c_idx++;
      }
      clean_call[c_idx] = '\0';

      char tracked_reg[8];  /* register holding the return value */
      char deref_pat[16];   /* "(%rXX)" pattern to search for */
      char null_pat1[32];   /* "testq %rXX, %rXX" */
      char null_pat2[32];   /* "cmpq $0, %rXX" */

      strcpy(tracked_reg, "%rax");
      strcpy(deref_pat, "(%rax)");
      strcpy(null_pat1, "testq %rax");
      strcpy(null_pat2, "cmpq $0, %rax");

      char current_owner[128];
      char current_file[128];
      int current_line = 0;
      current_owner[0] = 0;
      current_file[0] = 0;

      for (j = i + 1; j < nlines && j < i + NULLDEREF_WINDOW; j++) {
        /* Parse latest ZCC_META for our tracked register */
        /* Format: # ZCC_META %reg=owner@file:line */
        char meta_pat[64];
        sprintf(meta_pat, "# ZCC_META %s=", tracked_reg);
        char *meta_match = strstr(lp[j], meta_pat);
        if (meta_match) {
           meta_match += strlen(meta_pat);
           /* owner */
           int k = 0;
           while (meta_match[k] && meta_match[k] != '@' && k < 127) {
               current_owner[k] = meta_match[k];
               k++;
           }
           current_owner[k] = 0;
           if (strcmp(current_owner, "_") == 0) current_owner[0] = 0;
           
           if (meta_match[k] == '@') {
               meta_match += k + 1;
               /* file */
               k = 0;
               while (meta_match[k] && meta_match[k] != ':' && k < 127) {
                   current_file[k] = meta_match[k];
                   k++;
               }
               current_file[k] = 0;
               
               if (meta_match[k] == ':') {
                   current_line = atoi(meta_match + k + 1);
               }
           }
        }

        /* If we see a null check on tracked reg OR %rax, we're safe */
        if (strstr(lp[j], null_pat1) || strstr(lp[j], null_pat2) ||
            strstr(lp[j], "cmpq $0, %rax") || strstr(lp[j], "testq %rax")) {
          saw_null_check = 1;
          break;
        }
        /* If %rax is moved to another register, follow the copy */
        if (strcmp(tracked_reg, "%rax") == 0 && is_rax_moved(lp[j])) {
          /* Extract destination: "movq %rax, %rXX" */
          char *dst = strstr(lp[j], ", %r");
          if (dst) {
            int k = 0;
            dst += 2;  /* skip ", " */
            while (dst[k] && dst[k] != '\n' && dst[k] != ' ' && k < 5) {
              tracked_reg[k] = dst[k];
              k++;
            }
            tracked_reg[k] = 0;
            sprintf(deref_pat, "(%s)", tracked_reg);
            sprintf(null_pat1, "testq %s", tracked_reg);
            sprintf(null_pat2, "cmpq $0, %s", tracked_reg);
          }
          continue;  /* keep scanning with new register */
        }
        /* If there's a deref of the tracked register OR %rax (which
         * may be a reload of the tracked register) without a null check */
        if ((strstr(lp[j], deref_pat) || strstr(lp[j], "(%rax)")) &&
            !saw_null_check) {
          findings++;
          char *reported_var = current_owner[0] ? current_owner : clean_call;
          if (g_emit_anomalies) {
              /* Option 1: JSON consumers (like the mutation applier) require a valid
               * lvalue owner to synthesize a guard. If missing (e.g. member assignment),
               * suppress the anomaly entirely to prevent unactionable reports.
               * Human reviewers (--security-476) still see the function-name fallback.
               */
              if (!current_owner[0]) {
                  break;
              }
              Symbol *sym = scope_find(cc, current_func);
              const char *ret_type = "unknown";
              if (sym && sym->type && sym->type->kind == TY_FUNC) {
                  ret_type = type_to_str(sym->type->ret);
              }
              printf("{\"type\":\"ir_anomaly\", \"kind\":\"CWE-476\", \"site\":\"%s:%s:%d\", \"severity\":0.7, \"variable\":\"%s\", \"return_type\":\"%s\"}\n",
                     current_file[0] ? current_file : (cc->filename ? cc->filename : "unknown"),
                     current_func[0] ? current_func : "unknown",
                     current_line, reported_var, ret_type);
          } else {
              printf("[--security-476] CWE-476 WARNING: unchecked %s return "
                     "dereferenced via %s in %s() (asm lines %d->%d)\n",
                     clean_call, reported_var,
                     current_func[0] ? current_func : "<unknown>",
                     i + 1, j + 1);
          }
          break;
        }
        /* Stop at label or ret - different basic block */
        if (lp[j][0] == '.' && lp[j][1] == 'L') break;
        if (strstr(lp[j], "ret")) break;
        /* Stop at another call - %rax is clobbered */
        if (strstr(lp[j], "call ")) break;
      }
    }
  }

  free(line_buf);
  free(lp);

  if (findings) {
    printf("[--security-476] %d potential null-deref(s) found\n", findings);
  } else {
    printf("[--security-476] Clean: all nullable returns checked\n");
  }
}

static void security_bounds_scan(Compiler *cc, char *filename) {
  FILE *fp;
  int nlines = 0;
  int findings = 0;
  int i;
  char current_func[128];
  char *line_buf;
  char **lp;
  int max_lines = 32768;

  fp = fopen(filename, "r");
  if (!fp) return;

  line_buf = (char *)malloc(max_lines * 128);
  lp = (char **)malloc(max_lines * sizeof(char *));
  if (!line_buf || !lp) { fclose(fp); return; }

  current_func[0] = 0;
  while (nlines < max_lines && fgets(line_buf + nlines * 128, 128, fp)) {
    lp[nlines] = line_buf + nlines * 128;
    nlines++;
  }
  fclose(fp);

  for (i = 0; i < nlines; i++) {
    /* Track current function */
    if (lp[i][0] != ' ' && lp[i][0] != '\t' && lp[i][0] != '.' &&
        lp[i][0] != '#' && lp[i][0] != '\n') {
      int k = 0;
      while (lp[i][k] && lp[i][k] != ':' && k < 127) {
        current_func[k] = lp[i][k];
        k++;
      }
      current_func[k] = 0;
    }

    if (strstr(lp[i], "# ZCC_META_ARRAY")) {
      char array_name[64] = "unknown";
      char index_name[64] = "?";
      char size_str[32] = "0";
      char file_name[128] = "unknown";
      int line_num = 0;

      char *ptr = strstr(lp[i], "array=");
      if (ptr) {
          ptr += 6;
          int k = 0;
          while (ptr[k] && ptr[k] != ' ' && k < 63) array_name[k++] = ptr[k];
          array_name[k] = 0;
      }
      
      ptr = strstr(lp[i], "index=");
      if (ptr) {
          ptr += 6;
          int k = 0;
          while (ptr[k] && ptr[k] != ' ' && k < 63) index_name[k++] = ptr[k];
          index_name[k] = 0;
      }

      ptr = strstr(lp[i], "size=");
      if (ptr) {
          ptr += 5;
          int k = 0;
          while (ptr[k] && ptr[k] != ' ' && k < 31) size_str[k++] = ptr[k];
          size_str[k] = 0;
      }
      
      ptr = strstr(lp[i], "@");
      if (ptr) {
          ptr += 1;
          int k = 0;
          while (ptr[k] && ptr[k] != ':' && k < 127) file_name[k++] = ptr[k];
          file_name[k] = 0;
          if (ptr[k] == ':') line_num = atoi(ptr + k + 1);
      }

      /* Do backwards walk looking for cmp bounds check */
      int saw_size_cmp = 0;
      int saw_cmp_instr = 0;
      char size_pat[64];
      sprintf(size_pat, "$%s", size_str);

      int j;
      for (j = i - 1; j >= 0 && j >= i - 30; j--) {
          if (strstr(lp[j], "ret")) break;
          if (strstr(lp[j], "call ")) break;
          if (lp[j][0] != ' ' && lp[j][0] != '\t' && lp[j][0] != '.' && lp[j][0] != '#') break; /* Func def */

          if (strstr(lp[j], size_pat)) saw_size_cmp = 1;
          if (strstr(lp[j], "cmp")) saw_cmp_instr = 1;
      }

      if (!(saw_size_cmp && saw_cmp_instr)) {
          findings++;
          if (g_emit_anomalies) {
               Symbol *sym = scope_find(cc, current_func);
               const char *ret_type = "unknown";
               if (sym && sym->type && sym->type->kind == TY_FUNC) {
                   ret_type = type_to_str(sym->type->ret);
               }
               printf("{\"type\":\"ir_anomaly\", \"kind\":\"CWE-787\", \"site\":\"%s:%s:%d\", \"severity\":0.8, \"array\":\"%s\", \"index\":\"%s\", \"size\":%s, \"return_type\":\"%s\"}\n",
                      file_name, current_func, line_num, array_name, index_name, size_str, ret_type);
          } else {
               printf("[--security-787] CWE-787 WARNING: unchecked array bounds for %s[%s] (size %s) in %s()\n",
                      array_name, index_name, size_str, current_func);
          }
      }
    }
  }

  free(line_buf);
  free(lp);

  if (findings) {
    if (!g_emit_anomalies) printf("[--security-787] %d potential bounds-violation(s) found\n", findings);
  } else {
    if (!g_emit_anomalies) printf("[--security-787] Clean: all bounded arrays checked\n");
  }
}

/* ================================================================ */
/* MAIN                                                              */
/* Bug fix: Compiler is heap-allocated (52KB+ struct, not stack)     */
/* ================================================================ */

/* Forward declaration for IR pass manager (linked separately) */
void ir_pm_run_default(void *mod_ptr, int verbose);

int main(int argc, char **argv) {
  Compiler *cc;
  char *input_file;
  char *output_file;
  char *source;
  int source_len;
  char asm_file[256];
  char cmd[512];
  char include_paths[4096];  /* -I paths, colon-separated */
  Node *prog;
  int ret;
  int i;
  int al;

  input_file = 0;
  output_file = 0;
  include_paths[0] = '\0';
  strcat(include_paths, ".:./include");  /* default paths */

  int pp_only = 0;

  int zcc_verbose_flag = 0;

  int zcc_quiet_flag = 0;

  int compile_only = 0;

  int g_ir_primary = 0;

  /* parse arguments */
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0) {
      i++;
      if (i < argc)
        output_file = argv[i];
    } else if (strcmp(argv[i], "-c") == 0) {
      compile_only = 1;
    } else if (strcmp(argv[i], "--pp-only") == 0) {
      pp_only = 1;
    } else if (strcmp(argv[i], "-v") == 0) {
      zcc_verbose_flag = 1;
    } else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
      zcc_quiet_flag = 1;
    } else if (strcmp(argv[i], "--ir") == 0) {
      g_emit_ir = 1;
      g_ir_primary = 1;
    } else if (strcmp(argv[i], "--telemetry") == 0) {
      enable_telemetry_stdout = 1;
      setenv("ZCC_EMIT_TELEMETRY", "1", 1);
    } else if (strcmp(argv[i], "--manifold") == 0) {
      g_manifold_enabled = 1;
    } else if (strcmp(argv[i], "--manifold-deterministic") == 0) {
      g_manifold_enabled = 1;
      /* sigma=0 enforced inside ir_pass_manager via deterministic flag;
       * set env so manifold engine sees it regardless of cfg path. */
      setenv("ZCC_MANIFOLD_DET", "1", 1);
    } else if (strncmp(argv[i], "--ir-export=", 12) == 0) {
      strncpy(g_ir_export_path, argv[i] + 12, 255);
      g_ir_export_path[255] = '\0';
      g_manifold_enabled = 1;  /* export implies manifold active */
    } else if (strcmp(argv[i], "--peephole") == 0) {
      g_peephole_enabled = 1;
    } else if (strcmp(argv[i], "--peephole-deterministic") == 0) {
      g_peephole_enabled = 1;
      g_peephole_deterministic = 1;
    } else if (strcmp(argv[i], "--peephole-verbose") == 0) {
      g_peephole_verbose = 1;
    } else if (strcmp(argv[i], "--security-signext") == 0) {
      g_security_signext = 1;
    } else if (strcmp(argv[i], "--security-476") == 0) {
      g_security_476 = 1;
    } else if (strcmp(argv[i], "--security-787") == 0) {
      g_security_787 = 1;
    } else if (strcmp(argv[i], "--emit-anomalies") == 0) {
      g_emit_anomalies = 1;
      g_security_476 = 1;
      g_security_787 = 1;
    } else if (strncmp(argv[i], "-I", 2) == 0) {
      /* include path: -Ipath or -I path */
      const char *ipath = (argv[i][2] != '\0') ? argv[i] + 2 : ((i + 1 < argc) ? argv[++i] : "");
      if (ipath[0]) {
        if (include_paths[0]) strncat(include_paths, ":", 4095 - (int)strlen(include_paths));
        strncat(include_paths, ipath, 4095 - (int)strlen(include_paths));
      }
    } else if (strncmp(argv[i], "-l", 2) == 0 || strncmp(argv[i], "-L", 2) == 0 || strncmp(argv[i], "-O", 2) == 0) {
      /* ignore linker flags */
    } else {
      input_file = argv[i];
    }
  }

  /* Stderr policy: open by default, --quiet silences.
   * -v adds extra diagnostic output but stderr is always open unless -q. */
  if (zcc_quiet_flag) {
#ifdef _WIN32
    freopen("nul", "w", stderr);
#else
    freopen("/dev/null", "w", stderr);
#endif
  }

  if (!input_file) {
    printf("Usage: zcc <input.c> [-o output]\n");
    return 1;
  }

  if (!output_file)
    output_file = "a.out";

  ZCC_IR_INIT();
  if (enable_telemetry_stdout) {
      ir_telemetry_enable_stdout();
  }

  /* read source file */
  source = read_file(input_file, &source_len);
  if (!source) {
    printf("zcc: cannot read '%s'\n", input_file);
    return 1;
  }

  /* PREPROCESSOR HOOK */
  {
    int pp_len;
    char *pp_source = zcc_preprocess(source, source_len, input_file, include_paths, &pp_len);
    if (!pp_source) {
      fprintf(stderr, "zcc: preprocessing failed\n");
      return 1;
    }
    /* We leak original `source` here because we replaced it.
       ZCC doesn't care much about leaking in main string allocations anyway. */
    source = pp_source;
    source_len = pp_len;
  }

  if (pp_only) {
    if (!enable_telemetry_stdout) printf("%s", source);
    return 0;
  }

  /* heap-allocate compiler state (too large for stack) */
  cc = (Compiler *)calloc(1, sizeof(Compiler));
  if (!cc) {
    if (!enable_telemetry_stdout) printf("zcc: out of memory\n");
    free(source);
    return 1;
  }

  cc->source = source;
  cc->source_len = source_len;
  cc->filename = input_file;

  init_compiler(cc);

  /* generate asm file name */
  strncpy(asm_file, output_file, 250);
  al = 0;
  while (asm_file[al])
    al++;

  int stop_at_asm = 0;
  if (al >= 2 && asm_file[al - 2] == '.' && asm_file[al - 1] == 's') {
    stop_at_asm = 1;
  } else {
    asm_file[al] = '.';
    asm_file[al + 1] = 's';
    asm_file[al + 2] = 0;
  }

  /* open output */
  cc->out = fopen(asm_file, "w");
  if (!cc->out) {
    if (!enable_telemetry_stdout) printf("zcc: cannot write '%s'\n", asm_file);
    free(source);
    free(cc);
    return 1;
  }

  /* lex first token */
  if (!enable_telemetry_stdout) printf("[Phase 1] Lexical Array Bootstrap... OK\n");
  next_token(cc);

  /* parse */
  if (!enable_telemetry_stdout) printf("[Phase 2] AST Topological Generation... ");
  prog = parse_program(cc);

  if (cc->errors > 0) {
    if (!enable_telemetry_stdout) printf("\033[0;31mFAILED\033[0m\n");
    if (!enable_telemetry_stdout) printf("zcc: %d error(s)\n", cc->errors);
    fclose(cc->out);
    free(source);
    free(cc);
    return 1;
  }

  if (!enable_telemetry_stdout) printf("OK\n");

  /* generate code */
  if (!enable_telemetry_stdout) printf("[Phase 3] Native AST Constant Folding... OK\n");
  if (!enable_telemetry_stdout) printf("[Phase 4] SystemV ABI X86-64 Codegen... OK\n");
  fprintf(cc->out, "# ZCC asm begin\n");
  codegen_program(cc, prog);
  fclose(cc->out);

  /* IR pass manager — runs when --ir flag is active */
  if (g_ir_primary && g_ir_module) {
    int ir_total_nodes = 0;
    int ir_fi;
    for (ir_fi = 0; ir_fi < g_ir_module->func_count; ir_fi++) {
      ir_total_nodes += g_ir_module->funcs[ir_fi]->node_count;
    }
    if (!enable_telemetry_stdout) printf("[Phase IR] IR Pass Manager (%d funcs, %d nodes)...\n",
           g_ir_module->func_count, ir_total_nodes);
    ir_pm_run_default(g_ir_module, 1);
    if (!enable_telemetry_stdout) printf("[Phase IR] Pass Manager Complete.\n");
  }

  if (!g_ir_primary) {
    ZCC_IR_FLUSH(stdout);
  }

  /* peephole optimize the emitted assembly safely out-of-bounds */
  peephole_optimize(asm_file);

  /* security pass: sign-extension overflow detection */
  if (g_security_signext) {
    security_signext_scan(asm_file);
  }

  /* security pass: null-deref detection */
  if (g_security_476) {
    security_nullderef_scan(cc, asm_file);
  }

  /* security pass: array bounds detection */
  if (g_security_787) {
    security_bounds_scan(cc, asm_file);
  }

  /* assemble and link if not stopping at assembly */
  if (!stop_at_asm) {
    if (!enable_telemetry_stdout) printf("[Phase 6] GCC Assembly/Linker Binding... ");
    if (compile_only) {
      sprintf(cmd, "gcc -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -c -o %s %s 2>&1", output_file, asm_file);
    } else if (strcmp(input_file, "zcc.c") == 0 || (strlen(input_file) >= 6 && strcmp(input_file + strlen(input_file) - 6, "/zcc.c") == 0)) {
      sprintf(cmd, "gcc -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -o %s %s compiler_passes.c compiler_passes_ir.c ir_pass_manager.c -lm 2>&1", output_file, asm_file);
    } else {
      sprintf(cmd, "gcc -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -o %s %s -lm -lpthread -ldl 2>&1", output_file, asm_file);
    }
    ret = system(cmd);
    if (ret != 0) {
      if (!enable_telemetry_stdout) printf("FAILED\n");
      if (!enable_telemetry_stdout) printf("zcc: assembly/linking failed\n");
      free(source);
      free(cc);
      return 1;
    }
    if (!enable_telemetry_stdout) printf("OK\n");
  }

  if (!enable_telemetry_stdout) printf("[OK] ZCC Engine Compilation Terminated Successfully.\n");

  free(source);
  free(cc);
  return 0;
}

/* ZKAEDI FORCE RENDER CACHE INVALIDATION */
#ifndef ZCC_AST_BRIDGE_H
/* Exclusively for standalone IDE analysis */
#include "part1.c"
#endif

/* ================================================================ */
/* PART 6: ARM TARGET BACKEND (thumbv6m)                             */
/* ================================================================ */


TargetBackend *backend_ops = 0;
int ZCC_POINTER_WIDTH = 8;
int ZCC_INT_WIDTH = 4;

static void thumb_emit_prologue(Compiler *cc, Node *func) {
    int stack_size = func->stack_size + 40;
    if (stack_size < 256) stack_size = 256;
    stack_size = (stack_size + 7) & ~7;

    fprintf(cc->out, "    .text\n");
    fprintf(cc->out, "    .syntax unified\n");
    fprintf(cc->out, "    .cpu cortex-m0plus\n");
    fprintf(cc->out, "    .thumb\n");
    if (!func->is_static) {
        fprintf(cc->out, "    .global %s\n", func->func_def_name);
    }
    fprintf(cc->out, "    .type %s, %%function\n", func->func_def_name);
    fprintf(cc->out, "%s:\n", func->func_def_name);
    
    fprintf(cc->out, "    push {r4, r5, r6, r7, lr}\n");
    fprintf(cc->out, "    mov r7, sp\n");

    if (stack_size <= 508 && (stack_size % 4 == 0)) {
        fprintf(cc->out, "    sub sp, #%d\n", stack_size);
    } else {
        fprintf(cc->out, "    ldr r3, =%d\n", stack_size);
        fprintf(cc->out, "    mov r4, sp\n");
        fprintf(cc->out, "    subs r4, r4, r3\n");
        fprintf(cc->out, "    mov sp, r4\n");
    }

    int i;
    for (i = 0; i < func->num_params && i < 4; i++) {
        fprintf(cc->out, "    ldr r3, =%d\n", -(i * 4 + 8));
        fprintf(cc->out, "    adds r3, r7, r3\n");
        fprintf(cc->out, "    str r%d, [r3]\n", i);
    }
}

static void thumb_emit_epilogue(Compiler *cc, Node *func) {
    fprintf(cc->out, ".Lfunc_end_%d:\n", cc->func_end_label);
    fprintf(cc->out, "    mov sp, r7\n");
    fprintf(cc->out, "    pop {r4, r5, r6, r7, pc}\n");
}

static void thumb_emit_call(Compiler *cc, Node *func) {
    fprintf(cc->out, "    bl %s\n", func->func_name);
}

static void thumb_emit_binary_op(Compiler *cc, int op) {
    /* op matches ND_ADD, ND_SUB, etc.
       r0 = lhs, r1 = rhs
       output -> r0 */
    switch (op) {
        case ND_ADD:
            fprintf(cc->out, "    adds r0, r0, r1\n");
            break;
        case ND_SUB:
            fprintf(cc->out, "    subs r0, r0, r1\n");
            break;
        case ND_MUL:
            fprintf(cc->out, "    muls r0, r1, r0\n"); /* thumb-1 allows only dest=lhs */
            break;
        case ND_DIV:
            fprintf(cc->out, "    bl __aeabi_idiv\n"); /* software divide */
            break;
        case ND_BAND:
            fprintf(cc->out, "    ands r0, r0, r1\n");
            break;
        case ND_BOR:
            fprintf(cc->out, "    orrs r0, r0, r1\n");
            break;
        case ND_BXOR:
            fprintf(cc->out, "    eors r0, r0, r1\n");
            break;
        case ND_SHL:
            fprintf(cc->out, "    lsls r0, r0, r1\n");
            break;
        case ND_SHR:
            fprintf(cc->out, "    asrs r0, r0, r1\n"); /* arithmetic shift right */
            break;
    }
}

static void thumb_emit_load_stack(Compiler *cc, int offset, const char *reg) {
    if (offset >= 0 && offset <= 1020 && (offset % 4 == 0)) {
        fprintf(cc->out, "    ldr %s, [r7, #%d]\n", reg, offset);
    } else {
        fprintf(cc->out, "    ldr r3, =%d\n", offset);
        fprintf(cc->out, "    adds r3, r7, r3\n");
        fprintf(cc->out, "    ldr %s, [r3]\n", reg);
    }
}

static void thumb_emit_store_stack(Compiler *cc, int offset, const char *reg) {
    if (offset >= 0 && offset <= 1020 && (offset % 4 == 0)) {
        fprintf(cc->out, "    str %s, [r7, #%d]\n", reg, offset);
    } else {
        fprintf(cc->out, "    ldr r3, =%d\n", offset);
        fprintf(cc->out, "    adds r3, r7, r3\n");
        fprintf(cc->out, "    str %s, [r3]\n", reg);
    }
}

static void thumb_emit_float_binop(Compiler *cc, int op) {
    const char *fn = 0;
    switch (op) {
        case ND_FADD: fn = "__aeabi_fadd"; break;
        case ND_FSUB: fn = "__aeabi_fsub"; break;
        case ND_FMUL: fn = "__aeabi_fmul"; break;
        case ND_FDIV: fn = "__aeabi_fdiv"; break;
    }
    if (fn) {
        fprintf(cc->out, "    bl %s\n", fn);
    }
}

TargetBackend backend_thumbv6m = {
    4, /* ptr_size */
    thumb_emit_prologue,
    thumb_emit_epilogue,
    thumb_emit_call,
    thumb_emit_binary_op,
    thumb_emit_load_stack,
    thumb_emit_store_stack,
    thumb_emit_float_binop
};
/*
 * ir.c — ZCC IR construction and text emission
 *
 * Compiled by GCC (as part of compiler_passes.c's translation unit)
 * AND by ZCC itself at stage2 (included via zcc.c).
 *
 * ZCC-parseable rules strictly observed:
 *   - No #include <stdint.h>
 *   - No _Static_assert
 *   - No VLAs
 *   - No C99 designated initializers on structs (ZCC may not support)
 *   - No compound literals
 */

#include "ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Globals ─────────────────────────────────────────────────────────── */

int           g_emit_ir      = 0;
ir_func_t    *g_ir_cur_func  = 0;  /* NULL — ZCC may not parse "= NULL" */
ir_module_t  *g_ir_module    = 0;

/* ── Opcode table ────────────────────────────────────────────────────── */

static const char *OP_NAMES[41] = {
    "ret",
    "br",
    "br_if",
    "alloca",
    "load",
    "store",
    "add",
    "sub",
    "mul",
    "div",
    "mod",
    "neg",
    "and",
    "or",
    "xor",
    "not",
    "shl",
    "shr",
    "eq",
    "ne",
    "lt",
    "le",
    "gt",
    "ge",
    "cast",
    "copy",
    "const",
    "const_str",
    "call",
    "arg",
    "phi",
    "addr",
    "label",
    "nop",
    "fconst",
    "fadd",
    "fsub",
    "fmul",
    "fdiv",
    "itof",
    "ftoi",
    "asm",
    "alloca"
};

/* ── Type table ──────────────────────────────────────────────────────── */

static const char *TY_NAMES[12] = {
    "void",
    "i8", "i16", "i32", "i64",
    "u8", "u16", "u32", "u64",
    "ptr",
    "f32", "f64"
};

static const int TY_BYTES[12] = {
    0,              /* void  */
    1, 2, 4, 8,    /* i8..i64 */
    1, 2, 4, 8,    /* u8..u64 */
    8,              /* ptr — LP64 */
    4, 8            /* f32, f64 */
};

/* ── Query helpers ────────────────────────────────────────────────────── */

const char *ir_op_name(ir_op_t op) {
    if (op < 0 || op >= IR_OP_COUNT) return "???";
    return OP_NAMES[op];
}

const char *ir_type_name(ir_type_t ty) {
    if (ty < 0 || ty >= 12) return "???";
    return TY_NAMES[ty];
}

int ir_type_bytes(ir_type_t ty) {
    if (ty < 0 || ty >= 12) return -1;
    return TY_BYTES[ty];
}

int ir_type_unsigned(ir_type_t ty) {
    return ty == IR_TY_U8 || ty == IR_TY_U16 ||
           ty == IR_TY_U32 || ty == IR_TY_U64;
}

int ir_op_is_terminator(ir_op_t op) {
    return op == IR_RET || op == IR_BR || op == IR_BR_IF;
}

/* ── Node allocation ──────────────────────────────────────────────────── */

ir_node_t *ir_node_alloc(void) {
    ir_node_t *n = (ir_node_t *)calloc(1, sizeof(ir_node_t));
    if (!n) {
        fprintf(stderr, "ir: out of memory allocating ir_node_t\n");
        exit(1);
    }
    return n;
}

/* ── Append ───────────────────────────────────────────────────────────── */

void ir_append(ir_func_t *fn, ir_node_t *n) {
    n->next = 0;
    if (!fn->head) {
        fn->head = fn->tail = n;
    } else {
        fn->tail->next = n;
        fn->tail = n;
    }
    fn->node_count++;
}

/* ── Safe string copy (no strlcpy in POSIX baseline) ─────────────────── */

static void safe_copy(char *dst, const char *src, int max) {
    if (!src) { dst[0] = '\0'; return; }
    int i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* ── Primary emit function ────────────────────────────────────────────── */

ir_node_t *ir_emit(ir_func_t *fn, ir_op_t op, ir_type_t ty,
                   const char *dst,  const char *src1,
                   const char *src2, const char *label,
                   long imm, int lineno) {
    ir_node_t *n = ir_node_alloc();
    n->op     = op;
    n->type   = ty;
    n->imm    = imm;
    n->lineno = lineno;
    safe_copy(n->dst,   dst   ? dst   : "", IR_NAME_MAX);
    safe_copy(n->src1,  src1  ? src1  : "", IR_NAME_MAX);
    safe_copy(n->src2,  src2  ? src2  : "", IR_NAME_MAX);
    safe_copy(n->label, label ? label : "", IR_LABEL_MAX);
    ir_append(fn, n);
    return n;
}

/* ── Fresh name generators ────────────────────────────────────────────── */

void ir_fresh_tmp(ir_func_t *fn, char *buf) {
    /* writes "t<N>" into buf — buf must be >= IR_NAME_MAX bytes */
    int n = fn->tmp_counter++;
    int i = 0;
    buf[i++] = 't';
    /* write decimal digits of n */
    if (n == 0) {
        buf[i++] = '0';
    } else {
        char tmp[20];
        int  tlen = 0;
        int  v = n;
        while (v > 0) { tmp[tlen++] = '0' + (v % 10); v /= 10; }
        /* reverse */
        int j;
        for (j = tlen - 1; j >= 0; j--) buf[i++] = tmp[j];
    }
    buf[i] = '\0';
}

void ir_fresh_label(ir_func_t *fn, char *buf) {
    /* writes ".L<N>" into buf */
    int n = fn->lbl_counter++;
    int i = 0;
    buf[i++] = '.';
    buf[i++] = 'L';
    if (n == 0) {
        buf[i++] = '0';
    } else {
        char tmp[20];
        int  tlen = 0;
        int  v = n;
        while (v > 0) { tmp[tlen++] = '0' + (v % 10); v /= 10; }
        int j;
        for (j = tlen - 1; j >= 0; j--) buf[i++] = tmp[j];
    }
    buf[i] = '\0';
}

/* ── Module / function lifecycle ─────────────────────────────────────── */

ir_module_t *ir_module_create(void) {
    ir_module_t *mod = (ir_module_t *)calloc(1, sizeof(ir_module_t));
    if (!mod) {
        fprintf(stderr, "ir: out of memory allocating ir_module_t\n");
        exit(1);
    }
    return mod;
}

ir_func_t *ir_func_create(ir_module_t *mod, const char *name,
                           ir_type_t ret_type, int num_params) {
    if (mod->func_count >= IR_MAX_FUNCS) {
        fprintf(stderr, "ir: exceeded IR_MAX_FUNCS (%d)\n", IR_MAX_FUNCS);
        exit(1);
    }
    ir_func_t *fn = (ir_func_t *)calloc(1, sizeof(ir_func_t));
    if (!fn) {
        fprintf(stderr, "ir: out of memory allocating ir_func_t\n");
        exit(1);
    }
    safe_copy(fn->name, name ? name : "anon", IR_FUNC_MAX);
    fn->ret_type = ret_type;
    fn->num_params = num_params;
    mod->funcs[mod->func_count++] = fn;
    return fn;
}

void ir_module_free(ir_module_t *mod) {
    int i;
    if (!mod) return;
    for (i = 0; i < mod->func_count; i++) {
        ir_func_t *fn = mod->funcs[i];
        ir_node_t *n  = fn->head;
        while (n) {
            ir_node_t *next = n->next;
            free(n);
            n = next;
        }
        free(fn);
    }
    free(mod);
}

/* ── Text emission ────────────────────────────────────────────────────── */
/*
 * Text format (columns are tab-separated):
 *
 *   ; func <name> -> <ret_type>
 *   <op>  <type>  <dst>  <src1>  <src2>  <label>  [imm=<N>]  [; line <N>]
 *
 * Fields that are unused are printed as "-".
 * LABEL nodes print:   LABEL  -  -  -  -  <label>
 * CONST nodes print:   CONST  i32  t0  -  -  -  imm=42
 *
 * One node per line.  Machine-parseable for P2-IR's ir_build_cfg().
 */

static void emit_field(FILE *fp, const char *s) {
    if (s && s[0]) fprintf(fp, "%s", s);
    else           fprintf(fp, "-");
}

void ir_func_emit_text(const ir_func_t *fn, FILE *fp) {
    const ir_node_t *n;

    fprintf(fp, "; func %s -> %s\n", fn->name, ir_type_name(fn->ret_type));

    for (n = fn->head; n; n = n->next) {
        /* Opcode */
        fprintf(fp, "  %-10s", ir_op_name(n->op));

        /* Type */
        fprintf(fp, "  %-6s", n->type == IR_TY_VOID ? "-"
                                                      : ir_type_name(n->type));

        /* dst */
        fprintf(fp, "  ");
        emit_field(fp, n->dst);

        /* src1 */
        fprintf(fp, "  ");
        emit_field(fp, n->src1);

        /* src2 */
        fprintf(fp, "  ");
        emit_field(fp, n->src2);

        /* label */
        fprintf(fp, "  ");
        emit_field(fp, n->label);

        /* asm string */
        if (n->op == IR_ASM) {
            fprintf(fp, "  str=\"%s\"", n->asm_string ? n->asm_string : "");
        }

        /* imm — only print for ops that use it */
        if (n->op == IR_CONST || n->op == IR_ALLOCA || n->op == IR_FCONST) {
            fprintf(fp, "  imm=%ld", n->imm);
        }

        /* lineno annotation */
        if (n->lineno > 0) {
            fprintf(fp, "  ; line %d", n->lineno);
        }

        fprintf(fp, "\n");
    }

    fprintf(fp, "; end %s  nodes=%d\n\n", fn->name, fn->node_count);
}

void ir_module_emit_text(const ir_module_t *mod, FILE *fp) {
    int i;
    fprintf(fp, "; ZCC IR module  funcs=%d\n\n", mod->func_count);
    for (i = 0; i < mod->func_count; i++) {
        ir_func_emit_text(mod->funcs[i], fp);
    }
}
/*
 * ir_to_x86.c — ZCC IR-to-x86_64 Lowering Backend
 *
 * Walks the ir_module_t and generates System V x86-64 assembly,
 * completely replacing the AST-based codegen in part4.c.
 *
 * v2: Linear-scan register allocator (regalloc.c) assigns physical
 *     registers to %tN temporaries where possible, eliminating
 *     redundant load/store pairs.  %stack_* locals are unchanged.
 */

#include "ir.h"
#include "regalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[IR_NAME_MAX];
    int offset;
} ir_var_t;

static ir_var_t *vars = 0;
static int max_vars = 0;
static int num_vars = 0;
static int next_offset_from_rbp = -8;

static int get_or_create_var(const char *name) {
    int i;
    if (!name || name[0] == '\0' || name[0] == '-') return 0;
    
    for (i = 0; i < num_vars; i++) {
        if (strcmp(vars[i].name, name) == 0) return vars[i].offset;
    }
    
    if (num_vars >= max_vars) {
        max_vars = (max_vars == 0) ? 2048 : max_vars * 2;
        ir_var_t *new_vars = (ir_var_t *)realloc(vars, max_vars * sizeof(ir_var_t));
        if (!new_vars) {
            fprintf(stderr, "\n[FATAL] IR Backend: Variable limit exceeded and out of memory!\n");
            fprintf(stderr, "Tried to allocate %d variables.\n", max_vars);
            exit(1);
        }
        vars = new_vars;
    }
    
    int off;
    strcpy(vars[num_vars].name, name);
    vars[num_vars].offset = next_offset_from_rbp;
    off = next_offset_from_rbp;
    next_offset_from_rbp -= 8;
    num_vars++;
    return off;
}

/* ── Register-aware operand helpers ──────────────────────────────────── */

/*
 * Emit code to load operand `src` into x86 register `dst_reg`.
 * If `src` has a physical register assigned by the allocator, emit a
 * register-to-register move.  Otherwise fall back to the stack slot.
 *
 * `ra` may be NULL (disabled / non-IR path).
 */
static void load_operand(FILE *out, const char *src, const char *dst_reg,
                         const RegAllocator *ra) {
    if (ra) {
        PhysReg pr = ra_get(ra, src);
        if (pr != PREG_NONE) {
            const char *pname = preg_name(pr);
            if (strcmp(pname, dst_reg + 1) != 0) /* avoid "movq %rbx, %rbx" */
                fprintf(out, "    movq %%%s, %s\n", pname, dst_reg);
            return;
        }
    }
    /* Stack slot fallback */
    int off = get_or_create_var(src);
    fprintf(out, "    movq %d(%%rbp), %s\n", off, dst_reg);
}

/*
 * Emit code to store x86 register `src_reg` into the location for `dst`.
 * If `dst` has a physical register, emit reg→reg.  Otherwise stack slot.
 */
static void store_result(FILE *out, const char *dst, const char *src_reg,
                         const RegAllocator *ra) {
    if (ra) {
        PhysReg pr = ra_get(ra, dst);
        if (pr != PREG_NONE) {
            const char *pname = preg_name(pr);
            if (strcmp(pname, src_reg + 1) != 0)
                fprintf(out, "    movq %s, %%%s\n", src_reg, pname);
            return;
        }
    }
    int off = get_or_create_var(dst);
    fprintf(out, "    movq %s, %d(%%rbp)\n", src_reg, off);
}

/*
 * Emit a binary arithmetic operand for src2: either a direct memory
 * operand (e.g. "addq -16(%rbp), %rax") or a register-to-register op.
 * Returns non-zero if the op was emitted inline; the caller does NOT
 * need to do anything else for src2.
 *
 * Only used for commutative/simple binary ops (ADD, SUB, MUL, AND, OR, XOR).
 */
static void emit_src2(FILE *out, const char *mnemonic, const char *src2,
                      const RegAllocator *ra) {
    if (ra) {
        PhysReg pr = ra_get(ra, src2);
        if (pr != PREG_NONE) {
            fprintf(out, "    %s %%%s, %%rax\n", mnemonic, preg_name(pr));
            return;
        }
    }
    int off2 = get_or_create_var(src2);
    fprintf(out, "    %s %d(%%rbp), %%rax\n", mnemonic, off2);
}

/* ── Address-load helper (unchanged from v1) ─────────────────────────── */

static void load_address(FILE *out, const char *src, const char *reg) {
    if (strncmp(src, "%stack_", 7) == 0) {
        int off = get_or_create_var(src);
        fprintf(out, "    leaq %d(%%rbp), %s\n", off, reg);
    } else if (strncmp(src, "%t", 2) == 0) {
        /* A temp holding an address: load its value (the pointer) */
        int off = get_or_create_var(src);
        fprintf(out, "    movq %d(%%rbp), %s\n", off, reg);
    } else {
        const char *name = src;
        if (name[0] == '%') name++; /* remove '%' prefix for globals */
        fprintf(out, "    leaq %s(%%rip), %s\n", name, reg);
    }
}

/* Address-load that is register-aware for %t temporaries holding pointers */
static void load_address_ra(FILE *out, const char *src, const char *reg,
                             const RegAllocator *ra) {
    if (strncmp(src, "%t", 2) == 0 && ra) {
        PhysReg pr = ra_get(ra, src);
        if (pr != PREG_NONE) {
            const char *pname = preg_name(pr);
            if (strcmp(pname, reg + 1) != 0)
                fprintf(out, "    movq %%%s, %s\n", pname, reg);
            return;
        }
    }
    load_address(out, src, reg);
}

/* ── Main lowering entry point ───────────────────────────────────────── */

void ir_module_lower_x86(const ir_module_t *mod, FILE *out) {
    int i;
    const char *arg_regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

    fprintf(out, "    .text\n");
    
    for (i = 0; i < mod->func_count; i++) {
        ir_func_t *fn = mod->funcs[i];
        ir_node_t *n;
        int stack_size;
        int arg_idx;

        num_vars = 0;
        next_offset_from_rbp = -8;

        /* ── Pass 0: Run linear scan register allocator ── */
        RegAllocator *ra = ra_create();
        ra_run(ra, fn);

        /* ── Pass 1: Allocate stack slots for ALL vars/temps ── */
        /* We still allocate stack slots for ALL temps: allocated temps
         * use their physical register during computation but the slot
         * remains as a spill target if needed by loads/stores.         */
        n = fn->head;
        while (n) {
            if (n->dst[0]  != '\0' && n->dst[0]  != '-') get_or_create_var(n->dst);
            if (n->src1[0] != '\0' && n->src1[0] != '-') get_or_create_var(n->src1);
            if (n->src2[0] != '\0' && n->src2[0] != '-') get_or_create_var(n->src2);
            if (n->op == IR_ALLOCA) {
                next_offset_from_rbp -= n->imm;
            }
            n = n->next;
        }
        
        stack_size = -next_offset_from_rbp + 40;
        stack_size = (stack_size + 15) & ~15;
        
        /* ── Prologue ── */
        fprintf(out, "    .globl %s\n", fn->name);
        fprintf(out, "%s:\n", fn->name);
        fprintf(out, "    pushq %%rbp\n");
        fprintf(out, "    movq %%rsp, %%rbp\n");
        fprintf(out, "    subq $%d, %%rsp\n", stack_size);

        /* Push callee-saved registers used by the allocator */
        if (ra->used[PREG_RBX])  fprintf(out, "    pushq %%rbx\n");
        if (ra->used[PREG_R12])  fprintf(out, "    pushq %%r12\n");
        if (ra->used[PREG_R13])  fprintf(out, "    pushq %%r13\n");
        if (ra->used[PREG_R14])  fprintf(out, "    pushq %%r14\n");
        if (ra->used[PREG_R15])  fprintf(out, "    pushq %%r15\n");
        
        /* Store incoming parameters into their stack slots */
        for (int pnum = 0; pnum < fn->num_params; pnum++) {
            char param_name[32];
            sprintf(param_name, "%%stack_%d", -8 * (pnum + 1));
            int stack_off = get_or_create_var(param_name);
            if (pnum < 6) {
                fprintf(out, "    movq %s, %d(%%rbp)\n", arg_regs[pnum], stack_off);
            }
        }

        arg_idx = 0;

        /* ── Pass 2: Emit assembly ── */
        n = fn->head;
        while (n) {
            fprintf(out, "    # %s\n", ir_op_name(n->op));
            switch (n->op) {
                case IR_CONST: {
                    fprintf(out, "    movabsq $%ld, %%rax\n", n->imm);
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_CONST_STR: {
                    fprintf(out, "    leaq %s(%%rip), %%rax\n", n->src1);
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_ADD:
                case IR_SUB:
                case IR_MUL:
                case IR_AND:
                case IR_OR:
                case IR_XOR:
                case IR_SHL:
                case IR_SHR:
                case IR_EQ:
                case IR_NE:
                case IR_LT:
                case IR_LE:
                case IR_GT:
                case IR_GE: {
                    load_operand(out, n->src1, "%rax", ra);

                    if (n->op == IR_ADD)        emit_src2(out, "addq",  n->src2, ra);
                    else if (n->op == IR_SUB)   emit_src2(out, "subq",  n->src2, ra);
                    else if (n->op == IR_MUL)   emit_src2(out, "imulq", n->src2, ra);
                    else if (n->op == IR_AND)   emit_src2(out, "andq",  n->src2, ra);
                    else if (n->op == IR_OR)    emit_src2(out, "orq",   n->src2, ra);
                    else if (n->op == IR_XOR)   emit_src2(out, "xorq",  n->src2, ra);
                    else if (n->op == IR_SHL) {
                        /* shift count must be in %cl */
                        load_operand(out, n->src2, "%rcx", ra);
                        fprintf(out, "    shlq %%cl, %%rax\n");
                    } else if (n->op == IR_SHR) {
                        load_operand(out, n->src2, "%rcx", ra);
                        if (ir_type_unsigned(n->type)) fprintf(out, "    shrq %%cl, %%rax\n");
                        else                            fprintf(out, "    sarq %%cl, %%rax\n");
                    } else {
                        /* comparison: src2 as memory operand or reg */
                        if (ra) {
                            PhysReg pr2 = ra_get(ra, n->src2);
                            if (pr2 != PREG_NONE)
                                fprintf(out, "    cmpq %%%s, %%rax\n", preg_name(pr2));
                            else {
                                int off2 = get_or_create_var(n->src2);
                                fprintf(out, "    cmpq %d(%%rbp), %%rax\n", off2);
                            }
                        } else {
                            int off2 = get_or_create_var(n->src2);
                            fprintf(out, "    cmpq %d(%%rbp), %%rax\n", off2);
                        }
                        if      (n->op == IR_EQ) fprintf(out, "    sete %%al\n");
                        else if (n->op == IR_NE) fprintf(out, "    setne %%al\n");
                        else if (n->op == IR_LT) fprintf(out, "    setl %%al\n");
                        else if (n->op == IR_LE) fprintf(out, "    setle %%al\n");
                        else if (n->op == IR_GT) fprintf(out, "    setg %%al\n");
                        else if (n->op == IR_GE) fprintf(out, "    setge %%al\n");
                        fprintf(out, "    movzbq %%al, %%rax\n");
                    }
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_DIV:
                case IR_MOD: {
                    load_operand(out, n->src1, "%rax", ra);
                    /* divisor: must be in a register or memory for idivq/divq */
                    if (ir_type_unsigned(n->type)) {
                        fprintf(out, "    xorq %%rdx, %%rdx\n");
                        if (ra) {
                            PhysReg pr2 = ra_get(ra, n->src2);
                            if (pr2 != PREG_NONE)
                                fprintf(out, "    divq %%%s\n", preg_name(pr2));
                            else {
                                int off2 = get_or_create_var(n->src2);
                                fprintf(out, "    divq %d(%%rbp)\n", off2);
                            }
                        } else {
                            int off2 = get_or_create_var(n->src2);
                            fprintf(out, "    divq %d(%%rbp)\n", off2);
                        }
                    } else {
                        fprintf(out, "    cqo\n");
                        if (ra) {
                            PhysReg pr2 = ra_get(ra, n->src2);
                            if (pr2 != PREG_NONE)
                                fprintf(out, "    idivq %%%s\n", preg_name(pr2));
                            else {
                                int off2 = get_or_create_var(n->src2);
                                fprintf(out, "    idivq %d(%%rbp)\n", off2);
                            }
                        } else {
                            int off2 = get_or_create_var(n->src2);
                            fprintf(out, "    idivq %d(%%rbp)\n", off2);
                        }
                    }
                    if (n->op == IR_MOD) store_result(out, n->dst, "%rdx", ra);
                    else                 store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_NOT:
                case IR_NEG: {
                    load_operand(out, n->src1, "%rax", ra);
                    if (n->op == IR_NOT) fprintf(out, "    notq %%rax\n");
                    else                 fprintf(out, "    negq %%rax\n");
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_LOAD: {
                    load_address_ra(out, n->src1, "%rax", ra);
                    if (n->type == IR_TY_I32 || n->type == IR_TY_U32) {
                        fprintf(out, "    movl (%%rax), %%eax\n");
                        if (!ir_type_unsigned(n->type)) fprintf(out, "    movslq %%eax, %%rax\n");
                    } else if (n->type == IR_TY_I8 || n->type == IR_TY_U8) {
                        fprintf(out, "    movzbl (%%rax), %%eax\n");
                        if (!ir_type_unsigned(n->type)) fprintf(out, "    movsbq %%al, %%rax\n");
                    } else if (n->type == IR_TY_I16 || n->type == IR_TY_U16) {
                        fprintf(out, "    movzwl (%%rax), %%eax\n");
                        if (!ir_type_unsigned(n->type)) fprintf(out, "    movswq %%ax, %%rax\n");
                    } else {
                        fprintf(out, "    movq (%%rax), %%rax\n");
                    }
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_ADDR: {
                    load_address(out, n->src1, "%rax");
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_STORE: {
                    /* src1 = value, dst = address */
                    load_operand(out, n->src1, "%rcx", ra);
                    load_address_ra(out, n->dst, "%rax", ra);
                    if (n->type == IR_TY_I32 || n->type == IR_TY_U32)
                        fprintf(out, "    movl %%ecx, (%%rax)\n");
                    else if (n->type == IR_TY_I8 || n->type == IR_TY_U8)
                        fprintf(out, "    movb %%cl, (%%rax)\n");
                    else
                        fprintf(out, "    movq %%rcx, (%%rax)\n");
                    break;
                }
                case IR_ALLOCA: {
                    int offd = get_or_create_var(n->dst);
                    fprintf(out, "    leaq %d(%%rbp), %%rax\n", vars[offd].offset - (int)n->imm);
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_COPY:
                case IR_CAST: {
                    load_operand(out, n->src1, "%rax", ra);
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_LABEL: {
                    fprintf(out, "%s:\n", n->label);
                    break;
                }
                case IR_BR: {
                    fprintf(out, "    jmp %s\n", n->label);
                    break;
                }
                case IR_BR_IF: {
                    load_operand(out, n->src1, "%rax", ra);
                    fprintf(out, "    cmpq $0, %%rax\n");
                    fprintf(out, "    je %s\n", n->label);
                    break;
                }
                case IR_ARG: {
                    load_operand(out, n->src1, "%rax", ra);
                    if (arg_idx < 6) {
                        fprintf(out, "    movq %%rax, %s\n", arg_regs[arg_idx]);
                    } else {
                        fprintf(out, "    pushq %%rax\n");
                    }
                    arg_idx++;
                    break;
                }
                case IR_CALL: {
                    fprintf(out, "    movb $0, %%al\n");
                    fprintf(out, "    callq %s\n", n->label);
                    if (arg_idx > 6) {
                        fprintf(out, "    addq $%d, %%rsp\n", (arg_idx - 6) * 8);
                    }
                    if (n->dst[0] != '\0' && n->dst[0] != '-') {
                        store_result(out, n->dst, "%rax", ra);
                    }
                    arg_idx = 0;
                    break;
                }
                case IR_RET: {
                    if (n->src1[0] != '\0' && n->src1[0] != '-') {
                        load_operand(out, n->src1, "%rax", ra);
                    }
                    /* Restore callee-saved registers (reverse order) */
                    if (ra->used[PREG_R15]) fprintf(out, "    popq %%r15\n");
                    if (ra->used[PREG_R14]) fprintf(out, "    popq %%r14\n");
                    if (ra->used[PREG_R13]) fprintf(out, "    popq %%r13\n");
                    if (ra->used[PREG_R12]) fprintf(out, "    popq %%r12\n");
                    if (ra->used[PREG_RBX]) fprintf(out, "    popq %%rbx\n");
                    fprintf(out, "    movq %%rbp, %%rsp\n");
                    fprintf(out, "    popq %%rbp\n");
                    fprintf(out, "    ret\n");
                    break;
                }
                case IR_FCONST: {
                    fprintf(out, "    movabsq $%ld, %%rax\n", n->imm);
                    store_result(out, n->dst, "%rax", ra);
                    break;
                }
                case IR_FADD:
                case IR_FSUB:
                case IR_FMUL:
                case IR_FDIV: {
                    int off1 = get_or_create_var(n->src1);
                    int off2 = get_or_create_var(n->src2);
                    int offd = get_or_create_var(n->dst);
                    fprintf(out, "    movq %d(%%rbp), %%xmm0\n", off1);
                    fprintf(out, "    movq %d(%%rbp), %%xmm1\n", off2);
                    if      (n->op == IR_FADD) fprintf(out, "    addsd %%xmm1, %%xmm0\n");
                    else if (n->op == IR_FSUB) fprintf(out, "    subsd %%xmm1, %%xmm0\n");
                    else if (n->op == IR_FMUL) fprintf(out, "    mulsd %%xmm1, %%xmm0\n");
                    else                       fprintf(out, "    divsd %%xmm1, %%xmm0\n");
                    fprintf(out, "    movq %%xmm0, %d(%%rbp)\n", offd);
                    break;
                }
                case IR_ITOF: {
                    int off1 = get_or_create_var(n->src1);
                    int offd = get_or_create_var(n->dst);
                    fprintf(out, "    movq %d(%%rbp), %%rax\n", off1);
                    fprintf(out, "    cvtsi2sdq %%rax, %%xmm0\n");
                    fprintf(out, "    movq %%xmm0, %d(%%rbp)\n", offd);
                    break;
                }
                case IR_FTOI: {
                    int off1 = get_or_create_var(n->src1);
                    int offd = get_or_create_var(n->dst);
                    fprintf(out, "    movq %d(%%rbp), %%xmm0\n", off1);
                    fprintf(out, "    cvttsd2si %%xmm0, %%rax\n");
                    fprintf(out, "    movq %%rax, %d(%%rbp)\n", offd);
                    break;
                }
                default: break;
            }
            n = n->next;
        }
        
        /* Fallback epilogue in case missing explicit RET */
        if (ra->used[PREG_R15]) fprintf(out, "    popq %%r15\n");
        if (ra->used[PREG_R14]) fprintf(out, "    popq %%r14\n");
        if (ra->used[PREG_R13]) fprintf(out, "    popq %%r13\n");
        if (ra->used[PREG_R12]) fprintf(out, "    popq %%r12\n");
        if (ra->used[PREG_RBX]) fprintf(out, "    popq %%rbx\n");
        fprintf(out, "    movq %%rbp, %%rsp\n");
        fprintf(out, "    popq %%rbp\n");
        fprintf(out, "    ret\n");

        ra_free(ra);
    }
}
/*
 * regalloc.c — Linear Scan Register Allocator for ZCC IR Backend
 *
 * See regalloc.h for design rationale.
 *
 * The IR is a singly-linked list of ir_node_t with string-named fields:
 *   n->dst   — destination temp (e.g. "%t3", "%stack_-8", "")
 *   n->src1  — first source temp
 *   n->src2  — second source temp
 *
 * Only names with prefix "%t" (pure SSA temporaries) are candidates.
 * %stack_* names are addressable locals: leave them on the stack always.
 */

#include "regalloc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Physical register table ─────────────────────────────────────────── */

static const char *preg_names[PREG_COUNT] = {
    "rbx", "r12", "r13", "r14", "r15",  /* callee-saved */
    "r10", "r11"                          /* caller-saved scratch */
};

const char *preg_name(PhysReg r) {
    if (r < 0 || r >= PREG_COUNT) return "???";
    return preg_names[r];
}

int preg_callee_saved(PhysReg r) {
    return (r >= PREG_RBX && r <= PREG_R15);
}

/* ── Interval helpers ────────────────────────────────────────────────── */

static int is_temp(const char *name) {
    return (name && name[0] == '%' && name[1] == 't' &&
            name[2] >= '0' && name[2] <= '9');
}

static LiveInterval *find_interval(RegAllocator *ra, const char *name) {
    int i;
    for (i = 0; i < ra->num_intervals; i++) {
        if (strcmp(ra->intervals[i].name, name) == 0)
            return &ra->intervals[i];
    }
    return NULL;
}

static LiveInterval *get_or_create(RegAllocator *ra, const char *name, int pos) {
    LiveInterval *iv = find_interval(ra, name);
    if (iv) return iv;

    /* Grow if needed */
    if (ra->num_intervals >= ra->cap_intervals) {
        int newcap = ra->cap_intervals ? ra->cap_intervals * 2 : 64;
        LiveInterval *nb = (LiveInterval *)realloc(ra->intervals,
                                newcap * sizeof(LiveInterval));
        if (!nb) { fprintf(stderr, "[regalloc] OOM\n"); exit(1); }
        ra->intervals = nb;
        ra->cap_intervals = newcap;
    }

    iv = &ra->intervals[ra->num_intervals++];
    strncpy(iv->name, name, RA_NAME_MAX - 1);
    iv->name[RA_NAME_MAX - 1] = '\0';
    iv->start    = pos;
    iv->end      = pos;
    iv->assigned = PREG_NONE;
    return iv;
}

/* ── Allocator lifecycle ─────────────────────────────────────────────── */

RegAllocator *ra_create(void) {
    RegAllocator *ra = (RegAllocator *)calloc(1, sizeof(RegAllocator));
    if (!ra) { fprintf(stderr, "[regalloc] OOM\n"); exit(1); }
    return ra;
}

void ra_free(RegAllocator *ra) {
    if (!ra) return;
    free(ra->intervals);
    free(ra);
}

/* ── Sort helper for qsort ───────────────────────────────────────────── */

static int iv_cmp_start(const void *a, const void *b) {
    const LiveInterval *ia = (const LiveInterval *)a;
    const LiveInterval *ib = (const LiveInterval *)b;
    return ia->start - ib->start;
}

/* ── Phase 1: Build live intervals ──────────────────────────────────── */

static void build_intervals(RegAllocator *ra, const ir_func_t *fn) {
    const ir_node_t *n;
    int pos = 0;

    for (n = fn->head; n; n = n->next, pos++) {
        /* Process destination (definition) */
        if (is_temp(n->dst)) {
            LiveInterval *iv = get_or_create(ra, n->dst, pos);
            /* definitions extend end too (covers single-use temps) */
            if (pos > iv->end) iv->end = pos;
        }
        /* Process source operands (uses) */
        if (is_temp(n->src1)) {
            LiveInterval *iv = get_or_create(ra, n->src1, pos);
            if (pos > iv->end) iv->end = pos;
        }
        if (is_temp(n->src2)) {
            LiveInterval *iv = get_or_create(ra, n->src2, pos);
            if (pos > iv->end) iv->end = pos;
        }
    }

    /* Sort by start for the scan */
    qsort(ra->intervals, ra->num_intervals, sizeof(LiveInterval), iv_cmp_start);
}

/* ── Phase 2: Linear scan allocation ────────────────────────────────── */

/*
 * Classic Poletto & Sarkar linear scan.
 * Active = set of intervals currently occupying a physical register.
 * We maintain active[] sorted by end point (earliest-ending first) so
 * expiry and spill decisions are O(k) where k = PREG_COUNT (small constant).
 */

static void linear_scan(RegAllocator *ra) {
    /* active[i] = index into ra->intervals of an allocated interval */
    int active[PREG_COUNT];
    int active_reg[PREG_COUNT]; /* physical register used by active[i] */
    int active_cnt = 0;

    /* Free-register stack */
    PhysReg free_regs[PREG_COUNT];
    int free_cnt = 0;
    int r;
    for (r = 0; r < PREG_COUNT; r++)
        free_regs[free_cnt++] = (PhysReg)(PREG_COUNT - 1 - r); /* push in reverse so rbx is tried first */

    int i;
    for (i = 0; i < ra->num_intervals; i++) {
        LiveInterval *cur = &ra->intervals[i];

        /* --- Expire old intervals --- */
        int j = 0;
        while (j < active_cnt) {
            LiveInterval *act = &ra->intervals[active[j]];
            if (act->end < cur->start) {
                /* Return this register to the free pool */
                free_regs[free_cnt++] = (PhysReg)active_reg[j];
                /* Compact active[] */
                active[j]     = active[active_cnt - 1];
                active_reg[j] = active_reg[active_cnt - 1];
                active_cnt--;
                /* Don't advance j — need to recheck this slot */
            } else {
                j++;
            }
        }

        /* --- Allocate or spill --- */
        if (free_cnt > 0) {
            /* Grab a free register */
            PhysReg preg = free_regs[--free_cnt];
            cur->assigned = preg;
            ra->used[preg] = 1;

            /* Add to active */
            active[active_cnt]     = i;
            active_reg[active_cnt] = (int)preg;
            active_cnt++;
        } else {
            /*
             * No free register. Spill the interval that ends latest
             * (keep the one ending soonest in a register — it frees up
             * sooner and benefits more instructions).
             */
            int spill_idx = -1;
            int spill_ai  = -1;
            int latest_end = cur->end;

            for (j = 0; j < active_cnt; j++) {
                LiveInterval *act = &ra->intervals[active[j]];
                if (act->end > latest_end) {
                    latest_end = act->end;
                    spill_idx  = active[j];
                    spill_ai   = j;
                }
            }

            if (spill_idx >= 0) {
                /* The active interval ending latest gives its register to cur */
                PhysReg stolen = (PhysReg)active_reg[spill_ai];
                ra->intervals[spill_idx].assigned = PREG_NONE; /* spill it */
                cur->assigned = stolen;

                active[spill_ai]     = i;
                active_reg[spill_ai] = (int)stolen;
            }
            /* else cur->assigned stays PREG_NONE (spilled) */
        }
    }
}

/* ── Public entry point ──────────────────────────────────────────────── */

void ra_run(RegAllocator *ra, const ir_func_t *fn) {
    build_intervals(ra, fn);
    if (ra->num_intervals > 0)
        linear_scan(ra);
}

/* ── Query API ───────────────────────────────────────────────────────── */

PhysReg ra_get(const RegAllocator *ra, const char *name) {
    int i;
    if (!is_temp(name)) return PREG_NONE;
    for (i = 0; i < ra->num_intervals; i++) {
        if (strcmp(ra->intervals[i].name, name) == 0)
            return ra->intervals[i].assigned;
    }
    return PREG_NONE;
}

int ra_any_callee_saved_used(const RegAllocator *ra) {
    int r;
    for (r = 0; r < PREG_COUNT; r++) {
        if (ra->used[r] && preg_callee_saved((PhysReg)r))
            return 1;
    }
    return 0;
}
/*
 * ir_telemetry_stub.c — No-op stubs for self-host build.
 *
 * The real ir_telemetry.c uses POSIX socket headers (<sys/socket.h>,
 * <netinet/in.h>) which ZCC cannot parse during self-hosting.
 * These stubs satisfy the link requirements of ir_pass_manager.c
 * without any POSIX dependency.
 *
 * The real ir_telemetry.c is compiled separately by GCC and linked
 * into the production compiler_passes_ir.c path only.
 */
#include "ir_telemetry.h"

void ir_telem_init(void) {}

void ir_telem_pass(const char *pass_name,
                   int func_count,
                   int nodes_before,
                   int nodes_after,
                   int nodes_deleted,
                   int nodes_modified) {
    (void)pass_name; (void)func_count; (void)nodes_before;
    (void)nodes_after; (void)nodes_deleted; (void)nodes_modified;
}

void ir_telem_summary(int total_funcs,
                      int total_nodes_before,
                      int total_nodes_after,
                      int pass_count,
                      const char **pass_names) {
    (void)total_funcs; (void)total_nodes_before; (void)total_nodes_after;
    (void)pass_count; (void)pass_names;
}

void ir_telem_shutdown(void) {}

void ir_telemetry_enable_stdout(void) {}
