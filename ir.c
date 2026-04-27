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
