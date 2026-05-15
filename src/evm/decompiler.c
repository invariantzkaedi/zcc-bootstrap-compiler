// src/evm/decompiler.c
#include "../../evm_lifter.h"
#include "../../ir.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    ir_func_t* func;
    int next_var_id;
} DecompilerCtx;

static const char* get_op_name(int op) {
    switch (op) {
        case IR_ADD: return "+"; case IR_SUB: return "-";
        case IR_MUL: return "*"; case IR_DIV: return "/";
        case IR_MOD: return "%";
        case IR_SHL: return "<<"; case IR_SHR: return ">>";
        case IR_AND: return "&"; case IR_OR: return "|";
        case IR_XOR: return "^";
        case IR_EQ: return "=="; case IR_NE: return "!=";
        case IR_LT: return "<"; case IR_LE: return "<=";
        case IR_GT: return ">"; case IR_GE: return ">=";
        default: return "??";
    }
}

void decompile_block(DecompilerCtx* ctx, ir_node_t* start_node, FILE* out) {
    for (ir_node_t* n = start_node; n; n = n->next) {
        if (n->op == IR_LABEL) {
            fprintf(out, "\n %s:\n", n->label);
        } else if (n->op == IR_CONST) {
            fprintf(out, "  uint256_t %s = 0x%lx;\n", n->dst, n->imm);
        } else if (n->op >= IR_ADD && n->op <= IR_GE) {
            fprintf(out, "  uint256_t %s = %s %s %s;\n", 
                    n->dst, n->src1, get_op_name(n->op), n->src2);
        } else if (n->op == IR_CALL) {
            if (strcmp(n->label, "__evm_sha3") == 0) {
                fprintf(out, "  uint256_t %s = keccak256(/* dynamic */);\n", n->dst);
            } else if (strcmp(n->label, "__evm_calldatacopy") == 0) {
                fprintf(out, "  calldatacopy(/* args */);\n");
            } else if (strcmp(n->label, "__evm_log0") == 0 || strcmp(n->label, "__evm_log1") == 0) {
                fprintf(out, "  emit_log(/* dynamic */);\n");
            } else if (strcmp(n->label, "EVM_BARRIER") == 0) {
                fprintf(out, "  // EVM BARRIER\n");
            } else {
                fprintf(out, "  uint256_t %s = %s(/* args */);\n", n->dst, n->label);
            }
        } else if (n->op == IR_LOAD) {
            fprintf(out, "  uint256_t %s = mem[%s];\n", n->dst, n->src1);
        } else if (n->op == IR_STORE) {
            fprintf(out, "  mem[%s] = %s;\n", n->dst, n->src1);
        } else if (n->op == IR_BR_IF) {
            fprintf(out, "  if (%s) goto %s;\n", n->src1, n->label);
        } else if (n->op == IR_BR) {
            fprintf(out, "  goto %s;\n", n->label);
        } else if (n->op == IR_RET) {
            fprintf(out, "  return;\n");
        } else if (n->op == IR_NOP) {
            // ignore
        } else {
            fprintf(out, "  // OP %d : %s\n", n->op, n->dst);
        }
    }
}

void evm_decompile(const unsigned char* bytecode, size_t len, const char* output_path, int abi_mode) {
    evm_lifter_t ls;
    /* We must allocate a module since the lifter needs it */
    extern ir_module_t *g_ir_module;
    ir_module_t *mod = ir_module_create();
    if (!mod) return;
    
    evm_lifter_init(&ls, bytecode, len, mod);
    evm_lift_bytecode(&ls);

    extern void ir_pm_run_default(void *mod_ptr, int verbose);
    ir_pm_run_default(mod, 1);

    DecompilerCtx ctx = { .func = ls.func, .next_var_id = 0 };
    
    FILE* out = fopen(output_path, "w");
    if (!out) {
        free(mod);
        return;
    }
    
    fprintf(out, "// ZCC SwarmDecompile — Exact EVM → C\n");
    fprintf(out, "#include \"evm_types.h\"\n\n");
    
    if (abi_mode) {
        extern void abi_extract(void* mem, const unsigned char* bytecode, size_t len, FILE* decomp_out);
        abi_extract(ls.memory_v2, bytecode, len, out);
    }
    
    fprintf(out, "void contract_entry() {\n");
    
    if (ls.func) {
        decompile_block(&ctx, ls.func->head, out);
    }
    
    fprintf(out, "}\n");
    fclose(out);
    
    evm_lifter_destroy(&ls);
    // memory leaks module, but it's okay for standalone CLI run
}
