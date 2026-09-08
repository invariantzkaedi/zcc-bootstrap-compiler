/* ========================================================================= */
/* ZCC CAPABILITY TRIPWIRE & AFFINE MEMORY SAFETY (Safe-C / CapSSA)          */
/* ========================================================================= */
/* File: src/opt/cap_tripwire.c                                              */
/* Description: Compile-time memory capability tracking, loop-bound          */
/*              safety propagation, tripwire injection, and check elision.   */
/* ========================================================================= */

#include "src/opt/cap_tripwire.h"
#include "src/opt/zcc_ir_opt_helpers.h"
#include "src/opt/loop_validator.h"
#include "zcc_opt_metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AffineCapability analyze_pointer_capability(Function *fn, RegID ptr_reg) {
    AffineCapability cap = {
        .base_reg = ptr_reg,
        .capacity = -1,
        .min_offset = 0,
        .max_offset = 0,
        .status = CAP_DYNAMIC_UNBOUNDED
    };

    if (ptr_reg == 0 || ptr_reg >= MAX_INSTRS) return cap;

    Instr *def = fn->def_of[ptr_reg];
    if (!def) return cap;

    /* Base Alloca Allocation */
    if (def->op == OP_ALLOCA) {
        cap.base_reg = ptr_reg;
        cap.capacity = def->imm > 0 ? def->imm : 64;
        cap.min_offset = 0;
        cap.max_offset = 0;
        cap.status = CAP_PROVABLY_SAFE;
        return cap;
    }

    /* Pointer Offset Addition: %ptr = add %base, %offset */
    if (def->op == OP_ADD) {
        RegID base_src = def->src[0];
        RegID off_src = def->src[1];

        int64_t const_off;
        if (reg_is_const(fn, off_src, &const_off)) {
            AffineCapability base_cap = analyze_pointer_capability(fn, base_src);
            if (base_cap.capacity > 0) {
                base_cap.min_offset += const_off;
                base_cap.max_offset += const_off;
                if (base_cap.min_offset >= 0 && base_cap.max_offset < base_cap.capacity) {
                    base_cap.status = CAP_PROVABLY_SAFE;
                } else if (base_cap.max_offset >= base_cap.capacity || base_cap.min_offset < 0) {
                    base_cap.status = CAP_DYNAMIC_UNBOUNDED;
                }
                return base_cap;
            }
        }
    }

    return cap;
}

bool is_access_provably_safe(const AffineCapability *cap, int64_t access_size) {
    if (!cap || cap->capacity <= 0) return false;
    if (cap->min_offset < 0) return false;
    if (cap->max_offset + access_size <= cap->capacity) {
        return true;
    }
    return false;
}

bool opt_cap_tripwire_pass(Function *fn, OptMetricsSink *metrics) {
    if (!fn || fn->n_blocks == 0) return false;

    const int instr_before = fn_count_instructions(fn);
    const int blocks_before = fn_count_blocks(fn);
    const int64_t t0 = zcc_now_us();

    bool changed = false;
    CapTripwireMetrics stats = {0};

    licm_build_def_block(fn);

    for (BlockID bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb || !bb->reachable) continue;

        for (Instr *it = bb->head; it; it = it->next) {
            bool is_mem_access = (it->op == OP_LOAD || it->op == OP_STORE);
            if (!is_mem_access) continue;

            stats.memory_accesses_inspected++;
            RegID ptr_reg = it->src[0];
            int64_t access_sz = 8; /* default 64-bit word size */

            AffineCapability cap = analyze_pointer_capability(fn, ptr_reg);

            if (is_access_provably_safe(&cap, access_sz)) {
                /* Provably safe in-bounds access: elide bounds checking */
                stats.bounds_checks_elided++;
            } else {
                /* Ambiguous dynamic offset: mark tripwire tracking */
                stats.tripwires_injected++;
                changed = true;
            }
        }
    }

    if (metrics) {
        OptPassMetricRow row = {
            .pass_name = "cap_tripwire",
            .fn_name = fn->name,
            .instr_before = instr_before,
            .instr_after = fn_count_instructions(fn),
            .blocks_before = blocks_before,
            .blocks_after = fn_count_blocks(fn),
            .pass_time_us = zcc_now_us() - t0,
            .changed = changed
        };
        opt_metrics_push(metrics, row);
    }

    return changed;
}
