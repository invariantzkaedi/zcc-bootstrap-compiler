/*
 * ir_pass_manager.c — ZCC IR Pass Manager Implementation
 * ========================================================
 * Compiled by GCC only (linked separately, NOT concatenated into zcc.c).
 * Operates on ir_func_t* linked lists defined in ir.h.
 *
 * Production passes:
 *   DCE            — backward liveness scan, unlinks dead definitions
 *   Constant Fold  — evaluates binary ops on known constants
 *   Strength Reduce — mul-by-0 → const 0, add/sub-by-0 → copy
 *
 * Default pipeline: DCE → const_fold → strength_reduce → DCE
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"

/* ── Globals referenced by zcc.c (part5.c declares these extern) ──────── */
int  g_manifold_enabled    = 0;
char g_ir_export_path[256] = {0};
int  g_peephole_enabled    = 0;
int  g_peephole_deterministic = 0;
int  g_peephole_verbose    = 0;


/* ── Pass result type ────────────────────────────────────────────────── */

typedef struct {
    int nodes_before;
    int nodes_after;
    int nodes_deleted;
    int nodes_modified;
    int changed;
} ir_pass_result_t;

typedef ir_pass_result_t (*ir_pass_fn)(void *fn_ptr);

#define IR_PM_MAX_PASSES 16

typedef struct {
    const char    *name;
    ir_pass_fn     fn;
    int            enabled;
} ir_pass_entry_t;

typedef struct {
    ir_pass_entry_t passes[IR_PM_MAX_PASSES];
    int             count;
    int             verbose;
} ir_pass_manager_t;

/* ── Helpers ─────────────────────────────────────────────────────────── */

static int count_nodes(ir_func_t *fn) {
    int count = 0;
    ir_node_t *n = fn->head;
    while (n) {
        count++;
        n = n->next;
    }
    return count;
}

/* Check if an opcode is side-effectful (must not be DCE'd) */
static int is_side_effect(ir_op_t op) {
    switch (op) {
    case IR_STORE:
    case IR_CALL:
    case IR_RET:
    case IR_BR:
    case IR_BR_IF:
    case IR_LABEL:
    case IR_ARG:
        return 1;
    default:
        return 0;
    }
}

/* Check if a node references 'name' as a source operand.
 * For IR_STORE, dst is actually a USE (the address), not a definition. */
static int node_uses(ir_node_t *n, const char *name) {
    if (name[0] == '\0') return 0;
    if (n->src1[0] && strcmp(n->src1, name) == 0) return 1;
    if (n->src2[0] && strcmp(n->src2, name) == 0) return 1;
    /* IR_STORE uses dst as the address operand — it's a USE, not a DEF */
    if (n->op == IR_STORE && n->dst[0] && strcmp(n->dst, name) == 0) return 1;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════
 * PASS 1: Dead Code Elimination (DCE)
 * ════════════════════════════════════════════════════════════════════════
 * Scan the node list.  A node is dead if:
 *   1) It defines a temp (dst[0] != '\0')
 *   2) It is NOT side-effectful (not store/call/ret/br/br_if/label/arg)
 *   3) It is NOT IR_STORE (where dst is a use, not a definition)
 *   4) No subsequent node references dst in src1, src2, or store-dst
 *
 * Complexity: O(N²) per function. With ~300 nodes/function average,
 * this is ~90K comparisons/function — negligible.
 */
static ir_pass_result_t ir_pass_dce(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_pass_result_t r;
    ir_node_t *prev;
    ir_node_t *n;
    ir_node_t *scan;
    ir_node_t *next;
    int deleted = 0;

    memset(&r, 0, sizeof(r));
    r.nodes_before = count_nodes(fn);

    prev = NULL;
    n = fn->head;
    while (n) {
        next = n->next;

        /* Does this node define a temp that could be dead? */
        if (n->dst[0] != '\0'
            && !is_side_effect(n->op)
            && n->op != IR_STORE) {

            /* Check if ANY subsequent node uses this temp */
            int used = 0;
            for (scan = next; scan; scan = scan->next) {
                if (node_uses(scan, n->dst)) {
                    used = 1;
                    break;
                }
            }

            if (!used) {
                /* Dead node — unlink from list */
                if (prev) {
                    prev->next = next;
                } else {
                    fn->head = next;
                }
                if (n == fn->tail) {
                    fn->tail = prev;
                }
                free(n);
                fn->node_count--;
                deleted++;
                /* Don't advance prev; it still points to the right place */
                n = next;
                continue;
            }
        }

        prev = n;
        n = next;
    }

    r.nodes_after = r.nodes_before - deleted;
    r.nodes_deleted = deleted;
    r.changed = deleted > 0;
    return r;
}

/* ════════════════════════════════════════════════════════════════════════
 * PASS 2: Constant Folding
 * ════════════════════════════════════════════════════════════════════════
 * Scan forward.  Track temps defined by IR_CONST.  When a binary op
 * has both src1 and src2 as known constants, evaluate at compile time
 * and replace the node with IR_CONST.
 */

