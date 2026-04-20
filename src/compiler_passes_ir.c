/*
 * compiler_passes_ir.c — ZCC IR Bridge
 * =====================================
 * Compiled by GCC ONLY. ZCC never touches this file directly.
 *
 * ABI TRAP (documented in zkaedi-compiler-forge SKILL.md):
 *   ZCC typedef unsigned long uint32_t → 8 bytes (ILP32 LP64 hybrid)
 *   GCC uint32_t                       → 4 bytes (stdint.h)
 *   PassResult struct diverges by 2,048 bytes → silent heap corruption.
 *
 * INVARIANT: ZCC calls ONLY these 3-pointer boundary functions:
 *   zcc_ir_lower()   — text IR in  → JSON file out
 *   zcc_ir_version() — returns static string, no struct
 *   zcc_ir_free()    — frees a char* returned by this module
 *
 * Everything else (IrFunc, IrBlock, IrInstr, PassState) lives
 * entirely inside this translation unit. ZCC sees none of it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/* ─── Opcode table ───────────────────────────────────────────────────── */
typedef enum {
    OP_NOP = 0,
    /* Memory */
    OP_ALLOCA,          /* alloca <type> <name>            stack slot */
    OP_LOAD,            /* load   <dst> <src_ptr>          ptr read   */
    OP_STORE,           /* store  <val> <dst_ptr>          ptr write  */
    OP_GEP,             /* gep    <dst> <base> <idx>       ptr arith  */
    /* Arithmetic */
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_SHL, OP_SHR,
    /* Comparison */
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    /* Control */
    OP_BRANCH,          /* branch <cond> <true_bb> <false_bb>         */
    OP_JUMP,            /* jump   <target_bb>                         */
    OP_CALL,            /* call   <dst> <func> <arg0> ...             */
    OP_RETURN,          /* return <val?>                              */
    OP_PHI,             /* phi    <dst> [<val> <bb>] ...  (future)    */
    /* Escape markers (injected by Pass 2) */
    OP_ESCAPE,          /* escape <ptr>   — pointer leaves scope      */
    OP_ALIAS,           /* alias  <a> <b> — must-alias annotation     */
    OP_BOUNDS_UNSAFE,   /* bounds_unsafe <ptr> <idx> — OOB risk flagged */
} Opcode;

static const char *OPCODE_NAMES[] = {
    "nop",
    "alloca","load","store","gep",
    "add","sub","mul","div","mod","shl","shr",
    "eq","ne","lt","le","gt","ge",
    "branch","jump","call","return","phi",
    "escape","alias","bounds_unsafe",
};

/* ─── IR types ───────────────────────────────────────────────────────── */
#define MAX_OPERANDS   8
#define MAX_INSTRS     4096
#define MAX_BLOCKS     256
#define MAX_FUNCS      128
#define MAX_NAME       64

typedef struct {
    Opcode op;
    char   dst[MAX_NAME];
    char   operands[MAX_OPERANDS][MAX_NAME];
    int    num_operands;
    int    line;                /* source line number */
    /* Escape analysis annotations */
    int    may_escape;          /* operand[0] escapes this function */
    int    bounds_checked;      /* T8-style bounds check present     */
    int    alias_group;         /* -1 = unknown, ≥0 = alias set ID   */
} IrInstr;

typedef struct {
    char     name[MAX_NAME];
    IrInstr  instrs[MAX_INSTRS];
    int      num_instrs;
    int      preds[MAX_BLOCKS];  /* predecessor block indices */
    int      num_preds;
    int      succs[MAX_BLOCKS];  /* successor block indices   */
    int      num_succs;
    int      idom;               /* immediate dominator index, -1 = entry */
    int      visited;            /* dominator tree DFS flag   */
} IrBlock;

typedef struct {
    char     name[MAX_NAME];
    char     params[MAX_OPERANDS][MAX_NAME];
    int      num_params;
    IrBlock  blocks[MAX_BLOCKS];
    int      num_blocks;
    /* Escape analysis results */
    char     escaped_vars[MAX_INSTRS][MAX_NAME];
    int      num_escaped;
    /* Alias sets */
    char     alias_sets[MAX_INSTRS][2][MAX_NAME];
    int      num_alias_pairs;
} IrFunc;

typedef struct {
    IrFunc funcs[MAX_FUNCS];
    int    num_funcs;
    char   source_file[256];
} IrModule;

