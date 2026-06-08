/*
 * regalloc.c — Linear Scan Register Allocator for ZCC IR Backend
 *
 * See regalloc.h for design rationale.
 *
 * The IR is a singly-linked list of ir_node_t with string-named fields:
 *   n->dst   — destination temp (e.g. "%t3", "%stack_-8", "")
 *   n->src1  — first source temp
 *   n->src2  — second source temp
 *
 * Only names with prefix "%t" (pure SSA temporaries) are candidates.
 * %stack_* names are addressable locals: leave them on the stack always.
 */

#include "regalloc.h"
#include "ir_dominance.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Physical register table ─────────────────────────────────────────── */

static const char *preg_names[PREG_COUNT] = {
    "rbx", "r12", "r13", "r14", "r15",  /* callee-saved */
    "r10", "r11",                         /* caller-saved scratch */
    "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
};

const char *preg_name(PhysReg r) {
    if (r < 0 || r >= PREG_COUNT) return "???";
    return preg_names[r];
}

int preg_callee_saved(PhysReg r) {
    return (r >= PREG_RBX && r <= PREG_R15);
}

/* ── Interval helpers ────────────────────────────────────────────────── */

static int is_temp(const char *name) {
    return (name && name[0] == '%' && name[1] == 't' &&
            name[2] >= '0' && name[2] <= '9');
}

static LiveInterval *find_interval(RegAllocator *ra, const char *name) {
    int i;
    for (i = 0; i < ra->num_intervals; i++) {
        if (strcmp(ra->intervals[i].name, name) == 0)
            return &ra->intervals[i];
    }
    return NULL;
}

static LiveInterval *get_or_create(RegAllocator *ra, const char *name, int pos) {
    LiveInterval *iv = find_interval(ra, name);
    if (iv) return iv;

    /* Grow if needed */
    if (ra->num_intervals >= ra->cap_intervals) {
        int newcap = ra->cap_intervals ? ra->cap_intervals * 2 : 64;
        LiveInterval *nb = (LiveInterval *)realloc(ra->intervals,
                                newcap * sizeof(LiveInterval));
        if (!nb) { fprintf(stderr, "[regalloc] OOM\n"); exit(1); }
        ra->intervals = nb;
        ra->cap_intervals = newcap;
    }

    iv = &ra->intervals[ra->num_intervals++];
    strncpy(iv->name, name, RA_NAME_MAX - 1);
    iv->name[RA_NAME_MAX - 1] = '\0';
    iv->start    = pos;
    iv->end      = pos;
    iv->assigned = PREG_NONE;
    iv->is_float = 0;
    iv->ref_count = 0;
    iv->loop_depth_weight = 0;
    iv->pressure_score = 0;
    iv->is_move = 0;
    return iv;
}

/* ── Allocator lifecycle ─────────────────────────────────────────────── */

RegAllocator *ra_create(void) {
    RegAllocator *ra = (RegAllocator *)calloc(1, sizeof(RegAllocator));
    if (!ra) { fprintf(stderr, "[regalloc] OOM\n"); exit(1); }
    return ra;
}

void ra_free(RegAllocator *ra) {
    if (!ra) return;
    free(ra->intervals);
    free(ra);
}

/* ── Sort helper for qsort ───────────────────────────────────────────── */

static int iv_cmp_start(const void *a, const void *b) {
    const LiveInterval *ia = (const LiveInterval *)a;
    const LiveInterval *ib = (const LiveInterval *)b;
    if (ia->start != ib->start)
        return ia->start - ib->start;
    return strcmp(ia->name, ib->name);
}

static void add_ref(RegAllocator *ra, const char *name, int pos, int weight, int is_def, ir_type_t ty) {
    LiveInterval *iv = get_or_create(ra, name, pos);
    if (is_def && (ty == IR_TY_F32 || ty == IR_TY_F64)) {
        iv->is_float = 1;
    }
    if (pos > iv->end) iv->end = pos;
    iv->ref_count++;
    iv->loop_depth_weight += weight;
}

