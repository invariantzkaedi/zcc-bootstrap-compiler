/* ========================================================================= */
/* ZCC QUANTUM-CLASSICAL LOOP INVARIANT CODE MOTION (Q-LICM) PASS            */
/* ========================================================================= */
/* File: src/opt/q_licm_pass.c                                               */
/* Description: Identifies loop-invariant quantum gate sequences, basis      */
/*              rotations, and variational ansatz initializations inside     */
/*              classical control loops and hoists them into preheaders.     */
/* ========================================================================= */

#include "src/opt/q_licm_pass.h"
#include "src/opt/zcc_ir_opt_helpers.h"
#include "src/opt/loop_validator.h"
#include "zcc_opt_metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* List of standard quantum gate functions and intrinsics recognized by Q-LICM */
static const char *k_quantum_gate_prefixes[] = {
    "qgate_",
    "qasm_",
    "q_",
    "qubit_",
    "dtqw_",
    "quantum_",
    "vqe_gate_",
    "qaoa_gate_",
    "apply_gate_",
    NULL
};

static const char *k_quantum_exact_names[] = {
    "h", "x", "y", "z", "s", "sdg", "t", "tdg",
    "rx", "ry", "rz", "p", "u1", "u2", "u3",
    "cx", "cz", "cnot", "swap", "toffoli", "ccx",
    "dtqw_step", "dtqw_hadamard", "dtqw_shift",
    NULL
};

bool is_quantum_gate_instr(const Instr *it) {
    if (!it) return false;

    /* OP_CALL with quantum intrinsic / gate name */
    if (it->op == OP_CALL && it->call_name) {
        for (int i = 0; k_quantum_gate_prefixes[i]; i++) {
            if (strncmp(it->call_name, k_quantum_gate_prefixes[i], strlen(k_quantum_gate_prefixes[i])) == 0) {
                return true;
            }
        }
        for (int i = 0; k_quantum_exact_names[i]; i++) {
            if (strcmp(it->call_name, k_quantum_exact_names[i]) == 0) {
                return true;
            }
        }
    }

    return false;
}

/* Check if all input registers of instruction are defined outside the loop (or in preheader) */
bool is_instr_loop_invariant_quantum(Function *fn, const Instr *it, BlockID preheader_id, const bool *in_loop) {
    if (!it) return false;

    for (int s = 0; s < 4; s++) {
        RegID r = it->src[s];
        if (r == 0) continue;

        /* If register has no definition, it's an argument / external symbol -> invariant */
        if (r >= MAX_INSTRS || !fn->def_of[r]) continue;

        BlockID def_b = fn->def_block[r];
        if (def_b == NO_BLOCK) continue;

        /* If defined inside the loop and not in the preheader, it is loop-variant */
        if (def_b != preheader_id && in_loop[def_b]) {
            return false;
        }
    }

    return true;
}

/* Discover all blocks belonging to the natural loop given header and latch */
static void build_natural_loop_body(Function *fn, BlockID header, BlockID latch, bool *in_loop) {
    memset(in_loop, 0, sizeof(bool) * fn->n_blocks);
    in_loop[header] = true;
    in_loop[latch] = true;

    if (header == latch) return;

    BlockID *stack = malloc(fn->n_blocks * sizeof(BlockID));
    int top = 0;
    stack[top++] = latch;

    while (top > 0) {
        BlockID curr = stack[--top];
        Block *bb = fn->blocks[curr];
        if (!bb) continue;

        for (uint32_t p = 0; p < bb->n_preds; p++) {
            BlockID pred = bb->preds[p];
            if (pred < fn->n_blocks && !in_loop[pred]) {
                in_loop[pred] = true;
                stack[top++] = pred;
            }
        }
    }

    free(stack);
}

/* Unlink instruction from current block and insert before terminator in target block */
static void hoist_instruction_to_block(Block *src_bb, Block *dst_bb, Instr *ins) {
    /* Unlink from src_bb */
    if (ins->prev) {
        ins->prev->next = ins->next;
    } else {
        src_bb->head = ins->next;
    }

    if (ins->next) {
        ins->next->prev = ins->prev;
    } else {
        src_bb->tail = ins->prev;
    }
    src_bb->n_instrs--;

    /* Find insertion point in dst_bb: insert before final terminator (OP_BR / OP_CONDBR / OP_RET) */
    Instr *insert_before = NULL;
    if (dst_bb->tail && (dst_bb->tail->op == OP_BR || dst_bb->tail->op == OP_CONDBR || dst_bb->tail->op == OP_RET)) {
        insert_before = dst_bb->tail;
    }

    if (insert_before) {
        ins->next = insert_before;
        ins->prev = insert_before->prev;
        if (insert_before->prev) {
            insert_before->prev->next = ins;
        } else {
            dst_bb->head = ins;
        }
        insert_before->prev = ins;
    } else {
        /* Append at tail */
        ins->next = NULL;
        ins->prev = dst_bb->tail;
        if (dst_bb->tail) {
            dst_bb->tail->next = ins;
        } else {
            dst_bb->head = ins;
        }
        dst_bb->tail = ins;
    }
    dst_bb->n_instrs++;
}

bool opt_q_licm_pass(Function *fn, OptMetricsSink *metrics) {
    if (!fn || fn->n_blocks == 0) return false;

    const int instr_before = fn_count_instructions(fn);
    const int blocks_before = fn_count_blocks(fn);
    const int64_t t0 = zcc_now_us();

    bool changed = false;
    uint32_t total_hoisted = 0;
    bool *in_loop = malloc(fn->n_blocks * sizeof(bool));
    if (!in_loop) return false;

    /* Build def-block lookup table */
    licm_build_def_block(fn);

    /* Sweep all blocks for canonical loop headers */
    for (BlockID bi = 0; bi < fn->n_blocks; bi++) {
        Block *hdr = fn->blocks[bi];
        if (!hdr || !hdr->reachable) continue;

        LoopCanonicalInfo linfo;
        if (!opt_detect_canonical_loop(fn, bi, &linfo)) {
            continue;
        }

        BlockID ph_id = linfo.preheader;
        if (ph_id >= fn->n_blocks || !fn->blocks[ph_id]) {
            continue;
        }
        Block *ph_bb = fn->blocks[ph_id];

        build_natural_loop_body(fn, linfo.header, linfo.latch, in_loop);

        /* Iterative fixed-point hoisting for cascading quantum invariants */
        bool loop_progress = true;
        while (loop_progress) {
            loop_progress = false;

            for (BlockID b = 0; b < fn->n_blocks; b++) {
                if (!in_loop[b] || b == ph_id) continue;
                Block *bb = fn->blocks[b];
                if (!bb) continue;

                Instr *ins = bb->head;
                while (ins) {
                    Instr *next = ins->next;

                    if (is_quantum_gate_instr(ins)) {
                        if (is_instr_loop_invariant_quantum(fn, ins, ph_id, in_loop)) {
                            hoist_instruction_to_block(bb, ph_bb, ins);
                            if (ins->dst && ins->dst < MAX_INSTRS) {
                                fn->def_block[ins->dst] = ph_id;
                            }
                            total_hoisted++;
                            loop_progress = true;
                            changed = true;
                        }
                    }
                    ins = next;
                }
            }
        }
    }

    free(in_loop);

    fn->stats.licm_hoisted += total_hoisted;

    if (metrics) {
        OptPassMetricRow row = {
            .pass_name = "q_licm",
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