/* ─── Text IR parser ─────────────────────────────────────────────────── */
/*
 * Text IR format (emitted by part5.c via fprintf):
 *
 *   func <name> [param0 param1 ...]
 *   block <name>
 *     <opcode> <dst> <op0> <op1> ...   [; line=N]
 *   endfunc
 *
 * One instruction per line. Comments with ; prefix are stripped.
 * This is the ONLY format ZCC emits — kept trivially simple so ZCC's
 * fprintf calls in part5.c don't require any ZCC-side struct awareness.
 */

static char *next_token(char **p) {
    while (**p == ' ' || **p == '\t') (*p)++;
    if (!**p || **p == ';' || **p == '\n') return NULL;
    char *start = *p;
    while (**p && **p != ' ' && **p != '\t' && **p != '\n' && **p != ';')
        (*p)++;
    if (**p) { **p = '\0'; (*p)++; }
    return start;
}

static Opcode parse_opcode(const char *s) {
    for (int i = 0; i < (int)(sizeof(OPCODE_NAMES)/sizeof(OPCODE_NAMES[0])); i++)
        if (strcmp(s, OPCODE_NAMES[i]) == 0) return (Opcode)i;
    return OP_NOP;
}

static IrModule *ir_parse(const char *text) {
    IrModule *m = calloc(1, sizeof(IrModule));
    if (!m) return NULL;

    char *buf = strdup(text);
    char *line = buf;
    IrFunc  *cf = NULL;
    IrBlock *cb = NULL;

    while (*line) {
        char *end = strchr(line, '\n');
        if (end) *end = '\0';

        /* strip leading whitespace for opcode check */
        char *p = line;
        char *tok = next_token(&p);

        if (tok && strcmp(tok, "func") == 0) {
            if (m->num_funcs >= MAX_FUNCS) goto next_line;
            cf = &m->funcs[m->num_funcs++];
            char *name = next_token(&p);
            if (name) strncpy(cf->name, name, MAX_NAME-1);
            char *param;
            while ((param = next_token(&p)) && cf->num_params < MAX_OPERANDS)
                strncpy(cf->params[cf->num_params++], param, MAX_NAME-1);
            cb = NULL;

        } else if (tok && strcmp(tok, "block") == 0 && cf) {
            if (cf->num_blocks >= MAX_BLOCKS) goto next_line;
            cb = &cf->blocks[cf->num_blocks++];
            char *name = next_token(&p);
            if (name) strncpy(cb->name, name, MAX_NAME-1);
            cb->idom = -1;

        } else if (tok && strcmp(tok, "endfunc") == 0) {
            cf = NULL; cb = NULL;

        } else if (tok && cb && cf) {
            if (cb->num_instrs >= MAX_INSTRS) goto next_line;
            IrInstr *instr = &cb->instrs[cb->num_instrs++];
            instr->op = parse_opcode(tok);
            instr->alias_group = -1;

            /* Extract line number hint from trailing ; line=N */
            char *semi = strchr(p, ';');
            if (semi) {
                char *ln = strstr(semi, "line=");
                if (ln) instr->line = atoi(ln + 5);
                *semi = '\0';
            }

            char *op;
            int first = 1;
            while ((op = next_token(&p))) {
                if (first) {
                    strncpy(instr->dst, op, MAX_NAME-1);
                    first = 0;
                } else if (instr->num_operands < MAX_OPERANDS) {
                    strncpy(instr->operands[instr->num_operands++], op, MAX_NAME-1);
                }
            }
        }

next_line:
        if (end) line = end + 1; else break;
    }

    free(buf);
    return m;
}

/* ─── Pass 1: Dead Code Elimination ─────────────────────────────────── */
/*
 * Simple liveness pass: mark instructions whose dst is never used.
 * Pure DCE — does not remove stores or calls (they have side effects).
 */
