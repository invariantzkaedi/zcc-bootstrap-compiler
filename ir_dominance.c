/*
 * ir_dominance.c — Basic Block Graph & Dominator Tree for ZCC IR
 * ===============================================================
 * Compiled by GCC (linked separately, NOT concatenated into zcc.c).
 *
 * Partitions the linear IR node list into basic blocks, wires CFG
 * edges, and computes the dominator tree using the standard iterative
 * data-flow algorithm.
 *
 * The dominator tree enables GVN to propagate value numbering across
 * block boundaries while maintaining correctness: a definition in
 * block A is visible in block B only if A dominates B.
 */

#include <stdio.h>
#include <string.h>
#include "ir.h"
#include "ir_dominance.h"

/* ── Global context for the current function ─────────────────────────── */
static dom_cfg_t s_dom_cfg;

const dom_cfg_t *dom_get_cfg(void) {
    return &s_dom_cfg;
}

/* ── BB Graph Construction ───────────────────────────────────────────── */

/*
 * Determine if an opcode terminates a basic block (is a block terminator).
 */
static int is_terminator(ir_op_t op) {
    return op == IR_BR || op == IR_BR_IF || op == IR_RET;
}

/*
 * Find the block index by label name, or -1 if not found.
 */
static int find_block_by_label(const dom_cfg_t *cfg, const char *label) {
    int i;
    if (!label || label[0] == '\0') return -1;
    for (i = 0; i < cfg->block_count; i++) {
        if (cfg->blocks[i].label[0] != '\0' &&
            strcmp(cfg->blocks[i].label, label) == 0) {
            return i;
        }
    }
    return -1;
}

/*
 * Add a predecessor edge: block[to].pred[] += from
 */
static void add_pred(dom_cfg_t *cfg, int to, int from) {
    int i;
    dom_bb_t *b;
    if (to < 0 || to >= cfg->block_count) return;
    if (from < 0 || from >= cfg->block_count) return;
    b = &cfg->blocks[to];
    /* Deduplicate */
    for (i = 0; i < b->pred_count; i++) {
        if (b->pred[i] == from) return;
    }
    if (b->pred_count < DOM_MAX_PREDS) {
        b->pred[b->pred_count++] = from;
    }
}

/*
 * Add a successor edge: block[from].succ[] += to
 */
static void add_succ(dom_cfg_t *cfg, int from, int to) {
    int i;
    dom_bb_t *b;
    if (from < 0 || from >= cfg->block_count) return;
    if (to < 0 || to >= cfg->block_count) return;
    b = &cfg->blocks[from];
    for (i = 0; i < b->succ_count; i++) {
        if (b->succ[i] == to) return;
    }
    if (b->succ_count < DOM_MAX_SUCCS) {
        b->succ[b->succ_count++] = to;
    }
}

