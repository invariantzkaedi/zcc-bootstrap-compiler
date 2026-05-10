/*
 * ir_symbolic_cfg.c — Backward-Tracing Symbolic Jump Resolver
 * ============================================================
 * Compiled by GCC (linked separately, NOT concatenated into zcc.c).
 *
 * Resolves dynamic EVM jumps that the lifter couldn't evaluate at
 * lift-time because the target was computed through arithmetic (e.g.,
 * PUSH4 0x1234 + PUSH1 0x02 → ADD → JUMP).
 *
 * The backward walker follows the def-use chain from the target VReg
 * back through the IR node list, recursively evaluating arithmetic
 * until it reaches a constant leaf or exhausts its depth budget.
 *
 * This is a MUTATING pass — it rewrites IR_NOP ghost nodes into proper
 * IR_BR / IR_BR_IF instructions with resolved labels.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "evm_lifter.h"
#include "ir_symbolic_cfg.h"

/* ── ANSI Escape Sequences ───────────────────────────────────────────── */
#define SC_RST   "\033[0m"
#define SC_NAVY  "\033[38;5;17m"
#define SC_CYAN  "\033[36m"
#define SC_MAG   "\033[35m"
#define SC_BOLD  "\033[1m"
#define SC_DIM   "\033[2m"
#define SC_BYEL  "\033[1;33m"
#define SC_BGRN  "\033[1;32m"
#define SC_BRED  "\033[1;31m"
#define SC_BCYN  "\033[1;36m"
#define SC_BMAG  "\033[1;35m"

/* ── Label Existence Table ───────────────────────────────────────────── */
/*
 * Pre-scan all IR_LABEL nodes to build a set of valid jump targets.
 * This is the IR-level equivalent of the lifter's valid_jumpdest bitmap.
 * We extract the numeric offset from `.L_evm_<N>` labels.
 */
#define SYMCFG_LABEL_MAX 8192

static long s_valid_labels[SYMCFG_LABEL_MAX];
static int  s_valid_label_count;

static void label_table_clear(void) {
    s_valid_label_count = 0;
}

static void label_table_add(long offset) {
    if (s_valid_label_count >= SYMCFG_LABEL_MAX) {
        fprintf(stderr, "[symcfg] WARNING: label table overflow at %d entries\n",
                SYMCFG_LABEL_MAX);
        return;
    }
    s_valid_labels[s_valid_label_count++] = offset;
}

static int label_table_has(long offset) {
    int i;
    for (i = 0; i < s_valid_label_count; i++) {
        if (s_valid_labels[i] == offset) return 1;
    }
    return 0;
}

static void label_table_build(ir_func_t *fn) {
    ir_node_t *n;
    label_table_clear();
    for (n = fn->head; n; n = n->next) {
        if (n->op == IR_LABEL && strncmp(n->label, ".L_evm_", 7) == 0) {
            long off = strtol(n->label + 7, NULL, 10);
            label_table_add(off);
        }
    }
}

/* ── Backward Definition Finder ──────────────────────────────────────── */
/*
 * Find the IR node that DEFINES `vreg` by walking backward from
 * `start` through the linked list.  Returns NULL if not found.
 *
 * Zero malloc: we iterate the linked list from fn->head to start,
 * tracking the last node whose dst matches vreg.  This is O(N) per
 * lookup but with EVM function sizes bounded to ~2K nodes, it's fast.
 */
static ir_node_t *find_def(ir_func_t *fn, ir_node_t *before, const char *vreg) {
    ir_node_t *n;
    ir_node_t *last_def = NULL;

    if (!vreg || vreg[0] == '\0') return NULL;

    for (n = fn->head; n && n != before; n = n->next) {
        if (n->dst[0] != '\0' && strcmp(n->dst, vreg) == 0) {
            last_def = n;
        }
    }
    return last_def;
}