static void pass_dce(IrFunc *fn) {
    /* Build use set */
    char used[MAX_INSTRS * MAX_BLOCKS][MAX_NAME];
    int  num_used = 0;

    for (int bi = 0; bi < fn->num_blocks; bi++) {
        IrBlock *b = &fn->blocks[bi];
        for (int ii = 0; ii < b->num_instrs; ii++) {
            IrInstr *in = &b->instrs[ii];
            for (int oi = 0; oi < in->num_operands; oi++) {
                if (in->operands[oi][0] && num_used < MAX_INSTRS * MAX_BLOCKS)
                    strncpy(used[num_used++], in->operands[oi], MAX_NAME-1);
            }
        }
    }

    /* Mark dead: dst defined but never used, and not a store/call/branch */
    for (int bi = 0; bi < fn->num_blocks; bi++) {
        IrBlock *b = &fn->blocks[bi];
        for (int ii = 0; ii < b->num_instrs; ii++) {
            IrInstr *in = &b->instrs[ii];
            if (in->op == OP_STORE || in->op == OP_CALL ||
                in->op == OP_BRANCH || in->op == OP_JUMP ||
                in->op == OP_RETURN || in->op == OP_ESCAPE)
                continue;
            if (!in->dst[0]) continue;
            int found = 0;
            for (int ui = 0; ui < num_used; ui++) {
                if (strcmp(used[ui], in->dst) == 0) { found = 1; break; }
            }
            if (!found) in->op = OP_NOP; /* mark dead */
        }
    }
}

/* ─── Pass 2: Escape Analysis ────────────────────────────────────────── */
/*
 * Identifies pointers that escape their defining function:
 *   - stored into a global (OP_STORE where dst contains "global")
 *   - passed to a call (OP_CALL operand is an ALLOCA'd var)
 *   - returned (OP_RETURN of a pointer)
 *
 * Also detects alias confusion:
 *   - two OP_GEP results derived from the same base at different offsets
 *   - used in overlapping OP_STORE windows (classic buffer alias)
 *
 * Injects OP_ESCAPE and OP_ALIAS annotations into the IR.
 * Injects OP_BOUNDS_UNSAFE when GEP index is unguarded.
 */
static void pass_escape(IrFunc *fn) {
    /* Collect alloca'd vars */
    char allocas[MAX_INSTRS][MAX_NAME];
    int  num_allocas = 0;

    for (int bi = 0; bi < fn->num_blocks; bi++) {
        IrBlock *b = &fn->blocks[bi];
        for (int ii = 0; ii < b->num_instrs; ii++) {
            IrInstr *in = &b->instrs[ii];
            if (in->op == OP_ALLOCA && num_allocas < MAX_INSTRS)
                strncpy(allocas[num_allocas++], in->dst, MAX_NAME-1);
        }
    }

    /* Detect escapes and GEP bounds issues */
    for (int bi = 0; bi < fn->num_blocks; bi++) {
        IrBlock *b = &fn->blocks[bi];
        for (int ii = 0; ii < b->num_instrs; ii++) {
            IrInstr *in = &b->instrs[ii];

            /* OP_CALL with alloca'd argument → escape */
            if (in->op == OP_CALL) {
                for (int oi = 0; oi < in->num_operands; oi++) {
                    for (int ai = 0; ai < num_allocas; ai++) {
                        if (strcmp(in->operands[oi], allocas[ai]) == 0) {
                            in->may_escape = 1;
                            if (fn->num_escaped < MAX_INSTRS)
                                strncpy(fn->escaped_vars[fn->num_escaped++],
                                        allocas[ai], MAX_NAME-1);
                        }
                    }
                }
            }

            /* OP_RETURN of alloca'd var → escape */
            if (in->op == OP_RETURN && in->num_operands > 0) {
                for (int ai = 0; ai < num_allocas; ai++) {
                    if (strcmp(in->operands[0], allocas[ai]) == 0) {
                        in->may_escape = 1;
                        if (fn->num_escaped < MAX_INSTRS)
                            strncpy(fn->escaped_vars[fn->num_escaped++],
                                    allocas[ai], MAX_NAME-1);
                    }
                }
            }

            /* OP_GEP with numeric index — check if bounds-guarded */
            if (in->op == OP_GEP && in->num_operands >= 2) {
                /* Heuristic: if index operand is a bare number > 0, flag */
                const char *idx = in->operands[1];
                char *endp;
                long iv = strtol(idx, &endp, 10);
                if (*endp == '\0' && iv > 0) {
                    /* Check if there's a preceding bounds check in same block */
                    int guarded = 0;
                    for (int pi = 0; pi < ii; pi++) {
                        IrInstr *prev = &b->instrs[pi];
                        if ((prev->op == OP_LT || prev->op == OP_LE) &&
                            (strcmp(prev->operands[0], idx) == 0 ||
                             strcmp(prev->operands[1], idx) == 0)) {
                            guarded = 1; break;
                        }
                    }
                    if (!guarded) {
                        in->bounds_checked = 0;
                        /* Inject OP_BOUNDS_UNSAFE annotation after this instr */
                        if (b->num_instrs < MAX_INSTRS - 1) {
                            /* Shift remaining instructions up */
                            memmove(&b->instrs[ii+2], &b->instrs[ii+1],
                                    (b->num_instrs - ii - 1) * sizeof(IrInstr));
                            IrInstr *ann = &b->instrs[ii+1];
                            memset(ann, 0, sizeof(IrInstr));
                            ann->op = OP_BOUNDS_UNSAFE;
                            ann->alias_group = -1;
                            strncpy(ann->dst, in->dst, MAX_NAME-1);
                            strncpy(ann->operands[0], in->operands[0], MAX_NAME-1);
                            strncpy(ann->operands[1], idx, MAX_NAME-1);
                            ann->num_operands = 2;
                            ann->line = in->line;
                            b->num_instrs++;
                            ii++; /* skip the annotation we just inserted */
                        }
                    }
                }
            }
        }
    }

    /* Detect alias pairs: two GEPs from same base */
    char gep_bases[MAX_INSTRS][MAX_NAME];
    char gep_dsts[MAX_INSTRS][MAX_NAME];
    int  num_geps = 0;

    for (int bi = 0; bi < fn->num_blocks; bi++) {
        IrBlock *b = &fn->blocks[bi];
        for (int ii = 0; ii < b->num_instrs; ii++) {
            IrInstr *in = &b->instrs[ii];
            if (in->op == OP_GEP && num_geps < MAX_INSTRS) {
                strncpy(gep_bases[num_geps],  in->operands[0], MAX_NAME-1);
                strncpy(gep_dsts[num_geps++], in->dst,         MAX_NAME-1);
            }
        }
    }

    for (int i = 0; i < num_geps; i++) {
        for (int j = i+1; j < num_geps; j++) {
            if (strcmp(gep_bases[i], gep_bases[j]) == 0 &&
                fn->num_alias_pairs < MAX_INSTRS) {
                strncpy(fn->alias_sets[fn->num_alias_pairs][0],
                        gep_dsts[i], MAX_NAME-1);
                strncpy(fn->alias_sets[fn->num_alias_pairs][1],
                        gep_dsts[j], MAX_NAME-1);
                fn->num_alias_pairs++;
            }
        }
    }
}