static void build_intervals(RegAllocator *ra, const ir_func_t *fn) {
    const ir_node_t *n;
    int pos = 0;

    /* Build CFG for loop depth calculations */
    dom_cfg_t cfg;
    int has_cfg = 0;
    int loop_depth[DOM_MAX_BLOCKS];
    memset(loop_depth, 0, sizeof(loop_depth));

    if (dom_build_cfg(&cfg, (ir_func_t *)fn) >= 0) {
        dom_compute_idom(&cfg);
        has_cfg = 1;

        /* Calculate loop depth for each block */
        for (int h = 0; h < cfg.block_count; h++) {
            int has_back_edge = 0;
            for (int pi = 0; pi < cfg.blocks[h].pred_count; pi++) {
                int pred = cfg.blocks[h].pred[pi];
                if (dom_dominates(&cfg, h, pred)) {
                    has_back_edge = 1;
                    break;
                }
            }
            if (!has_back_edge) continue;

            char in_loop[DOM_MAX_BLOCKS];
            memset(in_loop, 0, sizeof(in_loop));
            in_loop[h] = 1;

            int queue[DOM_MAX_BLOCKS];
            int qhead = 0, qtail = 0;

            for (int pi = 0; pi < cfg.blocks[h].pred_count; pi++) {
                int pred = cfg.blocks[h].pred[pi];
                if (dom_dominates(&cfg, h, pred)) {
                    if (!in_loop[pred]) {
                        in_loop[pred] = 1;
                        queue[qtail++] = pred;
                    }
                }
            }

            while (qhead < qtail) {
                int curr = queue[qhead++];
                for (int pi = 0; pi < cfg.blocks[curr].pred_count; pi++) {
                    int pred = cfg.blocks[curr].pred[pi];
                    if (!in_loop[pred]) {
                        in_loop[pred] = 1;
                        queue[qtail++] = pred;
                    }
                }
            }

            for (int b = 0; b < cfg.block_count; b++) {
                if (in_loop[b]) {
                    loop_depth[b]++;
                }
            }
        }
    }

    for (n = fn->head; n; n = n->next, pos++) {
        int bid = -1;
        if (has_cfg) {
            bid = dom_find_block_for_node(&cfg, n);
        }
        int depth = (bid >= 0 && bid < cfg.block_count) ? loop_depth[bid] : 0;
        int weight = 1;
        if (depth > 6) depth = 6;
        for (int d = 0; d < depth; d++) {
            weight *= 10;
        }

        /* Process destination (definition) */
        if (is_temp(n->dst)) {
            add_ref(ra, n->dst, pos, weight, 1, n->type);
        }
        /* Process source operands (uses) */
        if (is_temp(n->src1)) {
            add_ref(ra, n->src1, pos, weight, 0, n->type);
        }
        if (is_temp(n->src2)) {
            add_ref(ra, n->src2, pos, weight, 0, n->type);
        }
        
        /* If it is a copy move/phi-related, mark it */
        if (n->op == IR_COPY && is_temp(n->dst)) {
            LiveInterval *iv = find_interval(ra, n->dst);
            if (iv) iv->is_move = 1;
        }
    }

    /* Sort by start for the scan */
    qsort(ra->intervals, ra->num_intervals, sizeof(LiveInterval), iv_cmp_start);
}

/* ── Phase 2: Chaitin-Briggs Graph Coloring ──────────────────────────── */