#define CONST_MAP_MAX 2048

typedef struct {
    char name[32];  /* IR_NAME_MAX */
    long value;
} const_map_entry_t;

static const_map_entry_t s_cmap[CONST_MAP_MAX];
static int s_cmap_count;

static void cmap_clear(void) {
    s_cmap_count = 0;
}

static void cmap_add(const char *name, long value) {
    int i;
    /* Update if exists */
    for (i = 0; i < s_cmap_count; i++) {
        if (strcmp(s_cmap[i].name, name) == 0) {
            s_cmap[i].value = value;
            return;
        }
    }
    /* Add new */
    if (s_cmap_count >= CONST_MAP_MAX) return;
    strncpy(s_cmap[s_cmap_count].name, name, 31);
    s_cmap[s_cmap_count].name[31] = '\0';
    s_cmap[s_cmap_count].value = value;
    s_cmap_count++;
}

static int cmap_get(const char *name, long *value) {
    int i;
    if (name[0] == '\0') return 0;
    for (i = 0; i < s_cmap_count; i++) {
        if (strcmp(s_cmap[i].name, name) == 0) {
            *value = s_cmap[i].value;
            return 1;
        }
    }
    return 0;
}

static ir_pass_result_t ir_pass_const_fold(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_pass_result_t r;
    ir_node_t *n;
    int modified = 0;

    memset(&r, 0, sizeof(r));
    r.nodes_before = count_nodes(fn);

    cmap_clear();

    for (n = fn->head; n; n = n->next) {
        /* Track constants */
        if (n->op == IR_CONST && n->dst[0]) {
            cmap_add(n->dst, n->imm);
            continue;
        }

        /* Check binary ops with two known constant operands */
        if (n->src1[0] && n->src2[0]) {
            long v1, v2, result;

            if (!cmap_get(n->src1, &v1)) continue;
            if (!cmap_get(n->src2, &v2)) continue;

            switch (n->op) {
            case IR_ADD: result = v1 + v2; break;
            case IR_SUB: result = v1 - v2; break;
            case IR_MUL: result = v1 * v2; break;
            case IR_DIV: if (v2 == 0) continue; result = v1 / v2; break;
            case IR_MOD: if (v2 == 0) continue; result = v1 % v2; break;
            case IR_AND: result = v1 & v2; break;
            case IR_OR:  result = v1 | v2; break;
            case IR_XOR: result = v1 ^ v2; break;
            case IR_SHL: result = v1 << v2; break;
            case IR_SHR: result = v1 >> v2; break;
            case IR_EQ:  result = (v1 == v2) ? 1 : 0; break;
            case IR_NE:  result = (v1 != v2) ? 1 : 0; break;
            case IR_LT:  result = (v1 < v2)  ? 1 : 0; break;
            case IR_LE:  result = (v1 <= v2) ? 1 : 0; break;
            case IR_GT:  result = (v1 > v2)  ? 1 : 0; break;
            case IR_GE:  result = (v1 >= v2) ? 1 : 0; break;
            default: continue;
            }

            /* Replace with IR_CONST */
            n->op = IR_CONST;
            n->imm = result;
            n->src1[0] = '\0';
            n->src2[0] = '\0';

            /* Track the new constant */
            cmap_add(n->dst, result);

            modified++;
        }
    }

    r.nodes_after = r.nodes_before; /* const fold mutates, doesn't delete */
    r.nodes_modified = modified;
    r.changed = modified > 0;
    return r;
}

/* ════════════════════════════════════════════════════════════════════════
 * PASS 3: Strength Reduction
 * ════════════════════════════════════════════════════════════════════════
 * Pattern-match on operations with one known constant operand:
 *   MUL dst, src, 0  → CONST dst, 0
 *   MUL dst, 0, src  → CONST dst, 0
 *   ADD dst, src, 0  → COPY  dst, src
 *   ADD dst, 0, src  → COPY  dst, src
 *   SUB dst, src, 0  → COPY  dst, src
 *
 * Phase 3 will add: MUL 2^N → SHL N, DIV 2^N → SHR N (unsigned)
 */