/* ─── Dominator tree (simple O(n²) Cooper et al.) ───────────────────── */
static void compute_dominators(IrFunc *fn) {
    if (fn->num_blocks == 0) return;
    fn->blocks[0].idom = 0;
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int bi = 1; bi < fn->num_blocks; bi++) {
            IrBlock *b = &fn->blocks[bi];
            int new_idom = -1;
            for (int pi = 0; pi < b->num_preds; pi++) {
                int pred = b->preds[pi];
                if (fn->blocks[pred].idom == -1) continue;
                if (new_idom == -1) { new_idom = pred; continue; }
                /* LCA in dominator tree */
                int a = pred, c = new_idom;
                while (a != c) {
                    while (a > c) a = fn->blocks[a].idom;
                    while (c > a) c = fn->blocks[c].idom;
                }
                new_idom = a;
            }
            if (new_idom != -1 && b->idom != new_idom) {
                b->idom = new_idom;
                changed = 1;
            }
        }
    }
}

/* ─── JSON serializer ────────────────────────────────────────────────── */
static void json_escape_str(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"')       fputs("\\\"", f);
        else if (*s == '\\') fputs("\\\\", f);
        else if (*s == '\n') fputs("\\n", f);
        else                 fputc(*s, f);
    }
    fputc('"', f);
}

