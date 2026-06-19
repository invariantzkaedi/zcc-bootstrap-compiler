/*
 * ir_serialization.c — ZCC IR Serialization & Replay Engine
 *
 * Freestanding C89-compliant implementation with zero external library dependencies.
 * Custom line-oriented text format replacing JSON.
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
    int telemetry_depth;
};

/* ========================================================================= */
/* HELPERS                                                                   */
/* ========================================================================= */

static void escape_string(char *dst, const char *src) {
    int i = 0, j = 0;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    for (i = 0; src[i] != '\0'; i++) {
        char c = src[i];
        if (c == '\n') { dst[j++] = '\\'; dst[j++] = 'n'; }
        else if (c == '\t') { dst[j++] = '\\'; dst[j++] = 't'; }
        else if (c == '\r') { dst[j++] = '\\'; dst[j++] = 'r'; }
        else if (c == '\\') { dst[j++] = '\\'; dst[j++] = '\\'; }
        else if (c == '"') { dst[j++] = '\\'; dst[j++] = '"'; }
        else dst[j++] = c;
    }
    dst[j] = '\0';
}

static void unescape_string(char *dst, const char *src) {
    int i = 0, j = 0;
    while (src[i] != '\0') {
        if (src[i] == '\\') {
            i++;
            if (src[i] == 'n') dst[j++] = '\n';
            else if (src[i] == 't') dst[j++] = '\t';
            else if (src[i] == 'r') dst[j++] = '\r';
            else if (src[i] == '\\') dst[j++] = '\\';
            else if (src[i] == '"') dst[j++] = '"';
            else dst[j++] = src[i];
        } else {
            dst[j++] = src[i];
        }
        i++;
    }
    dst[j] = '\0';
}

static int find_arg(const char *line, const char *key, char *val_buf) {
    const char *p = strstr(line, key);
    if (!p) {
        val_buf[0] = '\0';
        return 0;
    }
    p += strlen(key);
    if (*p == '"') {
        int i = 0;
        p++;
        while (*p && *p != '"') {
            if (*p == '\\') {
                p++;
                if (*p == 'n') val_buf[i++] = '\n';
                else if (*p == 't') val_buf[i++] = '\t';
                else if (*p == 'r') val_buf[i++] = '\r';
                else if (*p == '\\') val_buf[i++] = '\\';
                else if (*p == '"') val_buf[i++] = '"';
                else val_buf[i++] = *p;
            } else {
                val_buf[i++] = *p;
            }
            p++;
        }
        val_buf[i] = '\0';
    } else {
        int i = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            val_buf[i++] = *p;
            p++;
        }
        val_buf[i] = '\0';
    }
    return 1;
}

static long long find_arg_int(const char *line, const char *key, long long default_val) {
    char buf[256];
    if (find_arg(line, key, buf)) {
        return atoll(buf);
    }
    return default_val;
}

static int parse_id_list(const char *str, int *out_ids, int max_ids) {
    int count = 0;
    const char *p = str;
    while (*p && count < max_ids) {
        out_ids[count++] = atoi(p);
        p = strchr(p, ',');
        if (!p) break;
        p++;
    }
    return count;
}

static int parse_string_list(const char *str, char out_names[][IR_NAME_MAX], int max_names) {
    int count = 0;
    const char *p = str;
    while (*p && count < max_names) {
        int i = 0;
        if (*p == '"') p++;
        while (*p && *p != '"' && i < IR_NAME_MAX - 1) {
            out_names[count][i++] = *p++;
        }
        out_names[count][i] = '\0';
        if (*p == '"') p++;
        count++;
        if (*p == ',') p++;
        else break;
    }
    return count;
}

static int parse_phi_list(const char *str, ir_phi_operand_t *ops, int max_ops) {
    int count = 0;
    const char *p = str;
    while (*p && count < max_ops) {
        int i = 0;
        if (*p == '"') p++;
        while (*p && *p != '"' && i < IR_NAME_MAX - 1) {
            ops[count].value[i++] = *p++;
        }
        ops[count].value[i] = '\0';
        if (*p == '"') p++;
        if (*p == ':') p++;
        if (*p == '"') p++;
        i = 0;
        while (*p && *p != '"' && i < IR_LABEL_MAX - 1) {
            ops[count].block[i++] = *p++;
        }
        ops[count].block[i] = '\0';
        if (*p == '"') p++;
        count++;
        if (*p == ',') p++;
        else break;
    }
    return count;
}

