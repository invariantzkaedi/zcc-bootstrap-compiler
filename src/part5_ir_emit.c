/*
 * part5_ir_emit.c — ZCC IR Text Emitter
 * =======================================
 * Drop-in additions for part5.c (or a new part6.c).
 *
 * These functions walk the same AST that codegen_expr/codegen_stmt
 * already processes and emit flat text IR to a FILE*.
 *
 * The text IR is then passed to zcc_ir_lower() via the
 * 3-pointer boundary function. ZCC never calls ir_parse() directly.
 *
 * ACTIVATION: Controlled by ZCC_IR_BRIDGE=1 environment variable.
 * When unset, emit_ir_* functions are compiled as no-ops.
 * Self-hosting always runs with ZCC_IR_BRIDGE unset.
 *
 * INTEGRATION POINTS in part5.c / main():
 *   1. Before codegen_program(): if ZCC_IR_BRIDGE, open ir_text FILE*
 *   2. After each function: flush ir_text buffer
 *   3. After codegen_program(): call zcc_ir_lower(buf, path, argv[1])
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── IR text emission helpers ───────────────────────────────────────── */
/*
 * All emit_ functions write to a FILE* (typically a memstream or tmpfile).
 * The caller collects the full text and passes it to zcc_ir_lower().
 *
 * IR text format:
 *   func <name> [param0 param1 ...]
 *   block <label>
 *     <opcode> <dst> <op0> <op1> ...  ; line=N
 *   endfunc
 */

static int ir_tmp_counter = 0;

/* Generate a fresh temporary name: %t0, %t1, ... */
static char *ir_tmp(char *buf, size_t n) {
    snprintf(buf, n, "%%t%d", ir_tmp_counter++);
    return buf;
}

/* Reset counter per function (call at func entry) */
static void ir_reset_tmps(void) {
    ir_tmp_counter = 0;
}

/* Emit function prologue */
static void emit_ir_func_begin(FILE *ir, const char *name,
                                char **params, int nparams) {
    ir_reset_tmps();
    fprintf(ir, "func %s", name);
    for (int i = 0; i < nparams; i++)
        fprintf(ir, " %s", params[i]);
    fprintf(ir, "\n");
    fprintf(ir, "block .entry\n");
}

static void emit_ir_func_end(FILE *ir) {
    fprintf(ir, "endfunc\n\n");
}

/* Emit a new basic block */
static void emit_ir_block(FILE *ir, const char *label) {
    fprintf(ir, "block %s\n", label);
}

/* ── Arithmetic ── */
static void emit_ir_binop(FILE *ir, const char *op,
                           const char *dst,
                           const char *lhs, const char *rhs, int line) {
    fprintf(ir, "  %s %s %s %s ; line=%d\n", op, dst, lhs, rhs, line);
}

/* ── Memory ── */
static void emit_ir_alloca(FILE *ir, const char *dst,
                            const char *type, int count, int line) {
    fprintf(ir, "  alloca %s %s %d ; line=%d\n", dst, type, count, line);
}

static void emit_ir_load(FILE *ir, const char *dst,
                          const char *src_ptr, int line) {
    fprintf(ir, "  load %s %s ; line=%d\n", dst, src_ptr, line);
}

static void emit_ir_store(FILE *ir, const char *val,
                           const char *dst_ptr, int line) {
    fprintf(ir, "  store %s %s ; line=%d\n", val, dst_ptr, line);
}

static void emit_ir_gep(FILE *ir, const char *dst,
                         const char *base, const char *idx, int line) {
    fprintf(ir, "  gep %s %s %s ; line=%d\n", dst, base, idx, line);
}

/* ── Control ── */
static void emit_ir_call(FILE *ir, const char *dst,
                          const char *func,
                          char **args, int nargs, int line) {
    fprintf(ir, "  call %s %s", dst, func);
    for (int i = 0; i < nargs; i++)
        fprintf(ir, " %s", args[i]);
    fprintf(ir, " ; line=%d\n", line);
}

static void emit_ir_branch(FILE *ir, const char *cond,
                             const char *true_bb, const char *false_bb,
                             int line) {
    fprintf(ir, "  branch %s %s %s ; line=%d\n",
            cond, true_bb, false_bb, line);
}

static void emit_ir_jump(FILE *ir, const char *target, int line) {
    fprintf(ir, "  jump %s ; line=%d\n", target, line);
}

static void emit_ir_return(FILE *ir, const char *val, int line) {
    if (val && val[0])
        fprintf(ir, "  return _ %s ; line=%d\n", val, line);
    else
        fprintf(ir, "  return _ ; line=%d\n", line);
}

/* ─── Integration shim for part5.c ──────────────────────────────────── */
/*
 * Call from main() in part5.c when ZCC_IR_BRIDGE=1:
 *
 *   char  *ir_buf  = NULL;
 *   size_t ir_size = 0;
 *   FILE  *ir_mem  = open_memstream(&ir_buf, &ir_size);
 *   // ... normal codegen_program() ...
 *   // emit_ir_* calls write to ir_mem during codegen
 *   fclose(ir_mem);
 *   const char *out_path = "zcc_ir_dump.json";
 *   zcc_ir_lower(ir_buf, out_path, source_filename);
 *   free(ir_buf);
 *
 * If open_memstream is unavailable (non-glibc):
 *   Use a tmpfile() + fread() pattern instead.
 */

static char *ir_collect_to_string(FILE *tmpf) {
    fseek(tmpf, 0, SEEK_END);
    long len = ftell(tmpf);
    fseek(tmpf, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) return NULL;
    fread(buf, 1, len, tmpf);
    buf[len] = '\0';
    return buf;
}

/*
 * High-level driver: emit IR for a compilation unit, run passes, dump JSON.
 * Call this after all emit_ir_* calls are done and ir_mem is flushed.
 */
static int ir_bridge_finalize(FILE *ir_mem,
                               const char *json_path,
                               const char *source_file) {
    /* Collect text IR */
    char *ir_text = ir_collect_to_string(ir_mem);
    if (!ir_text) return -1;

    /* Hand off to GCC-compiled boundary function */
    int rc = zcc_ir_lower(ir_text, json_path, source_file);
    free(ir_text);
    return rc;
}