/* ── Recursive Symbolic Evaluator ────────────────────────────────────── */
/*
 * resolve_target: recursively evaluate a VReg to a constant integer.
 *
 * Algorithm:
 *   1. Find the defining node for `vreg`.
 *   2. If IR_CONST → return imm.
 *   3. If arithmetic (ADD, SUB, MUL, SHL, SHR, AND, OR, XOR) →
 *      recursively resolve both operands; if both resolve, compute.
 *   4. If COPY → follow through to src1.
 *   5. Otherwise → UNRESOLVED.
 *
 * Stack budget: MAX_SYMBOLIC_DEPTH frames.  Each frame is ~64 bytes
 * (two recursive calls + locals), so worst case ~2KB stack usage.
 */
static symcfg_status_t resolve_target(ir_func_t *fn, ir_node_t *before,
                                       const char *vreg, int depth,
                                       long *out_value) {
    ir_node_t *def;
    long v1, v2;
    symcfg_status_t s1, s2;

    if (depth >= MAX_SYMBOLIC_DEPTH) return SYMCFG_DEPTH_EXCEEDED;
    if (!vreg || vreg[0] == '\0') return SYMCFG_UNRESOLVED;

    def = find_def(fn, before, vreg);
    if (!def) return SYMCFG_UNRESOLVED;

    /* Leaf: constant */
    if (def->op == IR_CONST) {
        *out_value = def->imm;
        return SYMCFG_RESOLVED;
    }

    /* Pass-through: copy */
    if (def->op == IR_COPY) {
        return resolve_target(fn, def, def->src1, depth + 1, out_value);
    }

    /* Binary arithmetic — resolve both operands */
    switch (def->op) {
    case IR_ADD: case IR_SUB: case IR_MUL:
    case IR_AND: case IR_OR:  case IR_XOR:
    case IR_SHL: case IR_SHR:
    case IR_DIV: case IR_MOD:
        s1 = resolve_target(fn, def, def->src1, depth + 1, &v1);
        if (s1 != SYMCFG_RESOLVED) return s1;
        s2 = resolve_target(fn, def, def->src2, depth + 1, &v2);
        if (s2 != SYMCFG_RESOLVED) return s2;
        break;
    default:
        return SYMCFG_UNRESOLVED;
    }

    /* Evaluate */
    switch (def->op) {
    case IR_ADD: *out_value = v1 + v2; break;
    case IR_SUB: *out_value = v1 - v2; break;
    case IR_MUL: *out_value = v1 * v2; break;
    case IR_AND: *out_value = v1 & v2; break;
    case IR_OR:  *out_value = v1 | v2; break;
    case IR_XOR: *out_value = v1 ^ v2; break;
    case IR_SHL: *out_value = (v2 >= 0 && v2 < 64) ? (v1 << v2) : 0; break;
    case IR_SHR: *out_value = (v2 >= 0 && v2 < 64) ? ((unsigned long)v1 >> v2) : 0; break;
    case IR_DIV: *out_value = (v2 != 0) ? (v1 / v2) : 0; break;
    case IR_MOD: *out_value = (v2 != 0) ? (v1 % v2) : 0; break;
    default:     return SYMCFG_UNRESOLVED;
    }

    return SYMCFG_RESOLVED;
}

/* ── Ghost Jump Detection ────────────────────────────────────────────── */

/*
 * Identify an unresolved JUMP ghost:
 *   - op == IR_NOP
 *   - tag == IR_TAG_NONE (not an explicit barrier)
 *   - src1 holds the target VReg (non-empty)
 *   - dst is empty (NOP doesn't define anything)
 */
static int is_ghost_jump(const ir_node_t *n) {
    return (n->op == IR_NOP &&
            n->tag == (int)IR_TAG_NONE &&
            n->src1[0] != '\0' &&
            n->dst[0] == '\0');
}

/*
 * Identify an unresolved JUMPI:
 *   - op == IR_BR_IF
 *   - label holds a VReg name (starts with 't', not '.L')
 *   - src1 holds the condition VReg
 */
