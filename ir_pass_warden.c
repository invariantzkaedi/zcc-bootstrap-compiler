/*
 * ir_pass_warden.c — ZCC IR Data-Flow Taint Analysis Engine
 * ==========================================================
 * Compiled by GCC (linked separately, NOT concatenated into zcc.c).
 *
 * Implements forward data-flow taint propagation across the IR node list.
 * Identifies when externally-controlled data (CALLDATALOAD, CALL return
 * values, CALLER, CALLVALUE) reaches security-critical sinks (SSTORE,
 * untrusted CALL, SELFDESTRUCT).
 *
 * This is a READ-ONLY analysis pass.  It never mutates the IR graph.
 */

#include <stdio.h>
#include <string.h>
#include "ir.h"
#include "evm_lifter.h"
#include "ir_pass_warden.h"

/* ── ANSI Escape Sequences ───────────────────────────────────────────── */
#define W_RST   "\033[0m"
#define W_NAVY  "\033[38;5;17m"
#define W_CYAN  "\033[36m"
#define W_MAG   "\033[35m"
#define W_RED   "\033[31m"
#define W_BOLD  "\033[1m"
#define W_DIM   "\033[2m"
#define W_BYEL  "\033[1;33m"
#define W_BGRN  "\033[1;32m"
#define W_BRED  "\033[1;31m"
#define W_BCYN  "\033[1;36m"
#define W_BMAG  "\033[1;35m"

/* ── Taint Map ───────────────────────────────────────────────────────── */
/*
 * Simple array-backed map: VReg name → taint state.
 * Sufficient for single-function analysis on EVM-lifted IR where VReg
 * counts are bounded by the bytecode length (< 4K for most contracts).
 */
#define WARDEN_MAP_MAX 4096

typedef struct {
    char            name[IR_NAME_MAX];
    warden_taint_t  state;
    int             source_lineno;   /* line where taint originated          */
    char            source_desc[64]; /* human-readable taint source          */
} warden_entry_t;

static warden_entry_t s_wmap[WARDEN_MAP_MAX];
static int            s_wmap_count;

static void wmap_clear(void) {
    s_wmap_count = 0;
}

static warden_entry_t *wmap_find(const char *name) {
    int i;
    if (!name || name[0] == '\0') return NULL;
    for (i = 0; i < s_wmap_count; i++) {
        if (strcmp(s_wmap[i].name, name) == 0)
            return &s_wmap[i];
    }
    return NULL;
}

static void wmap_set(const char *name, warden_taint_t state,
                     int lineno, const char *desc) {
    warden_entry_t *e;
    if (!name || name[0] == '\0') return;

    e = wmap_find(name);
    if (e) {
        /* Only escalate: SAFE < CHECKED < TAINTED */
        if ((int)state > (int)e->state) {
            e->state = state;
            e->source_lineno = lineno;
            if (desc) {
                strncpy(e->source_desc, desc, 63);
                e->source_desc[63] = '\0';
            }
        }
        return;
    }

    if (s_wmap_count >= WARDEN_MAP_MAX) return;

    strncpy(s_wmap[s_wmap_count].name, name, IR_NAME_MAX - 1);
    s_wmap[s_wmap_count].name[IR_NAME_MAX - 1] = '\0';
    s_wmap[s_wmap_count].state = state;
    s_wmap[s_wmap_count].source_lineno = lineno;
    if (desc) {
        strncpy(s_wmap[s_wmap_count].source_desc, desc, 63);
        s_wmap[s_wmap_count].source_desc[63] = '\0';
    } else {
        s_wmap[s_wmap_count].source_desc[0] = '\0';
    }
    s_wmap_count++;
}

static warden_taint_t wmap_get(const char *name) {
    warden_entry_t *e = wmap_find(name);
    return e ? e->state : WARDEN_SAFE;
}

/* ── Taint Source Detection ──────────────────────────────────────────── */

/*
 * Determine if a node is a taint source:
 *   1) IR_CALL tagged IR_TAG_UNTRUSTED_EXTERNAL_CALL → return value tainted
 *   2) IR_LOAD (CALLDATALOAD, SLOAD from unknown) → tainted
 *   3) IR_CALL to CALLER, ORIGIN, CALLVALUE env reads → tainted
 *
 * We use the .tag field (evm_ir_tag_t) set by the EVM lifter.
 */
