#include "zcc_ir_opt_passes.h"
#include "zcc_opt_metrics.h"
#include "zcc_ir_verify.h"

bool opt_cfg_simplify_pass(Function *fn, OptMetricsSink *metrics) {
    bool changed = false;

    // 1) mark reachable from entry
    // 2) remove unreachable BBs
    // 3) fix preds/succs lists
    // 4) drop PHI incoming from removed preds
    // 5) collapse trivial jump-only blocks when legal
    // 6) rebuild block ids/maps if required by backend invariants

    // if anything changed:
    //   rebuild_def_use(fn);
    //   recompute_domtree_if_cached(fn);

    return changed;
}