int dom_build_cfg(dom_cfg_t *cfg, ir_func_t *fn) {
    ir_node_t *n;
    int bid;
    int i;

    memset(cfg, 0, sizeof(*cfg));
    cfg->fn = fn;

    if (!fn || !fn->head) return 0;

    /* ── Phase 1: Identify block boundaries ──────────────────────── */
    /*
     * A new block starts:
     *   1. At the first node of the function
     *   2. At any IR_LABEL node
     *   3. At the node immediately following a terminator
     */
    bid = 0;
    cfg->blocks[0].id = 0;
    cfg->blocks[0].first = fn->head;
    cfg->blocks[0].label[0] = '\0';
    cfg->blocks[0].idom = -1;

    /* If the first node is a label, capture it */
    if (fn->head->op == IR_LABEL) {
        strncpy(cfg->blocks[0].label, fn->head->label, IR_LABEL_MAX - 1);
        cfg->blocks[0].label[IR_LABEL_MAX - 1] = '\0';
    }

    for (n = fn->head; n; n = n->next) {
        /* Check if this node starts a new block */
        int start_new = 0;

        if (n != fn->head) {
            /* After a terminator, the next node starts a new block */
            if (n->op == IR_LABEL) {
                start_new = 1;
            }
        }

        if (start_new) {
            /* Close current block */
            ir_node_t *prev = fn->head;
            ir_node_t *scan;
            for (scan = fn->head; scan && scan->next != n; scan = scan->next)
                prev = scan;
            /* prev->next == n, so prev is the last node of the prev block
             * only if we haven't already passed it */
            cfg->blocks[bid].last = scan;

            /* Open new block */
            bid++;
            if (bid >= DOM_MAX_BLOCKS) return -1;
            cfg->blocks[bid].id = bid;
            cfg->blocks[bid].first = n;
            cfg->blocks[bid].idom = -1;

            if (n->op == IR_LABEL) {
                strncpy(cfg->blocks[bid].label, n->label, IR_LABEL_MAX - 1);
                cfg->blocks[bid].label[IR_LABEL_MAX - 1] = '\0';
            } else {
                cfg->blocks[bid].label[0] = '\0';
            }
        }
    }

    /* Close the last block */
    {
        ir_node_t *tail = fn->head;
        while (tail->next) tail = tail->next;
        cfg->blocks[bid].last = tail;
    }

    cfg->block_count = bid + 1;

    /* ── Phase 2: Wire CFG edges ─────────────────────────────────── */
    for (i = 0; i < cfg->block_count; i++) {
        ir_node_t *last = cfg->blocks[i].last;
        if (!last) continue;

        if (last->op == IR_BR) {
            /* Unconditional branch to label */
            int target = find_block_by_label(cfg, last->label);
            if (target >= 0) {
                add_succ(cfg, i, target);
                add_pred(cfg, target, i);
            }
        } else if (last->op == IR_BR_IF) {
            /* Conditional: taken branch to label, fallthrough to next block */
            int target = find_block_by_label(cfg, last->label);
            if (target >= 0) {
                add_succ(cfg, i, target);
                add_pred(cfg, target, i);
            }
            /* Fallthrough to next block */
            if (i + 1 < cfg->block_count) {
                add_succ(cfg, i, i + 1);
                add_pred(cfg, i + 1, i);
            }
        } else if (last->op == IR_RET) {
            /* No successors */
        } else {
            /* Fallthrough: last instruction is not a terminator */
            if (i + 1 < cfg->block_count) {
                add_succ(cfg, i, i + 1);
                add_pred(cfg, i + 1, i);
            }
        }
    }

    return 0;
}

/* ── Dominator Computation ───────────────────────────────────────────── */

/*
 * Iterative dominator algorithm (Cooper, Harvey, Kennedy, 2001 simplified).
 *
 * dom[n] = intersection of dom[p] for all predecessors p of n, union {n}
 *
 * We use bit-sets (one bit per block) for dom sets.
 * With DOM_MAX_BLOCKS = 512, we need 512/64 = 8 uint64_t words per set.
 */
#define DOM_WORDS ((DOM_MAX_BLOCKS + 63) / 64)

typedef struct {
    unsigned long long bits[DOM_WORDS];
} dom_bitset_t;

static void bs_clear(dom_bitset_t *s) {
    int i;
    for (i = 0; i < DOM_WORDS; i++) s->bits[i] = 0;
}

static void bs_fill(dom_bitset_t *s, int n) {
    int i;
    int full_words = n / 64;
    int rem = n % 64;
    for (i = 0; i < full_words; i++) s->bits[i] = ~0ULL;
    if (rem > 0) s->bits[full_words] = (1ULL << rem) - 1;
    for (i = full_words + 1; i < DOM_WORDS; i++) s->bits[i] = 0;
}

static void bs_set(dom_bitset_t *s, int bit) {
    s->bits[bit / 64] |= (1ULL << (bit % 64));
}

static int bs_test(const dom_bitset_t *s, int bit) {
    return (s->bits[bit / 64] >> (bit % 64)) & 1;
}

static void bs_and(dom_bitset_t *dst, const dom_bitset_t *a, const dom_bitset_t *b) {
    int i;
    for (i = 0; i < DOM_WORDS; i++) dst->bits[i] = a->bits[i] & b->bits[i];
}

static int bs_equal(const dom_bitset_t *a, const dom_bitset_t *b) {
    int i;
    for (i = 0; i < DOM_WORDS; i++) {
        if (a->bits[i] != b->bits[i]) return 0;
    }
    return 1;
}

/* Static storage for dominator sets — avoids malloc */
static dom_bitset_t s_dom_sets[DOM_MAX_BLOCKS];