static int is_taint_source(const ir_node_t *n, char *desc, int desc_sz) {
    /* Untrusted external call return values */
    if (n->op == IR_CALL && n->tag == (int)IR_TAG_UNTRUSTED_EXTERNAL_CALL) {
        snprintf(desc, desc_sz, "CALL/CALLCODE return [%s]", n->label);
        return 1;
    }

    /* IR_LOAD — in EVM context, CALLDATALOAD and SLOAD both emit IR_LOAD.
     * CALLDATALOAD data is always externally controlled.
     * SLOAD from unknown slots is conservatively tainted. */
    if (n->op == IR_LOAD && n->tag == (int)IR_TAG_NONE) {
        snprintf(desc, desc_sz, "CALLDATALOAD/SLOAD");
        return 1;
    }

    /* CALLER/CALLVALUE/ORIGIN are environment values emitted as plain
     * IR_CONST or IR_LOAD with no tag.  Since we can't distinguish them
     * structurally from the lifter output (they're just IR_LOAD into
     * unknown temporaries), the above IR_LOAD rule covers them. */

    return 0;
}

/* ── Propagation Engine ──────────────────────────────────────────────── */

/*
 * Returns the combined taint state of a node's source operands.
 * If ANY source is TAINTED, the result is TAINTED.
 * If any source is CHECKED but none are TAINTED, the result is CHECKED.
 */
static warden_taint_t compute_propagated_taint(const ir_node_t *n) {
    warden_taint_t t1 = WARDEN_SAFE;
    warden_taint_t t2 = WARDEN_SAFE;
    warden_taint_t result;

    if (n->src1[0] != '\0') t1 = wmap_get(n->src1);
    if (n->src2[0] != '\0') t2 = wmap_get(n->src2);

    /* Take the maximum taint level */
    result = (int)t1 > (int)t2 ? t1 : t2;
    return result;
}

/* Check if an opcode propagates taint (arithmetic, bitwise, cast, copy) */
static int is_propagating_op(ir_op_t op) {
    switch (op) {
    /* Arithmetic */
    case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
    case IR_NEG:
    /* Bitwise */
    case IR_AND: case IR_OR: case IR_XOR: case IR_NOT:
    case IR_SHL: case IR_SHR:
    /* Type operations */
    case IR_CAST: case IR_COPY:
    /* Floating-point */
    case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
    case IR_ITOF: case IR_FTOI:
        return 1;
    default:
        return 0;
    }
}

/* Check if a comparison op can serve as a sanitizer */
static int is_comparison_op(ir_op_t op) {
    switch (op) {
    case IR_EQ: case IR_NE: case IR_LT: case IR_LE:
    case IR_GT: case IR_GE:
        return 1;
    default:
        return 0;
    }
}

/* ── Sink Detection ──────────────────────────────────────────────────── */

typedef struct {
    const ir_node_t *node;
    const char      *operand_name;
    const char      *sink_type;
    warden_taint_t   taint;
} warden_violation_t;

