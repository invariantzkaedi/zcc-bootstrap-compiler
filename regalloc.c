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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "src/zcc_oracle_substrate.h"

#define ABS(x) ((x) >= 0 ? (x) : -(x))

extern void zcc_telem_phase(int phase, const char *phase_name, const char *status, int duration_us,
                            const char *metric_key1, long long metric_val1,
                            const char *metric_key2, long long metric_val2,
                            const char *metric_key3, long long metric_val3);

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
    iv->loop_depth_weight = 1;
    iv->pressure_score = 1;
    iv->is_move  = 0;
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

/* ── Phase 1: Build live intervals ──────────────────────────────────── */

static void build_intervals(RegAllocator *ra, const ir_func_t *fn) {
    const ir_node_t *n;
    int pos = 0;
    int count = fn->node_count;
    int *depth = NULL;

    if (count > 0) {
        depth = (int *)calloc(count, sizeof(int));
    }

    /* Map each label's name to its pos */
    typedef struct {
        char name[IR_LABEL_MAX];
        int pos;
    } LblRecord;
    int num_labels = 0;
    int cap_labels = 128;
    LblRecord *labels = (LblRecord *)calloc(cap_labels, sizeof(LblRecord));

    pos = 0;
    for (n = fn->head; n; n = n->next, pos++) {
        if (n->op == IR_LABEL) {
            if (num_labels >= cap_labels) {
                cap_labels *= 2;
                labels = (LblRecord *)realloc(labels, cap_labels * sizeof(LblRecord));
            }
            strncpy(labels[num_labels].name, n->label, IR_LABEL_MAX - 1);
            labels[num_labels].name[IR_LABEL_MAX - 1] = '\0';
            labels[num_labels].pos = pos;
            num_labels++;
        }
    }

    /* Compute loop depths based on back-edges */
    int num_backedges = 0;
    pos = 0;
    for (n = fn->head; n; n = n->next, pos++) {
        if (n->op == IR_BR || n->op == IR_BR_IF) {
            /* Find target label pos */
            int target_pos = -1;
            int i;
            for (i = 0; i < num_labels; i++) {
                if (strcmp(labels[i].name, n->label) == 0) {
                    target_pos = labels[i].pos;
                    break;
                }
            }
            if (target_pos != -1 && target_pos < pos) {
                /* Back-edge found! Loop range is [target_pos, pos] */
                num_backedges++;
                int p;
                for (p = target_pos; p <= pos; p++) {
                    if (depth && p < count) {
                        depth[p]++;
                    }
                }
            }
        }
    }
    int complexity = num_backedges * 10 + num_labels;
    free(labels);

    pos = 0;
    for (n = fn->head; n; n = n->next, pos++) {
        int w = 1;
#if REGALLOC_LOOP_WEIGHT_ENABLED
        if (depth && pos < count) {
            int d = depth[pos];
            if (d > 4) d = 4;
            
            /* Dynamic Genetic Loop-Weight Tournament Candidates (Gen 49) */
            int c1 = 4;
            int c2 = 8;
            int c3 = 12;
            int c4 = 16;
            int c5 = 20;
            
            /* Tournament scores based on CFG complexity complexity factors */
            int score1 = 100 - (complexity * 2);
            int score2 = 120 - ABS(complexity - 15);
            int score3 = 150 - ABS(complexity - 45);
            int score4 = 180 - ABS(complexity - 100);
            int score5 = 200 - ABS(complexity - 200);
            
            /* Evolve base according to the highest scoring tournament candidate */
            int max_score = score1;
            int base = c1;
            if (score2 > max_score) { max_score = score2; base = c2; }
            if (score3 > max_score) { max_score = score3; base = c3; }
            if (score4 > max_score) { max_score = score4; base = c4; }
            if (score5 > max_score) { max_score = score5; base = c5; }
            
            while (d > 0) {
                w *= base;
                d--;
            }
        }
#endif

        /* Process destination (definition) */
        if (is_temp(n->dst)) {
            LiveInterval *iv = get_or_create(ra, n->dst, pos);
            iv->ref_count++;
            if (w > iv->loop_depth_weight) iv->loop_depth_weight = w;
            if (n->type == IR_TY_F32 || n->type == IR_TY_F64) iv->is_float = 1;
            /* definitions extend end too (covers single-use temps) */
            if (pos > iv->end) iv->end = pos;
        }
        /* Process source operands (uses) */
        if (is_temp(n->src1)) {
            LiveInterval *iv = get_or_create(ra, n->src1, pos);
            iv->ref_count++;
            if (w > iv->loop_depth_weight) iv->loop_depth_weight = w;
            if (pos > iv->end) iv->end = pos;
        }
        if (is_temp(n->src2)) {
            LiveInterval *iv = get_or_create(ra, n->src2, pos);
            iv->ref_count++;
            if (w > iv->loop_depth_weight) iv->loop_depth_weight = w;
            if (pos > iv->end) iv->end = pos;
        }
        if (n->op == IR_PHI) {
            int idx;
            for (idx = 0; idx < n->phi_count; idx++) {
                if (is_temp(n->phi_ops[idx].value)) {
                    LiveInterval *iv = get_or_create(ra, n->phi_ops[idx].value, pos);
                    iv->ref_count++;
                    if (w > iv->loop_depth_weight) iv->loop_depth_weight = w;
                    if (pos > iv->end) iv->end = pos;
                }
            }
        }

        /* Flag move-related (copy/phi) nodes */
        if (n->op == IR_COPY) {
            if (is_temp(n->dst)) {
                LiveInterval *iv = get_or_create(ra, n->dst, pos);
                iv->is_move = 1;
            }
            if (is_temp(n->src1)) {
                LiveInterval *iv = get_or_create(ra, n->src1, pos);
                iv->is_move = 1;
            }
        } else if (n->op == IR_PHI) {
            if (is_temp(n->dst)) {
                LiveInterval *iv = get_or_create(ra, n->dst, pos);
                iv->is_move = 1;
            }
            int idx;
            for (idx = 0; idx < n->phi_count; idx++) {
                if (is_temp(n->phi_ops[idx].value)) {
                    LiveInterval *iv = get_or_create(ra, n->phi_ops[idx].value, pos);
                    iv->is_move = 1;
                }
            }
        }
    }

    if (depth) free(depth);

    /* Compute dynamic register pressure at each position index */
    int *pressure = NULL;
    if (count > 0) {
        pressure = (int *)calloc(count, sizeof(int));
    }
    int i;
    for (i = 0; i < ra->num_intervals; i++) {
        int p;
        for (p = ra->intervals[i].start; p <= ra->intervals[i].end; p++) {
            if (pressure && p < count) {
                pressure[p]++;
            }
        }
    }

    /* Assign peak pressure_score to each interval */
    for (i = 0; i < ra->num_intervals; i++) {
        int max_p = 1;
        int p;
        for (p = ra->intervals[i].start; p <= ra->intervals[i].end; p++) {
            if (pressure && p < count && pressure[p] > max_p) {
                max_p = pressure[p];
            }
        }
        ra->intervals[i].pressure_score = max_p;
    }

    int global_max = 1;
    if (pressure && count > 0) {
        int p;
        for (p = 0; p < count; p++) {
            if (pressure[p] > global_max) {
                global_max = pressure[p];
            }
        }
    }
    ra->global_peak_pressure = global_max;

    if (pressure) free(pressure);

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

#if REGALLOC_COALESCING_ENABLED
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
                if (u != v) {
                    /* Conservative Coalescing (Gen 46): only merge if it preserves colorability */
                    int K = ra->intervals[u].is_float ? FLOAT_K : GPR_K;
                    if (degree[u] < K && degree[v] < K) {
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
        } else if (n->op == IR_PHI && is_temp(n->dst)) {
            int u = -1;
            for (i = 0; i < N; i++) {
                if (strcmp(ra->intervals[i].name, n->dst) == 0) u = i;
            }
            if (u != -1) {
                int idx;
                for (idx = 0; idx < n->phi_count; idx++) {
                    if (is_temp(n->phi_ops[idx].value)) {
                        int v = -1;
                        for (i = 0; i < N; i++) {
                            if (strcmp(ra->intervals[i].name, n->phi_ops[idx].value) == 0) v = i;
                        }
                        if (v != -1) {
                            int root_u = u;
                            int root_v = v;
                            while(alias[root_u] != root_u) root_u = alias[root_u];
                            while(alias[root_v] != root_v) root_v = alias[root_v];
                            if (root_u != root_v) {
                                int K = ra->intervals[root_u].is_float ? FLOAT_K : GPR_K;
                                if (degree[root_u] < K && degree[root_v] < K) {
                                    /* Merge root_v into root_u */
                                    alias[root_v] = root_u;
                                    for (i = 0; i < N; i++) {
                                        if (adj[root_v*N + i] && !adj[root_u*N + i] && root_u != i) {
                                            adj[root_u*N + i] = 1;
                                            adj[i*N + root_u] = 1;
                                            degree[root_u]++;
                                            degree[i]++;
                                        }
                                    }
                                    removed[root_v] = 1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
#endif

    /* Simplify & Spill */
    nodes_left = 0;
    for (i = 0; i < N; i++) if (!removed[i]) nodes_left++;

    while (nodes_left > 0) {
        int target = -1;
        for (i = 0; i < N; i++) {
            if (removed[i]) continue;
            int K = ra->intervals[i].is_float ? FLOAT_K : GPR_K;
            if (degree[i] < K) {
                if (target == -1 || strcmp(ra->intervals[i].name, ra->intervals[target].name) < 0) {
                    target = i;
                }
            }
        }

        if (target == -1) {
            /* Spill: pick node with highest degree / lowest cost */
            uint64_t max_num = 0;
            uint64_t max_den = 1;
            int has_max = 0;
            for (i = 0; i < N; i++) {
                if (!removed[i]) {
                    uint64_t num = degree[i];
                    uint64_t den = (uint64_t)ra->intervals[i].ref_count * 
                                   ra->intervals[i].loop_depth_weight * 
                                   ra->intervals[i].pressure_score;
                    
#if REGALLOC_PRESSURE_WEIGHT_ENABLED
                    /* Global Pressure Tournament local-to-global relative factor (Gen 49) */
                    int global_pressure = ra->global_peak_pressure > 0 ? ra->global_peak_pressure : 1;
                    int local_pressure_factor = (ra->intervals[i].pressure_score * 8) / global_pressure + 1;
                    den *= local_pressure_factor;
#endif

                    if (ra->intervals[i].is_move) {
                        den *= 2;
                    }
                    if (den <= 0) den = 1;
                    if (!has_max || num * max_den > max_num * den) {
                        max_num = num;
                        max_den = den;
                        target = i;
                        has_max = 1;
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
        int K = ra->intervals[target].is_float ? FLOAT_K : GPR_K;
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
            {
                char details[256];
                sprintf(details, "Allocated %s to register %s", ra->intervals[i].name, preg_name(preg));
                record_transform("regalloc", (uint64_t)i, details);
            }
        } else {
            ra->intervals[i].assigned = PREG_NONE;
            {
                char details[256];
                sprintf(details, "Spilled %s to stack slot", ra->intervals[i].name);
                record_transform("regalloc", (uint64_t)i, details);
            }
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
    record_pass_begin("regalloc");
    uint64_t hash_pre = compute_cfg_topology_hash((void *)fn);
    build_intervals(ra, fn);
    if (ra->num_intervals > 0)
        chaitin_briggs(ra, fn);
    uint64_t hash_post = compute_cfg_topology_hash((void *)fn);
    assert_cfg_invariance("regalloc", hash_pre, hash_post);
    record_proof("cfg_topology_invariance", "regalloc", hash_pre, 1, 0, 1, 1);

    /* Phase 1 Fusion: Broadcast register allocation telemetry over UDP to God's Eye! */
    zcc_telem_phase(5, "regalloc", fn->name, 0,
                    "intervals", (long long)ra->num_intervals,
                    "peak_pressure", (long long)ra->global_peak_pressure,
                    "any_callee_saved", (long long)ra_any_callee_saved_used(ra));

    record_pass_end("regalloc");
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
