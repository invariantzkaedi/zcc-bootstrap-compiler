// ir_pass_taint.c — Flow-sensitive taint propagation for EVM FFI boundaries
#include "ir.h"
#include "ir_pass_taint.h"
#include "evm_lifter.h"
#include "ir_vuln_tag.h"

typedef struct {
    ir_bitset_t *taint;        // per-variable taint set
    int          tainted_host_context;
} taint_state_t;

static void taint_propagate_node(ir_node_t* n, taint_state_t* state) {
    switch (n->op) {
        case IR_CONST:
            if (n->tag == IR_TAG_HOST_CONTEXT) {
                state->tainted_host_context = 1;           // source of host context
                ir_bitset_set(state->taint, n->dst);
            }
            break;

        case IR_LOAD:
            if (n->tag == IR_TAG_CALLDATALOAD) {
                state->tainted_host_context = 1;
                ir_bitset_set(state->taint, n->dst);
            } else if (n->label[0] && strcmp(n->label, "__evm_sload") == 0) {
                if (ir_bitset_test(state->taint, n->src1)) {
                    ir_bitset_set(state->taint, n->dst);   // tainted key → tainted value
                }
            } else {
                // generic load
                if (ir_bitset_test(state->taint, n->src1)) {
                    ir_bitset_set(state->taint, n->dst);
                }
            }
            break;

        case IR_CALL:
            if (n->tag == IR_TAG_UNTRUSTED_EXTERNAL_CALL && (n->vuln_tags & IR_VULN_DELEGATE_CALL)) {
                if (ir_bitset_test(state->taint, n->label)) {  // target address tainted (n->label holds addr variable in EVM lifter)
                    ir_vuln_tag_set(n, IR_VULN_PRIV_BOUNDARY);
                    printf("[TAINT] DELEGATECALL target tainted by host context → IR_VULN_PRIV_BOUNDARY\n");
                }
            } else {
                if (ir_bitset_test(state->taint, n->label) || ir_bitset_test(state->taint, n->src1) || ir_bitset_test(state->taint, n->src2)) {
                    ir_bitset_set(state->taint, n->dst);
                }
            }
            break;

        case IR_STORE:
            if (n->tag == IR_TAG_SSTORE) {
                if (ir_bitset_test(state->taint, n->src1) && state->tainted_host_context) {
                    ir_vuln_tag_set(n, IR_VULN_PRIV_BOUNDARY);
                    printf("[TAINT] SSTORE of tainted host value → IR_VULN_PRIV_BOUNDARY\n");
                }
            }
            break;

        case IR_PHI:
            // PHI merges taint from all predecessors
            for (int i = 0; i < n->phi_count; i++) {
                if (ir_bitset_test(state->taint, n->phi_ops[i].value)) {
                    ir_bitset_set(state->taint, n->dst);
                    break;
                }
            }
            break;

        default:
            // Default propagation: if any source is tainted, destination is tainted
            if ((n->src1[0] && ir_bitset_test(state->taint, n->src1)) ||
                (n->src2[0] && ir_bitset_test(state->taint, n->src2))) {
                ir_bitset_set(state->taint, n->dst);
            }
            break;
    }
}

ir_pass_result_t ir_pass_taint_propagate(void* fn_ptr) {
    ir_func_t* func = (ir_func_t*)fn_ptr;
    ir_pass_result_t r;
    memset(&r, 0, sizeof(r));
    r.nodes_before = func->node_count;

    taint_state_t state;
    state.taint = ir_bitset_create(func->tmp_counter + 64);
    state.tainted_host_context = 0;

    for (ir_node_t* n = func->head; n; n = n->next) {
        taint_propagate_node(n, &state);
    }

    ir_bitset_free(state.taint);
    r.nodes_after = func->node_count;
    return r;
}
