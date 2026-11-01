/*
 * ir_pass_manager.h — ZCC IR Pass Manager API
 * ==============================================
 * Defines the pass function signature, registry, and runner API.
 * This header is included by ir_pass_manager.c (compiled separately by GCC)
 * and forward-declared in part5.c (concatenated into zcc.c).
 */

#ifndef ZCC_IR_PASS_MANAGER_H
#define ZCC_IR_PASS_MANAGER_H

/* ── Pass result metrics ─────────────────────────────────────────────── */
typedef struct {
    int nodes_before;     /* node count entering the pass            */
    int nodes_after;      /* node count leaving the pass             */
    int nodes_deleted;    /* nodes removed (DCE, unreachable)        */
    int nodes_modified;   /* nodes mutated (const fold, strength)    */
    int changed;          /* non-zero if anything changed            */
} ir_pass_result_t;

/* ── Pass function type ──────────────────────────────────────────────── */
/*
 * Every pass takes an ir_func_t*, mutates its node list IN PLACE,
 * and returns metrics.  Passes never allocate new functions.
 */
typedef ir_pass_result_t (*ir_pass_fn)(void *fn_ptr);

/* ── Pass registry ───────────────────────────────────────────────────── */
#define IR_PM_MAX_PASSES 16

typedef struct {
    const char    *name;        /* human-readable: "dce", "constfold" */
    ir_pass_fn     fn;          /* the pass function                   */
    int            enabled;     /* 1 = active, 0 = skip                */
} ir_pass_entry_t;

typedef struct {
    ir_pass_entry_t passes[IR_PM_MAX_PASSES];
    int             count;
    int             verbose;    /* print per-pass metrics to stderr    */
    struct ir_manifold *manifold; /* NULL when --manifold not active */
} ir_pass_manager_t;

/* ── API ─────────────────────────────────────────────────────────────── */

/* Run the default pass pipeline (DCE → fold → reduce → DCE) on module.
 * This is the primary entry point called from part5.c.
 * Parameters are void* to avoid requiring ir.h in the caller. */
void ir_pm_run_default(void *mod_ptr, int verbose);

#endif /* ZCC_IR_PASS_MANAGER_H */
