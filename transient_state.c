/*
 * transient_state.c — ZCC EIP-1153 Transient Lock Analysis Pass
 * ==============================================================
 * Compiled by GCC only (linked separately, NOT concatenated into zcc.c).
 *
 * Detects transient state bleed: TSTORE(key, v!=0) not followed by
 * TSTORE(key, 0) before IR_RET exit blocks.
 *
 * EVM ENCODING (ir.h UNTOUCHED):
 *   TSTORE → IR_CALL void "" __tstore [IR_ARG slot] [IR_ARG value]
 *   TLOAD  → IR_CALL i64 dst __tload  [IR_ARG slot]
 *
 * EIP-1153 REVERT SEMANTICS (VERIFIED):
 *   Revert frames auto-clear transient storage. Only IR_RET (successful
 *   exits) are audited. Do NOT add IR_TRAP to the exit set.
 *
 * ALGORITHM: 5-phase forward dataflow over monotone lattice.
 *   Convergence: max 768 iterations (256 blocks × lattice height 3).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"

/* ── Constants ───────────────────────────────────────────────────────── */
#define TS_MAX_BLOCKS   256
#define TS_MAX_SLOTS     64
#define TS_MAX_PREDS     16
#define TS_CMAP_MAX    2048

/* ── Pass result (must match ir_pass_manager.c layout exactly) ────────── */
typedef struct {
    int nodes_before;
    int nodes_after;
    int nodes_deleted;
    int nodes_modified;
    int changed;
} ir_pass_result_t;

/* ── Slot state lattice: UNSET < CLEARED < LOCKED ────────────────────── */
typedef enum { SLOT_UNSET = 0, SLOT_CLEARED = 1, SLOT_LOCKED = 2 } slot_state_t;

/* ── Per-block transfer effect (precomputed) ─────────────────────────── */
typedef enum { TS_XFER_NONE = 0, TS_XFER_LOCK = 1, TS_XFER_CLEAR = 2 } ts_xfer_t;

/* ── Basic block ─────────────────────────────────────────────────────── */
typedef struct {
    char        label[64];
    ir_node_t  *head;
    ir_node_t  *tail;
    int         succ[2];
    int         pred[TS_MAX_PREDS];
    int         pred_count;
    int         is_exit;
} ts_block_t;

/* ── Slot entry ──────────────────────────────────────────────────────── */
typedef struct {
    long  key;
    int   is_resolved;
    char  temp[64];
} ts_slot_t;

/* ── Analysis context (~100KB, heap-allocated) ───────────────────────── */
typedef struct {
    ts_block_t   blocks[TS_MAX_BLOCKS];
    int          block_count;

    ts_slot_t    slots[TS_MAX_SLOTS];
    int          slot_count;

    ts_xfer_t    xfer[TS_MAX_BLOCKS][TS_MAX_SLOTS];
    slot_state_t slot_out[TS_MAX_BLOCKS][TS_MAX_SLOTS];

    struct { char name[32]; long value; } cmap[TS_CMAP_MAX];
    int          cmap_count;
    int          cmap_overflow;  /* H3 Fracture 3: overflow sentinel */

    int          wl[TS_MAX_BLOCKS];
    int          wl_head, wl_tail;
    unsigned char wl_in[TS_MAX_BLOCKS];
} ts_ctx_t;

/* ── Verdict ─────────────────────────────────────────────────────────── */
typedef struct {
    int unsafe_count;
    int unsafe_slots[TS_MAX_SLOTS];
    int unsafe_blocks[TS_MAX_SLOTS];
} ts_verdict_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════════════════ */

static int ts_count_nodes(ir_func_t *fn) {
    int c = 0;
    ir_node_t *n = fn->head;
    while (n) { c++; n = n->next; }
    return c;
}

/*
 * H3 Fracture 1 Patch: skip IR_NOP/IR_LABEL noise between IR_CALL
 * and its IR_ARG sequence.  Prior DCE passes overwrite dead nodes
 * with IR_NOP rather than unlinking them.
 */
static ir_node_t *ts_next_active(ir_node_t *n) {
    while (n && (n->op == IR_NOP || n->op == IR_LABEL))
        n = n->next;
    return n;
}

/* ── Constant map ────────────────────────────────────────────────────── */