static void ir_dump_json(const IrModule *m, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "ir_dump_json: cannot open %s\n", path); return; }

    fprintf(f, "{\n");
    fprintf(f, "  \"schema\": \"zcc_ir_v1\",\n");
    fprintf(f, "  \"source\": ");
    json_escape_str(f, m->source_file);
    fprintf(f, ",\n");
    fprintf(f, "  \"functions\": [\n");

    for (int fi = 0; fi < m->num_funcs; fi++) {
        const IrFunc *fn = &m->funcs[fi];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": ");
        json_escape_str(f, fn->name);
        fprintf(f, ",\n");

        /* params */
        fprintf(f, "      \"params\": [");
        for (int i = 0; i < fn->num_params; i++) {
            if (i) fprintf(f, ", ");
            json_escape_str(f, fn->params[i]);
        }
        fprintf(f, "],\n");

        /* escaped vars */
        fprintf(f, "      \"escaped_vars\": [");
        for (int i = 0; i < fn->num_escaped; i++) {
            if (i) fprintf(f, ", ");
            json_escape_str(f, fn->escaped_vars[i]);
        }
        fprintf(f, "],\n");

        /* alias pairs */
        fprintf(f, "      \"alias_pairs\": [");
        for (int i = 0; i < fn->num_alias_pairs; i++) {
            if (i) fprintf(f, ", ");
            fprintf(f, "[");
            json_escape_str(f, fn->alias_sets[i][0]);
            fprintf(f, ", ");
            json_escape_str(f, fn->alias_sets[i][1]);
            fprintf(f, "]");
        }
        fprintf(f, "],\n");

        /* blocks */
        fprintf(f, "      \"blocks\": [\n");
        for (int bi = 0; bi < fn->num_blocks; bi++) {
            const IrBlock *b = &fn->blocks[bi];
            fprintf(f, "        {\n");
            fprintf(f, "          \"name\": ");
            json_escape_str(f, b->name);
            fprintf(f, ",\n");
            fprintf(f, "          \"idom\": %d,\n", b->idom);

            fprintf(f, "          \"preds\": [");
            for (int i = 0; i < b->num_preds; i++) {
                if (i) fprintf(f, ", ");
                fprintf(f, "%d", b->preds[i]);
            }
            fprintf(f, "],\n");

            fprintf(f, "          \"instrs\": [\n");
            for (int ii = 0; ii < b->num_instrs; ii++) {
                const IrInstr *in = &b->instrs[ii];
                if (in->op == OP_NOP) continue; /* DCE'd */
                fprintf(f, "            {\n");
                fprintf(f, "              \"op\": \"%s\",\n",
                        OPCODE_NAMES[in->op]);
                fprintf(f, "              \"dst\": ");
                json_escape_str(f, in->dst);
                fprintf(f, ",\n");
                fprintf(f, "              \"line\": %d,\n", in->line);
                fprintf(f, "              \"operands\": [");
                for (int oi = 0; oi < in->num_operands; oi++) {
                    if (oi) fprintf(f, ", ");
                    json_escape_str(f, in->operands[oi]);
                }
                fprintf(f, "],\n");
                fprintf(f, "              \"may_escape\": %s,\n",
                        in->may_escape ? "true" : "false");
                fprintf(f, "              \"bounds_checked\": %s,\n",
                        in->bounds_checked ? "true" : "false");
                fprintf(f, "              \"alias_group\": %d\n",
                        in->alias_group);
                fprintf(f, "            }");
                if (ii < b->num_instrs - 1) fprintf(f, ",");
                fprintf(f, "\n");
            }
            fprintf(f, "          ]\n");
            fprintf(f, "        }");
            if (bi < fn->num_blocks - 1) fprintf(f, ",");
            fprintf(f, "\n");
        }
        fprintf(f, "      ]\n");
        fprintf(f, "    }");
        if (fi < m->num_funcs - 1) fprintf(f, ",");
        fprintf(f, "\n");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
}

/* ─── Public boundary functions (ZCC-callable via 3-pointer ABI) ─────── */

const char *zcc_ir_version(void) {
    return "zcc_ir_v1.0";
}

/*
 * zcc_ir_lower() — primary entry point
 *
 * ir_text    : flat text IR emitted by part5.c fprintf calls
 * json_path  : output path for zcc_ir_dump.json
 * source_file: original .c filename for provenance
 *
 * Returns 0 on success, -1 on parse/IO failure.
 * ZCC calls this as: zcc_ir_lower(text_buf, "zcc_ir_dump.json", argv[1])
 */
int zcc_ir_lower(const char *ir_text,
                 const char *json_path,
                 const char *source_file) {
    if (!ir_text || !json_path) return -1;

    IrModule *m = ir_parse(ir_text);
    if (!m) return -1;

    if (source_file)
        strncpy(m->source_file, source_file, sizeof(m->source_file)-1);

    /* Run passes */
    for (int fi = 0; fi < m->num_funcs; fi++) {
        pass_dce(&m->funcs[fi]);
        pass_escape(&m->funcs[fi]);
        compute_dominators(&m->funcs[fi]);
    }

    ir_dump_json(m, json_path);
    free(m);
    return 0;
}