void dom_compute_idom(dom_cfg_t *cfg) {
    int n = cfg->block_count;
    int i, j, changed;
    dom_bitset_t tmp;

    if (n == 0) return;

    /* Initialize: dom[entry] = {entry}, dom[all others] = all blocks */
    for (i = 0; i < n; i++) {
        if (i == 0) {
            bs_clear(&s_dom_sets[i]);
            bs_set(&s_dom_sets[i], 0);
        } else {
            bs_fill(&s_dom_sets[i], n);
        }
    }

    /* Iterate until fixed point */
    do {
        changed = 0;
        for (i = 1; i < n; i++) {  /* skip entry block */
            dom_bitset_t new_dom;

            if (cfg->blocks[i].pred_count == 0) {
                /* Unreachable block — dom = {self} */
                bs_clear(&new_dom);
                bs_set(&new_dom, i);
            } else {
                /* Start with dom[first predecessor] */
                int first_pred = cfg->blocks[i].pred[0];
                new_dom = s_dom_sets[first_pred];

                /* Intersect with dom of all other predecessors */
                for (j = 1; j < cfg->blocks[i].pred_count; j++) {
                    int p = cfg->blocks[i].pred[j];
                    bs_and(&tmp, &new_dom, &s_dom_sets[p]);
                    new_dom = tmp;
                }
            }

            /* Union with self */
            bs_set(&new_dom, i);

            if (!bs_equal(&new_dom, &s_dom_sets[i])) {
                s_dom_sets[i] = new_dom;
                changed = 1;
            }
        }
    } while (changed);

    /* Extract IDOM from dom sets.
     * idom[n] = the closest strict dominator of n.
     * For each block b (b != entry), idom is the block d in dom[b] \ {b}
     * such that d dominates all other blocks in dom[b] \ {b}.
     * Equivalently: d such that |dom[d]| is maximal among dom[b] \ {b}. */
    cfg->blocks[0].idom = -1;  /* entry has no dominator */
    for (i = 1; i < n; i++) {
        int best = -1;
        int best_dom_size = -1;
        for (j = 0; j < n; j++) {
            if (j == i) continue;
            if (!bs_test(&s_dom_sets[i], j)) continue;
            /* j dominates i — count |dom[j]| */
            int dom_size = 0;
            int k;
            for (k = 0; k < n; k++) {
                if (bs_test(&s_dom_sets[j], k)) dom_size++;
            }
            if (dom_size > best_dom_size) {
                best_dom_size = dom_size;
                best = j;
            }
        }
        cfg->blocks[i].idom = best;
    }
}

/* ── Dominator Tree Construction ─────────────────────────────────────── */

static void dom_preorder(dom_cfg_t *cfg, int bid, int *counter) {
    int i;
    cfg->blocks[bid].dom_order = (*counter)++;
    for (i = 0; i < cfg->blocks[bid].child_count; i++) {
        dom_preorder(cfg, cfg->blocks[bid].children[i], counter);
    }
}

void dom_build_tree(dom_cfg_t *cfg) {
    int i, counter;

    /* Clear children arrays */
    for (i = 0; i < cfg->block_count; i++) {
        cfg->blocks[i].child_count = 0;
        cfg->blocks[i].dom_order = -1;
    }

    /* Build children from idom */
    for (i = 1; i < cfg->block_count; i++) {
        int parent = cfg->blocks[i].idom;
        if (parent >= 0 && parent < cfg->block_count) {
            dom_bb_t *p = &cfg->blocks[parent];
            if (p->child_count < DOM_MAX_CHILDREN) {
                p->children[p->child_count++] = i;
            }
        }
    }

    /* Pre-order numbering */
    counter = 0;
    dom_preorder(cfg, 0, &counter);

    /* Number unreachable blocks */
    for (i = 0; i < cfg->block_count; i++) {
        if (cfg->blocks[i].dom_order < 0) {
            cfg->blocks[i].dom_order = counter++;
        }
    }
}

/* ── Utility ─────────────────────────────────────────────────────────── */

int dom_find_block_for_node(const dom_cfg_t *cfg, const ir_node_t *node) {
    int i;
    for (i = 0; i < cfg->block_count; i++) {
        ir_node_t *n;
        for (n = cfg->blocks[i].first; n; n = n->next) {
            if (n == node) return i;
            if (n == cfg->blocks[i].last) break;
        }
    }
    return -1;
}

/* ── Pass Entry Point ────────────────────────────────────────────────── */

ir_pass_result_t ir_pass_dominance(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_pass_result_t r;

    memset(&r, 0, sizeof(r));
    if (!fn || !fn->head) return r;

    r.nodes_before = fn->node_count;
    r.nodes_after  = fn->node_count;

    /* Build CFG */
    if (dom_build_cfg(&s_dom_cfg, fn) < 0) {
        fprintf(stderr, "[dominance] WARNING: block count exceeded %d\n",
                DOM_MAX_BLOCKS);
        return r;
    }

    /* Compute dominators */
    dom_compute_idom(&s_dom_cfg);

    /* Build dominator tree with pre-order numbering */
    dom_build_tree(&s_dom_cfg);

    r.nodes_modified = s_dom_cfg.block_count;
    r.changed = 0;  /* read-only analysis pass */
    return r;
}