static void ts_cmap_add(ts_ctx_t *ctx, const char *name, long value) {
    int i;
    for (i = 0; i < ctx->cmap_count; i++) {
        if (strcmp(ctx->cmap[i].name, name) == 0) {
            ctx->cmap[i].value = value;
            return;
        }
    }
    if (ctx->cmap_count >= TS_CMAP_MAX) {
        ctx->cmap_overflow = 1;  /* H3 Fracture 3: flag, don't crash */
        return;
    }
    strncpy(ctx->cmap[ctx->cmap_count].name, name, 31);
    ctx->cmap[ctx->cmap_count].name[31] = '\0';
    ctx->cmap[ctx->cmap_count].value = value;
    ctx->cmap_count++;
}

static int ts_cmap_get(ts_ctx_t *ctx, const char *name, long *val) {
    int i;
    if (name[0] == '\0') return 0;
    for (i = 0; i < ctx->cmap_count; i++) {
        if (strcmp(ctx->cmap[i].name, name) == 0) {
            *val = ctx->cmap[i].value;
            return 1;
        }
    }
    return 0;
}

/* ── Worklist (circular queue with dedup) ────────────────────────────── */

static void ts_wl_enqueue(ts_ctx_t *ctx, int bi) {
    if (bi < 0 || bi >= ctx->block_count) return;
    if (ctx->wl_in[bi]) return;
    ctx->wl[ctx->wl_tail] = bi;
    ctx->wl_tail = (ctx->wl_tail + 1) % TS_MAX_BLOCKS;
    ctx->wl_in[bi] = 1;
}

static int ts_wl_dequeue(ts_ctx_t *ctx) {
    int bi = ctx->wl[ctx->wl_head];
    ctx->wl_head = (ctx->wl_head + 1) % TS_MAX_BLOCKS;
    ctx->wl_in[bi] = 0;
    return bi;
}

