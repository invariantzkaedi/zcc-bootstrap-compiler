#include "zcc_ir_opt_passes.h"
#include "zcc_sccp.h"
#include "zcc_opt_metrics.h"
#include "zcc_ir_verify.h"

static SccpValue sccp_eval_instr(Function *fn, SccpState *st, Instr *it);
static SccpValue sccp_meet(SccpValue a, SccpValue b);
static bool sccp_mark_edge_exec(SccpState *st, int edge_id);
static void sccp_enqueue_users(Function *fn, SccpState *st, int vreg);
static bool sccp_rewrite_constants(Function *fn, SccpState *st);

bool opt_sccp_pass(Function *fn, OptMetricsSink *metrics) {
    const int instr_before = fn_count_instructions(fn);
    const int blocks_before = fn_count_blocks(fn);
    const int64_t t0 = zcc_now_us();

    bool changed = false;

    SccpState st = {0};
    sccp_init_state(fn, &st); // allocate value_of[0..n_regs), exec bitsets, wl buffers

    // seed
    sccp_mark_block_executable(&st, fn->entry_bb_id);
    sccp_seed_entry_edges(fn, &st);

    while (!sccp_cfg_wl_empty(&st) || !sccp_ssa_wl_empty(&st)) {

        while (!sccp_cfg_wl_empty(&st)) {
            int edge_id = sccp_cfg_wl_pop(&st);
            if (!sccp_mark_edge_exec(&st, edge_id)) continue;

            int b = edge_succ(fn, edge_id);
            bool first_exec = sccp_mark_block_executable(&st, b);

            if (first_exec) {
                // visit PHIs then normal instructions
                for (Instr *it = bb_first_phi(fn, b); it; it = it->next_phi) {
                    SccpValue nv = sccp_eval_instr(fn, &st, it);
                    if (sccp_update_dst_and_enqueue(fn, &st, it->dst, nv))
                        changed = true;
                }
                for (Instr *it = bb_first_non_phi(fn, b); it; it = it->next) {
                    SccpValue nv = sccp_eval_instr(fn, &st, it);
                    if (instr_has_dst(it) &&
                        sccp_update_dst_and_enqueue(fn, &st, it->dst, nv))
                        changed = true;

                    if (is_terminator(it)) {
                        sccp_process_terminator_edges(fn, &st, it);
                    }
                }
            }
        }

        while (!sccp_ssa_wl_empty(&st)) {
            int v = sccp_ssa_wl_pop(&st);
            for (Use *u = first_use(fn, v); u; u = u->next) {
                Instr *it = u->user;
                if (!sccp_block_is_executable(&st, it->bb_id)) continue;

                SccpValue nv = sccp_eval_instr(fn, &st, it);
                if (instr_has_dst(it) &&
                    sccp_update_dst_and_enqueue(fn, &st, it->dst, nv))
                    changed = true;

                if (is_terminator(it)) {
                    sccp_process_terminator_edges(fn, &st, it);
                }
            }
        }
    }

    // Materialize
    if (sccp_rewrite_constants(fn, &st)) changed = true;
    if (opt_cfg_simplify_pass(fn, metrics)) changed = true; // unreachable cleanup

    rebuild_def_use(fn);
    sccp_destroy_state(&st);

#ifdef ZCC_ENABLE_VERIFY
    VerifyReport vr = {0};
    if (!verify_function_ir(fn, &vr)) {
        zcc_fatal_verify("sccp produced invalid IR", &vr);
    }
#endif

    const int64_t t1 = zcc_now_us();
    if (metrics) {
        opt_metrics_push(metrics, (OptPassMetricRow){
            .pass_name = "sccp",
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
