/*
 * ir_serialization.c — ZCC IR JSON Serialization & Replay Engine
 *
 * Freestanding C89-compliant implementation with zero external library dependencies.
 */

#include "ir_serialization.h"
#include "../ir_dominance.h"
#include "../ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations of ZCC internal AST structures so ir_serialization is self-contained */
typedef struct Type Type;
typedef struct Node Node;
typedef struct Symbol Symbol;
typedef struct Scope Scope;
typedef struct Compiler Compiler;
typedef struct ArenaBlock ArenaBlock;
typedef struct StringEntry StringEntry;
typedef struct StructField StructField;
typedef struct FuncParams FuncParams;

enum {
    MAX_IDENT   = 128,
    MAX_STR     = 16384,
    MAX_STRINGS = 262144,
    MAX_GLOBALS = 262144,
    MAX_STRUCTS = 65536,
    MAX_PARAMS  = 128,
    MAX_CALL_ARGS = 256,
    MAX_CASES   = 4096,
    MAX_INIT    = 1048576
};

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
    Type *base;
    int array_len;
    Type *ret;
    Type **params;
    int num_params;
    int is_variadic;
    char tag[MAX_IDENT];
    StructField *fields;
    int is_complete;
    int is_packed;
    int explicit_align;
    int is_tbfp;
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
    int stack_offset;
    char asm_name[MAX_IDENT];
    char *assigned_reg;
    int live_start;
    int live_end;
    Symbol *next;
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
    long long int_val;
    double f_val;
    int str_id;
    char name[MAX_IDENT];
    Symbol *sym;
    Node *lhs;
    Node *rhs;
    char func_name[MAX_IDENT];
    Node **args;
    int num_args;
    Node *cond;
    Node *then_body;
    Node *else_body;
    Node *init;
    Node *inc;
    Node **stmts;
    int num_stmts;
    char func_def_name[MAX_IDENT];
    Type *func_type;
    struct FuncParams *func_params;
    int num_params;
    Node *body;
    int stack_size;
    Type *param_types[MAX_PARAMS];
    char param_names_buf[MAX_PARAMS][MAX_IDENT];
    char member_name[MAX_IDENT];
    int member_offset;
    int member_size;
    Node **cases;
    int num_cases;
    Node *default_case;
    long long case_val;
    Node *case_body;
    char label_name[MAX_IDENT];
    int compound_op;
    Type *cast_type;
    int is_static;
    int is_extern;
    Node *initializer;
    int is_bitfield;
    int bit_offset;
    int bit_size;
    char *asm_string;
    Node *next;
};

struct Compiler {
    int verbose;
    char *source;
    int source_len;
    int pos;
    char *filename;
    int tk;
    long long tk_val;
    double tk_fval;
    char tk_text[MAX_IDENT];
    char tk_str[MAX_STR];
    int tk_str_len;
    int tk_line;
    int tk_col;
    int has_peek;
    int peek_tk;
    long long peek_val;
    double peek_fval;
    char peek_text[MAX_IDENT];
    char peek_str[MAX_STR];
    int peek_str_len;
    int peek_line;
    int peek_col;
    int line;
    int col;
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
    Scope *current_scope;
    StringEntry strings[MAX_STRINGS];
    int num_strings;
    Type *structs[MAX_STRUCTS];
    int num_structs;
    Node *globals[MAX_GLOBALS];
    int num_globals;
    FILE *out;
    int label_count;
    int str_label_count;
    int stack_depth;
    int break_label;
    int continue_label;
    int switch_end_label;
    char current_func[MAX_IDENT];
    int func_end_label;
    ArenaBlock arena;
    int errors;
    int local_offset;
    int current_is_static;
    int pending_packed;
    int pending_aligned_n;
    int pending_tbfp;
    int debug_abi_classes;
    int abi_scratch_offset;
    int sret_offset;
    int used_regs_mask;
    int is_forced_mask;
};

/* Lexer / Tokenizer structures */
typedef enum {
    TOK_EOF = 0,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACK,
    TOK_RBRACK,
    TOK_COLON,
    TOK_COMMA,
    TOK_STRING,
    TOK_NUMBER,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NULL,
    TOK_ERROR
} tok_kind_t;

typedef struct {
    FILE *fp;
    int ch;
    char val_str[4096];
    long val_int;
} json_lexer_t;

static void lex_init(json_lexer_t *lex, FILE *fp) {
    lex->fp = fp;
    lex->ch = fgetc(fp);
    lex->val_str[0] = '\0';
    lex->val_int = 0;
}

static void next_ch(json_lexer_t *lex) {
    if (lex->ch != EOF) {
        lex->ch = fgetc(lex->fp);
    }
}

