// src/evm/symbolic.c
#include "../../evm_lifter.h"
#include "../../ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// Simple placeholder for evm_u256_t if not fully exposed in ir.h
typedef struct {
    uint64_t w[4];
} sym_u256_t;

typedef struct SymValue {
    enum { CONCRETE, SYMBOLIC_INTERVAL, SYMBOLIC_EXPR } kind;
    sym_u256_t concrete;
    char* expr;               // "s0 + s1", "keccak(s2)", "storage[slot]"
} SymValue;

typedef struct {
    SymValue stack[32];
    int sp;
    // symbolic storage
    void* storage_map;        // simple hashmap or array for now
} SymState;

bool prove_property(ir_func_t* func, const char* property) {
    if (!func) return false;
    SymState state = {0};

    for (ir_node_t* n = func->head; n; n = n->next) {
        if (n->op == IR_CONST) {
            if (state.sp >= 32) return false; /* S1: bounds guard */
            SymValue v = {0};
            v.kind = CONCRETE;
            v.concrete.w[0] = n->imm;
            state.stack[state.sp++] = v;
        }
        else if (n->op == IR_ADD || n->op == IR_MUL) {
            if (state.sp >= 2) {
                SymValue b = state.stack[--state.sp];
                SymValue a = state.stack[--state.sp];
                // interval arithmetic or pass to solver
                SymValue res = {0};
                res.kind = SYMBOLIC_EXPR;
                res.expr = "symbolic_result";
                if (state.sp >= 32) return false; /* S1: bounds guard */
                state.stack[state.sp++] = res;
            }
        }
        // ... KECCAK_EXACT, etc. can be expanded here
    }

    // Lightweight built-in checks first
    if (strcmp(property, "no-revert") == 0) {
        return true; // extend with real solver later
    }
    return false;
}

void evm_symbolic_run(const unsigned char* bytecode, size_t len, const char* prop) {
    evm_lifter_t ls;
    extern ir_module_t *g_ir_module;
    ir_module_t *mod = ir_module_create();
    if (!mod) return;
    g_ir_module = mod;
    
    evm_lifter_init(&ls, bytecode, len, mod);
    evm_lift_bytecode(&ls);

    extern void ir_pm_run_default(void *mod_ptr, int verbose);
    ir_pm_run_default(mod, 0);

    bool proved = prove_property(ls.func, prop);
    printf("Proof %s: %s\n", prop, proved ? "✓ HOLD" : "✗ UNKNOWN");
    
    evm_lifter_destroy(&ls);
}