static void emit_violation(const warden_violation_t *v, const char *fn_name) {
    warden_entry_t *source_entry;
    const char *taint_str;
    const char *taint_color;

    if (v->taint == WARDEN_TAINTED) {
        taint_str = "TAINTED";
        taint_color = W_BRED;
    } else {
        taint_str = "CHECKED";
        taint_color = W_BYEL;
    }

    fprintf(stderr, "\n");
    fprintf(stderr, W_NAVY "╔═══════════════════════════════════════════════════════════════╗" W_RST "\n");
    fprintf(stderr, W_NAVY "║" W_BCYN "  ▸ WARDEN — TAINT VIOLATION DETECTED" W_NAVY "                        ║" W_RST "\n");
    fprintf(stderr, W_NAVY "╚═══════════════════════════════════════════════════════════════╝" W_RST "\n");
    fprintf(stderr, "\n");

    fprintf(stderr, "  " W_DIM W_CYAN "%-20s" W_RST " %s\n", "Function", fn_name);
    fprintf(stderr, "  " W_DIM W_CYAN "%-20s" W_RST " %s\n", "Sink Type", v->sink_type);
    fprintf(stderr, "  " W_DIM W_CYAN "%-20s" W_RST " %s (line %d)\n", "Sink Node",
            ir_op_name(v->node->op), v->node->lineno);
    fprintf(stderr, "  " W_DIM W_CYAN "%-20s" W_RST " %s%s" W_RST "\n",
            "Tainted Operand", taint_color, v->operand_name);
    fprintf(stderr, "  " W_DIM W_CYAN "%-20s" W_RST " %s%s" W_RST "\n",
            "Taint State", taint_color, taint_str);

    /* Trace back to source */
    source_entry = wmap_find(v->operand_name);
    if (source_entry && source_entry->source_desc[0] != '\0') {
        fprintf(stderr, "\n");
        fprintf(stderr, "  " W_BMAG "── Taint Trace ──" W_RST "\n");
        fprintf(stderr, "  " W_MAG "Source" W_RST "   %s (line %d)\n",
                source_entry->source_desc, source_entry->source_lineno);
        fprintf(stderr, "  " W_MAG "→ VReg" W_RST "   %s\n", v->operand_name);
        fprintf(stderr, "  " W_MAG "→ Sink" W_RST "   %s at %s.%s (line %d)\n",
                v->sink_type, fn_name, ir_op_name(v->node->op), v->node->lineno);
    }

    fprintf(stderr, "\n");
}

/* ── Main Pass ───────────────────────────────────────────────────────── */