static tok_kind_t lex_next(json_lexer_t *lex) {
    while (lex->ch != EOF && (lex->ch == ' ' || lex->ch == '\t' || lex->ch == '\n' || lex->ch == '\r')) {
        next_ch(lex);
    }

    if (lex->ch == EOF) return TOK_EOF;

    switch (lex->ch) {
        case '{': next_ch(lex); return TOK_LBRACE;
        case '}': next_ch(lex); return TOK_RBRACE;
        case '[': next_ch(lex); return TOK_LBRACK;
        case ']': next_ch(lex); return TOK_RBRACK;
        case ':': next_ch(lex); return TOK_COLON;
        case ',': next_ch(lex); return TOK_COMMA;
        case '"': {
            next_ch(lex); /* skip opening quote */
            int i = 0;
            while (lex->ch != EOF && lex->ch != '"') {
                if (lex->ch == '\\') {
                    next_ch(lex);
                    if (lex->ch == EOF) break;
                    if (lex->ch == 'n') lex->val_str[i++] = '\n';
                    else if (lex->ch == 't') lex->val_str[i++] = '\t';
                    else if (lex->ch == 'r') lex->val_str[i++] = '\r';
                    else if (lex->ch == 'b') lex->val_str[i++] = '\b';
                    else if (lex->ch == 'f') lex->val_str[i++] = '\f';
                    else lex->val_str[i++] = lex->ch;
                } else {
                    lex->val_str[i++] = lex->ch;
                }
                if (i >= 4095) break; /* prevent buffer overflow */
                next_ch(lex);
            }
            lex->val_str[i] = '\0';
            if (lex->ch == '"') {
                next_ch(lex);
            }
            return TOK_STRING;
        }
    }

    /* parse number or identifier */
    if (lex->ch == '-' || (lex->ch >= '0' && lex->ch <= '9')) {
        int i = 0;
        int is_neg = 0;
        long val = 0;
        if (lex->ch == '-') {
            is_neg = 1;
            lex->val_str[i++] = '-';
            next_ch(lex);
        }
        while (lex->ch != EOF && (lex->ch >= '0' && lex->ch <= '9')) {
            lex->val_str[i++] = lex->ch;
            val = val * 10 + (lex->ch - '0');
            if (i >= 4095) break;
            next_ch(lex);
        }
        /* Handle decimal part if any */
        if (lex->ch == '.') {
            lex->val_str[i++] = '.';
            next_ch(lex);
            while (lex->ch != EOF && (lex->ch >= '0' && lex->ch <= '9')) {
                lex->val_str[i++] = lex->ch;
                next_ch(lex);
            }
        }
        lex->val_str[i] = '\0';
        lex->val_int = is_neg ? -val : val;
        return TOK_NUMBER;
    }

    /* parse true/false/null */
    if ((lex->ch >= 'a' && lex->ch <= 'z') || (lex->ch >= 'A' && lex->ch <= 'Z')) {
        int i = 0;
        while (lex->ch != EOF && ((lex->ch >= 'a' && lex->ch <= 'z') || (lex->ch >= 'A' && lex->ch <= 'Z'))) {
            lex->val_str[i++] = lex->ch;
            if (i >= 4095) break;
            next_ch(lex);
        }
        lex->val_str[i] = '\0';
        if (strcmp(lex->val_str, "true") == 0) return TOK_TRUE;
        if (strcmp(lex->val_str, "false") == 0) return TOK_FALSE;
        if (strcmp(lex->val_str, "null") == 0) return TOK_NULL;
        return TOK_ERROR;
    }

    /* skip unknown characters */
    next_ch(lex);
    return TOK_ERROR;
}

static int parse_expect(json_lexer_t *lex, tok_kind_t expected) {
    tok_kind_t tok = lex_next(lex);
    if (tok != expected) {
        fprintf(stderr, "json parse error: expected token %d, got %d (value: %s)\n", expected, tok, lex->val_str);
        return 0;
    }
    return 1;
}

static void skip_json_value(json_lexer_t *lex, tok_kind_t tok) {
    if (tok == TOK_LBRACE) {
        int depth = 1;
        while (depth > 0) {
            tok_kind_t t = lex_next(lex);
            if (t == TOK_EOF) break;
            if (t == TOK_LBRACE) depth++;
            if (t == TOK_RBRACE) depth--;
        }
    } else if (tok == TOK_LBRACK) {
        int depth = 1;
        while (depth > 0) {
            tok_kind_t t = lex_next(lex);
            if (t == TOK_EOF) break;
            if (t == TOK_LBRACK) depth++;
            if (t == TOK_RBRACK) depth--;
        }
    } else {
        /* single token values */
    }
}

