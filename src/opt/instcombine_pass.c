#include "zcc_ir_opt_passes.h"
#include "zcc_opt_metrics.h"
#include "zcc_ir_verify.h"

static bool ic_try_fold_identity(Function *fn, Instr *it);
static bool ic_try_fold_reassoc(Function *fn, Instr *it);
static bool ic_try_fold_cmp(Function *fn, Instr *it);

bool opt_instcombine_pass(Function *fn, OptMetricsSink *metrics) {
    const int instr_before = fn_count_instructions(fn);
    const int blocks_before = fn_count_blocks(fn);
    const int64_t t0 = zcc_now_us();

    bool changed = false;
    int rewrites = 0;
    const int REWRITE_CAP = 10000;

    for (BasicBlock *bb = fn->first_bb; bb; bb = bb->next) {
        for (Instr *it = bb->first; it; ) {
            Instr *next = it->next; // safe against erase
            bool local = false;

            local |= ic_try_fold_identity(fn, it);
            if (!local) local |= ic_try_fold_reassoc(fn, it);
            if (!local) local |= ic_try_fold_cmp(fn, it);

            if (local) {
                changed = true;
                rewrites++;
                if (rewrites >= REWRITE_CAP) goto done;
            }
            it = next;
        }
    }

done:
    if (changed) {
        rebuild_def_use(fn);   // or incremental updater if available
    }

#ifdef ZCC_ENABLE_VERIFY
    VerifyReport vr = {0};
    if (!verify_function_ir(fn, &vr)) {
        zcc_fatal_verify("instcombine produced invalid IR", &vr);
    }
#endif

    const int64_t t1 = zcc_now_us();
    if (metrics) {
        opt_metrics_push(metrics, (OptPassMetricRow){
            .pass_name = "instcombine",
            .fn_name = fn->name,
            .instr_before = instr_before,
            .instr_after = fn_count_instructions(fn),
            .blocks_before = blocks_before,
            .blocks_after = fn_count_blocks(fn),
            .pass_time_us = (t1 - t0),
            .changed = changed
        });
    }
    return changed;
}
