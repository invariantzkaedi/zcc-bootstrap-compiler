#include "../../ir.h"
#include "../../evm_lifter.h"
#include "../../ir_vuln_tag.h"
#include <stdio.h>
#include <string.h>

static void smt_declare_vreg(FILE *out, const char *vreg) {
    if (vreg[0] == '\0') return;
    fprintf(out, "(declare-fun %s () (_ BitVec 256))\n", vreg);
}

static const char* smt_map_op(ir_op_t op, int is_signed) {
    switch(op) {
        case IR_ADD: return "bvadd";
        case IR_SUB: return "bvsub";
        case IR_MUL: return "bvmul";
        case IR_DIV: return is_signed ? "bvsdiv" : "bvudiv";
        case IR_MOD: return is_signed ? "bvsrem" : "bvurem";
        case IR_AND: return "bvand";
        case IR_OR:  return "bvor";
        case IR_XOR: return "bvxor";
        case IR_SHL: return "bvshl";
        case IR_SHR: return is_signed ? "bvashr" : "bvlshr";
        case IR_EQ:  return "=";
        case IR_LT:  return is_signed ? "bvslt" : "bvult";
        case IR_GT:  return is_signed ? "bvsgt" : "bvugt";
        default:     return "UNKNOWN_OP";
    }
}

static void smt_print_uint256(FILE *out, uint256_t val) {
    fprintf(out, "#x%016llx%016llx%016llx%016llx", 
            (unsigned long long)val.limbs[3], (unsigned long long)val.limbs[2], (unsigned long long)val.limbs[1], (unsigned long long)val.limbs[0]);
}

static int is_declared(ir_node_t *head, ir_node_t *current) {
    for (ir_node_t *n = head; n != current; n = n->next) {
        if (n->dst[0] && strcmp(n->dst, current->dst) == 0) return 1;
    }
    return 0;
}

void export_smt2(ir_func_t* fn, const char* path) {
    FILE *out = fopen(path, "w");
    if (!out) return;

    fprintf(out, "; ZKAEDI ORACLE: SMT-LIBv2 PROOF GENERATION\n");
    fprintf(out, "(set-logic QF_BV)\n\n");

    fprintf(out, "(declare-fun path_cond_to_vuln () Bool)\n");

    // 1. Variable Declaration Pass
    for (ir_node_t *n = fn->head; n; n = n->next) {
        if (!is_declared(fn->head, n)) {
            smt_declare_vreg(out, n->dst);
        }
        if (n->op == IR_PHI && n->phi_count == 2) {
            fprintf(out, "(declare-fun path_cond_%s () Bool)\n", n->phi_ops[0].block);
        }
    }
    fprintf(out, "\n");

    // 2. Constraint Generation Pass
    for (ir_node_t *n = fn->head; n; n = n->next) {
        if (n->op == IR_CONST && n->tag == IR_TAG_TRUNCATED_WIDE_CONST) {
            fprintf(out, "(assert (= %s ", n->dst);
            smt_print_uint256(out, n->imm256);
            fprintf(out, "))\n");
        } 
        else if (n->op == IR_CONST) {
            fprintf(out, "(assert (= %s (_ bv%lld 256)))\n", n->dst, (long long)n->imm);
        }
        else if (n->op >= IR_EQ && n->op <= IR_GE) { // Comparisons
            int is_signed = (n->type == IR_TY_I64 || n->type == IR_TY_I32);
            fprintf(out, "(assert (= %s (ite (%s %s %s) (_ bv1 256) (_ bv0 256))))\n", 
                    n->dst, smt_map_op(n->op, is_signed), n->src1, n->src2);
        }
        else if (n->op >= IR_ADD && n->op <= IR_SHR) { // Arithmetic / Logic
            int is_signed = (n->type == IR_TY_I64 || n->type == IR_TY_I32);
            fprintf(out, "(assert (= %s (%s %s %s)))\n", 
                    n->dst, smt_map_op(n->op, is_signed), n->src1, n->src2);
        }
        else if (n->op == IR_PHI) {
            // Simplified ITE generation for 2-way merge blocks
            if (n->phi_count == 2) {
                fprintf(out, "(assert (= %s (ite path_cond_%s %s %s)))\n",
                        n->dst, n->phi_ops[0].block, n->phi_ops[0].value, n->phi_ops[1].value);
            }
        }
        
        // 3. Vulnerability Proving (The Target Assertion)
        if (n->vuln_tags & IR_VULN_STATE_WRITE && n->vuln_tags & IR_VULN_PRIV_BOUNDARY) {
            fprintf(out, "; TARGET: Can we trigger the Privilege Boundary State Write?\n");
            fprintf(out, "(assert path_cond_to_vuln)\n");
        }
    }

    fprintf(out, "\n(check-sat)\n");
    fprintf(out, "(get-model)\n");
    fclose(out);
    printf("[ORACLE] SMT-LIBv2 proof exported to %s\n", path);
}