static int get_gvar_size(const struct Node *gvar) {
    if (!gvar || !gvar->type) return 8;
    extern int type_size(Type *t);
    return type_size(gvar->type);
}

/* ========================================================================= */
/* TYPE & NODE GRAPH TRAVERSAL                                              */
/* ========================================================================= */

#define MAX_TYPES_LIST 65536
static Type *types_list_ser[MAX_TYPES_LIST];
static int num_types_ser = 0;

static int collect_type(Type *t) {
    int i;
    if (!t) return -1;
    for (i = 0; i < num_types_ser; i++) {
        if (types_list_ser[i] == t) return i;
    }
    if (num_types_ser >= MAX_TYPES_LIST) {
        fprintf(stderr, "ir_serialization: too many unique types\n");
        exit(1);
    }
    int id = num_types_ser++;
    types_list_ser[id] = t;
    if (t->base) collect_type(t->base);
    if (t->ret) collect_type(t->ret);
    for (i = 0; i < t->num_params; i++) {
        collect_type(t->params[i]);
    }
    StructField *f = t->fields;
    while (f) {
        collect_type(f->type);
        f = f->next;
    }
    return id;
}

#define MAX_NODES_LIST 1048576
static Node *nodes_list_ser[MAX_NODES_LIST];
static int num_nodes_ser = 0;

static int collect_node(Node *n) {
    int i;
    if (!n) return -1;
    for (i = 0; i < num_nodes_ser; i++) {
        if (nodes_list_ser[i] == n) return i;
    }
    if (num_nodes_ser >= MAX_NODES_LIST) {
        fprintf(stderr, "ir_serialization: too many unique initializer nodes\n");
        exit(1);
    }
    int id = num_nodes_ser++;
    nodes_list_ser[id] = n;
    if (n->lhs) collect_node(n->lhs);
    if (n->rhs) collect_node(n->rhs);
    for (i = 0; i < n->num_args; i++) {
        collect_node(n->args[i]);
    }
    return id;
}

/* ========================================================================= */
/* PUBLIC API                                                                */
/* ========================================================================= */