static int is_ghost_jumpi(const ir_node_t *n) {
    return (n->op == IR_BR_IF &&
            n->label[0] != '\0' &&
            n->label[0] != '.' &&
            n->src1[0] != '\0');
}

/* ── Diagnostic Emission ─────────────────────────────────────────────── */

static void emit_resolved(const char *fn_name, int lineno,
                           const char *vreg, long target) {
    fprintf(stderr,
        SC_NAVY "╔═══════════════════════════════════════════════════════════════╗" SC_RST "\n"
        SC_NAVY "║" SC_BGRN "  ▸ SYMBOLIC CFG — JUMP RESOLVED" SC_NAVY "                             ║" SC_RST "\n"
        SC_NAVY "╚═══════════════════════════════════════════════════════════════╝" SC_RST "\n"
        "  " SC_DIM SC_CYAN "%-20s" SC_RST " %s\n"
        "  " SC_DIM SC_CYAN "%-20s" SC_RST " %s (line %d)\n"
        "  " SC_DIM SC_CYAN "%-20s" SC_RST SC_BGRN " .L_evm_%ld" SC_RST "\n\n",
        "Function", fn_name,
        "Source VReg", vreg, lineno,
        "Resolved Target", target);
}

static void emit_unresolved(const char *fn_name, int lineno,
                             const char *vreg, const char *reason) {
    fprintf(stderr,
        SC_NAVY "╔═══════════════════════════════════════════════════════════════╗" SC_RST "\n"
        SC_NAVY "║" SC_BYEL "  ▸ SYMBOLIC CFG — OPAQUE DYNAMIC JUMP" SC_NAVY "                       ║" SC_RST "\n"
        SC_NAVY "╚═══════════════════════════════════════════════════════════════╝" SC_RST "\n"
        "  " SC_DIM SC_CYAN "%-20s" SC_RST " %s\n"
        "  " SC_DIM SC_CYAN "%-20s" SC_RST " %s (line %d)\n"
        "  " SC_DIM SC_CYAN "%-20s" SC_RST SC_BYEL " %s" SC_RST "\n\n",
        "Function", fn_name,
        "Target VReg", vreg, lineno,
        "Status", reason);
}

/* ── Main Pass ───────────────────────────────────────────────────────── */