ir_pass_result_t ir_pass_warden(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_node_t *n;
    ir_pass_result_t r;
    int violations = 0;
    int nodes = 0;

    memset(&r, 0, sizeof(r));
    if (!fn || !fn->head) return r;

    r.nodes_before = fn->node_count;
    r.nodes_after  = fn->node_count;

    wmap_clear();

    /* ── Forward pass: identify sources and propagate taint ──────── */
    for (n = fn->head; n; n = n->next) {
        char desc[64];
        nodes++;

        /* Phase 1: Taint source identification */
        if (n->dst[0] != '\0' && is_taint_source(n, desc, sizeof(desc))) {
            wmap_set(n->dst, WARDEN_TAINTED, n->lineno, desc);
            continue;
        }

        /* Phase 2: Comparison sanitization check.
         * If a comparison op has one tainted and one constant operand,
         * the result is CHECKED (conditionally sanitized). */
        if (is_comparison_op(n->op) && n->dst[0] != '\0') {
            warden_taint_t t_src1 = wmap_get(n->src1);
            warden_taint_t t_src2 = wmap_get(n->src2);

            if ((t_src1 == WARDEN_TAINTED && t_src2 == WARDEN_SAFE) ||
                (t_src2 == WARDEN_TAINTED && t_src1 == WARDEN_SAFE)) {
                /* Tainted operand compared against a known-safe value.
                 * This is a sanitization boundary — mark result CHECKED. */
                warden_entry_t *tainted_src;
                const char *tainted_name = (t_src1 == WARDEN_TAINTED) ? n->src1 : n->src2;
                tainted_src = wmap_find(tainted_name);
                wmap_set(n->dst, WARDEN_CHECKED, n->lineno,
                         tainted_src ? tainted_src->source_desc : "comparison-sanitized");
                continue;
            }
            /* If both are tainted, the comparison result is still tainted */
            if (t_src1 != WARDEN_SAFE || t_src2 != WARDEN_SAFE) {
                warden_taint_t combined = (int)t_src1 > (int)t_src2 ? t_src1 : t_src2;
                warden_entry_t *src_entry = wmap_find(
                    (int)t_src1 >= (int)t_src2 ? n->src1 : n->src2);
                wmap_set(n->dst, combined, n->lineno,
                         src_entry ? src_entry->source_desc : "tainted-comparison");
                continue;
            }
        }

        /* Phase 3: Standard taint propagation */
        if (is_propagating_op(n->op) && n->dst[0] != '\0') {
            warden_taint_t propagated = compute_propagated_taint(n);
            if (propagated != WARDEN_SAFE) {
                /* Find the source entry for trace lineage */
                const char *src_name = (wmap_get(n->src1) != WARDEN_SAFE) ? n->src1 : n->src2;
                warden_entry_t *src_entry = wmap_find(src_name);
                wmap_set(n->dst, propagated, n->lineno,
                         src_entry ? src_entry->source_desc : "propagated");
            }
            continue;
        }

        /* Phase 4: IR_LOAD from tainted address → result is tainted */
        if (n->op == IR_LOAD && n->dst[0] != '\0') {
            warden_taint_t addr_taint = wmap_get(n->src1);
            if (addr_taint != WARDEN_SAFE) {
                warden_entry_t *addr_entry = wmap_find(n->src1);
                wmap_set(n->dst, WARDEN_TAINTED, n->lineno,
                         addr_entry ? addr_entry->source_desc : "tainted-deref");
            }
            /* Note: if not tainted and not a source, this LOAD result
             * remains SAFE (e.g., loading from a known constant address). */
            continue;
        }

        /* Phase 5: Sink detection ─────────────────────────────────── */

        /* SSTORE sink: IR_STORE tagged IR_TAG_SSTORE */
        if (n->op == IR_STORE && n->tag == (int)IR_TAG_SSTORE) {
            /* In SSTORE: dst = slot address, src1 = value.
             * Both are security-critical operands. */
            warden_taint_t slot_taint = wmap_get(n->dst);
            warden_taint_t val_taint  = wmap_get(n->src1);

            if (slot_taint == WARDEN_TAINTED) {
                warden_violation_t v;
                v.node = n;
                v.operand_name = n->dst;
                v.sink_type = "SSTORE (tainted slot address)";
                v.taint = slot_taint;
                emit_violation(&v, fn->name);
                violations++;
            }
            if (val_taint == WARDEN_TAINTED) {
                warden_violation_t v;
                v.node = n;
                v.operand_name = n->src1;
                v.sink_type = "SSTORE (tainted value)";
                v.taint = val_taint;
                emit_violation(&v, fn->name);
                violations++;
            }
            /* CHECKED taint at SSTORE is a softer warning */
            if (slot_taint == WARDEN_CHECKED || val_taint == WARDEN_CHECKED) {
                warden_violation_t v;
                v.node = n;
                v.operand_name = (slot_taint == WARDEN_CHECKED) ? n->dst : n->src1;
                v.sink_type = "SSTORE (checked — conditional sanitization)";
                v.taint = WARDEN_CHECKED;
                emit_violation(&v, fn->name);
                violations++;
            }
            continue;
        }

        /* Untrusted CALL sink: if the call's label (target address) is tainted,
         * that means we're calling an attacker-controlled address. */
        if (n->op == IR_CALL && n->tag == (int)IR_TAG_UNTRUSTED_EXTERNAL_CALL) {
            warden_taint_t target_taint = wmap_get(n->label);
            if (target_taint == WARDEN_TAINTED) {
                warden_violation_t v;
                v.node = n;
                v.operand_name = n->label;
                v.sink_type = "CALL (tainted target address)";
                v.taint = target_taint;
                emit_violation(&v, fn->name);
                violations++;
            }
        }

        /* SELFDESTRUCT sink */
        if (n->tag == (int)IR_TAG_SELFDESTRUCT) {
            warden_taint_t dst_taint = wmap_get(n->src1);
            if (dst_taint != WARDEN_SAFE) {
                warden_violation_t v;
                v.node = n;
                v.operand_name = n->src1;
                v.sink_type = "SELFDESTRUCT (tainted beneficiary)";
                v.taint = dst_taint;
                emit_violation(&v, fn->name);
                violations++;
            }
        }
    }

    /* ── Summary ─────────────────────────────────────────────────── */
    if (violations > 0) {
        fprintf(stderr, W_NAVY "╔═══════════════════════════════════════════════════════════════╗" W_RST "\n");
        fprintf(stderr, W_NAVY "║" W_BRED "  ▸ WARDEN SUMMARY: %d violation(s) in %s" W_NAVY,
                violations, fn->name);
        /* Pad to fixed width */
        {
            int pad = 42 - 4 - (int)strlen(fn->name);
            int i;
            if (pad < 0) pad = 0;
            for (i = 0; i < pad; i++) fputc(' ', stderr);
        }
        fprintf(stderr, "║" W_RST "\n");
        fprintf(stderr, W_NAVY "╚═══════════════════════════════════════════════════════════════╝" W_RST "\n");
        fprintf(stderr, "\n");
    }

    r.nodes_modified = violations;
    r.changed = 0; /* read-only pass */
    return r;
}
