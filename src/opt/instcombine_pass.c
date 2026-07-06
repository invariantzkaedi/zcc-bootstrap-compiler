#include "zcc_ir.h"
#include "zcc_ir_opt_helpers.h"
#include "zcc_ir_opt_passes.h"
#include "zcc_opt_metrics.h"
#include "zcc_ir_verify.h"

typedef struct {
    Function *fn;
    Instr *it;
} ICtx;

bool ic_try_rules(Function *fn, Instr *it);

Instr g_ic_snapshot[MAX_INSTRS];

bool opt_instcombine_pass(Function *fn, OptMetricsSink *metrics) {
    (void)metrics;
    const int instr_before = fn_count_instructions(fn);
    const int blocks_before = fn_count_blocks(fn);
    const int64_t t0 = zcc_now_us();

    rebuild_def_use(fn);
    memset(g_ic_snapshot, 0, sizeof(g_ic_snapshot));
    for (int i = 0; i < MAX_INSTRS; i++) {
        if (fn->def_of[i]) {
            g_ic_snapshot[i] = *(fn->def_of[i]);
        } else {
            g_ic_snapshot[i].op = OP_NOP;
        }
    }
    bool changed = false;
    int rewrites = 0;
    const int REWRITE_CAP = 10000;

    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;
        for (Instr *it = bb->head; it; ) {
            Instr *next = it->next; // safe against erase
            bool local = ic_try_rules(fn, it);

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
        rebuild_def_use(fn);
    }

    VerifyReport vr = {
        .ok = true,
        .errors = NULL,
        .n_errors = 0,
        .cap_errors = 0
    };
    verify_function_ir(fn, &vr);

    return changed;
}