static void chaitin_briggs(RegAllocator *ra, const ir_func_t *fn) {
    int N = ra->num_intervals;
    int i;
    int j;
    int stack_top;
    int nodes_left;
    char *adj;
    int *degree;
    int *removed;
    int *stack;
    int *alias;

    if (N <= 0) return;

    adj     = (char *)calloc(N * N, 1);
    degree  = (int *)calloc(N, sizeof(int));
    removed = (int *)calloc(N, sizeof(int));
    stack   = (int *)calloc(N, sizeof(int));
    alias   = (int *)calloc(N, sizeof(int));
    stack_top = 0;

    for (i = 0; i < N; i++) alias[i] = i;

    /* Populate adj matrix based on overlapping intervals */
    for (i = 0; i < N; i++) {
        for (j = i + 1; j < N; j++) {
            if (ra->intervals[i].is_float != ra->intervals[j].is_float) continue;
            int overlap = (ra->intervals[i].start <= ra->intervals[j].end && ra->intervals[j].start <= ra->intervals[i].end);
            if (overlap) {
                adj[i*N + j] = 1;
                adj[j*N + i] = 1;
                degree[i]++;
                degree[j]++;
            }
        }
    }

    /* Coalesce copies */
    const ir_node_t *n;
    for (n = fn->head; n; n = n->next) {
        if (n->op == IR_COPY && is_temp(n->dst) && is_temp(n->src1)) {
            int u = -1, v = -1;
            for (i = 0; i < N; i++) {
                if (strcmp(ra->intervals[i].name, n->dst) == 0) u = i;
                if (strcmp(ra->intervals[i].name, n->src1) == 0) v = i;
            }
            if (u != -1 && v != -1) {
                while(alias[u] != u) u = alias[u];
                while(alias[v] != v) v = alias[v];
                if (u != v && ra->intervals[u].is_float == ra->intervals[v].is_float && !adj[u*N + v]) {
                    /* Briggs conservative coalescing check */
                    int K = ra->intervals[u].is_float ? 8 : 7;
                    int significant_neighbors = 0;
                    for (int k = 0; k < N; k++) {
                        if (k == u || k == v) continue;
                        if (adj[u*N + k] || adj[v*N + k]) {
                            if (degree[k] >= K) {
                                significant_neighbors++;
                            }
                        }
                    }
                    if (significant_neighbors < K) {
                        /* Merge v into u */
                        alias[v] = u;
                        for (i = 0; i < N; i++) {
                            if (adj[v*N + i] && !adj[u*N + i] && u != i) {
                                adj[u*N + i] = 1;
                                adj[i*N + u] = 1;
                                degree[u]++;
                                degree[i]++;
                            }
                        }
                        removed[v] = 1;
                    }
                }
            }
        }
    }

    /* Simplify & Spill */
    nodes_left = 0;
    for (i = 0; i < N; i++) if (!removed[i]) nodes_left++;

    while (nodes_left > 0) {
        int target = -1;
        for (i = 0; i < N; i++) {
            if (removed[i]) continue;
            int K = ra->intervals[i].is_float ? 8 : 7;
            if (degree[i] < K) {
                if (target == -1 || strcmp(ra->intervals[i].name, ra->intervals[target].name) < 0) {
                    target = i;
                }
            }
        }

        if (target == -1) {
            /* Spill: pick node with highest degree / lowest cost (cost = loop_depth_weight + ref_count) */
            int max_num = -1;
            int max_den = 1;
            for (i = 0; i < N; i++) {
                if (!removed[i]) {
                    int num = degree[i];
                    int den = ra->intervals[i].loop_depth_weight;
                    if (den <= 0) den = ra->intervals[i].ref_count;
                    if (den <= 0) den = 1;
                    if (max_num == -1 || num * max_den > max_num * den) {
                        max_num = num;
                        max_den = den;
                        target = i;
                    } else if (num * max_den == max_num * den) {
                        if (target == -1 || strcmp(ra->intervals[i].name, ra->intervals[target].name) < 0) {
                            max_num = num;
                            max_den = den;
                            target = i;
                        }
                    }
                }
            }
        }

        removed[target] = 1;
        stack[stack_top++] = target;
        nodes_left--;

        for (j = 0; j < N; j++) {
            if (!removed[j] && adj[target*N + j]) {
                degree[j]--;
            }
        }
    }

    /* Select Colors */
    for (i = 0; i < N; i++) removed[i] = 0;
    {
    int *color = (int *)calloc(N, sizeof(int));
    int c;
    for (i = 0; i < N; i++) color[i] = -1;

    /* Canonical callee-save preference order matching GCC: rbx first.
     * CRITICAL: this order must be stable across ZCC stage-1 and stage-2
     * or the register coloring will produce different assembly. r10/r11 are
     * caller-saved scratch — prefer them last to minimize push/pop in hot
     * paths, but keep them after all callee-saved to maintain the canon. */
    PhysReg gpr_colors[7] = {PREG_RBX, PREG_R12, PREG_R13, PREG_R14, PREG_R15, PREG_R10, PREG_R11};
    PhysReg xmm_colors[8] = {PREG_XMM0, PREG_XMM1, PREG_XMM2, PREG_XMM3, PREG_XMM4, PREG_XMM5, PREG_XMM6, PREG_XMM7};

    while (stack_top > 0) {
        int target = stack[--stack_top];
        int K = ra->intervals[target].is_float ? 8 : 7;
        int used_colors = 0;

        for (j = 0; j < N; j++) {
            if (removed[j] && adj[target*N + j]) {
                int root = j;
                while (alias[root] != root) root = alias[root];
                int c = color[root];
                if (c != -1) used_colors |= (1 << c);
            }
        }

        c = -1;
        for (i = 0; i < K; i++) {
            if (!(used_colors & (1 << i))) {
                c = i;
                break;
            }
        }

        color[target] = c;
        removed[target] = 1;
    }

    /* Assign to struct */
    for (i = 0; i < N; i++) {
        int root = i;
        while (alias[root] != root) root = alias[root];
        c = color[root];
        if (c != -1) {
            PhysReg preg = ra->intervals[i].is_float ? xmm_colors[c] : gpr_colors[c];
            ra->intervals[i].assigned = preg;
            ra->used[preg] = 1;
        } else {
            ra->intervals[i].assigned = PREG_NONE;
        }
    }

    free(color);
    } /* end color block */

    free(adj);
    free(degree);
    free(removed);
    free(stack);
    free(alias);
}

/* ── Public entry point ──────────────────────────────────────────────── */

void ra_run(RegAllocator *ra, const ir_func_t *fn) {
    build_intervals(ra, fn);
    if (ra->num_intervals > 0)
        chaitin_briggs(ra, fn);
}

/* ── Query API ───────────────────────────────────────────────────────── */

PhysReg ra_get(const RegAllocator *ra, const char *name) {
    int i;
    if (!is_temp(name)) return PREG_NONE;
    for (i = 0; i < ra->num_intervals; i++) {
        if (strcmp(ra->intervals[i].name, name) == 0)
            return ra->intervals[i].assigned;
    }
    return PREG_NONE;
}

int ra_any_callee_saved_used(const RegAllocator *ra) {
    int r;
    for (r = 0; r < PREG_COUNT; r++) {
        if (ra->used[r] && preg_callee_saved((PhysReg)r))
            return 1;
    }
    return 0;
}
