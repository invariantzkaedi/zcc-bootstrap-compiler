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

/* ── Physical register table ─────────────────────────────────────────── */

static const char *preg_names[PREG_COUNT] = {
    "rbx", "r12", "r13", "r14", "r15",  /* callee-saved */
    "r10", "r11"                          /* caller-saved scratch */
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
    return ia->start - ib->start;
}

/* ── Phase 1: Build live intervals ──────────────────────────────────── */

static void build_intervals(RegAllocator *ra, const ir_func_t *fn) {
    const ir_node_t *n;
    int pos = 0;

    for (n = fn->head; n; n = n->next, pos++) {
        /* Process destination (definition) */
        if (is_temp(n->dst)) {
            LiveInterval *iv = get_or_create(ra, n->dst, pos);
            /* definitions extend end too (covers single-use temps) */
            if (pos > iv->end) iv->end = pos;
        }
        /* Process source operands (uses) */
        if (is_temp(n->src1)) {
            LiveInterval *iv = get_or_create(ra, n->src1, pos);
            if (pos > iv->end) iv->end = pos;
        }
        if (is_temp(n->src2)) {
            LiveInterval *iv = get_or_create(ra, n->src2, pos);
            if (pos > iv->end) iv->end = pos;
        }
    }

    /* Sort by start for the scan */
    qsort(ra->intervals, ra->num_intervals, sizeof(LiveInterval), iv_cmp_start);
}

/* ── Phase 2: Linear scan allocation ────────────────────────────────── */

/*
 * Classic Poletto & Sarkar linear scan.
 * Active = set of intervals currently occupying a physical register.
 * We maintain active[] sorted by end point (earliest-ending first) so
 * expiry and spill decisions are O(k) where k = PREG_COUNT (small constant).
 */

static void linear_scan(RegAllocator *ra) {
    /* active[i] = index into ra->intervals of an allocated interval */
    int active[PREG_COUNT];
    int active_reg[PREG_COUNT]; /* physical register used by active[i] */
    int active_cnt = 0;

    /* Free-register stack */
    PhysReg free_regs[PREG_COUNT];
    int free_cnt = 0;
    int r;
    for (r = 0; r < PREG_COUNT; r++)
        free_regs[free_cnt++] = (PhysReg)(PREG_COUNT - 1 - r); /* push in reverse so rbx is tried first */

    int i;
    for (i = 0; i < ra->num_intervals; i++) {
        LiveInterval *cur = &ra->intervals[i];

        /* --- Expire old intervals --- */
        int j = 0;
        while (j < active_cnt) {
            LiveInterval *act = &ra->intervals[active[j]];
            if (act->end < cur->start) {
                /* Return this register to the free pool */
                free_regs[free_cnt++] = (PhysReg)active_reg[j];
                /* Compact active[] */
                active[j]     = active[active_cnt - 1];
                active_reg[j] = active_reg[active_cnt - 1];
                active_cnt--;
                /* Don't advance j — need to recheck this slot */
            } else {
                j++;
            }
        }

        /* --- Allocate or spill --- */
        if (free_cnt > 0) {
            /* Grab a free register */
            PhysReg preg = free_regs[--free_cnt];
            cur->assigned = preg;
            ra->used[preg] = 1;

            /* Add to active */
            active[active_cnt]     = i;
            active_reg[active_cnt] = (int)preg;
            active_cnt++;
        } else {
            /*
             * No free register. Spill the interval that ends latest
             * (keep the one ending soonest in a register — it frees up
             * sooner and benefits more instructions).
             */
            int spill_idx = -1;
            int spill_ai  = -1;
            int latest_end = cur->end;

            for (j = 0; j < active_cnt; j++) {
                LiveInterval *act = &ra->intervals[active[j]];
                if (act->end > latest_end) {
                    latest_end = act->end;
                    spill_idx  = active[j];
                    spill_ai   = j;
                }
            }

            if (spill_idx >= 0) {
                /* The active interval ending latest gives its register to cur */
                PhysReg stolen = (PhysReg)active_reg[spill_ai];
                ra->intervals[spill_idx].assigned = PREG_NONE; /* spill it */
                cur->assigned = stolen;

                active[spill_ai]     = i;
                active_reg[spill_ai] = (int)stolen;
            }
            /* else cur->assigned stays PREG_NONE (spilled) */
        }
    }
}

/* ── Public entry point ──────────────────────────────────────────────── */

void ra_run(RegAllocator *ra, const ir_func_t *fn) {
    build_intervals(ra, fn);
    if (ra->num_intervals > 0)
        linear_scan(ra);
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