int ir_serialize_json(const ir_module_t *mod, const char *out_filename, const char *source_file, const struct Compiler *cc) {
    FILE *fp = fopen(out_filename, "w");
    int i, j;
    if (!fp) {
        return -1;
    }

    fprintf(fp, "; ZCC IR Graph v1.0.0\n");
    fprintf(fp, "; source: \"%s\"\n\n", source_file);

    /* 1. Strings Table */
    if (cc && cc->num_strings > 0) {
        for (i = 0; i < cc->num_strings; i++) {
            char esc[MAX_STR];
            escape_string(esc, cc->strings[i].data);
            fprintf(fp, "string label_id=%d len=%d data=\"%s\"\n", cc->strings[i].label_id, cc->strings[i].len, esc);
        }
        fprintf(fp, "\n");
    }

    /* Collect types and nodes */
    num_types_ser = 0;
    num_nodes_ser = 0;
    if (cc) {
        for (i = 0; i < cc->num_globals; i++) {
            Node *gvar = cc->globals[i];
            if (gvar && gvar->kind == 61 /* ND_GLOBAL_VAR */) {
                if (gvar->is_extern) continue;
                collect_type(gvar->type);
                if (gvar->initializer) {
                    collect_node(gvar->initializer);
                }
            }
        }
        /* Collect types inside collected nodes as well */
        for (i = 0; i < num_nodes_ser; i++) {
            collect_type(nodes_list_ser[i]->type);
        }
    }

    /* 2. Type Table declarations */
    for (i = 0; i < num_types_ser; i++) {
        Type *t = types_list_ser[i];
        fprintf(fp, "type %d kind=%d size=%d align=%d is_complete=%d is_packed=%d explicit_align=%d is_tbfp=%d array_len=%d",
                i, t->kind, t->size, t->align, t->is_complete, t->is_packed, t->explicit_align, t->is_tbfp, t->array_len);
        if (t->tag[0]) {
            char esc[256];
            escape_string(esc, t->tag);
            fprintf(fp, " tag=\"%s\"", esc);
        }
        fprintf(fp, "\n");
    }

    /* type links */
    for (i = 0; i < num_types_ser; i++) {
        Type *t = types_list_ser[i];
        if (t->base || t->ret || t->num_params > 0) {
            fprintf(fp, "type_link %d base=%d ret=%d", i, collect_type(t->base), collect_type(t->ret));
            if (t->num_params > 0) {
                fprintf(fp, " params=");
                for (j = 0; j < t->num_params; j++) {
                    fprintf(fp, "%d%s", collect_type(t->params[j]), (j + 1 < t->num_params) ? "," : "");
                }
            }
            fprintf(fp, "\n");
        }
    }

    /* type fields */
    for (i = 0; i < num_types_ser; i++) {
        Type *t = types_list_ser[i];
        StructField *f = t->fields;
        while (f) {
            char esc[256];
            escape_string(esc, f->name);
            fprintf(fp, "type_field %d name=\"%s\" type=%d offset=%d is_bitfield=%d bit_offset=%d bit_size=%d\n",
                    i, esc, collect_type(f->type), f->offset, f->is_bitfield, f->bit_offset, f->bit_size);
            f = f->next;
        }
    }
    fprintf(fp, "\n");

    /* 3. Initializer Nodes */
    for (i = 0; i < num_nodes_ser; i++) {
        Node *n = nodes_list_ser[i];
        fprintf(fp, "init_node %d kind=%d type=%d int_val=%lld f_val=%g str_id=%d",
                i, n->kind, collect_type(n->type), n->int_val, n->f_val, n->str_id);
        if (n->name[0]) {
            char esc[256];
            escape_string(esc, n->name);
            fprintf(fp, " name=\"%s\"", esc);
        }
        fprintf(fp, "\n");
    }

    /* initializer links */
    for (i = 0; i < num_nodes_ser; i++) {
        Node *n = nodes_list_ser[i];
        if (n->lhs || n->rhs || n->num_args > 0) {
            fprintf(fp, "init_link %d lhs=%d rhs=%d", i, collect_node(n->lhs), collect_node(n->rhs));
            if (n->num_args > 0) {
                fprintf(fp, " args=");
                for (j = 0; j < n->num_args; j++) {
                    fprintf(fp, "%d%s", collect_node(n->args[j]), (j + 1 < n->num_args) ? "," : "");
                }
            }
            fprintf(fp, "\n");
        }
    }
    fprintf(fp, "\n");

    /* 4. Global Variables */
    if (cc) {
        for (i = 0; i < cc->num_globals; i++) {
            Node *gvar = cc->globals[i];
            if (gvar && gvar->kind == 61 /* ND_GLOBAL_VAR */) {
                if (gvar->is_extern) continue;
                char esc[256];
                escape_string(esc, gvar->name);
                fprintf(fp, "global name=\"%s\" type=%d static=%d extern=%d init=%d\n",
                        esc, collect_type(gvar->type), gvar->is_static, gvar->is_extern, collect_node(gvar->initializer));
            }
        }
        fprintf(fp, "\n");
    }

    /* 5. Functions, Blocks, and Instructions */
    for (int f_idx = 0; f_idx < mod->func_count; f_idx++) {
        ir_func_t *fn = mod->funcs[f_idx];
        char esc[256];
        escape_string(esc, fn->name);
        fprintf(fp, "func name=\"%s\" ret=%s num_params=%d", esc, ir_type_name(fn->ret_type), fn->num_params);
        if (fn->num_params > 0) {
            fprintf(fp, " params=");
            for (j = 0; j < fn->num_params; j++) {
                char esc_p[256];
                escape_string(esc_p, fn->param_names[j]);
                fprintf(fp, "\"%s\"%s", esc_p, (j + 1 < fn->num_params) ? "," : "");
            }
        }
        fprintf(fp, "\n");

        dom_cfg_t cfg;
        memset(&cfg, 0, sizeof(dom_cfg_t));
        dom_build_cfg(&cfg, fn);
        dom_compute_idom(&cfg);
        dom_build_tree(&cfg);

        df_set_t *df_sets = (df_set_t *)calloc(cfg.block_count, sizeof(df_set_t));
        for (i = 0; i < cfg.block_count; i++) {
            df_sets[i].capacity = 8;
            df_sets[i].frontier = (int *)malloc(df_sets[i].capacity * sizeof(int));
            df_sets[i].count = 0;
        }

        for (int b = 0; b < cfg.block_count; b++) {
            if (cfg.blocks[b].pred_count > 1) {
                for (j = 0; j < cfg.blocks[b].pred_count; j++) {
                    int runner = cfg.blocks[b].pred[j];
                    while (runner != -1 && runner != cfg.blocks[b].idom) {
                        df_set_t *set = &df_sets[runner];
                        int found = 0;
                        for (int k = 0; k < set->count; k++) {
                            if (set->frontier[k] == b) {
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            if (set->count >= set->capacity) {
                                set->capacity *= 2;
                                set->frontier = (int *)realloc(set->frontier, set->capacity * sizeof(int));
                            }
                            set->frontier[set->count++] = b;
                        }
                        runner = cfg.blocks[runner].idom;
                    }
                }
            }
        }

        for (i = 0; i < cfg.block_count; i++) {
            dom_bb_t *bb = &cfg.blocks[i];
            char esc_lbl[256];
            escape_string(esc_lbl, bb->label);
            fprintf(fp, "block id=%d label=\"%s\"", bb->id, esc_lbl);
            fprintf(fp, " preds=");
            for (j = 0; j < bb->pred_count; j++) {
                fprintf(fp, "%d%s", bb->pred[j], (j + 1 < bb->pred_count) ? "," : "");
            }
            fprintf(fp, " succs=");
            for (j = 0; j < bb->succ_count; j++) {
                fprintf(fp, "%d%s", bb->succ[j], (j + 1 < bb->succ_count) ? "," : "");
            }
            if (df_sets && df_sets[i].count > 0) {
                fprintf(fp, " df=");
                for (j = 0; j < df_sets[i].count; j++) {
                    fprintf(fp, "%d%s", df_sets[i].frontier[j], (j + 1 < df_sets[i].count) ? "," : "");
                }
            }
            fprintf(fp, "\n");

            ir_node_t *n = bb->first;
            while (n) {
                char esc_dst[256], esc_src1[256], esc_src2[256], esc_l1[256], esc_l2[256];
                escape_string(esc_dst, n->dst);
                escape_string(esc_src1, n->src1);
                escape_string(esc_src2, n->src2);
                escape_string(esc_l1, n->label);
                escape_string(esc_l2, n->label2);

                fprintf(fp, "  inst op=%s type=%s dst=\"%s\" src1=\"%s\" src2=\"%s\" label=\"%s\" label2=\"%s\" imm=%ld lineno=%d tag=%d vuln_tags=%u flags=%u",
                        ir_op_name(n->op), ir_type_name(n->type), esc_dst, esc_src1, esc_src2, esc_l1, esc_l2, n->imm, n->lineno, n->tag, n->vuln_tags, n->flags);

                if (n->phi_count > 0) {
                    fprintf(fp, " phi=");
                    for (j = 0; j < n->phi_count; j++) {
                        char esc_v[256], esc_b[256];
                        escape_string(esc_v, n->phi_ops[j].value);
                        escape_string(esc_b, n->phi_ops[j].block);
                        fprintf(fp, "\"%s\":\"%s\"%s", esc_v, esc_b, (j + 1 < n->phi_count) ? "," : "");
                    }
                }

                if (n->asm_string) {
                    char esc_asm[4096];
                    escape_string(esc_asm, n->asm_string);
                    fprintf(fp, " asm=\"%s\"", esc_asm);
                }

                fprintf(fp, "\n");

                if (n == bb->last) break;
                n = n->next;
            }
        }
        if (df_sets) {
            for (int k = 0; k < cfg.block_count; k++) {
                free(df_sets[k].frontier);
            }
            free(df_sets);
        }
        fprintf(fp, "end func\n\n");
    }

    fclose(fp);
    return 0;
}

static Symbol *local_scope_find(Compiler *cc, const char *name) {
    Scope *s;
    Symbol *sym;
    if (!cc) return NULL;
    s = cc->current_scope;
    while (s) {
        sym = s->symbols;
        while (sym) {
            if (strcmp(sym->name, name) == 0) return sym;
            sym = sym->next;
        }
        s = s->parent;
    }
    return NULL;
}

int ir_deserialize_json(ir_module_t *mod, const char *in_filename, struct Compiler *cc) {
    FILE *fp = fopen(in_filename, "r");
    char line[32768];
    Type **types_list = NULL;
    Node **nodes_list = NULL;
    ir_func_t *curr_fn = NULL;

    if (!fp) {
        fprintf(stderr, "ir_serialization: cannot open input file %s\n", in_filename);
        return -1;
    }

    if (cc) {
        if (!cc->current_scope) {
            extern void *cc_alloc(struct Compiler *cc, int size);
            Scope *gs = (Scope *)cc_alloc(cc, sizeof(Scope));
            gs->parent = NULL;
            gs->symbols = NULL;
            cc->current_scope = gs;
        }
    }

    types_list = (Type **)calloc(65536, sizeof(Type *));
    nodes_list = (Node **)calloc(1048576, sizeof(Node *));

    /* PASS 1: Allocate Types, Nodes, Strings, and Globals skeleton */
    while (fgets(line, sizeof(line), fp)) {
        /* strip comment/whitespace */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';' || *p == '\n' || *p == '\r' || *p == '\0') continue;

        if (strncmp(p, "string", 6) == 0) {
            int s_lbl = (int)find_arg_int(p, "label_id=", -1);
            int s_len = (int)find_arg_int(p, "len=", 0);
            char s_data[MAX_STR] = "";
            find_arg(p, "data=", s_data);

            if (cc && cc->num_strings < MAX_STRINGS) {
                struct StringEntry *se = &cc->strings[cc->num_strings++];
                se->label_id = s_lbl;
                se->len = s_len;
                extern void *cc_alloc(struct Compiler *cc, int size);
                se->data = (char *)cc_alloc(cc, s_len + 1);
                memcpy(se->data, s_data, s_len);
                se->data[s_len] = '\0';
            }
        } else if (strncmp(p, "type ", 5) == 0) {
            int id = -1;
            if (sscanf(p, "type %d", &id) == 1 && id >= 0 && id < 65536) {
                extern void *cc_alloc(struct Compiler *cc, int size);
                Type *t = (Type *)cc_alloc(cc, sizeof(Type));
                t->magic = 0x8877665544332211ULL;
                t->kind = (int)find_arg_int(p, "kind=", 0);
                t->size = (int)find_arg_int(p, "size=", 0);
                t->align = (int)find_arg_int(p, "align=", 1);
                t->is_complete = (int)find_arg_int(p, "is_complete=", 0);
                t->is_packed = (int)find_arg_int(p, "is_packed=", 0);
                t->explicit_align = (int)find_arg_int(p, "explicit_align=", 0);
                t->is_tbfp = (int)find_arg_int(p, "is_tbfp=", 0);
                t->array_len = (int)find_arg_int(p, "array_len=", 0);
                
                char tag[256];
                if (find_arg(p, "tag=", tag)) {
                    strcpy(t->tag, tag);
                }
                types_list[id] = t;
            }
        } else if (strncmp(p, "init_node", 9) == 0) {
            int id = -1;
            if (sscanf(p, "init_node %d", &id) == 1 && id >= 0 && id < 1048576) {
                extern void *cc_alloc(struct Compiler *cc, int size);
                Node *n = (Node *)cc_alloc(cc, sizeof(Node));
                n->magic = 0xC0FFEEBAD1234567ULL;
                n->kind = (int)find_arg_int(p, "kind=", 0);
                n->int_val = find_arg_int(p, "int_val=", 0);
                
                char f_val_str[256];
                if (find_arg(p, "f_val=", f_val_str)) {
                    n->f_val = atof(f_val_str);
                }
                n->str_id = (int)find_arg_int(p, "str_id=", 0);
                
                char name[256];
                if (find_arg(p, "name=", name)) {
                    strcpy(n->name, name);
                }
                nodes_list[id] = n;
            }
        }
    }

    /* PASS 2: Link Types, Nodes, parse Globals, Functions and Instructions */
    rewind(fp);
    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';' || *p == '\n' || *p == '\r' || *p == '\0') continue;

        if (strncmp(p, "type_link", 9) == 0) {
            int id = -1;
            if (sscanf(p, "type_link %d", &id) == 1 && id >= 0 && id < 65536 && types_list[id]) {
                Type *t = types_list[id];
                int base_id = (int)find_arg_int(p, "base=", -1);
                int ret_id = (int)find_arg_int(p, "ret=", -1);
                if (base_id != -1) t->base = types_list[base_id];
                if (ret_id != -1) t->ret = types_list[ret_id];

                char params_str[4096];
                if (find_arg(p, "params=", params_str)) {
                    int param_ids[MAX_PARAMS];
                    int n_params = parse_id_list(params_str, param_ids, MAX_PARAMS);
                    t->num_params = n_params;
                    extern void *cc_alloc(struct Compiler *cc, int size);
                    t->params = (Type **)cc_alloc(cc, n_params * sizeof(Type *));
                    for (int k = 0; k < n_params; k++) {
                        t->params[k] = types_list[param_ids[k]];
                    }
                }
            }
        } else if (strncmp(p, "type_field", 10) == 0) {
            int id = -1;
            if (sscanf(p, "type_field %d", &id) == 1 && id >= 0 && id < 65536 && types_list[id]) {
                Type *t = types_list[id];
                char f_name[256];
                find_arg(p, "name=", f_name);
                int f_type_id = (int)find_arg_int(p, "type=", -1);
                int f_offset = (int)find_arg_int(p, "offset=", 0);
                int f_is_bf = (int)find_arg_int(p, "is_bitfield=", 0);
                int f_bit_off = (int)find_arg_int(p, "bit_offset=", 0);
                int f_bit_sz = (int)find_arg_int(p, "bit_size=", 0);

                extern void *cc_alloc(struct Compiler *cc, int size);
                StructField *f = (StructField *)cc_alloc(cc, sizeof(StructField));
                strcpy(f->name, f_name);
                f->type = types_list[f_type_id];
                f->offset = f_offset;
                f->is_bitfield = f_is_bf;
                f->bit_offset = f_bit_off;
                f->bit_size = f_bit_sz;
                f->next = NULL;

                if (!t->fields) {
                    t->fields = f;
                } else {
                    StructField *curr = t->fields;
                    while (curr->next) curr = curr->next;
                    curr->next = f;
                }
            }
        } else if (strncmp(p, "init_node", 9) == 0) {
            int id = -1;
            if (sscanf(p, "init_node %d", &id) == 1 && id >= 0 && id < 1048576 && nodes_list[id]) {
                int type_id = (int)find_arg_int(p, "type=", -1);
                if (type_id != -1) nodes_list[id]->type = types_list[type_id];
            }
        } else if (strncmp(p, "init_link", 9) == 0) {
            int id = -1;
            if (sscanf(p, "init_link %d", &id) == 1 && id >= 0 && id < 1048576 && nodes_list[id]) {
                Node *n = nodes_list[id];
                int lhs_id = (int)find_arg_int(p, "lhs=", -1);
                int rhs_id = (int)find_arg_int(p, "rhs=", -1);
                if (lhs_id != -1) n->lhs = nodes_list[lhs_id];
                if (rhs_id != -1) n->rhs = nodes_list[rhs_id];

                char args_str[4096];
                if (find_arg(p, "args=", args_str)) {
                    int arg_ids[MAX_CALL_ARGS];
                    int n_args = parse_id_list(args_str, arg_ids, MAX_CALL_ARGS);
                    n->num_args = n_args;
                    extern void *cc_alloc(struct Compiler *cc, int size);
                    n->args = (Node **)cc_alloc(cc, n_args * sizeof(Node *));
                    for (int k = 0; k < n_args; k++) {
                        n->args[k] = nodes_list[arg_ids[k]];
                    }
                }
            }
        } else if (strncmp(p, "global", 6) == 0) {
            char g_name[256];
            find_arg(p, "name=", g_name);
            int g_type_id = (int)find_arg_int(p, "type=", -1);
            int g_static = (int)find_arg_int(p, "static=", 0);
            int g_extern = (int)find_arg_int(p, "extern=", 0);
            int g_init_id = (int)find_arg_int(p, "init=", -1);

            if (cc && cc->num_globals < MAX_GLOBALS) {
                extern void *cc_alloc(struct Compiler *cc, int size);
                Node *gvar = (Node *)cc_alloc(cc, sizeof(Node));
                Symbol *sym = NULL;
                gvar->magic = 0xC0FFEEBAD1234567ULL;
                gvar->kind = 61; /* ND_GLOBAL_VAR */
                strcpy(gvar->name, g_name);
                gvar->type = types_list[g_type_id];
                gvar->is_static = g_static;
                gvar->is_extern = g_extern;
                if (g_init_id != -1) gvar->initializer = nodes_list[g_init_id];

                cc->globals[cc->num_globals++] = gvar;

                if (cc->current_scope) {
                    sym = (Symbol *)cc_alloc(cc, sizeof(Symbol));
                    strcpy(sym->name, g_name);
                    sym->type = gvar->type;
                    sym->is_global = 1;
                    sym->is_local = 0;
                    sym->next = cc->current_scope->symbols;
                    cc->current_scope->symbols = sym;
                }
            }
        } else if (strncmp(p, "func", 4) == 0) {
            char fn_name[256];
            find_arg(p, "name=", fn_name);
            char fn_ret_str[256];
            find_arg(p, "ret=", fn_ret_str);
            int fn_num_params = (int)find_arg_int(p, "num_params=", 0);

            ir_type_t fn_ret = IR_TY_VOID;
            for (int t = 0; t < 12; t++) {
                if (strcmp(ir_type_name(t), fn_ret_str) == 0) {
                    fn_ret = (ir_type_t)t;
                    break;
                }
            }

            curr_fn = ir_func_create(mod, fn_name, fn_ret, fn_num_params);

            char params_str[4096];
            if (find_arg(p, "params=", params_str)) {
                char param_names[8][IR_NAME_MAX];
                int p_count = parse_string_list(params_str, param_names, 8);
                for (int k = 0; k < p_count && k < 8; k++) {
                    strcpy(curr_fn->param_names[k], param_names[k]);
                }
            }

            if (cc && cc->current_scope) {
                extern void *cc_alloc(struct Compiler *cc, int size);
                Symbol *sym = (Symbol *)cc_alloc(cc, sizeof(Symbol));
                Type *ft = (Type *)cc_alloc(cc, sizeof(Type));
                strcpy(sym->name, fn_name);
                ft->magic = 0x8877665544332211ULL;
                ft->kind = 10; /* TY_FUNC */
                ft->size = 8;
                ft->align = 8;
                sym->type = ft;
                sym->is_global = 1;
                sym->is_local = 0;
                sym->next = cc->current_scope->symbols;
                cc->current_scope->symbols = sym;
            }
        } else if (strncmp(p, "inst", 4) == 0 && curr_fn) {
            char op_str[256], type_str[256];
            find_arg(p, "op=", op_str);
            find_arg(p, "type=", type_str);

            ir_op_t op = IR_NOP;
            for (int o = 0; o < IR_OP_COUNT; o++) {
                if (strcmp(ir_op_name(o), op_str) == 0) {
                    op = (ir_op_t)o;
                    break;
                }
            }

            ir_type_t ty = IR_TY_VOID;
            for (int t = 0; t < 12; t++) {
                if (strcmp(ir_type_name(t), type_str) == 0) {
                    ty = (ir_type_t)t;
                    break;
                }
            }

            ir_node_t *n = ir_node_alloc();
            n->op = op;
            n->type = ty;
            find_arg(p, "dst=", n->dst);
            find_arg(p, "src1=", n->src1);
            find_arg(p, "src2=", n->src2);
            find_arg(p, "label=", n->label);
            find_arg(p, "label2=", n->label2);
            n->imm = find_arg_int(p, "imm=", 0);
            n->lineno = (int)find_arg_int(p, "lineno=", 0);
            n->tag = (int)find_arg_int(p, "tag=", 0);
            n->vuln_tags = (unsigned int)find_arg_int(p, "vuln_tags=", 0);
            n->flags = (unsigned int)find_arg_int(p, "flags=", 0);

            char phi_str[4096];
            if (find_arg(p, "phi=", phi_str)) {
                /* count commas to determine phi count roughly */
                int comm = 0;
                for (int c = 0; phi_str[c]; c++) if (phi_str[c] == ',') comm++;
                int phi_cap = comm + 2;
                n->phi_ops = (ir_phi_operand_t *)calloc(phi_cap, sizeof(ir_phi_operand_t));
                n->phi_count = parse_phi_list(phi_str, n->phi_ops, phi_cap);
                n->phi_capacity = phi_cap;
            }

            char asm_str[4096];
            if (find_arg(p, "asm=", asm_str)) {
                n->asm_string = strdup(asm_str);
            }

            ir_append(curr_fn, n);
        } else if (strncmp(p, "end func", 8) == 0) {
            curr_fn = NULL;
        }
    }

    /* PASS 3: Resolve node symbols */
    if (cc) {
        int idx;
        for (idx = 0; idx < 1048576; idx++) {
            Node *n = nodes_list[idx];
            if (n && n->name[0] != '\0') {
                n->sym = local_scope_find(cc, n->name);
            }
        }
    }

    free(types_list);
    free(nodes_list);
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
                        printf("[diff-ir] MISMATCH in '%s()' BB%d: instruction density mismatch\n",
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