static void escape_json_string(char *dst, const char *src) {
    int i, j;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    j = 0;
    for (i = 0; src[i] != '\0'; i++) {
        if (src[i] == '"') {
            dst[j++] = '\\';
            dst[j++] = '"';
        } else if (src[i] == '\\') {
            dst[j++] = '\\';
            dst[j++] = '\\';
        } else if (src[i] == '\n') {
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else if (src[i] == '\t') {
            dst[j++] = '\\';
            dst[j++] = 't';
        } else if (src[i] == '\r') {
            dst[j++] = '\\';
            dst[j++] = 'r';
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

static int get_gvar_size(const struct Node *gvar) {
    if (!gvar || !gvar->type) return 8;
    extern int type_size(Type *t);
    return type_size(gvar->type);
}

static void serialize_initializer(FILE *fp, const struct Node *init) {
    if (!init) {
        fprintf(fp, "null");
        return;
    }
    fprintf(fp, "{\n");
    fprintf(fp, "        \"kind\": %d,\n", init->kind);
    fprintf(fp, "        \"int_val\": %lld,\n", init->int_val);
    fprintf(fp, "        \"f_val\": %g,\n", init->f_val);
    fprintf(fp, "        \"str_id\": %d,\n", init->str_id);
    
    char escaped_name[256];
    escape_json_string(escaped_name, init->name);
    fprintf(fp, "        \"name\": \"%s\"", escaped_name);
    
    if (init->kind == 63 /* ND_INIT_LIST */) {
        fprintf(fp, ",\n        \"num_args\": %d,\n", init->num_args);
        fprintf(fp, "        \"args\": [\n");
        for (int i = 0; i < init->num_args; i++) {
            fprintf(fp, "          ");
            serialize_initializer(fp, init->args[i]);
            if (i + 1 < init->num_args) fprintf(fp, ",\n");
        }
        fprintf(fp, "\n        ]");
    }
    if (init->lhs) {
        fprintf(fp, ",\n        \"lhs\": ");
        serialize_initializer(fp, init->lhs);
    }
    if (init->rhs) {
        fprintf(fp, ",\n        \"rhs\": ");
        serialize_initializer(fp, init->rhs);
    }
    fprintf(fp, "\n      }");
}

static struct Node *deserialize_initializer_val(json_lexer_t *lex, struct Compiler *cc, tok_kind_t first_tok) {
    if (first_tok != TOK_LBRACE) return NULL;
    
    extern void *cc_alloc(struct Compiler *cc, int size);
    
    struct Node *n = (struct Node *)cc_alloc(cc, sizeof(struct Node));
    memset(n, 0, sizeof(struct Node));
    n->magic = 0x1122334455667788ULL;
    
    tok_kind_t tok;
    while (1) {
        tok = lex_next(lex);
        if (tok == TOK_COMMA) continue;
        if (tok == TOK_RBRACE) break;
        if (tok != TOK_STRING) return NULL;
        
        char key[256];
        strcpy(key, lex->val_str);
        
        if (!parse_expect(lex, TOK_COLON)) return NULL;
        
        if (strcmp(key, "kind") == 0) {
            if (!parse_expect(lex, TOK_NUMBER)) return NULL;
            n->kind = (int)lex->val_int;
        } else if (strcmp(key, "int_val") == 0) {
            if (!parse_expect(lex, TOK_NUMBER)) return NULL;
            n->int_val = lex->val_int;
        } else if (strcmp(key, "f_val") == 0) {
            if (!parse_expect(lex, TOK_NUMBER)) return NULL;
            n->f_val = atof(lex->val_str);
        } else if (strcmp(key, "str_id") == 0) {
            if (!parse_expect(lex, TOK_NUMBER)) return NULL;
            n->str_id = (int)lex->val_int;
        } else if (strcmp(key, "name") == 0) {
            if (!parse_expect(lex, TOK_STRING)) return NULL;
            strcpy(n->name, lex->val_str);
        } else if (strcmp(key, "num_args") == 0) {
            if (!parse_expect(lex, TOK_NUMBER)) return NULL;
            n->num_args = (int)lex->val_int;
        } else if (strcmp(key, "args") == 0) {
            if (!parse_expect(lex, TOK_LBRACK)) return NULL;
            n->args = (struct Node **)cc_alloc(cc, (n->num_args > 0 ? n->num_args : 1) * sizeof(struct Node *));
            int idx = 0;
            while (1) {
                tok = lex_next(lex);
                if (tok == TOK_RBRACK) break;
                if (tok == TOK_COMMA) continue;
                if (tok == TOK_LBRACE) {
                    struct Node *child = deserialize_initializer_val(lex, cc, tok);
                    if (idx < n->num_args) {
                        n->args[idx++] = child;
                    }
                }
            }
        } else if (strcmp(key, "lhs") == 0) {
            tok = lex_next(lex);
            n->lhs = deserialize_initializer_val(lex, cc, tok);
        } else if (strcmp(key, "rhs") == 0) {
            tok = lex_next(lex);
            n->rhs = deserialize_initializer_val(lex, cc, tok);
        } else {
            tok = lex_next(lex);
            skip_json_value(lex, tok);
        }
    }
    return n;
}

/* ========================================================================= */
/* PUBLIC API                                                                */
/* ========================================================================= */

int ir_serialize_json(const ir_module_t *mod, const char *out_filename, const char *source_file, const struct Compiler *cc) {
    FILE *fp = fopen(out_filename, "w");
    int f_idx;
    if (!fp) {
        return -1;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"zcc_ir_version\": \"1.0.0\",\n");
    
    char escaped_source[2048];
    escape_json_string(escaped_source, source_file);
    fprintf(fp, "  \"source\": \"%s\",\n", escaped_source);
    
    /* Serialize Strings Table */
    if (cc && cc->num_strings > 0) {
        fprintf(fp, "  \"strings\": [\n");
        for (int i = 0; i < cc->num_strings; i++) {
            char escaped_data[8192];
            escape_json_string(escaped_data, cc->strings[i].data);
            fprintf(fp, "    {\n");
            fprintf(fp, "      \"label_id\": %d,\n", cc->strings[i].label_id);
            fprintf(fp, "      \"data\": \"%s\",\n", escaped_data);
            fprintf(fp, "      \"len\": %d\n", cc->strings[i].len);
            fprintf(fp, "    }%s\n", (i + 1 < cc->num_strings) ? "," : "");
        }
        fprintf(fp, "  ],\n");
    } else {
        fprintf(fp, "  \"strings\": [],\n");
    }
    
    /* Serialize Global Variables */
    if (cc && cc->num_globals > 0) {
        fprintf(fp, "  \"globals\": [\n");
        int emitted_globals = 0;
        for (int i = 0; i < cc->num_globals; i++) {
            const struct Node *gvar = cc->globals[i];
            if (!gvar || gvar->kind != 61 /* ND_GLOBAL_VAR */) continue;
            if (gvar->is_extern) continue;
            
            if (emitted_globals > 0) fprintf(fp, ",\n");
            emitted_globals++;
            
            char escaped_name[256];
            escape_json_string(escaped_name, gvar->name);
            fprintf(fp, "    {\n");
            fprintf(fp, "      \"name\": \"%s\",\n", escaped_name);
            fprintf(fp, "      \"size\": %d,\n", get_gvar_size(gvar));
            fprintf(fp, "      \"is_static\": %s,\n", gvar->is_static ? "true" : "false");
            fprintf(fp, "      \"is_extern\": %s,\n", gvar->is_extern ? "true" : "false");
            fprintf(fp, "      \"initializer\": ");
            serialize_initializer(fp, gvar->initializer);
            fprintf(fp, "\n    }");
        }
        fprintf(fp, "\n  ],\n");
    } else {
        fprintf(fp, "  \"globals\": [],\n");
    }
    
    fprintf(fp, "  \"functions\": [\n");
    
    for (f_idx = 0; f_idx < mod->func_count; f_idx++) {
        ir_func_t *fn = mod->funcs[f_idx];
        fprintf(fp, "    {\n");
        
        char escaped_name[256];
        escape_json_string(escaped_name, fn->name);
        fprintf(fp, "      \"name\": \"%s\",\n", escaped_name);
        fprintf(fp, "      \"ret_type\": \"%s\",\n", ir_type_name(fn->ret_type));
        fprintf(fp, "      \"num_params\": %d,\n", fn->num_params);
        
        fprintf(fp, "      \"param_names\": [");
        int p_idx;
        for (p_idx = 0; p_idx < fn->num_params; p_idx++) {
            char escaped_param[256];
            escape_json_string(escaped_param, fn->param_names[p_idx]);
            fprintf(fp, "\"%s\"%s", escaped_param, (p_idx + 1 < fn->num_params) ? ", " : "");
        }
        fprintf(fp, "],\n");

        /* Build CFG */
        dom_cfg_t cfg;
        memset(&cfg, 0, sizeof(dom_cfg_t));
        dom_build_cfg(&cfg, fn);

        fprintf(fp, "      \"blocks\": [\n");
        int b_idx;
        for (b_idx = 0; b_idx < cfg.block_count; b_idx++) {
            dom_bb_t *bb = &cfg.blocks[b_idx];
            fprintf(fp, "        {\n");
            fprintf(fp, "          \"id\": %d,\n", bb->id);
            
            char escaped_label[256];
            escape_json_string(escaped_label, bb->label);
            fprintf(fp, "          \"label\": \"%s\",\n", escaped_label);
            
            /* Predecessors */
            fprintf(fp, "          \"preds\": [");
            int pr_idx;
            for (pr_idx = 0; pr_idx < bb->pred_count; pr_idx++) {
                fprintf(fp, "%d%s", bb->pred[pr_idx], (pr_idx + 1 < bb->pred_count) ? ", " : "");
            }
            fprintf(fp, "],\n");

            /* Successors */
            fprintf(fp, "          \"succs\": [");
            int sc_idx;
            for (sc_idx = 0; sc_idx < bb->succ_count; sc_idx++) {
                fprintf(fp, "%d%s", bb->succ[sc_idx], (sc_idx + 1 < bb->succ_count) ? ", " : "");
            }
            fprintf(fp, "],\n");

            /* Instructions under this block */
            fprintf(fp, "          \"instructions\": [\n");
            ir_node_t *n = bb->first;
            while (n) {
                fprintf(fp, "            {\n");
                fprintf(fp, "              \"op\": \"%s\",\n", ir_op_name(n->op));
                fprintf(fp, "              \"type\": \"%s\",\n", ir_type_name(n->type));
                
                char esc_dst[256], esc_src1[256], esc_src2[256];
                char esc_lbl[256], esc_lbl2[256], esc_asm[4096];
                escape_json_string(esc_dst, n->dst);
                escape_json_string(esc_src1, n->src1);
                escape_json_string(esc_src2, n->src2);
                escape_json_string(esc_lbl, n->label);
                escape_json_string(esc_lbl2, n->label2);
                
                fprintf(fp, "              \"dst\": \"%s\",\n", esc_dst);
                fprintf(fp, "              \"src1\": \"%s\",\n", esc_src1);
                fprintf(fp, "              \"src2\": \"%s\",\n", esc_src2);
                fprintf(fp, "              \"label\": \"%s\",\n", esc_lbl);
                fprintf(fp, "              \"label2\": \"%s\",\n", esc_lbl2);
                
                if (n->asm_string) {
                    escape_json_string(esc_asm, n->asm_string);
                    fprintf(fp, "              \"asm_string\": \"%s\",\n", esc_asm);
                } else {
                    fprintf(fp, "              \"asm_string\": \"\",\n");
                }
                
                fprintf(fp, "              \"imm\": %ld,\n", n->imm);
                fprintf(fp, "              \"lineno\": %d,\n", n->lineno);
                fprintf(fp, "              \"tag\": %d,\n", n->tag);
                fprintf(fp, "              \"vuln_tags\": %u%s\n", n->vuln_tags, (n->phi_count > 0) ? "," : "");

                if (n->phi_count > 0) {
                    fprintf(fp, "              \"phi_ops\": [\n");
                    int phi_idx;
                    for (phi_idx = 0; phi_idx < n->phi_count; phi_idx++) {
                        char esc_phi_val[256], esc_phi_blk[256];
                        escape_json_string(esc_phi_val, n->phi_ops[phi_idx].value);
                        escape_json_string(esc_phi_blk, n->phi_ops[phi_idx].block);
                        fprintf(fp, "                {\"value\": \"%s\", \"block\": \"%s\"}%s\n",
                                esc_phi_val, esc_phi_blk, (phi_idx + 1 < n->phi_count) ? "," : "");
                    }
                    fprintf(fp, "              ]\n");
                }
                
                fprintf(fp, "            }%s\n", (n == bb->last) ? "" : ",");
                
                if (n == bb->last) break;
                n = n->next;
            }
            fprintf(fp, "          ]\n");
            
            fprintf(fp, "        }%s\n", (b_idx + 1 < cfg.block_count) ? "," : "");
        }
        fprintf(fp, "      ]\n");
        
        fprintf(fp, "    }%s\n", (f_idx + 1 < mod->func_count) ? "," : "");
    }
    
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);
    return 0;
}

int ir_deserialize_json(ir_module_t *mod, const char *in_filename, struct Compiler *cc) {
    FILE *fp = fopen(in_filename, "r");
    tok_kind_t tok;
    if (!fp) {
        fprintf(stderr, "cannot open IR json file %s\n", in_filename);
        return -1;
    }
    
    json_lexer_t lex;
    lex_init(&lex, fp);
    
    tok = lex_next(&lex);
    if (tok != TOK_LBRACE) {
        fclose(fp);
        return -1;
    }
    
    /* Parse module object keys */
    while (1) {
        tok = lex_next(&lex);
        if (tok == TOK_COMMA) continue;
        if (tok == TOK_RBRACE) break; /* end of module object */
        if (tok != TOK_STRING) {
            fclose(fp);
            return -1;
        }
        
        char key[256];
        strcpy(key, lex.val_str);
        
        if (!parse_expect(&lex, TOK_COLON)) {
            fclose(fp);
            return -1;
        }
        
        if (strcmp(key, "strings") == 0) {
            if (!parse_expect(&lex, TOK_LBRACK)) {
                fclose(fp);
                return -1;
            }
            while (1) {
                tok = lex_next(&lex);
                if (tok == TOK_RBRACK) break;
                if (tok == TOK_COMMA) continue;
                if (tok != TOK_LBRACE) {
                    fclose(fp);
                    return -1;
                }
                int lbl_id = 0;
                char str_data[8192] = "";
                int str_len = 0;
                while (1) {
                    tok = lex_next(&lex);
                    if (tok == TOK_COMMA) continue;
                    if (tok == TOK_RBRACE) break;
                    if (tok != TOK_STRING) { fclose(fp); return -1; }
                    char skey[256];
                    strcpy(skey, lex.val_str);
                    if (!parse_expect(&lex, TOK_COLON)) { fclose(fp); return -1; }
                    if (strcmp(skey, "label_id") == 0) {
                        if (!parse_expect(&lex, TOK_NUMBER)) { fclose(fp); return -1; }
                        lbl_id = (int)lex.val_int;
                    } else if (strcmp(skey, "data") == 0) {
                        if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                        strcpy(str_data, lex.val_str);
                    } else if (strcmp(skey, "len") == 0) {
                        if (!parse_expect(&lex, TOK_NUMBER)) { fclose(fp); return -1; }
                        str_len = (int)lex.val_int;
                    } else {
                        tok = lex_next(&lex);
                        skip_json_value(&lex, tok);
                    }
                }
                if (cc && cc->num_strings < MAX_STRINGS) {
                    struct StringEntry *se = &cc->strings[cc->num_strings++];
                    se->label_id = lbl_id;
                    se->len = str_len;
                    extern void *cc_alloc(struct Compiler *cc, int size);
                    se->data = (char *)cc_alloc(cc, str_len + 1);
                    memcpy(se->data, str_data, str_len);
                    se->data[str_len] = '\0';
                }
            }
        } else if (strcmp(key, "globals") == 0) {
            if (!parse_expect(&lex, TOK_LBRACK)) {
                fclose(fp);
                return -1;
            }
            while (1) {
                tok = lex_next(&lex);
                if (tok == TOK_RBRACK) break;
                if (tok == TOK_COMMA) continue;
                if (tok != TOK_LBRACE) {
                    fclose(fp);
                    return -1;
                }
                char gname[256] = "";
                int gsize = 8;
                int gstatic = 0;
                int gextern = 0;
                struct Node *ginit = NULL;
                while (1) {
                    tok = lex_next(&lex);
                    if (tok == TOK_COMMA) continue;
                    if (tok == TOK_RBRACE) break;
                    if (tok != TOK_STRING) { fclose(fp); return -1; }
                    char gkey[256];
                    strcpy(gkey, lex.val_str);
                    if (!parse_expect(&lex, TOK_COLON)) { fclose(fp); return -1; }
                    if (strcmp(gkey, "name") == 0) {
                        if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                        strcpy(gname, lex.val_str);
                    } else if (strcmp(gkey, "size") == 0) {
                        if (!parse_expect(&lex, TOK_NUMBER)) { fclose(fp); return -1; }
                        gsize = (int)lex.val_int;
                    } else if (strcmp(gkey, "is_static") == 0) {
                        tok = lex_next(&lex);
                        gstatic = (tok == TOK_TRUE || (tok == TOK_NUMBER && lex.val_int != 0));
                    } else if (strcmp(gkey, "is_extern") == 0) {
                        tok = lex_next(&lex);
                        gextern = (tok == TOK_TRUE || (tok == TOK_NUMBER && lex.val_int != 0));
                    } else if (strcmp(gkey, "initializer") == 0) {
                        tok = lex_next(&lex);
                        ginit = deserialize_initializer_val(&lex, cc, tok);
                    } else {
                        tok = lex_next(&lex);
                        skip_json_value(&lex, tok);
                    }
                }
                if (cc && cc->num_globals < MAX_GLOBALS) {
                    extern void *cc_alloc(struct Compiler *cc, int size);
                    struct Node *gvar = (struct Node *)cc_alloc(cc, sizeof(struct Node));
                    memset(gvar, 0, sizeof(struct Node));
                    gvar->magic = 0x1122334455667788ULL;
                    gvar->kind = 61; /* ND_GLOBAL_VAR */
                    strcpy(gvar->name, gname);
                    gvar->is_static = gstatic;
                    gvar->is_extern = gextern;
                    struct Type *t = (struct Type *)cc_alloc(cc, sizeof(struct Type));
                    memset(t, 0, sizeof(struct Type));
                    t->magic = 0x8877665544332211ULL;
                    t->size = gsize;
                    if (gsize == 1) t->kind = 1;
                    else if (gsize == 2) t->kind = 3;
                    else if (gsize == 4) t->kind = 5;
                    else t->kind = 7;
                    gvar->type = t;
                    gvar->initializer = ginit;
                    cc->globals[cc->num_globals++] = gvar;
                }
            }
        } else if (strcmp(key, "functions") == 0) {
            if (!parse_expect(&lex, TOK_LBRACK)) {
                fclose(fp);
                return -1;
            }
            
            /* Parse functions array */
            while (1) {
                tok = lex_next(&lex);
                if (tok == TOK_RBRACK) break; /* end of functions array */
                if (tok == TOK_COMMA) continue;
                if (tok != TOK_LBRACE) {
                    fclose(fp);
                    return -1;
                }
                
                /* Parse function object keys */
                char fn_name[256] = "";
                ir_type_t fn_ret = IR_TY_VOID;
                int fn_num_params = 0;
                char fn_param_names[8][IR_NAME_MAX];
                memset(fn_param_names, 0, sizeof(fn_param_names));
                
                ir_func_t *fn = NULL;
                
                while (1) {
                    tok = lex_next(&lex);
                    if (tok == TOK_COMMA) continue;
                    if (tok == TOK_RBRACE) break; /* end of function object */
                    if (tok != TOK_STRING) {
                        fclose(fp);
                        return -1;
                    }
                    
                    char fkey[256];
                    strcpy(fkey, lex.val_str);
                    
                    if (!parse_expect(&lex, TOK_COLON)) {
                        fclose(fp);
                        return -1;
                    }
                    
                    if (strcmp(fkey, "name") == 0) {
                        if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                        strcpy(fn_name, lex.val_str);
                    } else if (strcmp(fkey, "ret_type") == 0) {
                        if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                        int t;
                        for (t = 0; t < 12; t++) {
                            if (strcmp(ir_type_name(t), lex.val_str) == 0) {
                                fn_ret = (ir_type_t)t;
                                break;
                            }
                        }
                    } else if (strcmp(fkey, "num_params") == 0) {
                        if (!parse_expect(&lex, TOK_NUMBER)) { fclose(fp); return -1; }
                        fn_num_params = (int)lex.val_int;
                    } else if (strcmp(fkey, "param_names") == 0) {
                        if (!parse_expect(&lex, TOK_LBRACK)) { fclose(fp); return -1; }
                        int p_count = 0;
                        while (1) {
                            tok = lex_next(&lex);
                            if (tok == TOK_RBRACK) break;
                            if (tok == TOK_COMMA) continue;
                            if (tok == TOK_STRING) {
                                if (p_count < 8) {
                                    strcpy(fn_param_names[p_count], lex.val_str);
                                }
                                p_count++;
                            }
                        }
                    } else if (strcmp(fkey, "blocks") == 0) {
                        if (!fn) {
                            fn = ir_func_create(mod, fn_name, fn_ret, fn_num_params);
                            int p;
                            for (p = 0; p < fn_num_params && p < 8; p++) {
                                strcpy(fn->param_names[p], fn_param_names[p]);
                            }
                        }
                        
                        if (!parse_expect(&lex, TOK_LBRACK)) { fclose(fp); return -1; }
                        
                        /* Parse blocks array */
                        while (1) {
                            tok = lex_next(&lex);
                            if (tok == TOK_RBRACK) break;
                            if (tok == TOK_COMMA) continue;
                            if (tok != TOK_LBRACE) { fclose(fp); return -1; }
                            
                            /* Parse block object */
                            while (1) {
                                tok = lex_next(&lex);
                                if (tok == TOK_COMMA) continue;
                                if (tok == TOK_RBRACE) break;
                                if (tok != TOK_STRING) { fclose(fp); return -1; }
                                
                                char bkey[256];
                                strcpy(bkey, lex.val_str);
                                
                                if (!parse_expect(&lex, TOK_COLON)) { fclose(fp); return -1; }
                                
                                if (strcmp(bkey, "id") == 0) {
                                    if (!parse_expect(&lex, TOK_NUMBER)) { fclose(fp); return -1; }
                                } else if (strcmp(bkey, "label") == 0) {
                                    if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                                } else if (strcmp(bkey, "preds") == 0) {
                                    if (!parse_expect(&lex, TOK_LBRACK)) { fclose(fp); return -1; }
                                    while (1) {
                                        tok = lex_next(&lex);
                                        if (tok == TOK_RBRACK) break;
                                    }
                                } else if (strcmp(bkey, "succs") == 0) {
                                    if (!parse_expect(&lex, TOK_LBRACK)) { fclose(fp); return -1; }
                                    while (1) {
                                        tok = lex_next(&lex);
                                        if (tok == TOK_RBRACK) break;
                                    }
                                } else if (strcmp(bkey, "instructions") == 0) {
                                    if (!parse_expect(&lex, TOK_LBRACK)) { fclose(fp); return -1; }
                                    
                                    /* Parse instructions array */
                                    while (1) {
                                        tok = lex_next(&lex);
                                        if (tok == TOK_RBRACK) break;
                                        if (tok == TOK_COMMA) continue;
                                        if (tok != TOK_LBRACE) { fclose(fp); return -1; }
                                        
                                        /* Parse instruction object */
                                        ir_node_t *n = ir_node_alloc();
                                        
                                        while (1) {
                                            tok = lex_next(&lex);
                                            if (tok == TOK_COMMA) continue;
                                            if (tok == TOK_RBRACE) break;
                                            if (tok != TOK_STRING) { fclose(fp); return -1; }
                                            
                                            char ikey[256];
                                            strcpy(ikey, lex.val_str);
                                            
                                            if (!parse_expect(&lex, TOK_COLON)) { fclose(fp); return -1; }
                                            
                                            if (strcmp(ikey, "op") == 0) {
                                                if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                                                int o;
                                                for (o = 0; o < IR_OP_COUNT; o++) {
                                                    if (strcmp(ir_op_name(o), lex.val_str) == 0) {
                                                        n->op = (ir_op_t)o;
                                                        break;
                                                    }
                                                }
                                            } else if (strcmp(ikey, "type") == 0) {
                                                if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                                                int t;
                                                for (t = 0; t < 12; t++) {
                                                    if (strcmp(ir_type_name(t), lex.val_str) == 0) {
                                                        n->type = (ir_type_t)t;
                                                        break;
                                                    }
                                                }
                                            } else if (strcmp(ikey, "dst") == 0) {
                                                if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                                                strcpy(n->dst, lex.val_str);
                                            } else if (strcmp(ikey, "src1") == 0) {
                                                if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                                                strcpy(n->src1, lex.val_str);
                                            } else if (strcmp(ikey, "src2") == 0) {
                                                if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                                                strcpy(n->src2, lex.val_str);
                                            } else if (strcmp(ikey, "label") == 0) {
                                                if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                                                strcpy(n->label, lex.val_str);
                                            } else if (strcmp(ikey, "label2") == 0) {
                                                if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                                                strcpy(n->label2, lex.val_str);
                                            } else if (strcmp(ikey, "asm_string") == 0) {
                                                if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                                                if (lex.val_str[0]) {
                                                    n->asm_string = strdup(lex.val_str);
                                                }
                                            } else if (strcmp(ikey, "imm") == 0) {
                                                if (!parse_expect(&lex, TOK_NUMBER)) { fclose(fp); return -1; }
                                                n->imm = lex.val_int;
                                            } else if (strcmp(ikey, "lineno") == 0) {
                                                if (!parse_expect(&lex, TOK_NUMBER)) { fclose(fp); return -1; }
                                                n->lineno = (int)lex.val_int;
                                            } else if (strcmp(ikey, "tag") == 0) {
                                                if (!parse_expect(&lex, TOK_NUMBER)) { fclose(fp); return -1; }
                                                n->tag = (int)lex.val_int;
                                            } else if (strcmp(ikey, "vuln_tags") == 0) {
                                                if (!parse_expect(&lex, TOK_NUMBER)) { fclose(fp); return -1; }
                                                n->vuln_tags = (unsigned int)lex.val_int;
                                            } else if (strcmp(ikey, "phi_ops") == 0) {
                                                if (!parse_expect(&lex, TOK_LBRACK)) { fclose(fp); return -1; }
                                                
                                                int p_cap = 4;
                                                n->phi_ops = (ir_phi_operand_t *)calloc(p_cap, sizeof(ir_phi_operand_t));
                                                n->phi_count = 0;
                                                
                                                while (1) {
                                                    tok = lex_next(&lex);
                                                    if (tok == TOK_RBRACK) break;
                                                    if (tok == TOK_COMMA) continue;
                                                    if (tok != TOK_LBRACE) { fclose(fp); return -1; }
                                                    
                                                    char p_val[256] = "";
                                                    char p_blk[256] = "";
                                                    
                                                    while (1) {
                                                        tok = lex_next(&lex);
                                                        if (tok == TOK_RBRACE) break;
                                                        if (tok == TOK_COMMA) continue;
                                                        if (tok != TOK_STRING) { fclose(fp); return -1; }
                                                        
                                                        char pkey[256];
                                                        strcpy(pkey, lex.val_str);
                                                        
                                                        if (!parse_expect(&lex, TOK_COLON)) { fclose(fp); return -1; }
                                                        if (!parse_expect(&lex, TOK_STRING)) { fclose(fp); return -1; }
                                                        
                                                        if (strcmp(pkey, "value") == 0) {
                                                            strcpy(p_val, lex.val_str);
                                                        } else if (strcmp(pkey, "block") == 0) {
                                                            strcpy(p_blk, lex.val_str);
                                                        }
                                                    }
                                                    
                                                    if (n->phi_count >= p_cap) {
                                                        p_cap *= 2;
                                                        n->phi_ops = (ir_phi_operand_t *)realloc(n->phi_ops, p_cap * sizeof(ir_phi_operand_t));
                                                    }
                                                    strcpy(n->phi_ops[n->phi_count].value, p_val);
                                                    strcpy(n->phi_ops[n->phi_count].block, p_blk);
                                                    n->phi_count++;
                                                }
                                                n->phi_capacity = p_cap;
                                            } else {
                                                tok_kind_t val_tok = lex_next(&lex);
                                                skip_json_value(&lex, val_tok);
                                            }
                                        }
                                        ir_append(fn, n);
                                    }
                                } else {
                                    tok_kind_t val_tok = lex_next(&lex);
                                    skip_json_value(&lex, val_tok);
                                }
                            }
                        }
                    } else {
                        tok_kind_t val_tok = lex_next(&lex);
                        skip_json_value(&lex, val_tok);
                    }
                }
            }
        } else {
            tok_kind_t val_tok = lex_next(&lex);
            skip_json_value(&lex, val_tok);
        }
    }
    
    fclose(fp);
    return 0;
}

int ir_diff_json(const char *path_a, const char *path_b) {
    ir_module_t *mod_a = ir_module_create();
    ir_module_t *mod_b = ir_module_create();
    int f_idx;
    
    if (ir_deserialize_json(mod_a, path_a, NULL) != 0) {
        fprintf(stderr, "diff-ir: failed to deserialize graph A '%s'\n", path_a);
        ir_module_free(mod_a);
        ir_module_free(mod_b);
        return 1;
    }
    if (ir_deserialize_json(mod_b, path_b, NULL) != 0) {
        fprintf(stderr, "diff-ir: failed to deserialize graph B '%s'\n", path_b);
        ir_module_free(mod_a);
        ir_module_free(mod_b);
        return 1;
    }
    
    if (mod_a->func_count != mod_b->func_count) {
        printf("[diff-ir] MISMATCH: function count A = %d, B = %d\n", mod_a->func_count, mod_b->func_count);
        ir_module_free(mod_a);
        ir_module_free(mod_b);
        return 1;
    }
    
    for (f_idx = 0; f_idx < mod_a->func_count; f_idx++) {
        ir_func_t *fn_a = mod_a->funcs[f_idx];
        ir_func_t *fn_b = NULL;
        int j;
        
        for (j = 0; j < mod_b->func_count; j++) {
            if (strcmp(mod_b->funcs[j]->name, fn_a->name) == 0) {
                fn_b = mod_b->funcs[j];
                break;
            }
        }
        
        if (!fn_b) {
            printf("[diff-ir] MISMATCH: function '%s' in A not found in B\n", fn_a->name);
            ir_module_free(mod_a);
            ir_module_free(mod_b);
            return 1;
        }
        
        dom_cfg_t cfg_a, cfg_b;
        memset(&cfg_a, 0, sizeof(dom_cfg_t));
        memset(&cfg_b, 0, sizeof(dom_cfg_t));
        dom_build_cfg(&cfg_a, fn_a);
        dom_build_cfg(&cfg_b, fn_b);
        
        if (cfg_a.block_count != cfg_b.block_count) {
            printf("[diff-ir] MISMATCH in '%s()': basic block count A = %d, B = %d\n",
                   fn_a->name, cfg_a.block_count, cfg_b.block_count);
            ir_module_free(mod_a);
            ir_module_free(mod_b);
            return 1;
        }
        
        int b_idx;
        for (b_idx = 0; b_idx < cfg_a.block_count; b_idx++) {
            dom_bb_t *bb_a = &cfg_a.blocks[b_idx];
            dom_bb_t *bb_b = &cfg_b.blocks[b_idx];
            
            if (bb_a->pred_count != bb_b->pred_count) {
                printf("[diff-ir] MISMATCH in '%s()' BB%d: predecessor count A = %d, B = %d\n",
                       fn_a->name, b_idx, bb_a->pred_count, bb_b->pred_count);
                ir_module_free(mod_a);
                ir_module_free(mod_b);
                return 1;
            }
            if (bb_a->succ_count != bb_b->succ_count) {
                printf("[diff-ir] MISMATCH in '%s()' BB%d: successor count A = %d, B = %d\n",
                       fn_a->name, b_idx, bb_a->succ_count, bb_b->succ_count);
                ir_module_free(mod_a);
                ir_module_free(mod_b);
                return 1;
            }
            
            ir_node_t *n_a = bb_a->first;
            ir_node_t *n_b = bb_b->first;
            int inst_idx = 0;
            
            while (n_a && n_b) {
                if (n_a->op != n_b->op) {
                    printf("[diff-ir] MISMATCH in '%s()' BB%d instruction %d: opcode A = %s, B = %s\n",
                           fn_a->name, b_idx, inst_idx, ir_op_name(n_a->op), ir_op_name(n_b->op));
                    ir_module_free(mod_a);
                    ir_module_free(mod_b);
                    return 1;
                }
                if (n_a->type != n_b->type) {
                    printf("[diff-ir] MISMATCH in '%s()' BB%d instruction %d: type A = %s, B = %s\n",
                           fn_a->name, b_idx, inst_idx, ir_type_name(n_a->type), ir_type_name(n_b->type));
                    ir_module_free(mod_a);
                    ir_module_free(mod_b);
                    return 1;
                }
                if (strcmp(n_a->dst, n_b->dst) != 0) {
                    printf("[diff-ir] MISMATCH in '%s()' BB%d instruction %d: dst register assignment A = '%s', B = '%s'\n",
                           fn_a->name, b_idx, inst_idx, n_a->dst, n_b->dst);
                    ir_module_free(mod_a);
                    ir_module_free(mod_b);
                    return 1;
                }
                if (strcmp(n_a->src1, n_b->src1) != 0 || strcmp(n_a->src2, n_b->src2) != 0) {
                    printf("[diff-ir] MISMATCH in '%s()' BB%d instruction %d: sources A = ('%s', '%s'), B = ('%s', '%s')\n",
                           fn_a->name, b_idx, inst_idx, n_a->src1, n_a->src2, n_b->src1, n_b->src2);
                    ir_module_free(mod_a);
                    ir_module_free(mod_b);
                    return 1;
                }
                if (n_a->imm != n_b->imm) {
                    printf("[diff-ir] MISMATCH in '%s()' BB%d instruction %d: immediate value A = %ld, B = %ld\n",
                           fn_a->name, b_idx, inst_idx, n_a->imm, n_b->imm);
                    ir_module_free(mod_a);
                    ir_module_free(mod_b);
                    return 1;
                }
                
                if (n_a == bb_a->last || n_b == bb_b->last) {
                    if (n_a != bb_a->last || n_b != bb_b->last) {
                        printf("[diff-ir] MISMATCH in '%s()' BB%d: instruction density mismatch (one block has more instructions than the other)\n",
                               fn_a->name, b_idx);
                        ir_module_free(mod_a);
                        ir_module_free(mod_b);
                        return 1;
                    }
                    break;
                }
                n_a = n_a->next;
                n_b = n_b->next;
                inst_idx++;
            }
        }
    }
    
    printf("[diff-ir] CFG topologies and instruction densities identical! OK\n");
    ir_module_free(mod_a);
    ir_module_free(mod_b);
    return 0;
}