static ir_pass_result_t ir_pass_strength_reduce(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_pass_result_t r;
    ir_node_t *n;
    int modified = 0;

    memset(&r, 0, sizeof(r));
    r.nodes_before = count_nodes(fn);

    /* Collect constants */
    cmap_clear();
    for (n = fn->head; n; n = n->next) {
        if (n->op == IR_CONST && n->dst[0]) {
            cmap_add(n->dst, n->imm);
        }
    }

    /* Apply strength reductions */
    for (n = fn->head; n; n = n->next) {
        long val;

        if (n->op == IR_MUL) {
            /* MUL by 0 (either operand) → CONST 0 */
            if (cmap_get(n->src2, &val) && val == 0) {
                n->op = IR_CONST;
                n->imm = 0;
                n->src1[0] = '\0';
                n->src2[0] = '\0';
                cmap_add(n->dst, 0);
                modified++;
                continue;
            }
            if (cmap_get(n->src1, &val) && val == 0) {
                n->op = IR_CONST;
                n->imm = 0;
                n->src1[0] = '\0';
                n->src2[0] = '\0';
                cmap_add(n->dst, 0);
                modified++;
                continue;
            }
            /* MUL by 1 → COPY */
            if (cmap_get(n->src2, &val) && val == 1) {
                n->op = IR_COPY;
                /* src1 stays, src2 cleared */
                n->src2[0] = '\0';
                modified++;
                continue;
            }
            if (cmap_get(n->src1, &val) && val == 1) {
                n->op = IR_COPY;
                /* swap src2 into src1 position */
                strncpy(n->src1, n->src2, 31);
                n->src1[31] = '\0';
                n->src2[0] = '\0';
                modified++;
                continue;
            }
        }

        if (n->op == IR_ADD) {
            /* ADD src, 0 → COPY src */
            if (cmap_get(n->src2, &val) && val == 0) {
                n->op = IR_COPY;
                n->src2[0] = '\0';
                modified++;
                continue;
            }
            if (cmap_get(n->src1, &val) && val == 0) {
                n->op = IR_COPY;
                strncpy(n->src1, n->src2, 31);
                n->src1[31] = '\0';
                n->src2[0] = '\0';
                modified++;
                continue;
            }
        }

        if (n->op == IR_SUB) {
            /* SUB src, 0 → COPY src */
            if (cmap_get(n->src2, &val) && val == 0) {
                n->op = IR_COPY;
                n->src2[0] = '\0';
                modified++;
                continue;
            }
        }
    }

    r.nodes_after = r.nodes_before; /* strength reduce mutates, doesn't delete */
    r.nodes_modified = modified;
    r.changed = modified > 0;
    return r;
}

/* ── Registry API ────────────────────────────────────────────────────── */

static ir_pass_manager_t *ir_pm_create(void) {
    ir_pass_manager_t *pm = (ir_pass_manager_t *)calloc(1, sizeof(ir_pass_manager_t));
    if (!pm) {
        fprintf(stderr, "ir_pm_create: out of memory\n");
        exit(1);
    }
    return pm;
}

static void ir_pm_register(ir_pass_manager_t *pm, const char *name, ir_pass_fn fn) {
    if (pm->count >= IR_PM_MAX_PASSES) {
        fprintf(stderr, "ir_pm_register: too many passes (max %d)\n", IR_PM_MAX_PASSES);
        return;
    }
    pm->passes[pm->count].name = name;
    pm->passes[pm->count].fn = fn;
    pm->passes[pm->count].enabled = 1;
    pm->count++;
}

static void ir_pm_run(ir_pass_manager_t *pm, ir_module_t *mod) {
    int p;
    int f;
    int total_nodes_in = 0;
    int total_nodes_out = 0;
    int total_deleted = 0;
    int total_modified = 0;

    for (p = 0; p < pm->count; p++) {
        int pass_before = 0;
        int pass_after = 0;
        int pass_deleted = 0;
        int pass_modified = 0;

        if (!pm->passes[p].enabled) continue;

        for (f = 0; f < mod->func_count; f++) {
            ir_pass_result_t r = pm->passes[p].fn(mod->funcs[f]);
            pass_before += r.nodes_before;
            pass_after += r.nodes_after;
            pass_deleted += r.nodes_deleted;
            pass_modified += r.nodes_modified;
        }

        if (pm->verbose) {
            fprintf(stderr, "  [IR Pass] %-18s: %d funcs, %d nodes -> %d nodes (%d deleted, %d modified)\n",
                    pm->passes[p].name, mod->func_count, pass_before, pass_after,
                    pass_deleted, pass_modified);
        }

        if (p == 0) total_nodes_in = pass_before;
        total_nodes_out = pass_after;
        total_deleted += pass_deleted;
        total_modified += pass_modified;
    }

    if (pm->verbose && pm->count > 0) {
        fprintf(stderr, "  [IR Pass] Pipeline complete: %d nodes -> %d nodes (total: %d deleted, %d modified)\n",
                total_nodes_in, total_nodes_out, total_deleted, total_modified);
    }
}

static void ir_pm_free(ir_pass_manager_t *pm) {
    if (pm) free(pm);
}

/* ── Primary entry point (called from part5.c) ──────────────────────── */

void ir_pm_run_default(void *mod_ptr, int verbose) {
    ir_module_t *mod = (ir_module_t *)mod_ptr;
    ir_pass_manager_t *pm;

    if (!mod || mod->func_count == 0) return;

    pm = ir_pm_create();
    pm->verbose = verbose;

    /* Default pipeline: DCE → const_fold → strength_reduce → DCE */
    ir_pm_register(pm, "dce", ir_pass_dce);
    ir_pm_register(pm, "const_fold", ir_pass_const_fold);
    ir_pm_register(pm, "strength_reduce", ir_pass_strength_reduce);
    ir_pm_register(pm, "dce2", ir_pass_dce);

    ir_pm_run(pm, mod);
    ir_pm_free(pm);
}