static int ts_wl_empty(ts_ctx_t *ctx) {
    return ctx->wl_head == ctx->wl_tail;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Phase 1: Block Segmentation — split on IR_LABEL boundaries
 * ═══════════════════════════════════════════════════════════════════════ */

static void ts_segment_blocks(ir_func_t *fn, ts_ctx_t *ctx) {
    ir_node_t *n;
    ir_node_t *prev;
    int bi;

    n = fn->head;
    if (!n) return;

    bi = 0;
    prev = NULL;
    ctx->blocks[0].head = n;
    ctx->blocks[0].label[0] = '\0';
    ctx->blocks[0].succ[0] = ctx->blocks[0].succ[1] = -1;

    while (n) {
        if (n->op == IR_LABEL && prev != NULL) {
            ctx->blocks[bi].tail = prev;
            bi++;
            if (bi >= TS_MAX_BLOCKS) break;
            ctx->blocks[bi].head = n;
            strncpy(ctx->blocks[bi].label, n->label, 63);
            ctx->blocks[bi].label[63] = '\0';
            ctx->blocks[bi].succ[0] = ctx->blocks[bi].succ[1] = -1;
        }
        /*
         * EIP-1153 §3: only successful exits commit transient state.
         * Reverted frames auto-clear. IR_RET = only valid exit target.
         */
        if (n->op == IR_RET)
            ctx->blocks[bi].is_exit = 1;

        prev = n;
        n = n->next;
    }

    if (bi < TS_MAX_BLOCKS && prev) {
        ctx->blocks[bi].tail = prev;
        bi++;
    }
    ctx->block_count = bi;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Phase 2: Edge Construction — successors + predecessors
 * ═══════════════════════════════════════════════════════════════════════ */

static int ts_find_block(ts_ctx_t *ctx, const char *label) {
    int i;
    for (i = 0; i < ctx->block_count; i++) {
        if (strcmp(ctx->blocks[i].label, label) == 0)
            return i;
    }
    return -1;
}

static void ts_add_pred(ts_ctx_t *ctx, int block_idx, int pred_idx) {
    ts_block_t *b;
    if (block_idx < 0 || block_idx >= ctx->block_count) return;
    b = &ctx->blocks[block_idx];
    if (b->pred_count < TS_MAX_PREDS)
        b->pred[b->pred_count++] = pred_idx;
}

static void ts_build_edges(ts_ctx_t *ctx) {
    int i, k;
    for (i = 0; i < ctx->block_count; i++) {
        ts_block_t *b = &ctx->blocks[i];
        ir_node_t *tail = b->tail;
        ir_node_t *prev_node;
        ir_node_t *scan;

        if (!tail) continue;

        if (tail->op == IR_RET) {
            /* Exit — no successors */
        } else if (tail->op == IR_BR) {
            /* Check if prev active node is IR_BR_IF (two-way branch) */
            prev_node = NULL;
            for (scan = b->head; scan != tail; scan = scan->next) {
                if (scan->op != IR_NOP && scan->op != IR_ARG)
                    prev_node = scan;
            }
            if (prev_node && prev_node->op == IR_BR_IF) {
                b->succ[0] = ts_find_block(ctx, prev_node->label);
                b->succ[1] = ts_find_block(ctx, tail->label);
            } else {
                b->succ[0] = ts_find_block(ctx, tail->label);
            }
        } else if (tail->op == IR_BR_IF) {
            /* Conditional with fallthrough to next linear block */
            b->succ[0] = ts_find_block(ctx, tail->label);
            b->succ[1] = (i + 1 < ctx->block_count) ? i + 1 : -1;
        }

        /* Wire predecessor edges */
        for (k = 0; k < 2; k++) {
            if (b->succ[k] >= 0)
                ts_add_pred(ctx, b->succ[k], i);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Phase 3: Slot Discovery + Transfer Table
 * ═══════════════════════════════════════════════════════════════════════ */

static void ts_build_cmap(ir_func_t *fn, ts_ctx_t *ctx) {
    ir_node_t *n;
    for (n = fn->head; n; n = n->next) {
        if (n->op == IR_CONST && n->dst[0])
            ts_cmap_add(ctx, n->dst, n->imm);
    }
}

static int ts_register_slot(ts_ctx_t *ctx, long key, int resolved,
                            const char *temp) {
    int i;
    if (resolved) {
        for (i = 0; i < ctx->slot_count; i++) {
            if (ctx->slots[i].is_resolved && ctx->slots[i].key == key)
                return i;
        }
    }
    if (ctx->slot_count >= TS_MAX_SLOTS) return -1;
    i = ctx->slot_count++;
    ctx->slots[i].key = key;
    ctx->slots[i].is_resolved = resolved;
    if (temp) {
        strncpy(ctx->slots[i].temp, temp, 63);
        ctx->slots[i].temp[63] = '\0';
    }
    return i;
}

static void ts_discover_and_build_xfer(ts_ctx_t *ctx) {
    int bi;
    ir_node_t *n;

    for (bi = 0; bi < ctx->block_count; bi++) {
        n = ctx->blocks[bi].head;
        while (n) {
            if (n->op == IR_CALL && strcmp(n->label, "__tstore") == 0) {
                ir_node_t *arg_slot;
                ir_node_t *arg_val;
                long slot_key, val;
                int resolved, val_known, si;

                /* H3 Fracture 1: skip NOP noise between CALL and ARGs */
                arg_slot = ts_next_active(n->next);
                arg_val  = arg_slot ? ts_next_active(arg_slot->next) : NULL;

                if (!arg_slot || arg_slot->op != IR_ARG) goto advance;
                if (!arg_val  || arg_val->op  != IR_ARG) goto advance;

                slot_key = 0;
                resolved = ts_cmap_get(ctx, arg_slot->src1, &slot_key);
                si = ts_register_slot(ctx, slot_key, resolved,
                                      resolved ? NULL : arg_slot->src1);
                if (si < 0) goto advance;

                if (!resolved) {
                    ctx->xfer[bi][si] = TS_XFER_LOCK;  /* dynamic = UNSAFE */
                    goto advance;
                }

                val = -1;
                val_known = ts_cmap_get(ctx, arg_val->src1, &val);
                ctx->xfer[bi][si] = (val_known && val == 0)
                                     ? TS_XFER_CLEAR : TS_XFER_LOCK;
            }
advance:
            if (n == ctx->blocks[bi].tail) break;
            n = n->next;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Phase 4: Forward Dataflow — Worklist BFS
 * ═══════════════════════════════════════════════════════════════════════
 * slot_out[b][s] = transfer( join(slot_out[pred] ∀ pred), block_b )
 * join = max (pessimistic: any LOCKED predecessor → LOCKED input)
 * transfer: XFER_LOCK→LOCKED, XFER_CLEAR→CLEARED, NONE→pass-through
 */

static void ts_run_dataflow(ts_ctx_t *ctx) {
    slot_state_t in_state[TS_MAX_SLOTS];
    slot_state_t new_out[TS_MAX_SLOTS];
    int bi, p, pi, s, k, changed;

    ts_wl_enqueue(ctx, 0);

    while (!ts_wl_empty(ctx)) {
        bi = ts_wl_dequeue(ctx);

        /* Join: in = max(slot_out[pred]) for all predecessors */
        memset(in_state, 0, (size_t)ctx->slot_count * sizeof(slot_state_t));
        for (p = 0; p < ctx->blocks[bi].pred_count; p++) {
            pi = ctx->blocks[bi].pred[p];
            for (s = 0; s < ctx->slot_count; s++) {
                if (ctx->slot_out[pi][s] > in_state[s])
                    in_state[s] = ctx->slot_out[pi][s];
            }
        }

        /* Transfer */
        for (s = 0; s < ctx->slot_count; s++) {
            switch (ctx->xfer[bi][s]) {
            case TS_XFER_LOCK:  new_out[s] = SLOT_LOCKED;  break;
            case TS_XFER_CLEAR: new_out[s] = SLOT_CLEARED; break;
            default:            new_out[s] = in_state[s];   break;
            }
        }

        /* Propagate if changed */
        changed = memcmp(ctx->slot_out[bi], new_out,
                         (size_t)ctx->slot_count * sizeof(slot_state_t));
        if (changed) {
            memcpy(ctx->slot_out[bi], new_out,
                   (size_t)ctx->slot_count * sizeof(slot_state_t));
            for (k = 0; k < 2; k++) {
                if (ctx->blocks[bi].succ[k] >= 0)
                    ts_wl_enqueue(ctx, ctx->blocks[bi].succ[k]);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Phase 5: Exit Block Audit
 * ═══════════════════════════════════════════════════════════════════════ */

static void ts_audit_exits(ts_ctx_t *ctx, ts_verdict_t *v) {
    int bi, s;
    memset(v, 0, sizeof(*v));
    for (bi = 0; bi < ctx->block_count; bi++) {
        if (!ctx->blocks[bi].is_exit) continue;
        for (s = 0; s < ctx->slot_count; s++) {
            if (ctx->slot_out[bi][s] == SLOT_LOCKED &&
                v->unsafe_count < TS_MAX_SLOTS) {
                v->unsafe_slots[v->unsafe_count] = s;
                v->unsafe_blocks[v->unsafe_count] = bi;
                v->unsafe_count++;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Entry Point
 * ═══════════════════════════════════════════════════════════════════════ */

ir_pass_result_t zcc_pass_transient_locks(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_pass_result_t r;
    ts_ctx_t *ctx;
    ts_verdict_t verdict;
    int i;

    memset(&r, 0, sizeof(r));
    r.nodes_before = ts_count_nodes(fn);

    ctx = (ts_ctx_t *)calloc(1, sizeof(ts_ctx_t));
    if (!ctx) {
        fprintf(stderr, "[transient_lock_audit] FATAL: OOM (%lu bytes)\n",
                (unsigned long)sizeof(ts_ctx_t));
        r.nodes_after = r.nodes_before;
        return r;
    }

    /* Phase 1 */ ts_segment_blocks(fn, ctx);
    /* Phase 2 */ ts_build_edges(ctx);
    /* Phase 3 */ ts_build_cmap(fn, ctx);
                  ts_discover_and_build_xfer(ctx);

    if (ctx->slot_count == 0) {
        free(ctx);
        r.nodes_after = r.nodes_before;
        return r;  /* No transient ops — trivially VERIFIED */
    }

    /* Phase 4 */ ts_run_dataflow(ctx);
    /* Phase 5 */ ts_audit_exits(ctx, &verdict);

    /* ── Report ─────────────────────────────────────────────────────── */
    if (verdict.unsafe_count > 0) {
        fprintf(stderr,
            "[transient_lock_audit] UNSAFE: %d slot(s) locked at exit in %s()\n",
            verdict.unsafe_count, fn->name);
        for (i = 0; i < verdict.unsafe_count; i++) {
            ts_slot_t *sl = &ctx->slots[verdict.unsafe_slots[i]];
            if (sl->is_resolved) {
                fprintf(stderr, "  slot key=%ld at exit block \"%s\"\n",
                        sl->key,
                        ctx->blocks[verdict.unsafe_blocks[i]].label);
            } else {
                fprintf(stderr,
                        "  DYNAMIC slot (temp=%s) at exit block \"%s\"\n",
                        sl->temp,
                        ctx->blocks[verdict.unsafe_blocks[i]].label);
            }
        }
        r.changed = 1;
    }

    /* H3 Fracture 3: warn if constant pool was exhausted */
    if (ctx->cmap_overflow) {
        fprintf(stderr,
            "[transient_lock_audit] WARNING: constant map overflow "
            "(>%d temps). Some slots may report false-positive UNSAFE.\n",
            TS_CMAP_MAX);
    }

    r.nodes_after = r.nodes_before;  /* analysis pass — no mutations */
    free(ctx);
    return r;
}