ir_pass_result_t ir_pass_symbolic_cfg(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_node_t *n;
    ir_pass_result_t r;
    int resolved_count = 0;
    int ghost_count = 0;
    char lbl_buf[IR_LABEL_MAX];

    memset(&r, 0, sizeof(r));
    if (!fn || !fn->head) return r;

    r.nodes_before = fn->node_count;

    /* Phase 0: Build label existence table */
    label_table_build(fn);

    /* Phase 1: Scan for ghost jumps and attempt resolution */
    for (n = fn->head; n; n = n->next) {

        /* ── Ghost JUMP (IR_NOP with target VReg in src1) ──────── */
        if (is_ghost_jump(n)) {
            long target_val = 0;
            symcfg_status_t status;
            char saved_vreg[IR_NAME_MAX];

            ghost_count++;
            /* Save the target VReg name before any mutation */
            strncpy(saved_vreg, n->src1, IR_NAME_MAX - 1);
            saved_vreg[IR_NAME_MAX - 1] = '\0';

            status = resolve_target(fn, n, n->src1, 0, &target_val);

            if (status == SYMCFG_RESOLVED) {
                /* Validate: does the target label exist in the IR? */
                if (label_table_has(target_val)) {
                    /* SUCCESS: Rewrite NOP → IR_BR */
                    snprintf(lbl_buf, IR_LABEL_MAX, ".L_evm_%ld", target_val);
                    n->op = IR_BR;
                    n->type = IR_TY_VOID;
                    strncpy(n->label, lbl_buf, IR_LABEL_MAX - 1);
                    n->label[IR_LABEL_MAX - 1] = '\0';
                    n->src1[0] = '\0';  /* clear the VReg reference */
                    resolved_count++;

                    emit_resolved(fn->name, n->lineno, saved_vreg, target_val);
                } else {
                    /* Resolved to an offset that has no JUMPDEST — invalid jump.
                     * Convert to an EVM barrier. */
                    n->tag = (int)IR_TAG_EVM_BARRIER;
                    emit_unresolved(fn->name, n->lineno, saved_vreg,
                                    "resolved to invalid offset (no JUMPDEST)");
                }
            } else if (status == SYMCFG_DEPTH_EXCEEDED) {
                n->tag = (int)IR_TAG_EVM_BARRIER;
                emit_unresolved(fn->name, n->lineno, saved_vreg,
                                "depth limit exceeded (possible loop)");
            } else {
                /* Truly runtime-dependent — leave as barrier */
                n->tag = (int)IR_TAG_EVM_BARRIER;
                emit_unresolved(fn->name, n->lineno, saved_vreg,
                                "runtime-dependent (calldata/storage)");
            }
            continue;
        }

        /* ── Ghost JUMPI (IR_BR_IF with temp name as label) ────── */
        if (is_ghost_jumpi(n)) {
            long target_val = 0;
            symcfg_status_t status;
            char saved_label[IR_LABEL_MAX];

            ghost_count++;

            /* Save the VReg name from the label field before overwriting */
            strncpy(saved_label, n->label, IR_LABEL_MAX - 1);
            saved_label[IR_LABEL_MAX - 1] = '\0';

            status = resolve_target(fn, n, saved_label, 0, &target_val);

            if (status == SYMCFG_RESOLVED && label_table_has(target_val)) {
                /* SUCCESS: Fix the label to point to the resolved address */
                snprintf(lbl_buf, IR_LABEL_MAX, ".L_evm_%ld", target_val);
                strncpy(n->label, lbl_buf, IR_LABEL_MAX - 1);
                n->label[IR_LABEL_MAX - 1] = '\0';
                resolved_count++;

                emit_resolved(fn->name, n->lineno, saved_label, target_val);
            } else {
                /* Can't resolve: downgrade to NOP barrier.
                 * We can't leave a BR_IF with a bogus label — that would
                 * corrupt the CFG more than a NOP. */
                n->op = IR_NOP;
                n->tag = (int)IR_TAG_EVM_BARRIER;
                n->label[0] = '\0';

                const char *reason = (status == SYMCFG_DEPTH_EXCEEDED)
                    ? "depth limit exceeded (possible loop)"
                    : (status == SYMCFG_RESOLVED)
                        ? "resolved to invalid offset (no JUMPDEST)"
                        : "runtime-dependent (calldata/storage)";
                emit_unresolved(fn->name, n->lineno, saved_label, reason);
            }
            continue;
        }
    }

    /* ── Summary ─────────────────────────────────────────────────── */
    if (ghost_count > 0) {
        fprintf(stderr,
            SC_NAVY "╔═══════════════════════════════════════════════════════════════╗" SC_RST "\n"
            SC_NAVY "║" SC_BCYN "  ▸ SYMBOLIC CFG SUMMARY: %d ghost(s), %d resolved" SC_NAVY,
            ghost_count, resolved_count);
        /* Pad to box width */
        {
            int pad = 38 - 2;  /* rough padding */
            int i;
            char buf[8];
            snprintf(buf, 8, "%d", ghost_count);
            pad -= (int)strlen(buf);
            snprintf(buf, 8, "%d", resolved_count);
            pad -= (int)strlen(buf);
            if (pad < 0) pad = 0;
            for (i = 0; i < pad; i++) fputc(' ', stderr);
        }
        fprintf(stderr, "║" SC_RST "\n");
        fprintf(stderr,
            SC_NAVY "╚═══════════════════════════════════════════════════════════════╝" SC_RST "\n\n");
    }

    r.nodes_after = fn->node_count;  /* no nodes added or removed */
    r.nodes_modified = resolved_count;
    r.changed = resolved_count > 0;
    return r;
}
