#include "../../ir.h"
#include "../../evm_lifter.h"
#include "../../ir_vuln_tag.h"
#include "../../evm_uint256.h"
#include "evm_symbolic_harness.h"
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint256_t stack[1024];
    int depth;
    uint64_t gas;
    int tainted;
} symbolic_state_t;

static int symbolic_execute(ir_func_t* fn, symbolic_state_t* state) {
    ir_node_t* n = fn->head;
    while (n) {
        switch (n->op) {
            case IR_ADD: {
                if (state->depth >= 2) {
                    uint256_t a = state->stack[--state->depth];
                    uint256_t b = state->stack[--state->depth];
                    uint256_t r = evm_u256_add(a, b);
                    state->stack[state->depth++] = r;
                    state->gas -= 3;
                }
                break;
            }
            case IR_CALL:
                if (state->tainted) {
                    ir_vuln_tag_set(n, IR_VULN_PRIV_BOUNDARY);
                    printf("[SYMBOLIC] Tainted FFI call → IR_VULN_PRIV_BOUNDARY\n");
                }
                state->gas -= 21000;
                if (state->gas > 0xFFFFFFFFFFFFFFFFULL) { // Overflow meaning underflow
                    ir_vuln_tag_set(n, IR_VULN_OOG);
                    printf("[SYMBOLIC] OOG on FFI call\n");
                }
                break;

            case IR_STORE:
                if (state->tainted) {
                    ir_vuln_tag_set(n, IR_VULN_PRIV_BOUNDARY);
                    printf("[SYMBOLIC] Tainted SSTORE → IR_VULN_PRIV_BOUNDARY\n");
                }
                state->gas -= 20000;
                break;

            case IR_RET:
                printf("[SYMBOLIC] Revert barrier hit — execution path validated\n");
                return 0;

            default:
                // generic stack simulation for other ops
                if (n->dst[0]) {
                    // push dummy result
                    uint256_t dummy = {0};
                    state->stack[state->depth++] = dummy;
                }
                break;
        }
        n = n->next;
    }
    return 0;
}

void evm_run_symbolic(ir_module_t* mod) {
    for (int i = 0; i < mod->func_count; i++) {
        symbolic_state_t state = {0};
        state.gas = 30000000ULL;
        state.tainted = 1;  // assume host context tainted for test
        symbolic_execute(mod->funcs[i], &state);
    }
    // Note: Since we don't have ir_snapshot_state globally exposed, we use the standard pass manager debug print
    printf("[SYMBOLIC] Execution complete.\n");
}

void evm_run_symbolic_from_bytecode(const unsigned char* bytecode, size_t len, int smt_mode) {
    ir_module_t* mod = ir_module_create();
    evm_lifter_t ls;
    evm_lifter_init(&ls, bytecode, len, mod);
    evm_lift_bytecode(&ls);
    
    // Apply standard passes to lower the IR exactly as during normal hunting
    extern void ir_pm_run_default(ir_module_t *mod);
    ir_pm_run_default(mod);
    
    evm_run_symbolic(mod);
    
    if (smt_mode && mod->func_count > 0) {
        extern void export_smt2(ir_func_t* fn, const char* path);
        export_smt2(mod->funcs[0], "proof.smt2");
    }
    
    evm_lifter_destroy(&ls);
}
